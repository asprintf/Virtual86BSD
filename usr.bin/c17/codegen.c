/*
 * Copyright (c) 2026, Joost Orchidee Winter
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* codegen.c - x86-64 System V code generation (GNU as syntax) */
#include "c17.h"

#define GP_MAX 6
#define FP_MAX 8

static FILE *output_file;
static int depth;      /* number of 8-byte slots pushed on the stack */
static char *argreg8[] = {"%dil", "%sil", "%dl", "%cl", "%r8b", "%r9b"};
static char *argreg16[] = {"%di", "%si", "%dx", "%cx", "%r8w", "%r9w"};
static char *argreg32[] = {"%edi", "%esi", "%edx", "%ecx", "%r8d", "%r9d"};
static char *argreg64[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
static Obj *current_fn;
static int64_t struct_ret_offset;  /* slot holding the hidden return pointer */

static void gen_expr(Node *node);
static void gen_stmt(Node *node);

/* Values represented by their address: structs, unions, complex. */
static bool is_aggval(Type *ty) { return is_su(ty) || ty->is_complex; }
/* Scalars held in a register */
static bool is_regval(Type *ty) { return is_scalar(ty) && !ty->is_complex; }
/* For ABI switches: complex types classify like structs */
static TypeKind abi_kind(Type *ty) { return ty->is_complex ? TY_STRUCT : ty->kind; }
static void gen_discard(Node *node);

PRINTF(1, 2)
static void println(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(output_file, fmt, ap);
  va_end(ap);
  fprintf(output_file, "\n");
}

static int count(void) {
  static int i = 1;
  return i++;
}

static void push(void) {
  println("  push %%rax");
  depth++;
}

static void pop(char *arg) {
  println("  pop %s", arg);
  depth--;
}

static void pushf(void) {
  println("  sub $8, %%rsp");
  println("  movsd %%xmm0, (%%rsp)");
  depth++;
}

static void popf(int reg) {
  println("  movsd (%%rsp), %%xmm%d", reg);
  println("  add $8, %%rsp");
  depth--;
}

/* long double: 16 bytes on the stack */
static void push_ld(void) {
  println("  sub $16, %%rsp");
  println("  fstpt (%%rsp)");
  depth += 2;
}

static void pop_ld(void) {
  println("  fldt (%%rsp)");
  println("  add $16, %%rsp");
  depth -= 2;
}

int align_to(int64_t n, int64_t align) { return (int)((n + align - 1) / align * align); }
int64_t align_to64(int64_t n, int64_t align) {
  if (align <= 0)
    return n;
  return (n + align - 1) / align * align;
}

static char *reg_dx(int sz) {
  switch (sz) {
  case 1: return "%dl";
  case 2: return "%dx";
  case 4: return "%edx";
  case 8: return "%rdx";
  }
  unreachable();
}

static char *reg_ax(int sz) {
  switch (sz) {
  case 1: return "%al";
  case 2: return "%ax";
  case 4: return "%eax";
  case 8: return "%rax";
  }
  unreachable();
}

/* ------------------------------------------------------------ -g */

static HashMap file_numbers;
static int nfile_numbers;
static char *last_loc_file;
static int last_loc_line;

static void emit_loc(Token *tok) {
  if (!opt_g || !tok)
    return;
  while (tok->origin)
    tok = tok->origin;
  if (!tok->file)
    return;
  char *fname = tok_filename(tok);
  int line = tok_line(tok);
  if (line <= 0)
    return;
  if (last_loc_file == fname && last_loc_line == line)
    return;
  intptr_t n = (intptr_t)hashmap_get(&file_numbers, fname);
  if (!n) {
    n = ++nfile_numbers;
    hashmap_put(&file_numbers, fname, (void *)n);
    fprintf(output_file, "  .file %d \"", (int)n);
    for (char *p = fname; *p; p++) {
      if (*p == '"' || *p == '\\')
        fputc('\\', output_file);
      fputc(*p, output_file);
    }
    fprintf(output_file, "\"\n");
  }
  println("  .loc %d %d", (int)n, line);
  last_loc_file = fname;
  last_loc_line = line;
}

/* ------------------------------------------------------- symbols */

static char *sym(Obj *var) { return var->asm_name ? var->asm_name : var->name; }

/* Is var defined in this shared object whatever happens: static, or of
   hidden (or internal) visibility? */
static bool is_local_def(Obj *var) {
  return var->is_static || (var->visibility && (!strcmp(var->visibility, "hidden") ||
                                                !strcmp(var->visibility, "internal")));
}

/* Does a reference to var need to go through the GOT? */
static bool use_got(Obj *var) {
  if (!opt_pic)
    return false;
  return !is_local_def(var);
}

/* Computes the address of a TLS variable into %rax. */
static void gen_tls_addr(Obj *var) {
  if (opt_pic && !var->tls_ie) {
    if (var->is_static) {
      /* local-dynamic simplified as general dynamic */
    }
    bool odd = depth % 2;
    if (odd)
      println("  sub $8, %%rsp");
    println("  .byte 0x66");
    println("  lea %s@tlsgd(%%rip), %%rdi", sym(var));
    println("  .value 0x6666");
    println("  rex64");
    println("  call __tls_get_addr@PLT");
    if (odd)
      println("  add $8, %%rsp");
    return;
  }
  if (!opt_pic && (var->is_static || var->is_definition)) {
    println("  mov %%fs:0, %%rax");
    println("  lea %s@tpoff(%%rax), %%rax", sym(var));
    return;
  }
  /* initial-exec (in a shared library also for its own variables) */
  println("  mov %s@gottpoff(%%rip), %%rax", sym(var));
  println("  add %%fs:0, %%rax");
}

/* --------------------------------------------------- address */

static void gen_addr(Node *node) {
  switch (node->kind) {
  case ND_VAR:
    if (node->var->is_local) {
      if (node->var->is_vla_ptr) {
        println("  mov %" PRId64 "(%%rbp), %%rax", node->var->offset);
        return;
      }
      println("  lea %" PRId64 "(%%rbp), %%rax", node->var->offset);
      return;
    }
    if (node->var->is_tls) {
      gen_tls_addr(node->var);
      return;
    }
    if (use_got(node->var)) {
      println("  mov %s@GOTPCREL(%%rip), %%rax", sym(node->var));
      return;
    }
    println("  lea %s(%%rip), %%rax", sym(node->var));
    return;
  case ND_VLA_PTR:
    println("  lea %" PRId64 "(%%rbp), %%rax", node->var->offset);
    return;
  case ND_DEREF:
    gen_expr(node->lhs);
    return;
  case ND_COMMA:
    gen_discard(node->lhs);
    gen_addr(node->rhs);
    return;
  case ND_MEMBER:
    gen_addr(node->lhs);
    if (node->member->offset)
      println("  add $%" PRId64 ", %%rax", node->member->offset);
    return;
  case ND_CAST:
    if (node->ty->is_complex) {
      gen_expr(node);
      return;
    }
    if (is_su(node->ty) || is_array(node->ty)) {
      gen_addr(node->lhs);
      return;
    }
    break;
  case ND_FUNCALL:
  case ND_ASSIGN:
  case ND_COND:
  case ND_STMT_EXPR:
  case ND_VA_ARG:
    if (is_aggval(node->ty) || is_array(node->ty)) {
      gen_expr(node);
      return;
    }
    break;
  default:
    break;
  }
  if (is_aggval(node->ty)) {
    gen_expr(node);
    return;
  }
  error_tok(node->tok, "not an lvalue");
}

/* Loads a value from where %rax is pointing to. */
static void load(Type *ty) {
  if (ty->is_complex)
    return;
  switch (ty->kind) {
  case TY_ARRAY:
  case TY_STRUCT:
  case TY_UNION:
  case TY_FUNC:
  case TY_VLA:
    return;
  case TY_FLOAT:
    println("  movss (%%rax), %%xmm0");
    return;
  case TY_DOUBLE:
    println("  movsd (%%rax), %%xmm0");
    return;
  case TY_LDOUBLE:
    println("  fldt (%%rax)");
    return;
  default:
    break;
  }
  char *insn = ty->is_unsigned ? "movz" : "movs";
  switch (type_size(ty)) {
  case 1:
    println("  %sbl (%%rax), %%eax", insn);
    return;
  case 2:
    println("  %swl (%%rax), %%eax", insn);
    return;
  case 4:
    if (ty->is_unsigned)
      println("  movl (%%rax), %%eax");
    else
      println("  movslq (%%rax), %%rax");
    return;
  default:
    println("  mov (%%rax), %%rax");
  }
}

/* Copies size bytes from (%rax) to (%rdi); clobbers rcx, rsi, rdx. */
static void copy_mem(int64_t size) {
  if (size <= 64) {
    int64_t i = 0;
    for (; i + 8 <= size; i += 8) {
      println("  mov %" PRId64 "(%%rax), %%rdx", i);
      println("  mov %%rdx, %" PRId64 "(%%rdi)", i);
    }
    for (; i + 4 <= size; i += 4) {
      println("  mov %" PRId64 "(%%rax), %%edx", i);
      println("  mov %%edx, %" PRId64 "(%%rdi)", i);
    }
    for (; i < size; i++) {
      println("  mov %" PRId64 "(%%rax), %%dl", i);
      println("  mov %%dl, %" PRId64 "(%%rdi)", i);
    }
    return;
  }
  println("  mov %%rdi, %%rdx");
  println("  mov %%rax, %%rsi");
  println("  mov $%" PRId64 ", %%rcx", size);
  println("  rep movsb");
  println("  mov %%rdx, %%rdi");
  println("  mov %%rsi, %%rax");
  println("  sub $%" PRId64 ", %%rax", size);
}

/* Stores %rax (or xmm0 / st0) to the address on top of the stack. */
static void store(Type *ty) {
  pop("%rdi");

  if (ty->is_complex) {
    copy_mem(type_size(ty));
    println("  mov %%rdi, %%rax");
    return;
  }
  switch (ty->kind) {
  case TY_STRUCT:
  case TY_UNION:
  case TY_ARRAY:
    copy_mem(type_size(ty));
    println("  mov %%rdi, %%rax");
    return;
  case TY_FLOAT:
    println("  movss %%xmm0, (%%rdi)");
    return;
  case TY_DOUBLE:
    println("  movsd %%xmm0, (%%rdi)");
    return;
  case TY_LDOUBLE:
    println("  fld %%st(0)");
    println("  fstpt (%%rdi)");
    return;
  default:
    break;
  }
  switch (type_size(ty)) {
  case 1: println("  mov %%al, (%%rdi)"); return;
  case 2: println("  mov %%ax, (%%rdi)"); return;
  case 4: println("  mov %%eax, (%%rdi)"); return;
  default: println("  mov %%rax, (%%rdi)"); return;
  }
}


/* ------------------------------------------- operand fast paths */

/* If the object designated by node can be addressed without code
   (local variable, non-TLS global, or a member of one), returns the
   operand string; otherwise NULL. */
static char *simple_addr2(Node *node, bool always);
static char *simple_addr(Node *node) { return simple_addr2(node, false); }

/* Does node call a function that returns twice (setjmp and the like)?
   Nothing may be kept on the stack across such a call, since the second
   return finds the stack as later code left it. */
static bool calls_returns_twice(Node *node) {
  while (node->kind == ND_CAST)
    node = node->lhs;
  if (node->kind != ND_FUNCALL)
    return false;
  Node *fn = node->lhs;
  while (fn->kind == ND_CAST)
    fn = fn->lhs;
  if (fn->kind != ND_ADDR || fn->lhs->kind != ND_VAR)
    return false;
  static char *names[] = {"setjmp", "_setjmp", "sigsetjmp", "__sigsetjmp", "vfork",
                          "getcontext", "savectx", NULL};
  for (int i = 0; names[i]; i++)
    if (!strcmp(fn->lhs->var->name, names[i]))
      return true;
  return false;
}

static char *simple_addr2(Node *node, bool always) {
  if (opt_O == 0 && !always)
    return NULL;
  switch (node->kind) {
  case ND_VAR: {
    Obj *v = node->var;
    if (v->is_local)
      return v->is_vla_ptr ? NULL : format("%" PRId64 "(%%rbp)", v->offset);
    if (v->is_tls || use_got(v) || v->is_function)
      return NULL;
    return format("%s(%%rip)", sym(v));
  }
  case ND_MEMBER: {
    if (node->member->is_bitfield)
      return NULL;
    Node *b = node->lhs;
    if (b->kind != ND_VAR)
      return NULL;
    Obj *v = b->var;
    int64_t off = node->member->offset;
    if (v->is_local)
      return v->is_vla_ptr ? NULL : format("%" PRId64 "(%%rbp)", v->offset + off);
    if (v->is_tls || use_got(v))
      return NULL;
    return format("%s%+" PRId64 "(%%rip)", sym(v), off);
  }
  default:
    return NULL;
  }
}

static bool is_int_like(Type *ty) {
  return is_integer(ty) || ty->kind == TY_PTR;
}

/* Loads a scalar of type ty from operand into %rax/%xmm0/st0. */
static void load_from(Type *ty, char *op) {
  switch (ty->kind) {
  case TY_FLOAT: println("  movss %s, %%xmm0", op); return;
  case TY_DOUBLE: println("  movsd %s, %%xmm0", op); return;
  case TY_LDOUBLE: println("  fldt %s", op); return;
  default: break;
  }
  char *insn = ty->is_unsigned ? "movz" : "movs";
  switch (type_size(ty)) {
  case 1: println("  %sbl %s, %%eax", insn, op); return;
  case 2: println("  %swl %s, %%eax", insn, op); return;
  case 4:
    if (ty->is_unsigned)
      println("  movl %s, %%eax", op);
    else
      println("  movslq %s, %%rax", op);
    return;
  default: println("  mov %s, %%rax", op); return;
  }
}

/* Loads an integer of type ty from operand into %rdi, extended to the
   full 64 bits (the conversion to any wider integer type). */
static void load_rdi_from(Type *ty, char *op) {
  switch (type_size(ty)) {
  case 1:
    if (ty->is_unsigned)
      println("  movzbl %s, %%edi", op);
    else
      println("  movsbq %s, %%rdi", op);
    return;
  case 2:
    if (ty->is_unsigned)
      println("  movzwl %s, %%edi", op);
    else
      println("  movswq %s, %%rdi", op);
    return;
  case 4:
    if (ty->is_unsigned)
      println("  movl %s, %%edi", op);
    else
      println("  movslq %s, %%rdi", op);
    return;
  default: println("  mov %s, %%rdi", op); return;
  }
}

/* Stores %rax/%xmm0/st0 (scalar of type ty) to operand. */
static void store_to(Type *ty, char *op) {
  switch (ty->kind) {
  case TY_FLOAT: println("  movss %%xmm0, %s", op); return;
  case TY_DOUBLE: println("  movsd %%xmm0, %s", op); return;
  case TY_LDOUBLE:
    println("  fld %%st(0)");
    println("  fstpt %s", op);
    return;
  default: break;
  }
  switch (type_size(ty)) {
  case 1: println("  mov %%al, %s", op); return;
  case 2: println("  mov %%ax, %s", op); return;
  case 4: println("  mov %%eax, %s", op); return;
  default: println("  mov %%rax, %s", op); return;
  }
}

/* Can an integer operand be loaded straight into %rdi without
   disturbing %rax?  (constant, simple variable, or an integer
   conversion of one) */
static bool simple_int_operand(Node *n) {
  if (opt_O == 0)
    return false;
  if (!n->ty || !is_int_like(n->ty) || n->ty->kind == TY_BOOL)
    return false;
  if (n->kind == ND_NUM)
    return true;
  if ((n->kind == ND_VAR || n->kind == ND_MEMBER) && simple_addr(n) && is_int_like(n->ty))
    return true;
  if (n->kind == ND_CAST && is_int_like(n->lhs->ty) && n->lhs->ty->kind != TY_BOOL &&
      (n->lhs->kind == ND_VAR || n->lhs->kind == ND_MEMBER) && simple_addr(n->lhs))
    return true;
  return false;
}

static void load_simple_rdi(Node *n) {
  if (n->kind == ND_NUM) {
    if (n->val == 0)
      println("  xor %%edi, %%edi");
    else if (n->val > 0 && n->val <= UINT32_MAX)
      println("  mov $%" PRId64 ", %%edi", n->val);
    else
      println("  mov $%" PRId64 ", %%rdi", n->val);
    return;
  }
  if (n->kind == ND_CAST) {
    /* Loading with the source type's extension gives the converted value
       in the low bits for any integer conversion. */
    load_rdi_from(n->lhs->ty, simple_addr(n->lhs));
    return;
  }
  load_rdi_from(n->ty, simple_addr(n));
}

/* Is n an integer constant usable as an immediate operand? */
static bool imm_operand(Node *n, bool wide, int64_t *v) {
  if (opt_O == 0)
    return false;
  if (n->kind != ND_NUM || !n->ty || !is_int_like(n->ty))
    return false;
  int64_t x = n->val;
  if (!wide)
    x = (int32_t)x;
  if (x < INT32_MIN || x > INT32_MAX)
    return false;
  *v = x;
  return true;
}

/* Evaluates lhs into %rax and rhs into %rdi. */
static void gen_int_operands(Node *lhs, Node *rhs) {
  if (simple_int_operand(rhs)) {
    gen_expr(lhs);
    load_simple_rdi(rhs);
    return;
  }
  gen_expr(rhs);
  push();
  gen_expr(lhs);
  pop("%rdi");
}

/* --------------------------------------------------------- casts */

static void cmp_zero(Type *ty) {
  switch (ty->kind) {
  case TY_FLOAT:
    println("  xorps %%xmm1, %%xmm1");
    println("  ucomiss %%xmm1, %%xmm0");
    return;
  case TY_DOUBLE:
    println("  xorpd %%xmm1, %%xmm1");
    println("  ucomisd %%xmm1, %%xmm0");
    return;
  case TY_LDOUBLE:
    println("  fldz");
    println("  fucomip %%st(1), %%st");
    println("  fstp %%st(0)");
    return;
  default:
    break;
  }
  if (is_integer(ty) && type_size(ty) <= 4)
    println("  cmp $0, %%eax");
  else
    println("  cmp $0, %%rax");
}

/* Sets %eax to 1 if the value (just compared with zero) is nonzero;
   NaN counts as nonzero. */
static void setne_flags(Type *ty) {
  if (is_flonum(ty)) {
    println("  setne %%al");
    println("  setp %%dl");
    println("  or %%dl, %%al");
  } else {
    println("  setne %%al");
  }
  println("  movzbl %%al, %%eax");
}

enum { I8, I16, I32, I64, U8, U16, U32, U64, F32, F64, F80 };

static int get_type_id(Type *ty) {
  switch (ty->kind) {
  case TY_CHAR:
    return ty->is_unsigned ? U8 : I8;
  case TY_SHORT:
    return ty->is_unsigned ? U16 : I16;
  case TY_INT:
    return ty->is_unsigned ? U32 : I32;
  case TY_ENUM:
    if (ty->size == 8)
      return ty->is_unsigned ? U64 : I64;
    return ty->is_unsigned ? U32 : I32;
  case TY_BOOL:
    return U8;
  case TY_LONG:
  case TY_LLONG:
    return ty->is_unsigned ? U64 : I64;
  case TY_FLOAT:
    return F32;
  case TY_DOUBLE:
    return F64;
  case TY_LDOUBLE:
    return F80;
  default:
    return U64;
  }
}

/* The table for type casts (rows: from, columns: to) */
static char i32i8[] = "movsbl %al, %eax";
static char i32u8[] = "movzbl %al, %eax";
static char i32i16[] = "movswl %ax, %eax";
static char i32u16[] = "movzwl %ax, %eax";
static char i32f32[] = "cvtsi2ssl %eax, %xmm0";
static char i32i64[] = "movslq %eax, %rax";
static char i32f64[] = "cvtsi2sdl %eax, %xmm0";
static char i32f80[] = "mov %eax, -4(%rsp); fildl -4(%rsp)";

static char u32f32[] = "mov %eax, %eax; cvtsi2ssq %rax, %xmm0";
static char u32i64[] = "mov %eax, %eax";
static char u32f64[] = "mov %eax, %eax; cvtsi2sdq %rax, %xmm0";
static char u32f80[] = "mov %eax, %eax; mov %rax, -8(%rsp); fildll -8(%rsp)";

static char i64f32[] = "cvtsi2ssq %rax, %xmm0";
static char i64f64[] = "cvtsi2sdq %rax, %xmm0";
static char i64f80[] = "movq %rax, -8(%rsp); fildll -8(%rsp)";

static char u64f32[] =
    "test %rax,%rax; js 1f; pxor %xmm0,%xmm0; cvtsi2ss %rax,%xmm0; jmp 2f; "
    "1: mov %rax,%rdi; and $1,%eax; pxor %xmm0,%xmm0; shr %rdi; "
    "or %rax,%rdi; cvtsi2ss %rdi,%xmm0; addss %xmm0,%xmm0; 2:";
static char u64f64[] =
    "test %rax,%rax; js 1f; pxor %xmm0,%xmm0; cvtsi2sd %rax,%xmm0; jmp 2f; "
    "1: mov %rax,%rdi; and $1,%eax; pxor %xmm0,%xmm0; shr %rdi; "
    "or %rax,%rdi; cvtsi2sd %rdi,%xmm0; addsd %xmm0,%xmm0; 2:";
static char u64f80[] =
    "mov %rax, -8(%rsp); fildq -8(%rsp); test %rax, %rax; jns 1f;"
    "mov $1602224128, %eax; mov %eax, -4(%rsp); fadds -4(%rsp); 1:";

static char f32i8[] = "cvttss2sil %xmm0, %eax; movsbl %al, %eax";
static char f32u8[] = "cvttss2sil %xmm0, %eax; movzbl %al, %eax";
static char f32i16[] = "cvttss2sil %xmm0, %eax; movswl %ax, %eax";
static char f32u16[] = "cvttss2sil %xmm0, %eax; movzwl %ax, %eax";
static char f32i32[] = "cvttss2sil %xmm0, %eax";
static char f32u32[] = "cvttss2siq %xmm0, %rax";
static char f32i64[] = "cvttss2siq %xmm0, %rax";
static char f32u64[] =
    "mov $1593835520, %eax; movd %eax, %xmm1; ucomiss %xmm1, %xmm0; jae 1f; "
    "cvttss2siq %xmm0, %rax; jmp 2f; 1: subss %xmm1, %xmm0; cvttss2siq %xmm0, %rax; "
    "btc $63, %rax; 2:";
static char f32f64[] = "cvtss2sd %xmm0, %xmm0";
static char f32f80[] = "movss %xmm0, -4(%rsp); flds -4(%rsp)";

static char f64i8[] = "cvttsd2sil %xmm0, %eax; movsbl %al, %eax";
static char f64u8[] = "cvttsd2sil %xmm0, %eax; movzbl %al, %eax";
static char f64i16[] = "cvttsd2sil %xmm0, %eax; movswl %ax, %eax";
static char f64u16[] = "cvttsd2sil %xmm0, %eax; movzwl %ax, %eax";
static char f64i32[] = "cvttsd2sil %xmm0, %eax";
static char f64u32[] = "cvttsd2siq %xmm0, %rax";
static char f64i64[] = "cvttsd2siq %xmm0, %rax";
static char f64u64[] =
    "mov $0x43e0000000000000, %rax; movq %rax, %xmm1; ucomisd %xmm1, %xmm0; jae 1f; "
    "cvttsd2siq %xmm0, %rax; jmp 2f; 1: subsd %xmm1, %xmm0; cvttsd2siq %xmm0, %rax; "
    "btc $63, %rax; 2:";
static char f64f32[] = "cvtsd2ss %xmm0, %xmm0";
static char f64f80[] = "movsd %xmm0, -8(%rsp); fldl -8(%rsp)";

#define FROM_F80_1                                                          \
  "fnstcw -10(%rsp); movzwl -10(%rsp), %eax; or $12, %ah; "                 \
  "mov %ax, -12(%rsp); fldcw -12(%rsp); "
#define FROM_F80_2 " -24(%rsp); fldcw -10(%rsp); "

static char f80i8[] = FROM_F80_1 "fistps" FROM_F80_2 "movsbl -24(%rsp), %eax";
static char f80u8[] = FROM_F80_1 "fistps" FROM_F80_2 "movzbl -24(%rsp), %eax";
static char f80i16[] = FROM_F80_1 "fistps" FROM_F80_2 "movswl -24(%rsp), %eax";
static char f80u16[] = FROM_F80_1 "fistpl" FROM_F80_2 "movzwl -24(%rsp), %eax";
static char f80i32[] = FROM_F80_1 "fistpl" FROM_F80_2 "mov -24(%rsp), %eax";
static char f80u32[] = FROM_F80_1 "fistpll" FROM_F80_2 "mov -24(%rsp), %eax";
static char f80i64[] = FROM_F80_1 "fistpll" FROM_F80_2 "mov -24(%rsp), %rax";
static char f80u64[] =
    "mov $1593835520, %eax; mov %eax, -4(%rsp); flds -4(%rsp); "
    "fxch %st(1); fucomi %st(1), %st; jae 1f; fstp %st(1); "
    FROM_F80_1 "fistpll" FROM_F80_2 "mov -24(%rsp), %rax; jmp 2f; "
    "1: fsub %st(1), %st; fstp %st(1); " FROM_F80_1 "fistpll" FROM_F80_2
    "mov -24(%rsp), %rax; btc $63, %rax; 2:";
static char f80f32[] = "fstps -8(%rsp); movss -8(%rsp), %xmm0";
static char f80f64[] = "fstpl -8(%rsp); movsd -8(%rsp), %xmm0";

static char *cast_table[][11] = {
    /* i8   i16     i32     i64     u8     u16     u32     u64     f32     f64     f80 */
    {NULL, NULL, NULL, i32i64, i32u8, i32u16, NULL, i32i64, i32f32, i32f64, i32f80},       // i8
    {i32i8, NULL, NULL, i32i64, i32u8, i32u16, NULL, i32i64, i32f32, i32f64, i32f80},      // i16
    {i32i8, i32i16, NULL, i32i64, i32u8, i32u16, NULL, i32i64, i32f32, i32f64, i32f80},    // i32
    {i32i8, i32i16, NULL, NULL, i32u8, i32u16, NULL, NULL, i64f32, i64f64, i64f80},        // i64
    {i32i8, NULL, NULL, i32i64, NULL, NULL, NULL, i32i64, i32f32, i32f64, i32f80},         // u8
    {i32i8, i32i16, NULL, i32i64, i32u8, NULL, NULL, i32i64, i32f32, i32f64, i32f80},      // u16
    {i32i8, i32i16, NULL, u32i64, i32u8, i32u16, NULL, u32i64, u32f32, u32f64, u32f80},    // u32
    {i32i8, i32i16, NULL, NULL, i32u8, i32u16, NULL, NULL, u64f32, u64f64, u64f80},        // u64
    {f32i8, f32i16, f32i32, f32i64, f32u8, f32u16, f32u32, f32u64, NULL, f32f64, f32f80},  // f32
    {f64i8, f64i16, f64i32, f64i64, f64u8, f64u16, f64u32, f64u64, f64f32, NULL, f64f80},  // f64
    {f80i8, f80i16, f80i32, f80i64, f80u8, f80u16, f80u32, f80u64, f80f32, f80f64, NULL},  // f80
};

/* Emits a cast sequence; numeric local labels (1:, 2:) are made unique. */
static void emit_cast_seq(char *seq) {
  int c = count();
  Buf b = {0};
  for (char *p = seq; *p; p++) {
    if ((*p == '1' || *p == '2') && p[1] == ':' && (p == seq || p[-1] == ' ' || p[-1] == ';')) {
      buf_printf(&b, ".L.cast%d_%c", c, *p);
      continue;
    }
    if ((*p == '1' || *p == '2') && (p[1] == 'f') && p > seq && p[-1] == ' ') {
      buf_printf(&b, ".L.cast%d_%c", c, *p);
      p++;
      continue;
    }
    buf_putc(&b, *p);
  }
  println("  %s", b.buf);
}

static void cast(Type *from, Type *to) {
  if (to->kind == TY_VOID) {
    if (from->kind == TY_LDOUBLE)
      println("  fstp %%st(0)");
    return;
  }

  if (to->kind == TY_BOOL) {
    cmp_zero(from);
    setne_flags(from);
    return;
  }

  if (is_su(to) || is_array(to) || is_su(from))
    return;

  int t1 = get_type_id(from);
  int t2 = get_type_id(to);
  if (cast_table[t1][t2])
    emit_cast_seq(cast_table[t1][t2]);
}

/* -------------------------------------------- struct classification */

/* Classification of one eightbyte of a struct: INTEGER unless every
   field in it is float/double. */
static bool has_flonum(Type *ty, int lo, int hi, int offset) {
  if (is_su(ty)) {
    for (Member *mem = struct_members(ty); mem; mem = mem->next)
      if (!has_flonum(mem->ty, lo, hi, offset + mem->offset))
        return false;
    return true;
  }
  if (ty->kind == TY_ARRAY) {
    for (int64_t i = 0; i < ty->array_len; i++)
      if (!has_flonum(ty->base, lo, hi, offset + type_size(ty->base) * i))
        return false;
    return true;
  }
  return offset < lo || hi <= offset || ty->kind == TY_FLOAT || ty->kind == TY_DOUBLE;
}

static bool has_flonum1(Type *ty) { return has_flonum(ty, 0, 8, 0); }
static bool has_flonum2(Type *ty) { return has_flonum(ty, 8, 16, 0); }

static bool has_ldouble(Type *ty) {
  if (is_su(ty)) {
    for (Member *mem = struct_members(ty); mem; mem = mem->next)
      if (has_ldouble(mem->ty))
        return true;
    return false;
  }
  if (ty->kind == TY_ARRAY)
    return has_ldouble(ty->base);
  return ty->kind == TY_LDOUBLE;
}

/* Unaligned members force MEMORY class. */
static bool has_unaligned(Type *ty, int offset) {
  if (is_su(ty)) {
    for (Member *mem = struct_members(ty); mem; mem = mem->next) {
      if (mem->is_bitfield)
        continue;
      if ((offset + mem->offset) % type_align(mem->ty))
        return true;
      if (has_unaligned(mem->ty, offset + mem->offset))
        return true;
    }
  }
  return false;
}

/* Does every scalar of ty lie at offset 0 and is a long double (the
   X87/X87UP classification)? */
static bool only_ldouble(Type *ty) {
  if (is_su(ty)) {
    Member *m = struct_members(ty);
    if (!m)
      return false;
    for (; m; m = m->next)
      if (m->offset != 0 || m->is_bitfield || !only_ldouble(m->ty))
        return false;
    return true;
  }
  if (ty->kind == TY_ARRAY)
    return ty->array_len == 1 && only_ldouble(ty->base);
  return ty->kind == TY_LDOUBLE;
}

/* A struct/union classified X87, X87UP is returned in %st0. */
static bool su_ret_x87(Type *ty) { return type_size(ty) == 16 && only_ldouble(ty); }

static bool su_in_memory(Type *ty);

/* _Complex long double is returned in %st0 (real) and %st1 (imag). */
static bool ret_complex_x87(Type *ty) { return ty->is_complex && ty->kind == TY_LDOUBLE; }

/* Is a struct/union returned in memory (via the hidden pointer)? */
static bool su_ret_in_memory(Type *ty) {
  return !su_ret_x87(ty) && !ret_complex_x87(ty) && su_in_memory(ty);
}

/* Is a struct passed in memory? */
static bool su_in_memory(Type *ty) {
  int64_t sz = type_size(ty);
  return sz > 16 || sz == 0 || has_ldouble(ty) || has_unaligned(ty, 0);
}

/* -------------------------------------------------------- calls */

static void push_struct(Type *ty) {
  int64_t sz = align_to64(type_size(ty), 8);
  println("  sub $%" PRId64 ", %%rsp", sz);
  depth += sz / 8;
  for (int64_t i = 0; i < type_size(ty); i++) {
    println("  mov %" PRId64 "(%%rax), %%r10b", i);
    println("  mov %%r10b, %" PRId64 "(%%rsp)", i);
  }
}

static void push_reg_args(Node *args) {
  if (!args)
    return;
  push_reg_args(args->next);
  if (args->pass_by_stack)
    return;
  gen_expr(args);
  switch (abi_kind(args->ty)) {
  case TY_STRUCT:
  case TY_UNION:
    push_struct(args->ty);
    break;
  case TY_FLOAT:
  case TY_DOUBLE:
    pushf();
    break;
  default:
    push();
  }
}

/* Alignment of an argument in the stack parameter area */
static int stack_arg_align(Type *ty) { return type_align(ty) > 8 ? 16 : 8; }

/* Classifies arguments, lays out the stack argument area, evaluates
   stack arguments into it and pushes register arguments.  Returns the
   number of 8-byte slots to pop after the call. */
static int push_args(Node *node) {
  int gp = 0, fp = 0;
  int64_t off = 0;

  if (node->ret_buffer && su_ret_in_memory(node->ty))
    gp++;

  for (Node *arg = node->args; arg; arg = arg->next) {
    Type *ty = arg->ty;
    arg->pass_by_stack = false;
    switch (abi_kind(ty)) {
    case TY_STRUCT:
    case TY_UNION:
      if (su_in_memory(ty)) {
        arg->pass_by_stack = true;
      } else {
        bool fp1 = has_flonum1(ty);
        bool fp2 = type_size(ty) > 8 && has_flonum2(ty);
        int nfp = fp1 + fp2;
        int ngp = !fp1 + (type_size(ty) > 8 && !fp2);
        if (fp + nfp > FP_MAX || gp + ngp > GP_MAX) {
          arg->pass_by_stack = true;
        } else {
          fp += nfp;
          gp += ngp;
        }
      }
      break;
    case TY_FLOAT:
    case TY_DOUBLE:
      if (fp < FP_MAX)
        fp++;
      else
        arg->pass_by_stack = true;
      break;
    case TY_LDOUBLE:
      arg->pass_by_stack = true;
      break;
    default:
      if (gp < GP_MAX)
        gp++;
      else
        arg->pass_by_stack = true;
    }
    if (arg->pass_by_stack) {
      off = align_to64(off, stack_arg_align(ty));
      arg->stack_off = off;
      off += align_to64(type_size(ty), 8);
    }
  }

  int64_t area = align_to64(off, 8);
  int slots = area / 8;
  /* rsp must be 16-byte aligned at the call */
  if ((depth + slots) % 2 == 1) {
    println("  sub $8, %%rsp");
    depth++;
    slots++;
  }
  if (area) {
    println("  sub $%" PRId64 ", %%rsp", area);
    depth += area / 8;
  }

  for (Node *arg = node->args; arg; arg = arg->next) {
    if (!arg->pass_by_stack)
      continue;
    gen_expr(arg);
    Type *ty = arg->ty;
    switch (abi_kind(ty)) {
    case TY_STRUCT:
    case TY_UNION:
      println("  lea %" PRId64 "(%%rsp), %%rdi", arg->stack_off);
      copy_mem(type_size(ty));
      break;
    case TY_FLOAT:
      println("  movss %%xmm0, %" PRId64 "(%%rsp)", arg->stack_off);
      break;
    case TY_DOUBLE:
      println("  movsd %%xmm0, %" PRId64 "(%%rsp)", arg->stack_off);
      break;
    case TY_LDOUBLE:
      println("  fstpt %" PRId64 "(%%rsp)", arg->stack_off);
      break;
    default:
      println("  mov %%rax, %" PRId64 "(%%rsp)", arg->stack_off);
    }
  }

  push_reg_args(node->args);

  /* The hidden pointer for a struct returned in memory */
  if (node->ret_buffer && su_ret_in_memory(node->ty)) {
    println("  lea %" PRId64 "(%%rbp), %%rax", node->ret_buffer->offset);
    push();
  }
  return slots;
}

static void copy_ret_buffer(Obj *var) {
  Type *ty = var->ty;
  int gp = 0, fp = 0;

  if (has_flonum1(ty)) {
    assert(type_size(ty) == 4 || 8 <= type_size(ty));
    if (type_size(ty) == 4)
      println("  movss %%xmm0, %" PRId64 "(%%rbp)", var->offset);
    else
      println("  movsd %%xmm0, %" PRId64 "(%%rbp)", var->offset);
    fp++;
  } else {
    for (int i = 0; i < (type_size(ty) < 8 ? type_size(ty) : 8); i++) {
      println("  mov %%al, %" PRId64 "(%%rbp)", var->offset + i);
      println("  shr $8, %%rax");
    }
    gp++;
  }

  if (type_size(ty) > 8) {
    if (has_flonum2(ty)) {
      assert(type_size(ty) == 12 || type_size(ty) == 16);
      if (type_size(ty) == 12)
        println("  movss %%xmm%d, %" PRId64 "(%%rbp)", fp, var->offset + 8);
      else
        println("  movsd %%xmm%d, %" PRId64 "(%%rbp)", fp, var->offset + 8);
    } else {
      char *reg1 = (gp == 0) ? "%al" : "%dl";
      char *reg2 = (gp == 0) ? "%rax" : "%rdx";
      for (int i = 8; i < (type_size(ty) < 16 ? type_size(ty) : 16); i++) {
        println("  mov %s, %" PRId64 "(%%rbp)", reg1, var->offset + i);
        println("  shr $8, %s", reg2);
      }
    }
  }
}

/* Returning a struct: load it into registers or copy to the hidden
   buffer. %rax holds its address. */
static void copy_struct_reg(void) {
  Type *ty = current_fn->ty->return_ty;
  int gp = 0, fp = 0;

  println("  mov %%rax, %%rdi");

  if (has_flonum(ty, 0, 8, 0)) {
    assert(type_size(ty) == 4 || 8 <= type_size(ty));
    if (type_size(ty) == 4)
      println("  movss (%%rdi), %%xmm0");
    else
      println("  movsd (%%rdi), %%xmm0");
    fp++;
  } else {
    println("  mov $0, %%rax");
    for (int64_t i = (type_size(ty) < 8 ? type_size(ty) : 8) - 1; i >= 0; i--) {
      println("  shl $8, %%rax");
      println("  mov %" PRId64 "(%%rdi), %%al", i);
    }
    gp++;
  }

  if (type_size(ty) > 8) {
    if (has_flonum(ty, 8, 16, 0)) {
      assert(type_size(ty) == 12 || type_size(ty) == 16);
      if (type_size(ty) == 12)
        println("  movss 8(%%rdi), %%xmm%d", fp);
      else
        println("  movsd 8(%%rdi), %%xmm%d", fp);
    } else {
      char *reg1 = (gp == 0) ? "%al" : "%dl";
      char *reg2 = (gp == 0) ? "%rax" : "%rdx";
      println("  mov $0, %s", reg2);
      for (int64_t i = (type_size(ty) < 16 ? type_size(ty) : 16) - 1; i >= 8; i--) {
        println("  shl $8, %s", reg2);
        println("  mov %" PRId64 "(%%rdi), %s", i, reg1);
      }
    }
  }
}

static void copy_struct_mem(void) {
  Type *ty = current_fn->ty->return_ty;
  println("  mov %" PRId64 "(%%rbp), %%rdi", struct_ret_offset);
  copy_mem(type_size(ty));
  println("  mov %%rdi, %%rax");
}

/* ------------------------------------------------------ builtins */

static void gen_va_start(Node *node) {
  /* va_list in %rax (pointer to the struct) */
  gen_expr(node->lhs);
  int gp = 0, fp = 0;
  Type *fty = current_fn->ty;
  if (is_su(fty->return_ty) && su_ret_in_memory(fty->return_ty))
    gp++;
  int64_t stack_off = 16;
  for (Param *p = fty->params; p; p = p->next) {
    Type *ty = p->ty;
    bool on_stack = false;
    if (is_aggval(ty)) {
      if (su_in_memory(ty)) {
        on_stack = true;
      } else {
        bool fp1 = has_flonum1(ty), fp2 = type_size(ty) > 8 && has_flonum2(ty);
        int nfp = fp1 + fp2;
        int ngp = !fp1 + (type_size(ty) > 8 && !fp2);
        if (fp + nfp > FP_MAX || gp + ngp > GP_MAX)
          on_stack = true;
        else {
          fp += nfp;
          gp += ngp;
        }
      }
    } else if (ty->kind == TY_FLOAT || ty->kind == TY_DOUBLE) {
      if (fp < FP_MAX)
        fp++;
      else
        on_stack = true;
    } else if (ty->kind == TY_LDOUBLE) {
      on_stack = true;
    } else {
      if (gp < GP_MAX)
        gp++;
      else
        on_stack = true;
    }
    if (on_stack) {
      stack_off = 16 + align_to64(stack_off - 16, stack_arg_align(ty));
      stack_off += align_to64(type_size(ty), 8);
    }
  }
  println("  movl $%d, (%%rax)", gp * 8);
  println("  movl $%d, 4(%%rax)", 48 + fp * 16);
  println("  lea %" PRId64 "(%%rbp), %%rdx", stack_off);
  println("  mov %%rdx, 8(%%rax)");
  println("  lea %" PRId64 "(%%rbp), %%rdx", current_fn->va_area->offset);
  println("  mov %%rdx, 16(%%rax)");
}

/* va_arg: leaves the address of the argument in %rax. */
static void gen_va_arg_addr(Node *node) {
  Type *ty = node->ty;
  gen_expr(node->lhs);
  println("  mov %%rax, %%rcx");  /* rcx = ap */
  int c = count();

  if (is_aggval(ty)) {
    if (!su_in_memory(ty)) {
      bool fp1 = has_flonum1(ty);
      bool fp2 = type_size(ty) > 8 && has_flonum2(ty);
      int nfp = fp1 + fp2;
      int ngp = !fp1 + (type_size(ty) > 8 && !fp2);
      /* Enough registers left? */
      if (ngp) {
        println("  cmpl $%d, (%%rcx)", 48 - ngp * 8);
        println("  ja .L.va_mem.%d", c);
      }
      if (nfp) {
        println("  cmpl $%d, 4(%%rcx)", 176 - nfp * 16);
        println("  ja .L.va_mem.%d", c);
      }
      /* Copy the eightbytes into the temporary */
      Obj *tmp = node->var;
      for (int i = 0; i < (type_size(ty) + 7) / 8; i++) {
        bool is_fp = i == 0 ? fp1 : fp2;
        if (is_fp) {
          println("  movl 4(%%rcx), %%eax");
          println("  add 16(%%rcx), %%rax");
          println("  mov (%%rax), %%rdx");
          println("  addl $16, 4(%%rcx)");
        } else {
          println("  movl (%%rcx), %%eax");
          println("  add 16(%%rcx), %%rax");
          println("  mov (%%rax), %%rdx");
          println("  addl $8, (%%rcx)");
        }
        println("  mov %%rdx, %" PRId64 "(%%rbp)", tmp->offset + i * 8);
      }
      println("  lea %" PRId64 "(%%rbp), %%rax", tmp->offset);
      println("  jmp .L.va_end.%d", c);
    }
    println(".L.va_mem.%d:", c);
    println("  mov 8(%%rcx), %%rax");
    if (type_align(ty) > 8) {
      println("  add $15, %%rax");
      println("  and $-16, %%rax");
    }
    println("  lea %" PRId64 "(%%rax), %%rdx", align_to64(type_size(ty), 8));
    println("  mov %%rdx, 8(%%rcx)");
    println(".L.va_end.%d:", c);
    return;
  }

  if (ty->kind == TY_LDOUBLE) {
    println("  mov 8(%%rcx), %%rax");
    println("  add $15, %%rax");
    println("  and $-16, %%rax");
    println("  lea 16(%%rax), %%rdx");
    println("  mov %%rdx, 8(%%rcx)");
    return;
  }

  if (ty->kind == TY_FLOAT || ty->kind == TY_DOUBLE) {
    println("  movl 4(%%rcx), %%eax");
    println("  cmp $160, %%eax");
    println("  ja .L.va_mem.%d", c);
    println("  add 16(%%rcx), %%rax");
    println("  addl $16, 4(%%rcx)");
    println("  jmp .L.va_end.%d", c);
  } else {
    println("  movl (%%rcx), %%eax");
    println("  cmp $40, %%eax");
    println("  ja .L.va_mem.%d", c);
    println("  add 16(%%rcx), %%rax");
    println("  addl $8, (%%rcx)");
    println("  jmp .L.va_end.%d", c);
  }
  println(".L.va_mem.%d:", c);
  println("  mov 8(%%rcx), %%rax");
  println("  lea 8(%%rax), %%rdx");
  println("  mov %%rdx, 8(%%rcx)");
  println(".L.va_end.%d:", c);
}

/* --------------------------------------------------- bit-fields */

static void bitfield_load(Member *mem, Type *ty) {
  /* %rax holds the loaded storage unit (extended to 64 bits) */
  int shl = 64 - mem->bit_width - mem->bit_offset;
  int shr = 64 - mem->bit_width;
  if (shl)
    println("  shl $%d, %%rax", shl);
  if (ty->is_unsigned || ty->kind == TY_BOOL)
    println("  shr $%d, %%rax", shr);
  else
    println("  sar $%d, %%rax", shr);
}


/* ------------------------------------------------------- complex */

static char *fsuf(Type *base) { return base->kind == TY_FLOAT ? "ss" : "sd"; }

/* Loads part (0 = real, 1 = imaginary) of the complex at (reg). */
static void cx_load_part(Type *base, char *reg, int part) {
  int64_t off = part * type_size(base);
  if (base->kind == TY_LDOUBLE)
    println("  fldt %" PRId64 "(%s)", off, reg);
  else
    println("  mov%s %" PRId64 "(%s), %%xmm0", fsuf(base), off, reg);
}

/* Stores xmm0/st0 into part of the node's temporary. */
static void cx_store_part(Type *base, Obj *var, int part) {
  int64_t off = var->offset + part * type_size(base);
  if (base->kind == TY_LDOUBLE)
    println("  fstpt %" PRId64 "(%%rbp)", off);
  else
    println("  mov%s %%xmm0, %" PRId64 "(%%rbp)", fsuf(base), off);
}

static void cast(Type *from, Type *to);

/* Conversions to and from complex types (6.3.1.7) */
static void gen_complex_cast(Node *node) {
  Type *from = node->lhs->ty, *to = node->ty;
  if (to->kind == TY_VOID) {
    gen_expr(node->lhs);
    if (from->kind == TY_LDOUBLE && !from->is_complex)
      println("  fstp %%st(0)");
    return;
  }
  if (to->is_complex) {
    Type *tb = complex_base(to);
    Obj *var = node->var;
    if (!from->is_complex) {
      /* real -> complex: imaginary part zero */
      gen_expr(node->lhs);
      cast(from, tb);
      cx_store_part(tb, var, 0);
      if (tb->kind == TY_LDOUBLE)
        println("  fldz");
      else
        println("  xorps %%xmm0, %%xmm0");
      cx_store_part(tb, var, 1);
    } else {
      Type *fb = complex_base(from);
      gen_expr(node->lhs);
      println("  mov %%rax, %%rsi");
      cx_load_part(fb, "%rsi", 0);
      cast(fb, tb);
      cx_store_part(tb, var, 0);
      cx_load_part(fb, "%rsi", 1);
      cast(fb, tb);
      cx_store_part(tb, var, 1);
    }
    println("  lea %" PRId64 "(%%rbp), %%rax", var->offset);
    return;
  }
  /* complex -> real, integer or _Bool */
  Type *fb = complex_base(from);
  gen_expr(node->lhs);
  if (to->kind == TY_BOOL) {
    println("  mov %%rax, %%rsi");
    cx_load_part(fb, "%rsi", 0);
    cast(fb, ty_bool);
    println("  mov %%eax, %%r8d");
    cx_load_part(fb, "%rsi", 1);
    cast(fb, ty_bool);
    println("  or %%r8d, %%eax");
    return;
  }
  println("  mov %%rax, %%rsi");
  cx_load_part(fb, "%rsi", 0);
  cast(fb, to);
}

static void gen_complex_num(Node *node) {
  Type *b = complex_base(node->ty);
  Obj *var = node->var;
  long double parts[2] = {node->fval, node->fimag};
  for (int i = 0; i < 2; i++) {
    int64_t off = var->offset + i * type_size(b);
    if (b->kind == TY_FLOAT) {
      union { float f; uint32_t u; } u = {(float)parts[i]};
      println("  movl $%u, %" PRId64 "(%%rbp)", u.u, off);
    } else if (b->kind == TY_DOUBLE) {
      union { double f; uint64_t u; } u = {(double)parts[i]};
      println("  mov $%" PRIu64 ", %%rax", u.u);
      println("  mov %%rax, %" PRId64 "(%%rbp)", off);
    } else {
      union { long double f; uint64_t u[2]; } u;
      memset(&u, 0, sizeof(u));
      u.f = parts[i];
      println("  mov $%" PRIu64 ", %%rax", u.u[0]);
      println("  mov %%rax, %" PRId64 "(%%rbp)", off);
      println("  mov $%" PRIu64 ", %%rax", u.u[1]);
      println("  mov %%rax, %" PRId64 "(%%rbp)", off + 8);
    }
  }
  println("  lea %" PRId64 "(%%rbp), %%rax", var->offset);
}

/* Binary operation on complex operands (already converted to the
   common type).  Multiplication and division call libgcc's Annex G
   routines, as GCC does. */
static void gen_complex_binop(Node *node) {
  Type *ty = node->lhs->ty;
  Type *b = complex_base(ty);
  int64_t sz = type_size(b);
  gen_expr(node->rhs);
  push();
  gen_expr(node->lhs);
  pop("%rdi");
  println("  mov %%rax, %%rsi");  /* rsi = lhs, rdi = rhs */

  if (node->kind == ND_EQ || node->kind == ND_NE) {
    for (int part = 0; part < 2; part++) {
      if (b->kind == TY_LDOUBLE) {
        println("  fldt %" PRId64 "(%%rdi)", part * sz);
        println("  fldt %" PRId64 "(%%rsi)", part * sz);
        println("  fucomip %%st(1), %%st");
        println("  fstp %%st(0)");
      } else {
        println("  mov%s %" PRId64 "(%%rsi), %%xmm0", fsuf(b), part * sz);
        println("  ucomi%s %" PRId64 "(%%rdi), %%xmm0", fsuf(b), part * sz);
      }
      println("  sete %%al");
      println("  setnp %%dl");
      println("  and %%dl, %%al");
      if (part == 0)
        println("  mov %%al, %%r8b");
    }
    println("  and %%r8b, %%al");
    if (node->kind == ND_NE)
      println("  xor $1, %%al");
    println("  movzbl %%al, %%eax");
    return;
  }

  Obj *var = node->var;
  if (node->kind == ND_ADD || node->kind == ND_SUB) {
    for (int part = 0; part < 2; part++) {
      if (b->kind == TY_LDOUBLE) {
        println("  fldt %" PRId64 "(%%rdi)", part * sz);
        println("  fldt %" PRId64 "(%%rsi)", part * sz);
        println("  %s", node->kind == ND_ADD ? "faddp" : "fsubrp");
        println("  fstpt %" PRId64 "(%%rbp)", var->offset + part * sz);
      } else {
        println("  mov%s %" PRId64 "(%%rsi), %%xmm0", fsuf(b), part * sz);
        println("  %s%s %" PRId64 "(%%rdi), %%xmm0", node->kind == ND_ADD ? "add" : "sub", fsuf(b),
                part * sz);
        println("  mov%s %%xmm0, %" PRId64 "(%%rbp)", fsuf(b), var->offset + part * sz);
      }
    }
    println("  lea %" PRId64 "(%%rbp), %%rax", var->offset);
    return;
  }

  /* ND_MUL / ND_DIV: __mulsc3(a, b, c, d) etc. */
  char *fn = format("__%s%cc3", node->kind == ND_MUL ? "mul" : "div",
                    b->kind == TY_FLOAT ? 's' : b->kind == TY_DOUBLE ? 'd' : 'x');
  if (b->kind == TY_LDOUBLE) {
    int pad = depth % 2 ? 8 : 0;
    println("  sub $%d, %%rsp", 64 + pad);
    println("  fldt (%%rsi)");
    println("  fstpt (%%rsp)");
    println("  fldt 16(%%rsi)");
    println("  fstpt 16(%%rsp)");
    println("  fldt (%%rdi)");
    println("  fstpt 32(%%rsp)");
    println("  fldt 16(%%rdi)");
    println("  fstpt 48(%%rsp)");
    println("  call %s%s", fn, opt_pic ? "@PLT" : "");
    println("  add $%d, %%rsp", 64 + pad);
    println("  fstpt %" PRId64 "(%%rbp)", var->offset);
    println("  fstpt %" PRId64 "(%%rbp)", var->offset + 16);
  } else {
    println("  mov%s (%%rsi), %%xmm0", fsuf(b));
    println("  mov%s %" PRId64 "(%%rsi), %%xmm1", fsuf(b), sz);
    println("  mov%s (%%rdi), %%xmm2", fsuf(b));
    println("  mov%s %" PRId64 "(%%rdi), %%xmm3", fsuf(b), sz);
    if (depth % 2)
      println("  sub $8, %%rsp");
    println("  call %s%s", fn, opt_pic ? "@PLT" : "");
    if (depth % 2)
      println("  add $8, %%rsp");
    if (b->kind == TY_FLOAT) {
      println("  movq %%xmm0, %" PRId64 "(%%rbp)", var->offset);
    } else {
      println("  movsd %%xmm0, %" PRId64 "(%%rbp)", var->offset);
      println("  movsd %%xmm1, %" PRId64 "(%%rbp)", var->offset + 8);
    }
  }
  println("  lea %" PRId64 "(%%rbp), %%rax", var->offset);
}

static void gen_complex_neg(Node *node) {
  Type *b = complex_base(node->ty);
  gen_expr(node->lhs);
  println("  mov %%rax, %%rsi");
  for (int part = 0; part < 2; part++) {
    cx_load_part(b, "%rsi", part);
    if (b->kind == TY_LDOUBLE) {
      println("  fchs");
    } else {
      println("  mov $1, %%rax");
      println("  shl $%d, %%rax", b->kind == TY_FLOAT ? 31 : 63);
      println("  movq %%rax, %%xmm1");
      println("  xorp%c %%xmm1, %%xmm0", b->kind == TY_FLOAT ? 's' : 'd');
    }
    cx_store_part(b, node->var, part);
  }
  println("  lea %" PRId64 "(%%rbp), %%rax", node->var->offset);
}

static void gen_mkcomplex(Node *node) {
  Type *b = complex_base(node->ty);
  gen_expr(node->lhs);
  cx_store_part(b, node->var, 0);
  gen_expr(node->rhs);
  cx_store_part(b, node->var, 1);
  println("  lea %" PRId64 "(%%rbp), %%rax", node->var->offset);
}


/* -------------------------------------------------------- atomics */

/* Moves the value of type ty (in rax/xmm0) into %rax as raw bits. */
static void value_to_rax(Type *ty) {
  if (ty->kind == TY_FLOAT)
    println("  movd %%xmm0, %%eax");
  else if (ty->kind == TY_DOUBLE)
    println("  movq %%xmm0, %%rax");
}

/* Moves raw bits in %rax back into the value register for ty. */
static void rax_to_value(Type *ty) {
  if (ty->kind == TY_FLOAT)
    println("  movd %%eax, %%xmm0");
  else if (ty->kind == TY_DOUBLE)
    println("  movq %%rax, %%xmm0");
}

/* Sign/zero-extends a raw value in rax of type ty (as load() would). */
static void extend_rax(Type *ty) {
  if (!is_integer(ty) && ty->kind != TY_PTR)
    return;
  int sz = type_size(ty);
  if (sz == 1)
    println("  %s %%al, %%eax", ty->is_unsigned || ty->kind == TY_BOOL ? "movzbl" : "movsbl");
  else if (sz == 2)
    println("  %s %%ax, %%eax", ty->is_unsigned ? "movzwl" : "movswl");
}

/* Atomic store: value computed first, then xchg (implies lock). */
static void gen_atomic_store(Node *addr_expr, Node *val, Type *ty, bool addr_is_lvalue) {
  if (addr_is_lvalue)
    gen_addr(addr_expr);
  else
    gen_expr(addr_expr);
  push();
  gen_expr(val);
  value_to_rax(ty);
  pop("%rdi");
  int sz = type_size(ty);
  println("  mov %%rax, %%rdx");
  println("  xchg %s, (%%rdi)", reg_dx(sz));
  rax_to_value(ty);
}

static void gen_atomic(Node *node) {
  switch (node->kind) {
  case ND_ATOMIC_LOAD: {
    gen_expr(node->lhs);
    /* On x86-64 an aligned load is sequentially consistent given that
       stores use xchg. */
    Type *ty = node->ty;
    int sz = type_size(ty);
    println("  mov (%%rax), %s", reg_dx(sz));
    println("  mov %%rdx, %%rax");
    extend_rax(ty);
    rax_to_value(ty);
    return;
  }
  case ND_ATOMIC_STORE:
    gen_atomic_store(node->lhs, node->rhs, node->rhs->ty, false);
    return;
  case ND_EXCH: {
    Type *ty = node->ty;
    gen_expr(node->lhs);
    push();
    gen_expr(node->rhs);
    value_to_rax(ty);
    pop("%rdi");
    println("  xchg %s, (%%rdi)", reg_ax(type_size(ty)));
    extend_rax(ty);
    rax_to_value(ty);
    return;
  }
  case ND_CAS: {
    /* bool cas(obj, expected*, desired) */
    Type *ty = node->cas_new->ty;
    int sz = type_size(ty);
    gen_expr(node->cas_addr);
    push();
    gen_expr(node->cas_old);
    push();
    gen_expr(node->cas_new);
    value_to_rax(ty);
    println("  mov %%rax, %%rdx");    /* desired */
    pop("%rsi");                      /* expected* */
    pop("%rdi");                      /* object */
    println("  mov (%%rsi), %s", reg_ax(sz));
    println("  lock cmpxchg %s, (%%rdi)", reg_dx(sz));
    int c = count();
    println("  sete %%cl");
    println("  jz .L.cas.%d", c);
    println("  mov %s, (%%rsi)", reg_ax(sz));
    println(".L.cas.%d:", c);
    println("  movzbl %%cl, %%eax");
    return;
  }
  case ND_ATOMIC_RMW: {
    /* do { new = f(old) } while (!cas(addr, &old, new)) */
    Type *ty = node->var->ty;
    int sz = type_size(ty);
    int c = count();
    int64_t old = node->var->offset;
    gen_expr(node->cas_addr);
    println("  mov (%%rax), %s", reg_dx(sz));
    println("  mov %s, %" PRId64 "(%%rbp)", reg_dx(sz), old);
    println(".L.rmw.%d:", c);
    gen_expr(node->lhs);              /* new value from old */
    value_to_rax(ty);
    push();
    gen_expr(node->cas_addr);
    println("  mov %%rax, %%rdi");
    pop("%rdx");                      /* new */
    println("  mov %" PRId64 "(%%rbp), %s", old, reg_ax(sz));
    println("  lock cmpxchg %s, (%%rdi)", reg_dx(sz));
    println("  je .L.rmw_done.%d", c);
    println("  mov %s, %" PRId64 "(%%rbp)", reg_ax(sz), old);
    println("  jmp .L.rmw.%d", c);
    println(".L.rmw_done.%d:", c);
    if (node->rmw_fetch_first) {
      println("  lea %" PRId64 "(%%rbp), %%rax", old);
      load(ty);
    } else {
      println("  mov %%rdx, %%rax");
      extend_rax(ty);
      rax_to_value(ty);
    }
    return;
  }
  case ND_FENCE:
    gen_expr(node->lhs);
    if (node->val)
      println("  mfence");
    return;
  default:
    unreachable();
  }
}

/* --------------------------------------------- expressions */

static void gen_discard(Node *node) {
  add_type(node);
  if (node->init && node->kind == ND_COMMA) {
    /* x++ / x-- whose value is unused: evaluate as ++x / --x */
    node = node->init;
    add_type(node);
  }
  gen_expr(node);
  if (node->ty && node->ty->kind == TY_LDOUBLE)
    println("  fstp %%st(0)");
}

static void gen_cond_jump(Node *cond, char *label, bool when);

/* Jumps to label if cond is false. */
static void gen_cond_jump_false(Node *cond, char *label) { gen_cond_jump(cond, label, false); }

/* Jumps to label if cond is true. */
static void gen_cond_jump_true(Node *cond, char *label) { gen_cond_jump(cond, label, true); }

static void gen_cond_jump(Node *cond, char *label, bool when) {
  add_type(cond);
  switch (cond->kind) {
  case ND_NOT:
    gen_cond_jump(cond->lhs, label, !when);
    return;
  case ND_LOGAND:
  case ND_LOGOR: {
    bool is_and = cond->kind == ND_LOGAND;
    if (is_and != when) {
      /* (a && b) false -> jump if !a or !b; (a || b) true -> jump if a or b */
      gen_cond_jump(cond->lhs, label, when);
      gen_cond_jump(cond->rhs, label, when);
    } else {
      int c = count();
      char *skip = format(".L.cskip.%d", c);
      gen_cond_jump(cond->lhs, skip, !when);
      gen_cond_jump(cond->rhs, label, when);
      println("%s:", skip);
    }
    return;
  }
  case ND_EQ:
  case ND_NE:
  case ND_LT:
  case ND_LE:
    if (is_int_like(cond->lhs->ty) && is_int_like(cond->rhs->ty)) {
      bool wide = type_size(cond->lhs->ty) == 8 || cond->lhs->ty->kind == TY_PTR;
      int64_t imm;
      if (imm_operand(cond->rhs, wide, &imm)) {
        gen_expr(cond->lhs);
        if (imm == 0 && (cond->kind == ND_EQ || cond->kind == ND_NE))
          println("  test %s, %s", wide ? "%rax" : "%eax", wide ? "%rax" : "%eax");
        else
          println("  cmp $%" PRId64 ", %s", imm, wide ? "%rax" : "%eax");
      } else {
        gen_int_operands(cond->lhs, cond->rhs);
        println("  cmp %s, %s", wide ? "%rdi" : "%edi", wide ? "%rax" : "%eax");
      }
      bool u = cond->lhs->ty->is_unsigned || cond->lhs->ty->kind == TY_PTR;
      char *j;
      switch (cond->kind) {
      case ND_EQ: j = when ? "je" : "jne"; break;
      case ND_NE: j = when ? "jne" : "je"; break;
      case ND_LT: j = when ? (u ? "jb" : "jl") : (u ? "jae" : "jge"); break;
      default: j = when ? (u ? "jbe" : "jle") : (u ? "ja" : "jg"); break;
      }
      println("  %s %s", j, label);
      return;
    }
    break;
  case ND_NUM:
    if (is_integer(cond->ty)) {
      if ((cond->val != 0) == when)
        println("  jmp %s", label);
      return;
    }
    break;
  default:
    break;
  }
  gen_expr(cond);
  cmp_zero(cond->ty);
  if (is_flonum(cond->ty)) {
    int c = count();
    if (when) {
      /* true if not equal or unordered */
      println("  jp %s", label);
      println("  jne %s", label);
    } else {
      println("  jp .L.cj.%d", c);
      println("  je %s", label);
      println(".L.cj.%d:", c);
    }
  } else {
    println("  %s %s", when ? "jne" : "je", label);
  }
}

static void gen_funcall(Node *node) {
  /* Direct call? */
  Node *fn = node->lhs;
  Obj *direct = NULL;
  if (fn->kind == ND_ADDR && fn->lhs->kind == ND_VAR && fn->lhs->var->is_function)
    direct = fn->lhs->var;
  else if (fn->kind == ND_CAST && fn->lhs->kind == ND_ADDR && fn->lhs->lhs->kind == ND_VAR &&
           fn->lhs->lhs->var->is_function)
    direct = fn->lhs->lhs->var;

  int stack_args = push_args(node);
  if (!direct) {
    gen_expr(node->lhs);
    println("  mov %%rax, %%r10");
  }

  int gp = 0, fp = 0;

  /* Hidden pointer for struct return in memory */
  if (node->ret_buffer && su_ret_in_memory(node->ty))
    pop(argreg64[gp++]);

  for (Node *arg = node->args; arg; arg = arg->next) {
    Type *ty = arg->ty;
    if (arg->pass_by_stack)
      continue;
    switch (abi_kind(ty)) {
    case TY_STRUCT:
    case TY_UNION: {
      int64_t sz = type_size(ty);
      if (has_flonum1(ty))
        popf(fp++);
      else
        pop(argreg64[gp++]);
      if (sz > 8) {
        if (has_flonum2(ty))
          popf(fp++);
        else
          pop(argreg64[gp++]);
      }
      break;
    }
    case TY_FLOAT:
    case TY_DOUBLE:
      popf(fp++);
      break;
    default:
      pop(argreg64[gp++]);
    }
  }

  if (direct) {
    println("  mov $%d, %%eax", fp);
    if (opt_pic && !is_local_def(direct))
      println("  call %s@PLT", sym(direct));
    else
      println("  call %s", sym(direct));
  } else {
    println("  mov $%d, %%eax", fp);
    println("  call *%%r10");
  }

  if (stack_args) {
    println("  add $%d, %%rsp", stack_args * 8);
    depth -= stack_args;
  }

  /* The callee may leave garbage in the upper bits of small return
     values; extend per the declared type. */
  switch (node->ty->kind) {
  case TY_BOOL:
    println("  movzbl %%al, %%eax");
    break;
  case TY_CHAR:
    if (node->ty->is_unsigned)
      println("  movzbl %%al, %%eax");
    else
      println("  movsbl %%al, %%eax");
    break;
  case TY_SHORT:
    if (node->ty->is_unsigned)
      println("  movzwl %%ax, %%eax");
    else
      println("  movswl %%ax, %%eax");
    break;
  default:
    break;
  }

  /* _Complex long double returned in %st0/%st1 */
  if (node->ret_buffer && ret_complex_x87(node->ty)) {
    println("  fstpt %" PRId64 "(%%rbp)", node->ret_buffer->offset);
    println("  fstpt %" PRId64 "(%%rbp)", node->ret_buffer->offset + 16);
    println("  lea %" PRId64 "(%%rbp), %%rax", node->ret_buffer->offset);
  } else
  /* struct { long double } returned in %st0 */
  if (node->ret_buffer && su_ret_x87(node->ty)) {
    println("  fstpt %" PRId64 "(%%rbp)", node->ret_buffer->offset);
    println("  lea %" PRId64 "(%%rbp), %%rax", node->ret_buffer->offset);
  } else
  /* Struct returned in registers: copy into the buffer */
  if (node->ret_buffer && !su_in_memory(node->ty)) {
    copy_ret_buffer(node->ret_buffer);
    println("  lea %" PRId64 "(%%rbp), %%rax", node->ret_buffer->offset);
  }
}

static void gen_bitfield_assign(Node *node) {
  Member *mem = node->lhs->member;
  gen_addr(node->lhs);
  push();
  gen_expr(node->rhs);
  /* rax = new value; (%rsp) = address of storage unit */
  int sz = type_size(mem->ty);
  uint64_t mask = mem->bit_width >= 64 ? ~0ULL : (1ULL << mem->bit_width) - 1;
  println("  mov %%rax, %%r8");                 /* keep for result */
  println("  mov %%rax, %%rdi");
  println("  mov $%" PRIu64 ", %%rax", mask);
  println("  and %%rax, %%rdi");
  if (mem->bit_offset)
    println("  shl $%d, %%rdi", mem->bit_offset);
  println("  mov (%%rsp), %%rax");
  load(mem->ty);
  uint64_t clear = ~(mask << mem->bit_offset);
  println("  mov $%" PRIu64 ", %%r9", clear);
  println("  and %%r9, %%rax");
  println("  or %%rdi, %%rax");
  store(mem->ty);
  (void)sz;
  /* The value of the assignment: the new bit-field value. */
  println("  mov %%r8, %%rax");
  int shl = 64 - mem->bit_width;
  if (shl) {
    println("  shl $%d, %%rax", shl);
    if (mem->ty->is_unsigned || mem->ty->kind == TY_BOOL)
      println("  shr $%d, %%rax", shl);
    else
      println("  sar $%d, %%rax", shl);
  }
}

static void gen_float_binop(Node *node) {
  Type *ty = node->lhs->ty;
  if (ty->kind == TY_LDOUBLE) {
    gen_expr(node->rhs);
    push_ld();
    gen_expr(node->lhs);
    pop_ld();
    /* st0 = rhs, st1 = lhs.  (In the GNU assembler's AT&T syntax,
       "fsubrp" computes st1 - st0 and "fdivrp" st1 / st0.) */
    switch (node->kind) {
    case ND_ADD: println("  faddp"); return;
    case ND_SUB: println("  fsubrp"); return;
    case ND_MUL: println("  fmulp"); return;
    case ND_DIV: println("  fdivrp"); return;
    default: break;
    }
    println("  fxch %%st(1)");
    /* st0 = lhs, st1 = rhs */
    switch (node->kind) {
    case ND_EQ:
    case ND_NE:
    case ND_LT:
    case ND_LE: {
      /* compare lhs (st0) with rhs (st1) */
      if (node->kind == ND_LT || node->kind == ND_LE) {
        /* rhs > lhs  <=>  lhs < rhs : compare st0=rhs, st1=lhs */
        println("  fxch %%st(1)");
        println("  fucomip %%st(1), %%st");
        println("  fstp %%st(0)");
        if (node->kind == ND_LT)
          println("  seta %%al");
        else
          println("  setae %%al");
      } else {
        println("  fucomip %%st(1), %%st");
        println("  fstp %%st(0)");
        if (node->kind == ND_EQ) {
          println("  sete %%al");
          println("  setnp %%dl");
          println("  and %%dl, %%al");
        } else {
          println("  setne %%al");
          println("  setp %%dl");
          println("  or %%dl, %%al");
        }
      }
      println("  movzbl %%al, %%eax");
      return;
    }
    default:
      error_tok(node->tok, "invalid expression");
    }
  }

  gen_expr(node->rhs);
  pushf();
  gen_expr(node->lhs);
  popf(1);

  char *sz = ty->kind == TY_FLOAT ? "ss" : "sd";
  switch (node->kind) {
  case ND_ADD: println("  add%s %%xmm1, %%xmm0", sz); return;
  case ND_SUB: println("  sub%s %%xmm1, %%xmm0", sz); return;
  case ND_MUL: println("  mul%s %%xmm1, %%xmm0", sz); return;
  case ND_DIV: println("  div%s %%xmm1, %%xmm0", sz); return;
  case ND_EQ:
  case ND_NE:
  case ND_LT:
  case ND_LE:
    if (node->kind == ND_LT || node->kind == ND_LE)
      println("  ucomi%s %%xmm0, %%xmm1", sz);
    else
      println("  ucomi%s %%xmm1, %%xmm0", sz);
    if (node->kind == ND_EQ) {
      println("  sete %%al");
      println("  setnp %%dl");
      println("  and %%dl, %%al");
    } else if (node->kind == ND_NE) {
      println("  setne %%al");
      println("  setp %%dl");
      println("  or %%dl, %%al");
    } else if (node->kind == ND_LT) {
      println("  seta %%al");
    } else {
      println("  setae %%al");
    }
    println("  movzbl %%al, %%eax");
    return;
  default:
    error_tok(node->tok, "invalid expression");
  }
}

static void gen_expr(Node *node) {
  if (!node->ty)
    add_type(node);
  emit_loc(node->tok);

  /* Complex arithmetic */
  if (node->ty && node->ty->is_complex && node->var) {
    switch (node->kind) {
    case ND_NUM: gen_complex_num(node); return;
    case ND_CAST: gen_complex_cast(node); return;
    case ND_NEG: gen_complex_neg(node); return;
    case ND_MKCOMPLEX: gen_mkcomplex(node); return;
    case ND_ADD: case ND_SUB: case ND_MUL: case ND_DIV:
      gen_complex_binop(node);
      return;
    default:
      break;
    }
  }
  if (node->kind == ND_CAST && node->lhs->ty && node->lhs->ty->is_complex) {
    add_type(node->lhs);
    gen_complex_cast(node);
    return;
  }
  if ((node->kind == ND_EQ || node->kind == ND_NE) && node->lhs->ty && node->lhs->ty->is_complex) {
    gen_complex_binop(node);
    return;
  }

  switch (node->kind) {
  case ND_NULL_EXPR:
    return;
  case ND_NUM: {
    switch (node->ty->kind) {
    case TY_FLOAT: {
      union { float f32; uint32_t u32; } u = {(float)node->fval};
      println("  mov $%u, %%eax  # float %Lg", u.u32, node->fval);
      println("  movq %%rax, %%xmm0");
      return;
    }
    case TY_DOUBLE: {
      union { double f64; uint64_t u64; } u = {(double)node->fval};
      println("  mov $%" PRIu64 ", %%rax  # double %Lg", u.u64, node->fval);
      println("  movq %%rax, %%xmm0");
      return;
    }
    case TY_LDOUBLE: {
      union { long double f80; uint64_t u64[2]; } u;
      memset(&u, 0, sizeof(u));
      u.f80 = node->fval;
      println("  mov $%" PRIu64 ", %%rax  # long double %Lg", u.u64[0], node->fval);
      println("  mov %%rax, -16(%%rsp)");
      println("  mov $%" PRIu64 ", %%rax", u.u64[1]);
      println("  mov %%rax, -8(%%rsp)");
      println("  fldt -16(%%rsp)");
      return;
    }
    default:
      break;
    }
    if (node->val == 0)
      println("  xor %%eax, %%eax");
    else if (node->val > 0 && node->val <= UINT32_MAX)
      println("  mov $%" PRId64 ", %%eax", node->val);
    else
      println("  mov $%" PRId64 ", %%rax", node->val);
    return;
  }
  case ND_NEG:
    gen_expr(node->lhs);
    switch (node->ty->kind) {
    case TY_FLOAT:
      println("  mov $1, %%rax");
      println("  shl $31, %%rax");
      println("  movq %%rax, %%xmm1");
      println("  xorps %%xmm1, %%xmm0");
      return;
    case TY_DOUBLE:
      println("  mov $1, %%rax");
      println("  shl $63, %%rax");
      println("  movq %%rax, %%xmm1");
      println("  xorpd %%xmm1, %%xmm0");
      return;
    case TY_LDOUBLE:
      println("  fchs");
      return;
    default:
      break;
    }
    if (type_size(node->ty) == 8)
      println("  neg %%rax");
    else
      println("  neg %%eax");
    return;
  case ND_VAR: {
    char *op = simple_addr(node);
    if (op && is_regval(node->ty)) {
      load_from(node->ty, op);
      return;
    }
    if (op && (is_aggval(node->ty) || is_array(node->ty))) {
      println("  lea %s, %%rax", op);
      return;
    }
    gen_addr(node);
    load(node->ty);
    return;
  }
  case ND_MEMBER: {
    char *op = simple_addr(node);
    if (op && is_regval(node->ty)) {
      load_from(node->ty, op);
      return;
    }
    if (!node->member->is_bitfield && is_regval(node->ty) && node->lhs->kind == ND_DEREF) {
      /* p->m: load with a displacement */
      gen_expr(node->lhs->lhs);
      load_from(node->ty, format("%" PRId64 "(%%rax)", node->member->offset));
      return;
    }
    gen_addr(node);
    load(node->ty);
    Member *mem = node->member;
    if (mem->is_bitfield)
      bitfield_load(mem, node->ty);
    return;
  }
  case ND_DEREF:
    gen_expr(node->lhs);
    load(node->ty);
    return;
  case ND_ADDR:
    gen_addr(node->lhs);
    return;
  case ND_ASSIGN: {
    if (node->lhs->kind == ND_MEMBER && node->lhs->member->is_bitfield) {
      gen_bitfield_assign(node);
      return;
    }
    if ((node->lhs->ty->qual & Q_ATOMIC) && is_regval(node->ty)) {
      gen_atomic_store(node->lhs, node->rhs, node->ty, true);
      return;
    }
    char *op = simple_addr2(node->lhs, calls_returns_twice(node->rhs));
    if (op && is_regval(node->ty)) {
      gen_expr(node->rhs);
      store_to(node->ty, op);
      return;
    }
    gen_addr(node->lhs);
    push();
    gen_expr(node->rhs);
    store(node->ty);
    return;
  }
  case ND_STMT_EXPR:
    for (Node *n = node->body; n; n = n->next) {
      if (!n->next && n->kind == ND_EXPR_STMT) {
        gen_expr(n->lhs);
        return;
      }
      gen_stmt(n);
    }
    return;
  case ND_COMMA:
    gen_discard(node->lhs);
    gen_expr(node->rhs);
    return;
  case ND_CAST:
    gen_expr(node->lhs);
    cast(node->lhs->ty, node->ty);
    return;
  case ND_MEMZERO: {
    int64_t sz = type_size(node->var->ty);
    int64_t off = node->var->offset;
    if (sz <= 128) {
      println("  xor %%eax, %%eax");
      int64_t i = 0;
      for (; i + 8 <= sz; i += 8)
        println("  mov %%rax, %" PRId64 "(%%rbp)", off + i);
      for (; i + 4 <= sz; i += 4)
        println("  mov %%eax, %" PRId64 "(%%rbp)", off + i);
      for (; i < sz; i++)
        println("  mov %%al, %" PRId64 "(%%rbp)", off + i);
      return;
    }
    println("  mov $%" PRId64 ", %%rcx", sz);
    println("  lea %" PRId64 "(%%rbp), %%rdi", off);
    println("  xor %%eax, %%eax");
    println("  rep stosb");
    return;
  }
  case ND_COND: {
    int c = count();
    gen_cond_jump_false(node->cond, format(".L.else.%d", c));
    gen_expr(node->then);
    println("  jmp .L.end.%d", c);
    println(".L.else.%d:", c);
    gen_expr(node->els);
    println(".L.end.%d:", c);
    return;
  }
  case ND_NOT:
    gen_expr(node->lhs);
    cmp_zero(node->lhs->ty);
    if (is_flonum(node->lhs->ty)) {
      println("  sete %%al");
      println("  setnp %%dl");
      println("  and %%dl, %%al");
    } else {
      println("  sete %%al");
    }
    println("  movzbl %%al, %%eax");
    return;
  case ND_BITNOT:
    gen_expr(node->lhs);
    println("  not %%rax");
    return;
  case ND_LOGAND: {
    int c = count();
    gen_cond_jump_false(node->lhs, format(".L.false.%d", c));
    gen_cond_jump_false(node->rhs, format(".L.false.%d", c));
    println("  mov $1, %%eax");
    println("  jmp .L.end.%d", c);
    println(".L.false.%d:", c);
    println("  xor %%eax, %%eax");
    println(".L.end.%d:", c);
    return;
  }
  case ND_LOGOR: {
    int c = count();
    gen_expr(node->lhs);
    cmp_zero(node->lhs->ty);
    if (is_flonum(node->lhs->ty))
      println("  jp .L.true.%d", c);
    println("  jne .L.true.%d", c);
    gen_expr(node->rhs);
    cmp_zero(node->rhs->ty);
    if (is_flonum(node->rhs->ty))
      println("  jp .L.true.%d", c);
    println("  jne .L.true.%d", c);
    println("  xor %%eax, %%eax");
    println("  jmp .L.end.%d", c);
    println(".L.true.%d:", c);
    println("  mov $1, %%eax");
    println(".L.end.%d:", c);
    return;
  }
  case ND_FUNCALL:
    gen_funcall(node);
    return;
  case ND_VA_START:
    gen_va_start(node);
    return;
  case ND_VA_ARG:
    gen_va_arg_addr(node);
    if (!is_aggval(node->ty))
      load(node->ty);
    return;
  case ND_VA_COPY:
    gen_expr(node->rhs);
    push();
    gen_expr(node->lhs);
    pop("%rdi");
    for (int i = 0; i < 24; i += 8) {
      println("  mov %d(%%rdi), %%rdx", i);
      println("  mov %%rdx, %d(%%rax)", i);
    }
    return;
  case ND_FRAMEADDR:
    println("  mov %%rbp, %%rax");
    for (int64_t i = 0; i < node->val; i++)
      println("  mov (%%rax), %%rax");
    if (node->rmw_op)
      println("  mov 8(%%rax), %%rax");
    return;
  case ND_ALLOCA: {
    if (node->var) {
      /* VLA declaration: save rsp, allocate, store the pointer. */
      gen_expr(node->lhs);  /* byte size */
      println("  mov %%rsp, %" PRId64 "(%%rbp)", node->vla_sp->offset);
      println("  add $15, %%rax");
      println("  and $-16, %%rax");
      println("  mov %%rsp, %%rsi");
      println("  sub %%rax, %%rsp");
      if (depth) {
        /* inside a statement expression: keep temporaries on top */
        println("  mov %%rsp, %%rdi");
        println("  mov $%d, %%rcx", depth * 8);
        println("  rep movsb");
      }
      println("  lea %d(%%rsp), %%rax", depth * 8);
      println("  mov %%rax, %" PRId64 "(%%rbp)", node->var->offset);
      return;
    }
    /* __builtin_alloca: the block goes below the pushed temporaries,
       which are moved down to stay on top of the stack. */
    gen_expr(node->lhs);
    println("  add $15, %%rax");
    println("  and $-16, %%rax");
    println("  mov %%rsp, %%rsi");
    println("  sub %%rax, %%rsp");
    if (depth) {
      println("  mov %%rsp, %%rdi");
      println("  mov $%d, %%rcx", depth * 8);
      println("  rep movsb");
    }
    println("  lea %d(%%rsp), %%rax", depth * 8);
    return;
  }
  case ND_UNREACHABLE:
    return;
  case ND_ATOMIC_LOAD:
  case ND_ATOMIC_STORE:
  case ND_EXCH:
  case ND_CAS:
  case ND_ATOMIC_RMW:
  case ND_FENCE:
    gen_atomic(node);
    return;
  default:
    break;
  }

  if (is_flonum(node->lhs->ty)) {
    gen_float_binop(node);
    return;
  }

  char *ax, *di, *dx;
  bool wide = type_size(node->lhs->ty) == 8 || node->lhs->ty->kind == TY_PTR;
  if (wide) {
    ax = "%rax";
    di = "%rdi";
    dx = "%rdx";
  } else {
    ax = "%eax";
    di = "%edi";
    dx = "%edx";
  }

  /* Commutative operations: put the simpler operand on the right. */
  Node *lhs = node->lhs, *rhs = node->rhs;
  bool commutative = node->kind == ND_ADD || node->kind == ND_MUL || node->kind == ND_BITAND ||
                     node->kind == ND_BITOR || node->kind == ND_BITXOR || node->kind == ND_EQ ||
                     node->kind == ND_NE;
  if (commutative && !simple_int_operand(rhs) && simple_int_operand(lhs) &&
      type_size(lhs->ty) == type_size(rhs->ty)) {
    Node *t = lhs;
    lhs = rhs;
    rhs = t;
  }

  /* Immediate right operand */
  int64_t imm;
  if (imm_operand(rhs, wide, &imm) && node->kind != ND_DIV && node->kind != ND_MOD) {
    gen_expr(lhs);
    switch (node->kind) {
    case ND_ADD: if (imm) println("  add $%" PRId64 ", %s", imm, ax); return;
    case ND_SUB: if (imm) println("  sub $%" PRId64 ", %s", imm, ax); return;
    case ND_MUL: println("  imul $%" PRId64 ", %s, %s", imm, ax, ax); return;
    case ND_BITAND: println("  and $%" PRId64 ", %s", imm, ax); return;
    case ND_BITOR: println("  or $%" PRId64 ", %s", imm, ax); return;
    case ND_BITXOR: println("  xor $%" PRId64 ", %s", imm, ax); return;
    case ND_SHL: println("  shl $%d, %s", (int)(imm & 63), ax); return;
    case ND_SHR:
      println("  %s $%d, %s", node->lhs->ty->is_unsigned ? "shr" : "sar", (int)(imm & 63), ax);
      return;
    case ND_EQ: case ND_NE: case ND_LT: case ND_LE: {
      println("  cmp $%" PRId64 ", %s", imm, ax);
      bool u = node->lhs->ty->is_unsigned || node->lhs->ty->kind == TY_PTR;
      char *set = node->kind == ND_EQ ? "sete" : node->kind == ND_NE ? "setne"
                : node->kind == ND_LT ? (u ? "setb" : "setl") : (u ? "setbe" : "setle");
      println("  %s %%al", set);
      println("  movzbl %%al, %%eax");
      return;
    }
    default:
      break;
    }
  }

  gen_int_operands(lhs, rhs);

  switch (node->kind) {
  case ND_ADD:
    println("  add %s, %s", di, ax);
    return;
  case ND_SUB:
    println("  sub %s, %s", di, ax);
    return;
  case ND_MUL:
    println("  imul %s, %s", di, ax);
    return;
  case ND_DIV:
  case ND_MOD:
    if (node->ty->is_unsigned) {
      println("  xor %%edx, %%edx");
      println("  div %s", di);
    } else {
      if (type_size(node->lhs->ty) == 8)
        println("  cqo");
      else
        println("  cltd");
      println("  idiv %s", di);
    }
    if (node->kind == ND_MOD)
      println("  mov %s, %s", dx, ax);
    return;
  case ND_BITAND:
    println("  and %s, %s", di, ax);
    return;
  case ND_BITOR:
    println("  or %s, %s", di, ax);
    return;
  case ND_BITXOR:
    println("  xor %s, %s", di, ax);
    return;
  case ND_EQ:
  case ND_NE:
  case ND_LT:
  case ND_LE:
    println("  cmp %s, %s", di, ax);
    if (node->kind == ND_EQ) {
      println("  sete %%al");
    } else if (node->kind == ND_NE) {
      println("  setne %%al");
    } else if (node->kind == ND_LT) {
      if (node->lhs->ty->is_unsigned)
        println("  setb %%al");
      else
        println("  setl %%al");
    } else if (node->kind == ND_LE) {
      if (node->lhs->ty->is_unsigned)
        println("  setbe %%al");
      else
        println("  setle %%al");
    }
    println("  movzbl %%al, %%eax");
    return;
  case ND_SHL:
    println("  mov %%rdi, %%rcx");
    println("  shl %%cl, %s", ax);
    return;
  case ND_SHR:
    println("  mov %%rdi, %%rcx");
    if (node->lhs->ty->is_unsigned)
      println("  shr %%cl, %s", ax);
    else
      println("  sar %%cl, %s", ax);
    return;
  default:
    break;
  }

  error_tok(node->tok, "invalid expression");
}

/* --------------------------------------------------- statements */

static void restore_vla_sp(Obj *slot) {
  if (slot)
    println("  mov %" PRId64 "(%%rbp), %%rsp", slot->offset);
}

static void gen_stmt(Node *node) {
  emit_loc(node->tok);

  switch (node->kind) {
  case ND_IF: {
    int c = count();
    gen_cond_jump_false(node->cond, format(".L.else.%d", c));
    gen_stmt(node->then);
    println("  jmp .L.end.%d", c);
    println(".L.else.%d:", c);
    if (node->els)
      gen_stmt(node->els);
    println(".L.end.%d:", c);
    return;
  }
  case ND_FOR: {
    int c = count();
    if (node->init)
      gen_stmt(node->init);
    println(".L.begin.%d:", c);
    if (node->cond)
      gen_cond_jump_false(node->cond, node->brk_label);
    gen_stmt(node->then);
    println("%s:", node->cont_label);
    if (node->inc)
      gen_discard(node->inc);
    println("  jmp .L.begin.%d", c);
    println("%s:", node->brk_label);
    restore_vla_sp(node->vla_sp);
    return;
  }
  case ND_DO: {
    int c = count();
    println(".L.begin.%d:", c);
    gen_stmt(node->then);
    println("%s:", node->cont_label);
    gen_cond_jump_true(node->cond, format(".L.begin.%d", c));
    println("%s:", node->brk_label);
    return;
  }
  case ND_SWITCH: {
    gen_expr(node->cond);
    char *ax = type_size(node->cond->ty) == 8 ? "%rax" : "%eax";
    char *di = type_size(node->cond->ty) == 8 ? "%rdi" : "%edi";

    /* Dense switches use a jump table. */
    int ncases = 0;
    int64_t lo = INT64_MAX, hi = INT64_MIN;
    uint64_t span = 0;
    for (Node *n = node->case_next; n; n = n->case_next) {
      ncases += (int)(n->end - n->begin + 1 > 0 ? (n->end - n->begin + 1 > 1000000 ? 1000000 : n->end - n->begin + 1) : 1);
      if (n->begin < lo) lo = n->begin;
      if (n->end > hi) hi = n->end;
    }
    if (ncases > 0)
      span = (uint64_t)hi - (uint64_t)lo;
    if (ncases >= 4 && span < 4096 && span <= (uint64_t)ncases * 4) {
      int c = count();
      char *dflt = node->default_case ? node->default_case->label : node->brk_label;
      if (lo != 0) {
        if (lo >= INT32_MIN && lo <= INT32_MAX)
          println("  sub $%" PRId64 ", %s", lo, ax);
        else {
          println("  mov $%" PRId64 ", %%rdi", lo);
          println("  sub %%rdi, %%rax");
        }
      }
      println("  cmp $%" PRIu64 ", %s", span, ax);
      println("  ja %s", dflt);
      if (type_size(node->cond->ty) != 8)
        println("  mov %%eax, %%eax");
      println("  lea .L.jt.%d(%%rip), %%rdx", c);
      println("  movslq (%%rdx,%%rax,4), %%rax");
      println("  add %%rdx, %%rax");
      println("  jmp *%%rax");
      println("  .section .rodata");
      println("  .balign 4");
      println(".L.jt.%d:", c);
      for (uint64_t v = 0; v <= span; v++) {
        int64_t val = (int64_t)((uint64_t)lo + v);
        char *target = dflt;
        for (Node *n = node->case_next; n; n = n->case_next)
          if (n->begin <= val && val <= n->end) {
            target = n->label;
            break;
          }
        println("  .long %s-.L.jt.%d", target, c);
      }
      println("  .text");
      gen_stmt(node->then);
      println("%s:", node->brk_label);
      return;
    }
    for (Node *n = node->case_next; n; n = n->case_next) {
      if (n->begin == n->end) {
        if (type_size(node->cond->ty) == 8 && (n->begin > INT32_MAX || n->begin < INT32_MIN)) {
          println("  mov $%" PRId64 ", %%rdi", n->begin);
          println("  cmp %%rdi, %%rax");
        } else {
          println("  cmp $%" PRId64 ", %s", n->begin, ax);
        }
        println("  je %s", n->label);
        continue;
      }
      /* case ranges */
      println("  mov %s, %s", ax, di);
      println("  mov $%" PRId64 ", %%rdx", n->begin);
      println("  sub %%rdx, %%rdi");
      println("  mov $%" PRId64 ", %%rdx", n->end - n->begin);
      if (type_size(node->cond->ty) == 8)
        println("  cmp %%rdx, %%rdi");
      else
        println("  cmp %%edx, %%edi");
      println("  jbe %s", n->label);
    }
    if (node->default_case)
      println("  jmp %s", node->default_case->label);
    println("  jmp %s", node->brk_label);
    gen_stmt(node->then);
    println("%s:", node->brk_label);
    return;
  }
  case ND_CASE:
    println("%s:", node->label);
    gen_stmt(node->lhs);
    return;
  case ND_BLOCK:
    for (Node *n = node->body; n; n = n->next)
      gen_stmt(n);
    restore_vla_sp(node->vla_sp);
    return;
  case ND_GOTO:
    restore_vla_sp(node->target_vla_sp);
    println("  jmp %s", node->unique_label);
    return;
  case ND_LABEL:
    println("%s:", node->unique_label);
    gen_stmt(node->lhs);
    return;
  case ND_RETURN:
    if (node->lhs) {
      gen_expr(node->lhs);
      Type *ty = node->lhs->ty;
      if (is_void(current_fn->ty->return_ty)) {
        if (ty->kind == TY_LDOUBLE)
          println("  fstp %%st(0)");
      } else if (is_aggval(ty)) {
        if (ret_complex_x87(ty)) {
          println("  fldt 16(%%rax)");
          println("  fldt (%%rax)");
        } else if (su_ret_x87(ty))
          println("  fldt (%%rax)");
        else if (su_in_memory(ty))
          copy_struct_mem();
        else
          copy_struct_reg();
      }
    }
    println("  jmp .L.return.%s", current_fn->name);
    return;
  case ND_EXPR_STMT:
    gen_discard(node->lhs);
    return;
  case ND_ASM:
    println("  %s", node->asm_str);
    return;
  default:
    break;
  }

  error_tok(node->tok, "invalid statement");
}

/* ------------------------------------------------ frame layout */

/* Assigns offsets to local variables; parameters passed on the stack
   keep their location in the caller's frame. */
static void assign_lvar_offsets(Obj *prog) {
  for (Obj *fn = prog; fn; fn = fn->next) {
    if (!fn->is_function || !fn->body)
      continue;

    int top = 16;
    int bottom = 0;
    int gp = 0, fp = 0;

    if (is_su(fn->ty->return_ty) && su_ret_in_memory(fn->ty->return_ty))
      gp++;

    /* Parameters passed by stack */
    Param *p = fn->ty->params;
    for (Obj *var = fn->params; var; var = var->next, p = p ? p->next : NULL) {
      Type *ty = var->ty;
      switch (abi_kind(ty)) {
      case TY_STRUCT:
      case TY_UNION:
        if (su_in_memory(ty))
          goto by_stack;
        {
          bool fp1 = has_flonum1(ty);
          bool fp2 = type_size(ty) > 8 && has_flonum2(ty);
          int nfp = fp1 + fp2;
          int ngp = !fp1 + (type_size(ty) > 8 && !fp2);
          if (fp + nfp <= FP_MAX && gp + ngp <= GP_MAX) {
            fp += nfp;
            gp += ngp;
            continue;
          }
        }
        goto by_stack;
      case TY_FLOAT:
      case TY_DOUBLE:
        if (fp < FP_MAX) {
          fp++;
          continue;
        }
        goto by_stack;
      case TY_LDOUBLE:
        goto by_stack;
      default:
        if (gp < GP_MAX) {
          gp++;
          continue;
        }
      }
    by_stack:
      top = 16 + align_to(top - 16, stack_arg_align(ty));
      var->offset = top;
      top += align_to64(type_size(var->ty), 8);
    }

    /* Register parameters and locals */
    for (Obj *var = fn->locals; var; var = var->next) {
      if (var->offset)
        continue;
      int64_t sz = var->is_vla_ptr ? 8 : type_size(var->ty);
      if (sz < 0)
        sz = 8;
      int align = var->is_vla_ptr ? 8 : var->align;
      if (align < 1)
        align = 1;
      /* AMD64 ABI: arrays of 16 bytes or more are 16-byte aligned */
      if (var->ty->kind == TY_ARRAY && sz >= 16 && align < 16)
        align = 16;
      bottom += sz;
      bottom = align_to(bottom, align);
      var->offset = -bottom;
    }
    bottom += 8;  /* hidden struct-return pointer slot */
    fn->struct_ret = xcalloc(1, sizeof(Obj));
    fn->struct_ret->offset = -bottom;
    fn->stack_size = align_to(bottom, 16);
  }
}

static void store_fp(int r, int64_t offset, int sz) {
  switch (sz) {
  case 4:
    println("  movss %%xmm%d, %" PRId64 "(%%rbp)", r, offset);
    return;
  case 8:
    println("  movsd %%xmm%d, %" PRId64 "(%%rbp)", r, offset);
    return;
  }
  unreachable();
}

static void store_gp(int r, int64_t offset, int sz) {
  switch (sz) {
  case 1:
    println("  mov %s, %" PRId64 "(%%rbp)", argreg8[r], offset);
    return;
  case 2:
    println("  mov %s, %" PRId64 "(%%rbp)", argreg16[r], offset);
    return;
  case 4:
    println("  mov %s, %" PRId64 "(%%rbp)", argreg32[r], offset);
    return;
  case 8:
    println("  mov %s, %" PRId64 "(%%rbp)", argreg64[r], offset);
    return;
  default:
    for (int i = 0; i < sz; i++) {
      println("  mov %s, %" PRId64 "(%%rbp)", argreg8[r], offset + i);
      println("  shr $8, %s", argreg64[r]);
    }
    return;
  }
}

/* ------------------------------------------------------ data */

static bool is_readonly(Obj *var) {
  if (var->is_string_lit)
    return true;
  if (var->is_tls)
    return false;
  Type *ty = var->ty;
  while (is_array(ty))
    ty = ty->base;
  return (ty->qual & Q_CONST) && !(ty->qual & Q_VOLATILE) && !is_su(ty);
}

static bool all_zero(char *p, int64_t n) {
  for (int64_t i = 0; i < n; i++)
    if (p[i])
      return false;
  return true;
}

static void emit_data(Obj *prog) {
  for (Obj *var = prog; var; var = var->next) {
    if (var->is_function)
      continue;
    if (var->name && !strcmp(var->name, "__toplevel_asm__") && !var->asm_name) {
      println("%s", var->init_data);
      continue;
    }
    if (!var->is_definition)
      continue;

    int64_t size = type_size(var->ty);
    if (size < 0)
      size = 0;
    if (var->data_size > size)
      size = var->data_size;
    int align = var->align;
    if (var->ty->kind == TY_ARRAY && size >= 16 && align < 16)
      align = 16;

    char *name = sym(var);
    if (var->is_static)
      println("  .local %s", name);
    else
      println("  .globl %s", name);

    bool zero = !var->init_data || (all_zero(var->init_data, size) && !var->rel);

    if (var->section) {
      println("  .section %s,\"aw%s\",@progbits", var->section, var->is_tls ? "T" : "");
      println("  .type %s, @%s", name, var->is_tls ? "tls_object" : "object");
      zero = false;
      if (!var->init_data)
        var->init_data = calloc(1, size ? size : 1);
    } else if (var->is_tls) {
      println("  .section .%s,\"awT\",@%s", zero ? "tbss" : "tdata", zero ? "nobits" : "progbits");
      println("  .type %s, @tls_object", name);
    } else if (zero) {
      println("  .bss");
      println("  .type %s, @object", name);
    } else if (is_readonly(var) && !var->rel) {
      println("  .section .rodata");
      println("  .type %s, @object", name);
    } else if (is_readonly(var) && var->rel && !opt_pic) {
      println("  .section .rodata");
      println("  .type %s, @object", name);
    } else {
      println("  .data");
      println("  .type %s, @object", name);
    }
    println("  .size %s, %" PRId64, name, size);
    println("  .balign %d", align);
    println("%s:", name);

    if (zero) {
      println("  .zero %" PRId64, size ? size : 1);
      continue;
    }

    Relocation *rel = var->rel;
    int64_t pos = 0;
    while (pos < size) {
      if (rel && rel->offset == pos) {
        if (rel->addend)
          println("  .quad %s%+" PRId64, *rel->label, rel->addend);
        else
          println("  .quad %s", *rel->label);
        rel = rel->next;
        pos += 8;
        continue;
      }
      /* Runs of zeros */
      int64_t end = rel ? rel->offset : size;
      int64_t z = pos;
      while (z < end && !var->init_data[z])
        z++;
      if (z - pos >= 8) {
        println("  .zero %" PRId64, z - pos);
        pos = z;
        continue;
      }
      println("  .byte %d", (unsigned char)var->init_data[pos++]);
    }
  }
}

/* ---------------------------------------------------- functions */

static void emit_text(Obj *prog) {
  for (Obj *fn = prog; fn; fn = fn->next) {
    if (!fn->is_function || !fn->body || !fn->is_live)
      continue;
    if (fn->is_inline && !fn->is_extern_inline && !fn->is_static)
      continue;  /* inline definition: no external definition here */

    char *name = sym(fn);
    if (fn->is_static)
      println("  .local %s", name);
    else
      println("  .globl %s", name);

    if (fn->section)
      println("  .section %s,\"ax\",@progbits", fn->section);
    else
      println("  .text");
    println("  .type %s, @function", name);
    println("  .p2align 4");
    println("%s:", name);
    current_fn = fn;
    struct_ret_offset = fn->struct_ret->offset;
    last_loc_file = NULL;
    emit_loc(fn->tok);  /* line entry at the function's entry point */
    last_loc_file = NULL;

    /* Prologue */
    println("  push %%rbp");
    println("  mov %%rsp, %%rbp");
    if (fn->stack_size)
      println("  sub $%" PRId64 ", %%rsp", fn->stack_size);
    depth = 0;

    /* Save the hidden struct-return pointer */
    int gp = 0, fp = 0;
    if (is_su(fn->ty->return_ty) && su_ret_in_memory(fn->ty->return_ty)) {
      println("  mov %s, %" PRId64 "(%%rbp)", argreg64[gp++], struct_ret_offset);
    }

    /* Register save area for variadic functions */
    if (fn->va_area) {
      int64_t off = fn->va_area->offset;
      for (int i = 0; i < 6; i++)
        println("  mov %s, %" PRId64 "(%%rbp)", argreg64[i], off + i * 8);
      int c = count();
      println("  test %%al, %%al");
      println("  je .L.nofp.%d", c);
      for (int i = 0; i < 8; i++)
        println("  movaps %%xmm%d, %" PRId64 "(%%rbp)", i, off + 48 + i * 16);
      println(".L.nofp.%d:", c);
    }

    /* Spill register parameters */
    bool oldstyle = !fn->ty->has_proto;
    for (Obj *var = fn->params; var; var = var->next) {
      if (var->offset > 0)
        continue;
      Type *ty = var->ty;
      switch (abi_kind(ty)) {
      case TY_STRUCT:
      case TY_UNION: {
        int64_t sz = type_size(ty);
        if (has_flonum(ty, 0, 8, 0))
          store_fp(fp++, var->offset, sz < 8 ? 4 : 8);
        else
          store_gp(gp++, var->offset, sz < 8 ? sz : 8);
        if (sz > 8) {
          if (has_flonum(ty, 8, 16, 0))
            store_fp(fp++, var->offset + 8, sz - 8 < 8 ? 4 : 8);
          else
            store_gp(gp++, var->offset + 8, sz - 8 < 8 ? sz - 8 : 8);
        }
        break;
      }
      case TY_FLOAT:
        if (oldstyle) {
          /* promoted to double by the caller */
          println("  cvtsd2ss %%xmm%d, %%xmm%d", fp, fp);
        }
        store_fp(fp++, var->offset, 4);
        break;
      case TY_DOUBLE:
        store_fp(fp++, var->offset, 8);
        break;
      default:
        store_gp(gp++, var->offset, type_size(ty));
      }
    }

    /* Body */
    gen_stmt(fn->body);
    assert(depth == 0);

    /* Reaching the end of main returns 0 (5.1.2.2.3). */
    if (!strcmp(fn->name, "main"))
      println("  xor %%eax, %%eax");

    /* Epilogue */
    println(".L.return.%s:", fn->name);
    println("  mov %%rbp, %%rsp");
    println("  pop %%rbp");
    println("  ret");
    println("  .size %s, .-%s", name, name);
  }
}

/* weak, alias, visibility, constructor and destructor (GNU attributes) */
static void emit_symbol_attributes(Obj *prog) {
  for (Obj *v = prog; v; v = v->next) {
    if (v->is_local || v->is_string_lit || v->is_compound_lit || !v->name)
      continue;
    if (v->is_function && v->body && !v->is_live)
      continue;
    char *name = sym(v);
    if (v->alias) {
      if (!v->is_static)
        println("  .globl %s", name);
      println("  .type %s, @%s", name, v->is_function ? "function" : "object");
      println("  .set %s, %s", name, v->alias);
    }
    bool referenced = v->is_definition || v->alias || v->is_used || v->is_addr_taken ||
                      (v->is_function && v->body);
    if (v->is_weak && referenced)
      println("  .weak %s", name);
    if (v->visibility && referenced)
      println("  .%s %s", v->visibility, name);
    if (v->is_function && (v->ctor_prio || v->dtor_prio)) {
      for (int k = 0; k < 2; k++) {
        int prio = k ? v->dtor_prio : v->ctor_prio;
        if (!prio)
          continue;
        char *sec = k ? ".fini_array" : ".init_array";
        if (prio == 65536)
          println("  .section %s,\"aw\"", sec);
        else
          println("  .section %s.%05d,\"aw\"", sec, prio);
        println("  .balign 8");
        println("  .quad %s", name);
      }
    }
  }
}

void codegen(Obj *prog, FILE *out) {
  output_file = out;
  file_numbers = (HashMap){0};
  nfile_numbers = 0;
  last_loc_file = NULL;

  if (opt_g && base_file) {
    fprintf(out, "  .file \"%s\"\n", base_file);
  }
  assign_lvar_offsets(prog);
  emit_data(prog);
  emit_text(prog);
  emit_symbol_attributes(prog);
  println("  .section .note.GNU-stack,\"\",@progbits");
}
