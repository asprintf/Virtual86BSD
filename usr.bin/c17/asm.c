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

/* asm.c - the assembler: x86-64 assembly in the AT&T syntax of the GNU
   assembler (the subset compilers and hand-written system code use) to
   ELF-64 relocatable object files.
 *
 * It assembles what c17's code generator writes and .s operands.  Jumps
 * and calls always take the 32-bit displacement form (no relaxation);
 * otherwise the shortest encodings are chosen, as the GNU assembler does.
 * .file/.loc directives produce DWARF line tables for -g.
 */
#include "c17.h"
#include "elf.h"

/* ------------------------------------------------------------ data */

typedef struct Section Section;
typedef struct Sym Sym;

typedef struct {
  int64_t offset;
  int type;
  Sym *sym;          /* NULL: relative to section `sec` */
  Section *sec;
  int64_t addend;
} Reloc;

struct Section {
  char *name;
  int index;         /* in the output */
  uint32_t type;
  uint64_t flags;
  uint64_t align;
  Buf data;
  int64_t size;      /* for SHT_NOBITS */
  Reloc *rel;
  int nrel, caprel;
  Sym *secsym;
  int symidx;        /* index of its section symbol */
};

struct Sym {
  char *name;
  Section *sec;      /* NULL: undefined (or absolute if is_abs) */
  int64_t value;
  bool defined;
  bool is_abs;
  bool global;
  bool weak;
  bool local_decl;   /* .local */
  bool used;
  bool is_common;
  int64_t common_align;
  int type;          /* STT_* */
  int vis;
  int64_t size;
  bool has_size;
  struct Expr *size_expr;
  struct Expr *set_expr;  /* .set */
  int index;         /* in .symtab */
};

typedef enum { RM_NONE, RM_PLT, RM_GOTPCREL, RM_TPOFF, RM_GOTTPOFF, RM_TLSGD, RM_TLSLD,
               RM_DTPOFF, RM_GOTOFF, RM_GOT } RelocMod;

typedef struct Expr {
  Sym *sym;
  Sym *sub;
  int64_t val;
  RelocMod mod;
  /* an operation whose operands are not known yet (a difference of
     labels defined later): op is the operator, l and r the operands */
  char op;
  struct Expr *l, *r;
} Expr;

/* A reference to be resolved when all labels are known */
typedef struct {
  Section *sec;
  int64_t offset;   /* of the field */
  int size;         /* 1, 2, 4 or 8 */
  bool pcrel;
  int64_t adjust;   /* added for pc-relative fields: -(bytes from field to end of insn) */
  Expr e;
  bool is_call;
  bool is_branch;
  bool sign32;      /* absolute 32-bit field that is sign-extended */
  int line;
} Fixup;

static char *filename;
static int lineno;
static HashMap syms;
static Section **sections;
static int nsections;
static Section *cur;
static Section *prev_sec;
static Fixup *fixups;
static int nfixups, capfixups;
static bool errors;
static int numlabel_count[10];   /* numeric local labels: definitions so far */
static int anon_count;

/* -g line information */
typedef struct {
  int file;
  int line;
  int64_t offset;   /* in .text */
  Section *sec;
} LineEntry;
static LineEntry *lines;
static int nlines, caplines;
static char **files;   /* .file N "name" */
static int nfiles;
static char *main_file;

static void asm_error(char *fmt, ...) PRINTF(1, 2);
static void asm_error(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fprintf(stderr, "%s:%d: error: ", filename, lineno);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  va_end(ap);
  errors = true;
}

static Section *get_section(char *name, uint32_t type, uint64_t flags) {
  for (int i = 0; i < nsections; i++)
    if (!strcmp(sections[i]->name, name))
      return sections[i];
  Section *s = xcalloc(1, sizeof(Section));
  s->name = xstrdup(name);
  s->type = type;
  s->flags = flags;
  s->align = 1;
  sections = xrealloc(sections, sizeof(Section *) * (nsections + 1));
  sections[nsections++] = s;
  return s;
}

/* The flags and type of a section known by name */
static Section *named_section(char *name) {
  if (!strcmp(name, ".text") || startswith(name, ".text."))
    return get_section(name, SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR);
  if (!strcmp(name, ".data") || startswith(name, ".data."))
    return get_section(name, SHT_PROGBITS, SHF_ALLOC | SHF_WRITE);
  if (!strcmp(name, ".bss") || startswith(name, ".bss."))
    return get_section(name, SHT_NOBITS, SHF_ALLOC | SHF_WRITE);
  if (!strcmp(name, ".rodata") || startswith(name, ".rodata."))
    return get_section(name, SHT_PROGBITS, SHF_ALLOC);
  if (!strcmp(name, ".tdata") || startswith(name, ".tdata."))
    return get_section(name, SHT_PROGBITS, SHF_ALLOC | SHF_WRITE | SHF_TLS);
  if (!strcmp(name, ".tbss") || startswith(name, ".tbss."))
    return get_section(name, SHT_NOBITS, SHF_ALLOC | SHF_WRITE | SHF_TLS);
  if (!strcmp(name, ".init_array") || startswith(name, ".init_array."))
    return get_section(name, SHT_INIT_ARRAY, SHF_ALLOC | SHF_WRITE);
  if (!strcmp(name, ".fini_array") || startswith(name, ".fini_array."))
    return get_section(name, SHT_FINI_ARRAY, SHF_ALLOC | SHF_WRITE);
  if (!strcmp(name, ".preinit_array"))
    return get_section(name, SHT_PREINIT_ARRAY, SHF_ALLOC | SHF_WRITE);
  if (!strcmp(name, ".init") || !strcmp(name, ".fini"))
    return get_section(name, SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR);
  if (startswith(name, ".note"))
    return get_section(name, SHT_NOTE, 0);
  if (startswith(name, ".debug") || !strcmp(name, ".comment"))
    return get_section(name, SHT_PROGBITS, 0);
  return get_section(name, SHT_PROGBITS, SHF_ALLOC);
}

static int64_t cur_offset(void) {
  return cur->type == SHT_NOBITS ? cur->size : (int64_t)cur->data.len;
}

static void emit8(int b) {
  if (cur->type == SHT_NOBITS) {
    if (b)
      asm_error("data in a section without contents (%s)", cur->name);
    cur->size++;
    return;
  }
  buf_putc(&cur->data, b);
}

static void emit16(uint16_t v) { emit8(v & 0xff); emit8(v >> 8); }
static void emit32(uint32_t v) { emit16(v & 0xffff); emit16(v >> 16); }

static void emitn(uint64_t v, int size) {
  for (int i = 0; i < size; i++)
    emit8((v >> (8 * i)) & 0xff);
}

/* ---------------------------------------------------------- symbols */

static Sym *get_sym(char *name) {
  Sym *s = hashmap_get(&syms, name);
  if (!s) {
    s = xcalloc(1, sizeof(Sym));
    s->name = xstrdup(name);
    hashmap_put(&syms, s->name, s);
  }
  return s;
}

static void define_label(char *name) {
  Sym *s = get_sym(name);
  if (s->defined) {
    asm_error("symbol '%s' is already defined", name);
    return;
  }
  s->defined = true;
  s->sec = cur;
  s->value = cur_offset();
}

/* A symbol for the current location ('.') */
static Sym *here(void) {
  char name[32];
  snprintf(name, sizeof(name), ".L.here.%d", anon_count++);
  Sym *s = get_sym(name);
  s->defined = true;
  s->sec = cur;
  s->value = cur_offset();
  return s;
}

static bool is_temp(Sym *s) { return startswith(s->name, ".L") || startswith(s->name, "\002"); }

/* ------------------------------------------------------- the lexer */

static char *p;   /* position in the current statement */

static void skip_ws(void) {
  while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\f' || *p == '\v')
    p++;
}

static bool is_sym1(int c) {
  return isalpha(c) || c == '_' || c == '.' || c == '$' || (unsigned char)c >= 0x80;
}
static bool is_sym2(int c) { return is_sym1(c) || isdigit(c); }

static char *read_name(void) {
  char *start = p;
  if (*p == '"') {  /* quoted symbol name */
    p++;
    while (*p && *p != '"')
      p++;
    char *n = xstrndup(start + 1, p - start - 1);
    if (*p == '"')
      p++;
    return n;
  }
  if (!is_sym1((unsigned char)*p))
    return NULL;
  while (is_sym2((unsigned char)*p))
    p++;
  return xstrndup(start, p - start);
}

static bool consume_ch(int c) {
  skip_ws();
  if (*p == c) {
    p++;
    return true;
  }
  return false;
}

/* Numeric local labels: "1:" defines, "1b"/"1f" refer backward/forward. */
static char *numlabel_name(int n, int instance) {
  return format("\002L%d.%d", n, instance);
}

/* ----------------------------------------------------- expressions */

static bool parse_expr(Expr *e);

static bool parse_number(int64_t *out) {
  char *start = p;
  uint64_t v = 0;
  if (*p == '\'' ) {  /* 'c */
    p++;
    int c = (unsigned char)*p;
    if (c == '\\') {
      p++;
      switch (*p) {
      case 'n': c = '\n'; break;
      case 't': c = '\t'; break;
      case '0': c = 0; break;
      case '\\': c = '\\'; break;
      case '\'': c = '\''; break;
      default: c = (unsigned char)*p;
      }
    }
    p++;
    if (*p == '\'')
      p++;
    *out = c;
    return true;
  }
  if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
    p += 2;
    if (!isxdigit((unsigned char)*p))
      return false;
    while (isxdigit((unsigned char)*p)) {
      int d = isdigit((unsigned char)*p) ? *p - '0' : (tolower((unsigned char)*p) - 'a' + 10);
      v = v * 16 + d;
      p++;
    }
  } else if (p[0] == '0' && (p[1] == 'b' || p[1] == 'B') && (p[2] == '0' || p[2] == '1')) {
    p += 2;
    while (*p == '0' || *p == '1')
      v = v * 2 + (*p++ - '0');
  } else if (p[0] == '0' && isdigit((unsigned char)p[1])) {
    while (*p >= '0' && *p <= '7')
      v = v * 8 + (*p++ - '0');
  } else if (isdigit((unsigned char)*p)) {
    while (isdigit((unsigned char)*p))
      v = v * 10 + (*p++ - '0');
  } else {
    return false;
  }
  if (is_sym2((unsigned char)*p) && !(p > start && (*p == 'b' || *p == 'f'))) {
    p = start;
    return false;
  }
  *out = (int64_t)v;
  return true;
}

static bool const_value(Expr *e, int64_t *v);
static bool combine(Expr *e, Expr *r, char op);

static void expr_add(Expr *a, Expr *b, int sign) {
  if (a->op || b->op) {
    combine(a, b, sign > 0 ? '+' : '-');
    return;
  }
  if (b->sym) {
    if (sign > 0) {
      if (a->sym) {
        asm_error("invalid expression: sum of two symbols");
        return;
      }
      a->sym = b->sym;
    } else {
      if (a->sub) {
        asm_error("invalid expression");
        return;
      }
      a->sub = b->sym;
    }
  }
  if (b->sub) {
    if (sign > 0) {
      if (a->sub) {
        asm_error("invalid expression");
        return;
      }
      a->sub = b->sub;
    } else {
      if (a->sym) {
        asm_error("invalid expression");
        return;
      }
      a->sym = b->sub;
    }
  }
  a->val += sign * b->val;
  if (b->mod)
    a->mod = b->mod;
}

static bool parse_primary(Expr *e) {
  memset(e, 0, sizeof(*e));
  skip_ws();
  if (*p == '(') {
    p++;
    if (!parse_expr(e))
      return false;
    if (!consume_ch(')')) {
      asm_error("expected ')'");
      return false;
    }
    return true;
  }
  if (*p == '-') {
    p++;
    Expr t;
    if (!parse_primary(&t))
      return false;
    if (t.sym || t.sub) {
      e->sym = t.sub;
      e->sub = t.sym;
    }
    e->val = -t.val;
    return true;
  }
  if (*p == '+') {
    p++;
    return parse_primary(e);
  }
  if (*p == '~') {
    p++;
    Expr t;
    if (!parse_primary(&t))
      return false;
    if (t.sym || t.sub)
      asm_error("invalid use of '~' on a symbol");
    e->val = ~t.val;
    return true;
  }
  if (isdigit((unsigned char)*p)) {
    /* numeric local label reference: 1b, 1f */
    if (isdigit((unsigned char)p[0]) && (p[1] == 'b' || p[1] == 'f') && !is_sym2((unsigned char)p[2])) {
      int n = p[0] - '0';
      int inst = p[1] == 'b' ? numlabel_count[n] - 1 : numlabel_count[n];
      if (inst < 0) {
        asm_error("backward reference to undefined label %db", n);
        return false;
      }
      e->sym = get_sym(numlabel_name(n, inst));
      p += 2;
      return true;
    }
    return parse_number(&e->val);
  }
  if (*p == '\'')
    return parse_number(&e->val);
  if (*p == '.' && !is_sym2((unsigned char)p[1])) {
    p++;
    e->sym = here();
    return true;
  }
  char *name = read_name();
  if (!name) {
    asm_error("expected an expression at '%.20s'", p);
    return false;
  }
  Sym *s = get_sym(name);
  s->used = true;
  if (s->set_expr && s->set_expr->op) {
    *e = *s->set_expr;
  } else if (s->set_expr && !s->set_expr->sym && !s->set_expr->sub) {
    e->val = s->set_expr->val;
  } else {
    e->sym = s;
  }
  /* @modifier */
  if (*p == '@') {
    p++;
    char *m = read_name();
    if (!m) {
      asm_error("expected a relocation modifier after '@'");
      return false;
    }
    if (!strcasecmp(m, "PLT")) e->mod = RM_PLT;
    else if (!strcasecmp(m, "GOTPCREL")) e->mod = RM_GOTPCREL;
    else if (!strcasecmp(m, "TPOFF")) e->mod = RM_TPOFF;
    else if (!strcasecmp(m, "GOTTPOFF")) e->mod = RM_GOTTPOFF;
    else if (!strcasecmp(m, "TLSGD")) e->mod = RM_TLSGD;
    else if (!strcasecmp(m, "TLSLD")) e->mod = RM_TLSLD;
    else if (!strcasecmp(m, "DTPOFF")) e->mod = RM_DTPOFF;
    else if (!strcasecmp(m, "GOTOFF")) e->mod = RM_GOTOFF;
    else if (!strcasecmp(m, "GOT")) e->mod = RM_GOT;
    else
      asm_error("unknown relocation modifier '@%s'", m);
  }
  return true;
}

static bool absolute(Expr *e) { return !e->sym && !e->sub && !e->mod && !e->op; }

static bool apply_op(char op, int64_t a, int64_t b, int64_t *v) {
  switch (op) {
  case '+': *v = a + b; return true;
  case '-': *v = a - b; return true;
  case '*': *v = a * b; return true;
  case '/':
  case '%':
    if (!b) {
      asm_error("division by zero");
      return false;
    }
    *v = op == '/' ? a / b : a % b;
    return true;
  case '<': *v = (int64_t)((uint64_t)a << b); return true;
  case '>': *v = a >> b; return true;
  case '&': *v = a & b; return true;
  case '|': *v = a | b; return true;
  case '^': *v = a ^ b; return true;
  }
  return false;
}

/* e = e op r, now if the values are known, else when they are */
static bool combine(Expr *e, Expr *r, char op) {
  int64_t a, b, v;
  if (const_value(e, &a) && const_value(r, &b)) {
    if (!apply_op(op, a, b, &v))
      return false;
    memset(e, 0, sizeof(*e));
    e->val = v;
    return true;
  }
  if (e->mod || r->mod) {
    asm_error("invalid operation on a relocatable expression");
    return false;
  }
  Expr *l = xcalloc(1, sizeof(Expr)), *rr = xcalloc(1, sizeof(Expr));
  *l = *e;
  *rr = *r;
  memset(e, 0, sizeof(*e));
  e->op = op;
  e->l = l;
  e->r = rr;
  return true;
}

static bool parse_mul(Expr *e) {
  if (!parse_primary(e))
    return false;
  for (;;) {
    skip_ws();
    char op = *p;
    if (op == '*' || op == '/' || op == '%' || (op == '<' && p[1] == '<') ||
        (op == '>' && p[1] == '>')) {
      p += (op == '<' || op == '>') ? 2 : 1;
      Expr r;
      if (!parse_primary(&r))
        return false;
      if (!combine(e, &r, op))
        return false;
      continue;
    }
    return true;
  }
}

static bool parse_add(Expr *e) {
  if (!parse_mul(e))
    return false;
  for (;;) {
    skip_ws();
    if (*p == '+' || *p == '-') {
      int sign = *p == '+' ? 1 : -1;
      p++;
      Expr r;
      if (!parse_mul(&r))
        return false;
      expr_add(e, &r, sign);
      continue;
    }
    return true;
  }
}

static bool parse_expr(Expr *e) {
  if (!parse_add(e))
    return false;
  for (;;) {
    skip_ws();
    if ((*p == '&' || *p == '|' || *p == '^') && p[1] != *p) {
      char op = *p++;
      Expr r;
      if (!parse_add(&r))
        return false;
      if (!combine(e, &r, op))
        return false;
      continue;
    }
    return true;
  }
}

/* Value of an expression that must be known now (directives) */
static bool const_value(Expr *e, int64_t *v) {
  if (e->op) {
    int64_t a, b;
    return const_value(e->l, &a) && const_value(e->r, &b) && apply_op(e->op, a, b, v);
  }
  if (e->mod)
    return false;
  if (!e->sym && !e->sub) {
    *v = e->val;
    return true;
  }
  if (e->sym && e->sub && e->sym->defined && e->sub->defined && e->sym->sec == e->sub->sec) {
    *v = e->sym->value - e->sub->value + e->val;
    return true;
  }
  return false;
}

/* --------------------------------------------------------- operands */

enum { OP_NONE, OP_REG, OP_XMM, OP_ST, OP_IMM, OP_MEM, OP_CR, OP_SEG };

/* .code32: 32-bit protected-mode code (a kernel's entry from a boot
   loader): no REX prefixes, 32-bit address registers, and an absolute
   address is a plain disp32. */
static bool code32;

typedef struct {
  int kind;
  int reg;        /* register number */
  int size;       /* register size in bytes (1, 2, 4, 8) */
  bool rex8;      /* spl/bpl/sil/dil: needs a REX prefix */
  bool high8;     /* ah, ch, dh, bh */
  Expr e;         /* immediate or displacement */
  int base, index, scale;  /* -1 if none */
  bool rip;
  int seg;        /* 0 or the prefix byte */
  bool indirect;  /* *operand */
} Operand;

static struct { char *name; int num; int size; } gpregs[] = {
    {"rax", 0, 8}, {"rcx", 1, 8}, {"rdx", 2, 8}, {"rbx", 3, 8}, {"rsp", 4, 8}, {"rbp", 5, 8},
    {"rsi", 6, 8}, {"rdi", 7, 8}, {"r8", 8, 8}, {"r9", 9, 8}, {"r10", 10, 8}, {"r11", 11, 8},
    {"r12", 12, 8}, {"r13", 13, 8}, {"r14", 14, 8}, {"r15", 15, 8},
    {"eax", 0, 4}, {"ecx", 1, 4}, {"edx", 2, 4}, {"ebx", 3, 4}, {"esp", 4, 4}, {"ebp", 5, 4},
    {"esi", 6, 4}, {"edi", 7, 4}, {"r8d", 8, 4}, {"r9d", 9, 4}, {"r10d", 10, 4},
    {"r11d", 11, 4}, {"r12d", 12, 4}, {"r13d", 13, 4}, {"r14d", 14, 4}, {"r15d", 15, 4},
    {"ax", 0, 2}, {"cx", 1, 2}, {"dx", 2, 2}, {"bx", 3, 2}, {"sp", 4, 2}, {"bp", 5, 2},
    {"si", 6, 2}, {"di", 7, 2}, {"r8w", 8, 2}, {"r9w", 9, 2}, {"r10w", 10, 2},
    {"r11w", 11, 2}, {"r12w", 12, 2}, {"r13w", 13, 2}, {"r14w", 14, 2}, {"r15w", 15, 2},
    {"al", 0, 1}, {"cl", 1, 1}, {"dl", 2, 1}, {"bl", 3, 1}, {"spl", 4, 1}, {"bpl", 5, 1},
    {"sil", 6, 1}, {"dil", 7, 1}, {"r8b", 8, 1}, {"r9b", 9, 1}, {"r10b", 10, 1},
    {"r11b", 11, 1}, {"r12b", 12, 1}, {"r13b", 13, 1}, {"r14b", 14, 1}, {"r15b", 15, 1},
    {"r8l", 8, 1}, {"r9l", 9, 1}, {"r10l", 10, 1}, {"r11l", 11, 1}, {"r12l", 12, 1},
    {"r13l", 13, 1}, {"r14l", 14, 1}, {"r15l", 15, 1},
};

static bool parse_register(Operand *op) {
  /* p points after '%' */
  char *start = p;
  while (isalnum((unsigned char)*p))
    p++;
  int len = p - start;
  char name[16];
  if (len >= (int)sizeof(name))
    return false;
  memcpy(name, start, len);
  name[len] = '\0';
  for (int i = 0; i < len; i++)
    name[i] = tolower((unsigned char)name[i]);

  if (!strcmp(name, "st")) {
    op->kind = OP_ST;
    op->reg = 0;
    if (*p == '(') {
      p++;
      if (!isdigit((unsigned char)*p))
        return false;
      op->reg = *p++ - '0';
      if (*p != ')')
        return false;
      p++;
    }
    return true;
  }
  if (startswith(name, "xmm")) {
    int n = atoi(name + 3);
    if (n < 0 || n > 15)
      return false;
    op->kind = OP_XMM;
    op->reg = n;
    op->size = 16;
    return true;
  }
  if (name[0] == 'c' && name[1] == 'r' && isdigit((unsigned char)name[2]) &&
      (!name[3] || (isdigit((unsigned char)name[3]) && !name[4]))) {
    int n = atoi(name + 2);
    if (n > 15)
      return false;
    op->kind = OP_CR;
    op->reg = n;
    op->size = 8;
    return true;
  }
  if (!strcmp(name, "rip")) {
    op->kind = OP_REG;
    op->reg = -2;
    op->size = 8;
    return true;
  }
  if (!strcmp(name, "ah") || !strcmp(name, "ch") || !strcmp(name, "dh") || !strcmp(name, "bh")) {
    op->kind = OP_REG;
    op->size = 1;
    op->high8 = true;
    op->reg = name[0] == 'a' ? 4 : name[0] == 'c' ? 5 : name[0] == 'd' ? 6 : 7;
    return true;
  }
  static char *segs[] = {"es", "cs", "ss", "ds", "fs", "gs"};
  static int segpfx[] = {0x26, 0x2e, 0x36, 0x3e, 0x64, 0x65};
  for (int i = 0; i < 6; i++)
    if (!strcmp(name, segs[i])) {
      op->kind = OP_REG;
      op->reg = -3;
      op->seg = segpfx[i];
      return true;
    }
  for (size_t i = 0; i < sizeof(gpregs) / sizeof(*gpregs); i++)
    if (!strcmp(name, gpregs[i].name)) {
      op->kind = OP_REG;
      op->reg = gpregs[i].num;
      op->size = gpregs[i].size;
      op->rex8 = op->size == 1 && op->reg >= 4 && op->reg <= 7;
      return true;
    }
  return false;
}

/* Memory operand after the displacement: "(base,index,scale)" */
static bool parse_mem_tail(Operand *op) {
  op->kind = OP_MEM;
  op->base = op->index = -1;
  op->scale = 1;
  if (!consume_ch('('))
    return true;
  skip_ws();
  if (*p == '%') {
    p++;
    Operand r = {0};
    if (!parse_register(&r) || r.kind != OP_REG) {
      asm_error("bad base register");
      return false;
    }
    if (r.reg == -2)
      op->rip = true;
    else if (r.size != (code32 ? 4 : 8)) {
      asm_error(code32 ? "base register must be 32-bit" : "base register must be 64-bit");
      return false;
    } else
      op->base = r.reg;
  }
  if (consume_ch(',')) {
    skip_ws();
    if (*p == '%') {
      p++;
      Operand r = {0};
      if (!parse_register(&r) || r.kind != OP_REG || r.size != (code32 ? 4 : 8) || r.reg == 4) {
        asm_error("bad index register");
        return false;
      }
      op->index = r.reg;
    }
    if (consume_ch(',')) {
      Expr s;
      if (!parse_expr(&s) || !absolute(&s) ||
          (s.val != 1 && s.val != 2 && s.val != 4 && s.val != 8)) {
        asm_error("scale factor must be 1, 2, 4 or 8");
        return false;
      }
      op->scale = s.val;
    }
  }
  if (!consume_ch(')')) {
    asm_error("expected ')' in memory operand");
    return false;
  }
  return true;
}

static bool parse_operand(Operand *op) {
  memset(op, 0, sizeof(*op));
  op->base = op->index = -1;
  op->scale = 1;
  skip_ws();
  if (*p == '*') {
    op->indirect = true;
    p++;
    skip_ws();
  }
  if (*p == '$') {
    p++;
    op->kind = OP_IMM;
    return parse_expr(&op->e);
  }
  if (*p == '%') {
    p++;
    if (!parse_register(op)) {
      asm_error("unknown register '%%%.10s'", p);
      return false;
    }
    if (op->reg == -3) {  /* segment override: %fs:disp(...) */
      int seg = op->seg;
      if (!consume_ch(':')) {  /* the segment register itself */
        static int segpfx[] = {0x26, 0x2e, 0x36, 0x3e, 0x64, 0x65};
        op->kind = OP_SEG;
        for (int i = 0; i < 6; i++)
          if (segpfx[i] == seg)
            op->reg = i;
        op->seg = 0;
        op->size = 2;
        return true;
      }
      memset(op, 0, sizeof(*op));
      op->seg = seg;
      skip_ws();
      if (*p != '(' && !parse_expr(&op->e))
        return false;
      int s2 = op->seg;
      if (!parse_mem_tail(op))
        return false;
      op->seg = s2;
      return true;
    }
    return true;
  }
  /* memory: disp(base,index,scale), or a bare expression; "(expr)" whose
     parenthesis holds no register is an expression (as in gas) */
  bool paren_expr = false;
  if (*p == '(') {
    char *q = p + 1;
    while (*q == ' ' || *q == '\t')
      q++;
    paren_expr = *q != '%' && *q != ',';
  }
  if (*p != '(' || paren_expr) {
    if (!parse_expr(&op->e))
      return false;
  }
  return parse_mem_tail(op);
}

/* ---------------------------------------------------------- encoding */

static void add_fixup(int size, bool pcrel, int64_t adjust, Expr *e) {
  if (nfixups == capfixups) {
    capfixups = capfixups ? capfixups * 2 : 1024;
    fixups = xrealloc(fixups, sizeof(Fixup) * capfixups);
  }
  Fixup *f = &fixups[nfixups++];
  memset(f, 0, sizeof(*f));
  f->sec = cur;
  f->offset = cur_offset();
  f->size = size;
  f->pcrel = pcrel;
  f->adjust = adjust;
  f->e = *e;
  f->line = lineno;
}

/* Emits a field holding an expression (absolute) */
static void emit_expr(Expr *e, int size, bool sign32) {
  if (absolute(e)) {
    emitn((uint64_t)e->val, size);
    return;
  }
  int64_t v;
  if (const_value(e, &v)) {
    emitn((uint64_t)v, size);
    return;
  }
  add_fixup(size, false, 0, e);
  fixups[nfixups - 1].sign32 = sign32;
  emitn(0, size);
}

typedef struct {
  int prefix;      /* 0x66, 0xf2, 0xf3 or 0 (mandatory/size prefix) */
  bool opsize16;   /* 0x66 operand-size prefix */
  bool rexw;
  bool force_rex;
  const unsigned char *op;
  int oplen;
  int reg;         /* ModRM.reg (register number or extension) */
  Operand *rm;     /* NULL: no ModRM */
  int imm_size;    /* bytes of immediate following */
} Insn;

static int seg_prefix;

/* Encodes ModRM, SIB and displacement for rm with the given reg field. */
static void emit_modrm(int reg, Operand *rm, int imm_size) {
  if (rm->kind == OP_REG || rm->kind == OP_XMM || rm->kind == OP_ST) {
    emit8(0xc0 | (reg & 7) << 3 | (rm->reg & 7));
    return;
  }
  /* memory */
  Expr *e = &rm->e;
  if (rm->rip) {
    emit8(0x00 | (reg & 7) << 3 | 5);
    /* rip-relative: target - end of instruction */
    if (absolute(e)) {
      asm_error("rip-relative operand needs a symbol");
      emit32(0);
      return;
    }
    add_fixup(4, true, -(4 + imm_size), e);
    emit32(0);
    return;
  }
  bool has_disp_sym = !absolute(e);
  int64_t disp = has_disp_sym ? 0 : e->val;
  if (rm->base < 0 && rm->index < 0 && code32) {
    /* 32-bit mode: mod 00, r/m 101 is a plain disp32 */
    emit8(0x05 | (reg & 7) << 3);
    if (has_disp_sym)
      emit_expr(e, 4, false);
    else
      emit32((uint32_t)disp);
    return;
  }
  if (rm->base < 0 && rm->index < 0) {
    /* absolute address: SIB with no base and no index */
    emit8(0x04 | (reg & 7) << 3);
    emit8(0x25);
    if (has_disp_sym)
      emit_expr(e, 4, true);
    else
      emit32((uint32_t)disp);
    return;
  }
  int mod;
  if (has_disp_sym)
    mod = 2;
  else if (disp == 0 && (rm->base < 0 || (rm->base & 7) != 5))
    mod = 0;
  else if (disp >= -128 && disp <= 127)
    mod = 1;
  else
    mod = 2;
  if (rm->base < 0)
    mod = 0;  /* [index*scale + disp32] */

  bool need_sib = rm->index >= 0 || rm->base < 0 || (rm->base & 7) == 4;
  if (!need_sib) {
    emit8(mod << 6 | (reg & 7) << 3 | (rm->base & 7));
  } else {
    emit8(mod << 6 | (reg & 7) << 3 | 4);
    int ss = rm->scale == 1 ? 0 : rm->scale == 2 ? 1 : rm->scale == 4 ? 2 : 3;
    int idx = rm->index >= 0 ? (rm->index & 7) : 4;
    int base = rm->base >= 0 ? (rm->base & 7) : 5;
    emit8(ss << 6 | idx << 3 | base);
  }
  if (rm->base < 0) {
    if (has_disp_sym)
      emit_expr(e, 4, true);
    else
      emit32((uint32_t)disp);
  } else if (mod == 1) {
    emit8((uint8_t)disp);
  } else if (mod == 2) {
    if (has_disp_sym)
      emit_expr(e, 4, true);
    else
      emit32((uint32_t)disp);
  }
}

static void emit_insn(Insn *in) {
  if (seg_prefix)
    emit8(seg_prefix);
  if (in->opsize16)
    emit8(0x66);
  if (in->prefix)
    emit8(in->prefix);
  int rex = 0;
  if (in->rexw)
    rex |= 8;
  if (in->reg >= 8)
    rex |= 4;
  if (in->rm) {
    Operand *rm = in->rm;
    if (rm->kind == OP_MEM) {
      if (rm->index >= 8)
        rex |= 2;
      if (rm->base >= 8)
        rex |= 1;
    } else if (rm->reg >= 8) {
      rex |= 1;
    }
  }
  if ((rex || in->force_rex) && code32)
    asm_error("instruction not encodable in 32-bit mode");
  else if (rex || in->force_rex)
    emit8(0x40 | rex);
  for (int i = 0; i < in->oplen; i++)
    emit8(in->op[i]);
  if (in->rm)
    emit_modrm(in->reg, in->rm, in->imm_size);
}

/* Register encoded in the low bits of the opcode (push, bswap, mov $imm, %r) */
static void emit_opreg(bool opsize16, bool rexw, int opbase, int reg, bool force_rex) {
  if (seg_prefix)
    emit8(seg_prefix);
  if (opsize16)
    emit8(0x66);
  int rex = (rexw ? 8 : 0) | (reg >= 8 ? 1 : 0);
  if ((rex || force_rex) && code32)
    asm_error("instruction not encodable in 32-bit mode");
  else if (rex || force_rex)
    emit8(0x40 | rex);
  emit8(opbase + (reg & 7));
}

static bool fits8(int64_t v) { return v >= -128 && v <= 127; }
static bool fits32(int64_t v) { return v >= INT32_MIN && v <= INT32_MAX; }

static void emit_imm(Operand *imm, int size) {
  emit_expr(&imm->e, size, size == 4 && !code32);
}

/* ----------------------------------------------------- instructions */

static int suffix_size(char c) {
  switch (c) {
  case 'b': return 1;
  case 'w': return 2;
  case 'l': return 4;
  case 'q': return 8;
  default: return 0;
  }
}

/* Operand size from register operands */
static int reg_size(Operand *ops, int n) {
  for (int i = n - 1; i >= 0; i--)
    if (ops[i].kind == OP_REG)
      return ops[i].size;
  return 0;
}

static bool check_size(int size, char *mn) {
  if (!size) {
    asm_error("operand size not specified for '%s'", mn);
    return false;
  }
  return true;
}

static int cond_code(char *s) {
  static struct { char *n; int c; } cc[] = {
      {"o", 0}, {"no", 1}, {"b", 2}, {"c", 2}, {"nae", 2}, {"ae", 3}, {"nb", 3}, {"nc", 3},
      {"e", 4}, {"z", 4}, {"ne", 5}, {"nz", 5}, {"be", 6}, {"na", 6}, {"a", 7}, {"nbe", 7},
      {"s", 8}, {"ns", 9}, {"p", 10}, {"pe", 10}, {"np", 11}, {"po", 11}, {"l", 12},
      {"nge", 12}, {"ge", 13}, {"nl", 13}, {"le", 14}, {"ng", 14}, {"g", 15}, {"nle", 15},
  };
  for (size_t i = 0; i < sizeof(cc) / sizeof(*cc); i++)
    if (!strcmp(s, cc[i].n))
      return cc[i].c;
  return -1;
}

static bool is_reg(Operand *o) { return o->kind == OP_REG; }
static bool is_mem(Operand *o) { return o->kind == OP_MEM; }
static bool is_rm(Operand *o) { return o->kind == OP_REG || o->kind == OP_MEM; }

static void need_rex8(Insn *in, Operand *a, Operand *b) {
  if ((a && a->kind == OP_REG && a->rex8) || (b && b->kind == OP_REG && b->rex8))
    in->force_rex = true;
}

/* ALU: add or adc sbb and sub xor cmp */
static bool enc_alu(int n, int size, Operand *src, Operand *dst) {
  unsigned char op[1];
  Insn in = {0};
  in.opsize16 = size == 2;
  in.rexw = size == 8;
  if (src->kind == OP_IMM) {
    if (!is_rm(dst))
      return false;
    int64_t v = src->e.val;
    bool absimm = absolute(&src->e);
    if (size == 1) {
      if (is_reg(dst) && dst->reg == 0) {
        emit8(0x04 + 8 * n);
        emit_imm(src, 1);
        return true;
      }
      op[0] = 0x80;
      in.op = op; in.oplen = 1; in.reg = n; in.rm = dst; in.imm_size = 1;
      need_rex8(&in, dst, NULL);
      emit_insn(&in);
      emit_imm(src, 1);
      return true;
    }
    if (absimm && fits8(v)) {
      op[0] = 0x83;
      in.op = op; in.oplen = 1; in.reg = n; in.rm = dst; in.imm_size = 1;
      emit_insn(&in);
      emit8((uint8_t)v);
      return true;
    }
    int isz = size == 2 ? 2 : 4;
    if (is_reg(dst) && dst->reg == 0) {
      if (in.opsize16) emit8(0x66);
      if (in.rexw) emit8(0x48);
      emit8(0x05 + 8 * n);
      emit_imm(src, isz);
      return true;
    }
    op[0] = 0x81;
    in.op = op; in.oplen = 1; in.reg = n; in.rm = dst; in.imm_size = isz;
    emit_insn(&in);
    emit_imm(src, isz);
    return true;
  }
  if (is_reg(src) && is_rm(dst)) {
    op[0] = (size == 1 ? 0x00 : 0x01) + 8 * n;
    in.op = op; in.oplen = 1; in.reg = src->reg; in.rm = dst;
    need_rex8(&in, src, dst);
    emit_insn(&in);
    return true;
  }
  if (is_mem(src) && is_reg(dst)) {
    op[0] = (size == 1 ? 0x02 : 0x03) + 8 * n;
    in.op = op; in.oplen = 1; in.reg = dst->reg; in.rm = src;
    need_rex8(&in, dst, NULL);
    emit_insn(&in);
    return true;
  }
  return false;
}

static bool enc_mov(int size, Operand *src, Operand *dst, bool movabs) {
  Insn in = {0};
  unsigned char op[1];
  in.opsize16 = size == 2;
  in.rexw = size == 8;
  if (src->kind == OP_IMM) {
    if (is_reg(dst)) {
      if (size == 8) {
        bool absimm = absolute(&src->e);
        int64_t v = src->e.val;
        if (movabs || (absimm && !fits32(v))) {
          emit_opreg(false, true, 0xb8, dst->reg, false);
          emit_expr(&src->e, 8, false);
          return true;
        }
        op[0] = 0xc7;
        in.op = op; in.oplen = 1; in.reg = 0; in.rm = dst; in.imm_size = 4;
        emit_insn(&in);
        emit_expr(&src->e, 4, true);
        return true;
      }
      emit_opreg(size == 2, false, size == 1 ? 0xb0 : 0xb8, dst->reg, dst->rex8);
      emit_imm(src, size == 1 ? 1 : size == 2 ? 2 : 4);
      return true;
    }
    if (is_mem(dst)) {
      op[0] = size == 1 ? 0xc6 : 0xc7;
      int isz = size == 1 ? 1 : size == 2 ? 2 : 4;
      in.op = op; in.oplen = 1; in.reg = 0; in.rm = dst; in.imm_size = isz;
      emit_insn(&in);
      emit_expr(&src->e, isz, size == 8);
      return true;
    }
    return false;
  }
  if (is_reg(src) && is_rm(dst)) {
    op[0] = size == 1 ? 0x88 : 0x89;
    in.op = op; in.oplen = 1; in.reg = src->reg; in.rm = dst;
    need_rex8(&in, src, dst);
    emit_insn(&in);
    return true;
  }
  if (is_mem(src) && is_reg(dst)) {
    op[0] = size == 1 ? 0x8a : 0x8b;
    in.op = op; in.oplen = 1; in.reg = dst->reg; in.rm = src;
    need_rex8(&in, dst, NULL);
    emit_insn(&in);
    return true;
  }
  return false;
}

/* SSE: [prefix] [REX] 0F op /r with reg = xmm destination (or source) */
static void enc_sse(int prefix, bool rexw, int opc, int reg, Operand *rm) {
  unsigned char op[2] = {0x0f, (unsigned char)opc};
  Insn in = {0};
  in.prefix = prefix;
  in.rexw = rexw;
  in.op = op; in.oplen = 2; in.reg = reg; in.rm = rm;
  emit_insn(&in);
}

static void enc_x87_mem(int opc, int ext, Operand *m) {
  unsigned char op[1] = {(unsigned char)opc};
  Insn in = {0};
  in.op = op; in.oplen = 1; in.reg = ext; in.rm = m;
  emit_insn(&in);
}

static void branch(int opc1, int opc2, Operand *target, bool is_call) {
  /* rel32 */
  if (opc2 >= 0) {
    emit8(opc1);
    emit8(opc2);
  } else {
    emit8(opc1);
  }
  add_fixup(4, true, -4, &target->e);
  fixups[nfixups - 1].is_call = is_call;
  fixups[nfixups - 1].is_branch = true;
  emit32(0);
}

/* Instructions for a kernel: port I/O, descriptor tables, control
   registers, far jumps, FPU/SSE state. */
static bool assemble_system(char *m, Operand *ops, int nops) {
  Operand *a = &ops[0], *b = &ops[1];
  unsigned char op[3];
  Insn in = {0};

  /* in/out: "inb %dx, %al", "inl $port, %eax", "outw %ax, %dx", ... */
  bool isin = !strcmp(m, "in") || !strcmp(m, "inb") || !strcmp(m, "inw") || !strcmp(m, "inl");
  bool isout = !strcmp(m, "out") || !strcmp(m, "outb") || !strcmp(m, "outw") ||
               !strcmp(m, "outl");
  if ((isin || isout) && nops == 2) {
    Operand *port = isin ? a : b, *acc = isin ? b : a;
    int size = suffix_size(m[isin ? 2 : 3]);
    if (!is_reg(acc) || acc->reg != 0 || acc->high8 || (size && acc->size != size) ||
        acc->size == 8)
      return false;
    size = acc->size;
    int base = isin ? 0xe4 : 0xe6;
    if (size != 1)
      base++;
    if (size == 2)
      emit8(0x66);
    if (is_reg(port) && port->reg == 2 && port->size == 2) {
      emit8(base + 8);  /* EC/ED/EE/EF */
      return true;
    }
    if (port->kind == OP_IMM) {
      emit8(base);
      emit_imm(port, 1);
      return true;
    }
    return false;
  }

  /* descriptor tables, TLB, FPU/SSE state: one memory (or register) operand */
  static struct { char *n; int op2; int ext; bool reg_ok; bool rexw; } grp[] = {
      {"sgdt", 0x01, 0, false, false}, {"sidt", 0x01, 1, false, false},
      {"lgdt", 0x01, 2, false, false}, {"lidt", 0x01, 3, false, false},
      {"sgdtq", 0x01, 0, false, false}, {"sidtq", 0x01, 1, false, false},
      {"lgdtq", 0x01, 2, false, false}, {"lidtq", 0x01, 3, false, false},
      {"sgdtl", 0x01, 0, false, false}, {"sidtl", 0x01, 1, false, false},
      {"lgdtl", 0x01, 2, false, false}, {"lidtl", 0x01, 3, false, false},
      {"invlpg", 0x01, 7, false, false}, {"smsw", 0x01, 4, true, false},
      {"lmsw", 0x01, 6, true, false}, {"sldt", 0x00, 0, true, false},
      {"str", 0x00, 1, true, false}, {"lldt", 0x00, 2, true, false},
      {"ltr", 0x00, 3, true, false}, {"verr", 0x00, 4, true, false},
      {"verw", 0x00, 5, true, false},
      {"fxsave", 0xae, 0, false, false}, {"fxrstor", 0xae, 1, false, false},
      {"fxsave64", 0xae, 0, false, true}, {"fxrstor64", 0xae, 1, false, true},
      {"fxsaveq", 0xae, 0, false, true}, {"fxrstorq", 0xae, 1, false, true},
      {"ldmxcsr", 0xae, 2, false, false}, {"stmxcsr", 0xae, 3, false, false},
      {"clflush", 0xae, 7, false, false},
  };
  for (size_t i = 0; i < sizeof(grp) / sizeof(*grp); i++) {
    if (strcmp(m, grp[i].n))
      continue;
    if (nops != 1 || !(is_mem(a) || (grp[i].reg_ok && is_reg(a))))
      return false;
    op[0] = 0x0f; op[1] = grp[i].op2;
    in.op = op; in.oplen = 2; in.reg = grp[i].ext; in.rm = a; in.rexw = grp[i].rexw;
    emit_insn(&in);
    return true;
  }

  /* mov to and from a control register: 0F 20 /r, 0F 22 /r; the
     register operand is the mode's full width */
  if ((!strcmp(m, "mov") || !strcmp(m, "movq") || !strcmp(m, "movl")) && nops == 2 &&
      (a->kind == OP_CR || b->kind == OP_CR)) {
    Operand *cr = a->kind == OP_CR ? a : b, *gp = a->kind == OP_CR ? b : a;
    if (!is_reg(gp) || gp->size != (code32 ? 4 : 8) || cr == gp)
      return false;
    op[0] = 0x0f; op[1] = cr == a ? 0x20 : 0x22;
    in.op = op; in.oplen = 2; in.reg = cr->reg; in.rm = gp;
    emit_insn(&in);
    return true;
  }

  /* mov to and from a segment register: 8E /r, 8C /r */
  if ((!strcmp(m, "mov") || !strcmp(m, "movw") || !strcmp(m, "movl") || !strcmp(m, "movq")) &&
      nops == 2 && (a->kind == OP_SEG || b->kind == OP_SEG)) {
    Operand *sr = a->kind == OP_SEG ? a : b, *o = sr == a ? b : a;
    if (o->kind == OP_SEG || !is_rm(o))
      return false;
    op[0] = sr == b ? 0x8e : 0x8c;
    in.op = op; in.oplen = 1; in.reg = sr->reg; in.rm = o;
    in.rexw = is_reg(o) && o->size == 8 && sr == a;
    in.opsize16 = (is_reg(o) && o->size == 2 && sr == a) || (is_mem(o) && m[3] == 'w' && sr == a);
    emit_insn(&in);
    return true;
  }
  /* push and pop of %fs and %gs */
  if ((!strcmp(m, "push") || !strcmp(m, "pushq") || !strcmp(m, "pop") || !strcmp(m, "popq")) &&
      nops == 1 && a->kind == OP_SEG && a->reg >= 4) {
    emit8(0x0f);
    emit8((m[1] == 'u' ? 0xa0 : 0xa1) + (a->reg == 5 ? 8 : 0));
    return true;
  }

  /* far jump and call: "ljmp $sel, $offset" (32-bit mode), "ljmp *mem" */
  if (!strcmp(m, "ljmp") || !strcmp(m, "lcall") || !strcmp(m, "ljmpl") ||
      !strcmp(m, "lcalll") || !strcmp(m, "ljmpq") || !strcmp(m, "lcallq")) {
    bool call = m[1] == 'c';
    if (nops == 2 && a->kind == OP_IMM && b->kind == OP_IMM) {
      if (!code32) {
        asm_error("a direct far %s needs 32-bit mode", call ? "call" : "jump");
        return true;
      }
      emit8(call ? 0x9a : 0xea);
      emit_imm(b, 4);
      emit_imm(a, 2);
      return true;
    }
    if (nops == 1 && a->indirect && is_mem(a)) {
      op[0] = 0xff;
      in.op = op; in.oplen = 1; in.reg = call ? 3 : 5; in.rm = a;
      in.rexw = m[strlen(m) - 1] == 'q';
      emit_insn(&in);
      return true;
    }
    return false;
  }
  return false;
}

static bool assemble_insn(char *mn, Operand *ops, int nops) {
  char m[32];
  snprintf(m, sizeof(m), "%s", mn);
  for (char *q = m; *q; q++)
    *q = tolower((unsigned char)*q);
  int len = strlen(m);
  Operand *a = &ops[0], *b = &ops[1], *c = &ops[2];
  unsigned char op[3];
  Insn in = {0};

  /* --- no-operand instructions */
  static struct { char *n; unsigned char bytes[4]; int len; } simple[] = {
      {"ret", {0xc3}, 1}, {"retq", {0xc3}, 1}, {"leave", {0xc9}, 1}, {"leaveq", {0xc9}, 1},
      {"nop", {0x90}, 1}, {"hlt", {0xf4}, 1}, {"syscall", {0x0f, 0x05}, 2},
      {"cqo", {0x48, 0x99}, 2}, {"cqto", {0x48, 0x99}, 2}, {"cltd", {0x99}, 1},
      {"cdq", {0x99}, 1}, {"cltq", {0x48, 0x98}, 2}, {"cdqe", {0x48, 0x98}, 2},
      {"cwtl", {0x98}, 1}, {"cwtd", {0x66, 0x99}, 2}, {"cbtw", {0x66, 0x98}, 2},
      {"ud2", {0x0f, 0x0b}, 2}, {"mfence", {0x0f, 0xae, 0xf0}, 3},
      {"lfence", {0x0f, 0xae, 0xe8}, 3}, {"sfence", {0x0f, 0xae, 0xf8}, 3},
      {"pause", {0xf3, 0x90}, 2}, {"int3", {0xcc}, 1}, {"cpuid", {0x0f, 0xa2}, 2},
      {"rdtsc", {0x0f, 0x31}, 2}, {"movsb", {0xa4}, 1}, {"movsw", {0x66, 0xa5}, 2},
      {"movsl", {0xa5}, 1}, {"movsq", {0x48, 0xa5}, 2}, {"stosb", {0xaa}, 1},
      {"stosw", {0x66, 0xab}, 2}, {"stosl", {0xab}, 1}, {"stosq", {0x48, 0xab}, 2},
      {"cmpsb", {0xa6}, 1}, {"scasb", {0xae}, 1}, {"lodsb", {0xac}, 1},
      {"fchs", {0xd9, 0xe0}, 2}, {"fabs", {0xd9, 0xe1}, 2}, {"fldz", {0xd9, 0xee}, 2},
      {"fld1", {0xd9, 0xe8}, 2}, {"fwait", {0x9b}, 1}, {"wait", {0x9b}, 1},
      {"fninit", {0xdb, 0xe3}, 2}, {"fucompp", {0xda, 0xe9}, 2}, {"fsqrt", {0xd9, 0xfa}, 2},
      {"frndint", {0xd9, 0xfc}, 2}, {"fscale", {0xd9, 0xfd}, 2}, {"fprem", {0xd9, 0xf8}, 2},
      {"fprem1", {0xd9, 0xf5}, 2}, {"fxtract", {0xd9, 0xf4}, 2}, {"f2xm1", {0xd9, 0xf0}, 2},
      {"fyl2x", {0xd9, 0xf1}, 2}, {"fldl2e", {0xd9, 0xea}, 2}, {"fldln2", {0xd9, 0xed}, 2},
      {"fldlg2", {0xd9, 0xec}, 2}, {"fldpi", {0xd9, 0xeb}, 2}, {"fsin", {0xd9, 0xfe}, 2},
      {"fcos", {0xd9, 0xff}, 2}, {"fpatan", {0xd9, 0xf3}, 2}, {"fptan", {0xd9, 0xf2}, 2},
      {"ftst", {0xd9, 0xe4}, 2}, {"fxam", {0xd9, 0xe5}, 2}, {"clc", {0xf8}, 1},
      {"stc", {0xf9}, 1}, {"cld", {0xfc}, 1}, {"std", {0xfd}, 1},
      /* system instructions (a kernel) */
      {"cli", {0xfa}, 1}, {"sti", {0xfb}, 1}, {"pushfq", {0x9c}, 1}, {"popfq", {0x9d}, 1},
      {"pushfl", {0x9c}, 1}, {"popfl", {0x9d}, 1}, {"pushf", {0x9c}, 1}, {"popf", {0x9d}, 1},
      {"iretq", {0x48, 0xcf}, 2}, {"iretl", {0xcf}, 1}, {"iret", {0xcf}, 1},
      {"lretq", {0x48, 0xcb}, 2}, {"lretl", {0xcb}, 1}, {"lret", {0xcb}, 1},
      {"rdmsr", {0x0f, 0x32}, 2}, {"wrmsr", {0x0f, 0x30}, 2}, {"rdpmc", {0x0f, 0x33}, 2},
      {"swapgs", {0x0f, 0x01, 0xf8}, 3}, {"rdtscp", {0x0f, 0x01, 0xf9}, 3},
      {"clts", {0x0f, 0x06}, 2}, {"invd", {0x0f, 0x08}, 2}, {"wbinvd", {0x0f, 0x09}, 2},
      {"sysretq", {0x48, 0x0f, 0x07}, 3}, {"sysretl", {0x0f, 0x07}, 2},
      {"insb", {0x6c}, 1}, {"insw", {0x66, 0x6d}, 2}, {"insl", {0x6d}, 1},
      {"outsb", {0x6e}, 1}, {"outsw", {0x66, 0x6f}, 2}, {"outsl", {0x6f}, 1},
      {"lodsw", {0x66, 0xad}, 2}, {"lodsl", {0xad}, 1}, {"lodsq", {0x48, 0xad}, 2},
      {"fnclex", {0xdb, 0xe2}, 2}, {"fclex", {0x9b, 0xdb, 0xe2}, 3},
      {"fyl2xp1", {0xd9, 0xf9}, 2},
  };
  if (nops == 0)
    for (size_t i = 0; i < sizeof(simple) / sizeof(*simple); i++)
      if (!strcmp(m, simple[i].n)) {
        for (int k = 0; k < simple[i].len; k++)
          emit8(simple[i].bytes[k]);
        return true;
      }

  if (assemble_system(m, ops, nops))
    return true;

  /* --- x87 */
  if (m[0] == 'f') {
    struct { char *n; int opc; int ext; } fmem[] = {
        {"flds", 0xd9, 0}, {"fldl", 0xdd, 0}, {"fldt", 0xdb, 5},
        {"filds", 0xdf, 0}, {"fild", 0xdf, 0}, {"fildl", 0xdb, 0}, {"fildll", 0xdf, 5},
        {"fildq", 0xdf, 5}, {"fsts", 0xd9, 2}, {"fstl", 0xdd, 2}, {"fstps", 0xd9, 3},
        {"fstpl", 0xdd, 3}, {"fstpt", 0xdb, 7}, {"fists", 0xdf, 2}, {"fistl", 0xdb, 2},
        {"fistps", 0xdf, 3}, {"fistpl", 0xdb, 3}, {"fistpll", 0xdf, 7}, {"fistpq", 0xdf, 7},
        {"fisttps", 0xdf, 1}, {"fisttpl", 0xdb, 1}, {"fisttpll", 0xdd, 1}, {"fisttpq", 0xdd, 1},
        {"fnstcw", 0xd9, 7}, {"fldcw", 0xd9, 5}, {"fnstenv", 0xd9, 6}, {"fldenv", 0xd9, 4},
        {"fadds", 0xd8, 0}, {"fmuls", 0xd8, 1}, {"fsubs", 0xd8, 4}, {"fsubrs", 0xd8, 5},
        {"fdivs", 0xd8, 6}, {"fdivrs", 0xd8, 7}, {"faddl", 0xdc, 0}, {"fmull", 0xdc, 1},
        {"fsubl", 0xdc, 4}, {"fsubrl", 0xdc, 5}, {"fdivl", 0xdc, 6}, {"fdivrl", 0xdc, 7},
        {"fnstsw", 0xdd, 7},
    };
    if (nops == 1 && is_mem(a))
      for (size_t i = 0; i < sizeof(fmem) / sizeof(*fmem); i++)
        if (!strcmp(m, fmem[i].n)) {
          enc_x87_mem(fmem[i].opc, fmem[i].ext, a);
          return true;
        }
    if (!strcmp(m, "fnstsw") && nops == 1 && is_reg(a) && a->reg == 0 && a->size == 2) {
      emit8(0xdf);
      emit8(0xe0);
      return true;
    }
    int sti = (nops >= 1 && a->kind == OP_ST) ? a->reg : -1;
    struct { char *n; int b1; int b2; int defreg; } fst[] = {
        {"fld", 0xd9, 0xc0, -1}, {"fst", 0xdd, 0xd0, -1}, {"fstp", 0xdd, 0xd8, -1},
        {"fxch", 0xd9, 0xc8, 1}, {"fucomi", 0xdb, 0xe8, 1}, {"fucomip", 0xdf, 0xe8, 1},
        {"fcomi", 0xdb, 0xf0, 1}, {"fcomip", 0xdf, 0xf0, 1}, {"fucom", 0xdd, 0xe0, 1},
        {"fucomp", 0xdd, 0xe8, 1}, {"ffree", 0xdd, 0xc0, -1},
        {"faddp", 0xde, 0xc0, 1}, {"fmulp", 0xde, 0xc8, 1}, {"fsubp", 0xde, 0xe0, 1},
        {"fsubrp", 0xde, 0xe8, 1}, {"fdivp", 0xde, 0xf0, 1}, {"fdivrp", 0xde, 0xf8, 1},
    };
    for (size_t i = 0; i < sizeof(fst) / sizeof(*fst); i++) {
      if (strcmp(m, fst[i].n))
        continue;
      int r;
      bool popping = m[strlen(m) - 1] == 'p' && strcmp(m, "fstp") && strcmp(m, "fucomp") &&
                     strncmp(m, "fucomi", 6) && strncmp(m, "fcomi", 5);
      if (nops == 0) {
        if (fst[i].defreg < 0)
          break;
        r = fst[i].defreg;
      } else if (nops == 1 && sti >= 0) {
        r = sti;
      } else if (nops == 2 && a->kind == OP_ST && b->kind == OP_ST) {
        /* "fucomip %st(i), %st" or "faddp %st, %st(i)" */
        r = popping ? b->reg : a->reg;
      } else {
        break;
      }
      emit8(fst[i].b1);
      emit8(fst[i].b2 + r);
      return true;
    }
    /* fadd/fsub/... with registers: "%st(i), %st" (D8) or "%st, %st(i)" (DC) */
    struct { char *n; int d8; int dc; } far[] = {
        /* with %st(i) as destination the GNU assembler's (System V) mnemonics
           name the reverse operation of Intel's; the encodings are its */
        {"fadd", 0xc0, 0xc0}, {"fmul", 0xc8, 0xc8}, {"fsub", 0xe0, 0xe0}, {"fsubr", 0xe8, 0xe8},
        {"fdiv", 0xf0, 0xf0}, {"fdivr", 0xf8, 0xf8},
    };
    for (size_t i = 0; i < sizeof(far) / sizeof(*far); i++) {
      if (strcmp(m, far[i].n) || nops != 2 || a->kind != OP_ST || b->kind != OP_ST)
        continue;
      if (b->reg == 0) {
        emit8(0xd8);
        emit8(far[i].d8 + a->reg);
      } else {
        emit8(0xdc);
        emit8(far[i].dc + b->reg);
      }
      return true;
    }
  }

  /* --- branches */
  if (!strcmp(m, "jmp") || !strcmp(m, "jmpq") || !strcmp(m, "call") || !strcmp(m, "callq")) {
    bool is_call = m[0] == 'c';
    if (nops != 1)
      return false;
    if (a->indirect || a->kind == OP_REG) {
      op[0] = 0xff;
      in.op = op; in.oplen = 1; in.reg = is_call ? 2 : 4; in.rm = a;
      if (a->kind == OP_MEM && !a->rip && a->base < 0 && a->index < 0 && !a->indirect)
        return false;
      emit_insn(&in);
      return true;
    }
    branch(is_call ? 0xe8 : 0xe9, -1, a, is_call);
    return true;
  }
  if (m[0] == 'j' && nops == 1) {
    int cc = cond_code(m + 1);
    if (cc >= 0) {
      branch(0x0f, 0x80 + cc, a, false);
      return true;
    }
  }
  if (!strncmp(m, "set", 3) && nops == 1) {
    int cc = cond_code(m + 3);
    if (cc >= 0 && is_rm(a)) {
      op[0] = 0x0f; op[1] = 0x90 + cc;
      in.op = op; in.oplen = 2; in.reg = 0; in.rm = a;
      need_rex8(&in, a, NULL);
      emit_insn(&in);
      return true;
    }
  }
  if (!strncmp(m, "cmov", 4) && nops == 2 && is_reg(b)) {
    char base[32];
    snprintf(base, sizeof(base), "%s", m + 4);
    int cc = cond_code(base);
    int size = b->size;
    if (cc < 0 && strlen(base) > 1 && suffix_size(base[strlen(base) - 1])) {
      base[strlen(base) - 1] = '\0';
      cc = cond_code(base);
    }
    if (cc >= 0 && is_rm(a)) {
      op[0] = 0x0f; op[1] = 0x40 + cc;
      in.op = op; in.oplen = 2; in.reg = b->reg; in.rm = a;
      in.rexw = size == 8; in.opsize16 = size == 2;
      emit_insn(&in);
      return true;
    }
  }

  /* --- SSE */
  struct { char *n; int pfx; int opc; } sse[] = {
      {"addss", 0xf3, 0x58}, {"addsd", 0xf2, 0x58}, {"subss", 0xf3, 0x5c}, {"subsd", 0xf2, 0x5c},
      {"mulss", 0xf3, 0x59}, {"mulsd", 0xf2, 0x59}, {"divss", 0xf3, 0x5e}, {"divsd", 0xf2, 0x5e},
      {"sqrtss", 0xf3, 0x51}, {"sqrtsd", 0xf2, 0x51}, {"minss", 0xf3, 0x5d}, {"minsd", 0xf2, 0x5d},
      {"maxss", 0xf3, 0x5f}, {"maxsd", 0xf2, 0x5f}, {"ucomiss", 0, 0x2e}, {"ucomisd", 0x66, 0x2e},
      {"comiss", 0, 0x2f}, {"comisd", 0x66, 0x2f}, {"xorps", 0, 0x57}, {"xorpd", 0x66, 0x57},
      {"andps", 0, 0x54}, {"andpd", 0x66, 0x54}, {"andnps", 0, 0x55}, {"andnpd", 0x66, 0x55},
      {"orps", 0, 0x56}, {"orpd", 0x66, 0x56}, {"pxor", 0x66, 0xef}, {"pand", 0x66, 0xdb},
      {"por", 0x66, 0xeb}, {"cvtss2sd", 0xf3, 0x5a}, {"cvtsd2ss", 0xf2, 0x5a},
      {"unpcklps", 0, 0x14}, {"unpcklpd", 0x66, 0x14}, {"addps", 0, 0x58}, {"addpd", 0x66, 0x58},
      {"mulps", 0, 0x59}, {"subps", 0, 0x5c}, {"divps", 0, 0x5e},
  };
  for (size_t i = 0; i < sizeof(sse) / sizeof(*sse); i++)
    if (!strcmp(m, sse[i].n) && nops == 2 && b->kind == OP_XMM &&
        (a->kind == OP_XMM || is_mem(a))) {
      enc_sse(sse[i].pfx, false, sse[i].opc, b->reg, a);
      return true;
    }
  struct { char *n; int pfx; int load; int store; } mv[] = {
      {"movss", 0xf3, 0x10, 0x11}, {"movsd", 0xf2, 0x10, 0x11}, {"movaps", 0, 0x28, 0x29},
      {"movapd", 0x66, 0x28, 0x29}, {"movups", 0, 0x10, 0x11}, {"movupd", 0x66, 0x10, 0x11},
      {"movdqa", 0x66, 0x6f, 0x7f}, {"movdqu", 0xf3, 0x6f, 0x7f}, {"movlps", 0, 0x12, 0x13},
      {"movlpd", 0x66, 0x12, 0x13}, {"movhps", 0, 0x16, 0x17},
  };
  for (size_t i = 0; i < sizeof(mv) / sizeof(*mv); i++) {
    if (strcmp(m, mv[i].n) || nops != 2)
      continue;
    if (b->kind == OP_XMM && (a->kind == OP_XMM || is_mem(a))) {
      enc_sse(mv[i].pfx, false, mv[i].load, b->reg, a);
      return true;
    }
    if (a->kind == OP_XMM && is_mem(b)) {
      enc_sse(mv[i].pfx, false, mv[i].store, a->reg, b);
      return true;
    }
  }
  if ((!strcmp(m, "movd") || !strcmp(m, "movq")) && nops == 2) {
    bool q = m[3] == 'q';
    if (b->kind == OP_XMM && a->kind == OP_REG) {
      enc_sse(0x66, a->size == 8, 0x6e, b->reg, a);
      return true;
    }
    if (a->kind == OP_XMM && b->kind == OP_REG) {
      enc_sse(0x66, b->size == 8, 0x7e, a->reg, b);
      return true;
    }
    if (q && b->kind == OP_XMM && (a->kind == OP_XMM || is_mem(a))) {
      enc_sse(0xf3, false, 0x7e, b->reg, a);
      return true;
    }
    if (q && a->kind == OP_XMM && is_mem(b)) {
      enc_sse(0x66, false, 0xd6, a->reg, b);
      return true;
    }
    if (!q && b->kind == OP_XMM && is_mem(a)) {
      enc_sse(0x66, false, 0x6e, b->reg, a);
      return true;
    }
    if (!q && a->kind == OP_XMM && is_mem(b)) {
      enc_sse(0x66, false, 0x7e, a->reg, b);
      return true;
    }
    /* movq between general registers/memory: falls through to mov */
  }
  /* conversions with general registers */
  struct { char *n; int pfx; int opc; int to_gpr; } cvt[] = {
      {"cvtsi2ss", 0xf3, 0x2a, 0}, {"cvtsi2sd", 0xf2, 0x2a, 0}, {"cvttss2si", 0xf3, 0x2c, 1},
      {"cvttsd2si", 0xf2, 0x2c, 1}, {"cvtss2si", 0xf3, 0x2d, 1}, {"cvtsd2si", 0xf2, 0x2d, 1},
  };
  for (size_t i = 0; i < sizeof(cvt) / sizeof(*cvt); i++) {
    int nl = strlen(cvt[i].n);
    if (strncmp(m, cvt[i].n, nl) || nops != 2)
      continue;
    int sz = 0;
    if (m[nl] && !m[nl + 1])
      sz = suffix_size(m[nl]);
    else if (m[nl])
      continue;
    if (!cvt[i].to_gpr) {
      if (b->kind != OP_XMM || !is_rm(a))
        continue;
      if (!sz)
        sz = a->kind == OP_REG ? a->size : 4;
      enc_sse(cvt[i].pfx, sz == 8, cvt[i].opc, b->reg, a);
    } else {
      if (b->kind != OP_REG || (a->kind != OP_XMM && !is_mem(a)))
        continue;
      if (!sz)
        sz = b->size;
      enc_sse(cvt[i].pfx, sz == 8, cvt[i].opc, b->reg, a);
    }
    return true;
  }

  /* --- general-purpose instructions with an optional size suffix */
  static char *alu_names[] = {"add", "or", "adc", "sbb", "and", "sub", "xor", "cmp"};
  char base[32];
  int size = 0;
  snprintf(base, sizeof(base), "%s", m);
  static char *known[] = {"add", "or", "adc", "sbb", "and", "sub", "xor", "cmp", "mov",
                          "movabs", "lea", "push", "pop", "test", "not", "neg", "mul",
                          "imul", "div", "idiv", "inc", "dec", "shl", "sal", "shr", "sar",
                          "rol", "ror", "rcl", "rcr", "xchg", "cmpxchg", "xadd", "bt", "bts",
                          "btr", "btc", "bswap", "bsf", "bsr", "popcnt", "lzcnt", "tzcnt",
                          NULL};
  bool found = false;
  for (int i = 0; known[i]; i++)
    if (!strcmp(base, known[i]))
      found = true;
  if (!found && len > 1 && suffix_size(m[len - 1])) {
    base[len - 1] = '\0';
    for (int i = 0; known[i]; i++)
      if (!strcmp(base, known[i]))
        found = true;
    if (found)
      size = suffix_size(m[len - 1]);
    else
      snprintf(base, sizeof(base), "%s", m);
  }
  if (!size)
    size = reg_size(ops, nops);

  for (int n = 0; n < 8; n++)
    if (!strcmp(base, alu_names[n]) && nops == 2) {
      if (!check_size(size, m))
        return true;
      return enc_alu(n, size, a, b);
    }

  if ((!strcmp(base, "mov") || !strcmp(base, "movabs")) && nops == 2) {
    if (!check_size(size, m))
      return true;
    return enc_mov(size, a, b, !strcmp(base, "movabs"));
  }

  /* movzx / movsx families */
  struct { char *n; int src; int dst; int opc; } ext[] = {
      {"movzbw", 1, 2, 0xb6}, {"movzbl", 1, 4, 0xb6}, {"movzbq", 1, 8, 0xb6},
      {"movzwl", 2, 4, 0xb7}, {"movzwq", 2, 8, 0xb7}, {"movsbw", 1, 2, 0xbe},
      {"movsbl", 1, 4, 0xbe}, {"movsbq", 1, 8, 0xbe}, {"movswl", 2, 4, 0xbf},
      {"movswq", 2, 8, 0xbf},
  };
  for (size_t i = 0; i < sizeof(ext) / sizeof(*ext); i++)
    if (!strcmp(m, ext[i].n) && nops == 2 && is_rm(a) && is_reg(b)) {
      op[0] = 0x0f; op[1] = ext[i].opc;
      in.op = op; in.oplen = 2; in.reg = b->reg; in.rm = a;
      in.rexw = ext[i].dst == 8; in.opsize16 = ext[i].dst == 2;
      need_rex8(&in, a, NULL);
      emit_insn(&in);
      return true;
    }
  if ((!strcmp(m, "movzx") || !strcmp(m, "movsx")) && nops == 2 && is_reg(a) && is_reg(b)) {
    op[0] = 0x0f; op[1] = (m[3] == 'z' ? 0xb6 : 0xbe) + (a->size == 2);
    in.op = op; in.oplen = 2; in.reg = b->reg; in.rm = a;
    in.rexw = b->size == 8; in.opsize16 = b->size == 2;
    need_rex8(&in, a, NULL);
    emit_insn(&in);
    return true;
  }
  if ((!strcmp(m, "movslq") || !strcmp(m, "movsxd")) && nops == 2 && is_rm(a) && is_reg(b)) {
    op[0] = 0x63;
    in.op = op; in.oplen = 1; in.reg = b->reg; in.rm = a; in.rexw = true;
    emit_insn(&in);
    return true;
  }

  if (!strcmp(base, "lea") && nops == 2 && is_mem(a) && is_reg(b)) {
    op[0] = 0x8d;
    in.op = op; in.oplen = 1; in.reg = b->reg; in.rm = a;
    in.rexw = b->size == 8; in.opsize16 = b->size == 2;
    emit_insn(&in);
    return true;
  }

  if ((!strcmp(base, "push") || !strcmp(base, "pop")) && nops == 1) {
    bool push = base[1] == 'u';
    if (is_reg(a)) {
      emit_opreg(a->size == 2, false, push ? 0x50 : 0x58, a->reg, false);
      return true;
    }
    if (push && a->kind == OP_IMM) {
      if (absolute(&a->e) && fits8(a->e.val)) {
        emit8(0x6a);
        emit8((uint8_t)a->e.val);
      } else {
        emit8(0x68);
        emit_imm(a, 4);
      }
      return true;
    }
    if (is_mem(a)) {
      op[0] = push ? 0xff : 0x8f;
      in.op = op; in.oplen = 1; in.reg = push ? 6 : 0; in.rm = a;
      emit_insn(&in);
      return true;
    }
    return false;
  }

  if (!strcmp(base, "test") && nops == 2) {
    if (!check_size(size, m))
      return true;
    in.opsize16 = size == 2;
    in.rexw = size == 8;
    if (a->kind == OP_IMM && is_rm(b)) {
      if (is_reg(b) && b->reg == 0) {
        if (in.opsize16) emit8(0x66);
        if (in.rexw) emit8(0x48);
        emit8(size == 1 ? 0xa8 : 0xa9);
        emit_imm(a, size == 1 ? 1 : size == 2 ? 2 : 4);
        return true;
      }
      op[0] = size == 1 ? 0xf6 : 0xf7;
      int isz = size == 1 ? 1 : size == 2 ? 2 : 4;
      in.op = op; in.oplen = 1; in.reg = 0; in.rm = b; in.imm_size = isz;
      need_rex8(&in, b, NULL);
      emit_insn(&in);
      emit_imm(a, isz);
      return true;
    }
    if (is_reg(a) && is_rm(b)) {
      op[0] = size == 1 ? 0x84 : 0x85;
      in.op = op; in.oplen = 1; in.reg = a->reg; in.rm = b;
      need_rex8(&in, a, b);
      emit_insn(&in);
      return true;
    }
    if (is_mem(a) && is_reg(b)) {
      op[0] = size == 1 ? 0x84 : 0x85;
      in.op = op; in.oplen = 1; in.reg = b->reg; in.rm = a;
      emit_insn(&in);
      return true;
    }
    return false;
  }

  static struct { char *n; int ext; } grp3[] = {
      {"not", 2}, {"neg", 3}, {"mul", 4}, {"div", 6}, {"idiv", 7},
  };
  for (size_t i = 0; i < sizeof(grp3) / sizeof(*grp3); i++)
    if (!strcmp(base, grp3[i].n) && nops == 1 && is_rm(a)) {
      if (!check_size(size, m))
        return true;
      op[0] = size == 1 ? 0xf6 : 0xf7;
      in.op = op; in.oplen = 1; in.reg = grp3[i].ext; in.rm = a;
      in.opsize16 = size == 2; in.rexw = size == 8;
      need_rex8(&in, a, NULL);
      emit_insn(&in);
      return true;
    }
  if (!strcmp(base, "imul")) {
    if (!check_size(size, m))
      return true;
    in.opsize16 = size == 2; in.rexw = size == 8;
    if (nops == 1 && is_rm(a)) {
      op[0] = size == 1 ? 0xf6 : 0xf7;
      in.op = op; in.oplen = 1; in.reg = 5; in.rm = a;
      emit_insn(&in);
      return true;
    }
    if (nops == 2 && is_rm(a) && is_reg(b)) {
      op[0] = 0x0f; op[1] = 0xaf;
      in.op = op; in.oplen = 2; in.reg = b->reg; in.rm = a;
      emit_insn(&in);
      return true;
    }
    if ((nops == 3 || nops == 2) && a->kind == OP_IMM) {
      Operand *src = nops == 3 ? b : b;
      Operand *dst = nops == 3 ? c : b;
      if (!is_reg(dst) || !is_rm(src))
        return false;
      bool small = absolute(&a->e) && fits8(a->e.val);
      op[0] = small ? 0x6b : 0x69;
      int isz = small ? 1 : size == 2 ? 2 : 4;
      in.op = op; in.oplen = 1; in.reg = dst->reg; in.rm = src; in.imm_size = isz;
      emit_insn(&in);
      emit_imm(a, isz);
      return true;
    }
    return false;
  }
  if ((!strcmp(base, "inc") || !strcmp(base, "dec")) && nops == 1 && is_rm(a)) {
    if (!check_size(size, m))
      return true;
    op[0] = size == 1 ? 0xfe : 0xff;
    in.op = op; in.oplen = 1; in.reg = base[0] == 'd'; in.rm = a;
    in.opsize16 = size == 2; in.rexw = size == 8;
    need_rex8(&in, a, NULL);
    emit_insn(&in);
    return true;
  }
  static struct { char *n; int ext; } shifts[] = {
      {"rol", 0}, {"ror", 1}, {"rcl", 2}, {"rcr", 3}, {"shl", 4}, {"sal", 4}, {"shr", 5},
      {"sar", 7},
  };
  for (size_t i = 0; i < sizeof(shifts) / sizeof(*shifts); i++) {
    if (strcmp(base, shifts[i].n))
      continue;
    Operand *dst = nops == 2 ? b : a;
    if (nops == 2 && is_reg(a) && a->reg == 1 && a->size == 1)
      size = size && !(m[len - 1] == 'b' || m[len - 1] == 'w' || m[len - 1] == 'l' ||
                       m[len - 1] == 'q')
                 ? size
                 : (suffix_size(m[len - 1]) && strcmp(base, m) ? size : dst->size);
    if (is_reg(dst))
      size = suffix_size(m[len - 1]) && strcmp(base, m) ? size : dst->size;
    if (!check_size(size, m) || !is_rm(dst))
      return true;
    in.opsize16 = size == 2; in.rexw = size == 8;
    in.reg = shifts[i].ext; in.rm = dst;
    need_rex8(&in, dst, NULL);
    if (nops == 1 || (a->kind == OP_IMM && absolute(&a->e) && a->e.val == 1)) {
      op[0] = size == 1 ? 0xd0 : 0xd1;
      in.op = op; in.oplen = 1;
      emit_insn(&in);
      return true;
    }
    if (is_reg(a) && a->reg == 1 && a->size == 1) {
      op[0] = size == 1 ? 0xd2 : 0xd3;
      in.op = op; in.oplen = 1;
      emit_insn(&in);
      return true;
    }
    if (a->kind == OP_IMM) {
      op[0] = size == 1 ? 0xc0 : 0xc1;
      in.op = op; in.oplen = 1; in.imm_size = 1;
      emit_insn(&in);
      emit_imm(a, 1);
      return true;
    }
    return false;
  }
  if (!strcmp(base, "xchg") && nops == 2) {
    if (!check_size(size, m))
      return true;
    Operand *r = is_reg(a) ? a : b;
    Operand *o = is_reg(a) ? b : a;
    if (!is_reg(r) || !is_rm(o))
      return false;
    if (size != 1 && is_reg(o) && (r->reg == 0 || o->reg == 0) && r->reg != o->reg) {
      /* accumulator short form 90+r */
      emit_opreg(size == 2, size == 8, 0x90, r->reg == 0 ? o->reg : r->reg, false);
      return true;
    }
    op[0] = size == 1 ? 0x86 : 0x87;
    in.op = op; in.oplen = 1; in.reg = r->reg; in.rm = o;
    in.opsize16 = size == 2; in.rexw = size == 8;
    need_rex8(&in, r, o);
    emit_insn(&in);
    return true;
  }
  if ((!strcmp(base, "cmpxchg") || !strcmp(base, "xadd")) && nops == 2 && is_reg(a) &&
      is_rm(b)) {
    if (!check_size(size, m))
      return true;
    op[0] = 0x0f;
    op[1] = (base[0] == 'c' ? 0xb0 : 0xc0) + (size != 1);
    in.op = op; in.oplen = 2; in.reg = a->reg; in.rm = b;
    in.opsize16 = size == 2; in.rexw = size == 8;
    need_rex8(&in, a, b);
    emit_insn(&in);
    return true;
  }
  static struct { char *n; int ext; int regop; } bts[] = {
      {"bt", 4, 0xa3}, {"bts", 5, 0xab}, {"btr", 6, 0xb3}, {"btc", 7, 0xbb},
  };
  for (size_t i = 0; i < sizeof(bts) / sizeof(*bts); i++) {
    if (strcmp(base, bts[i].n) || nops != 2)
      continue;
    if (is_reg(b))
      size = b->size;
    if (!check_size(size, m))
      return true;
    in.opsize16 = size == 2; in.rexw = size == 8;
    if (a->kind == OP_IMM) {
      op[0] = 0x0f; op[1] = 0xba;
      in.op = op; in.oplen = 2; in.reg = bts[i].ext; in.rm = b; in.imm_size = 1;
      emit_insn(&in);
      emit_imm(a, 1);
      return true;
    }
    if (is_reg(a)) {
      op[0] = 0x0f; op[1] = bts[i].regop;
      in.op = op; in.oplen = 2; in.reg = a->reg; in.rm = b;
      emit_insn(&in);
      return true;
    }
    return false;
  }
  if (!strcmp(base, "bswap") && nops == 1 && is_reg(a)) {
    if (seg_prefix) emit8(seg_prefix);
    int rex = (a->size == 8 ? 8 : 0) | (a->reg >= 8);
    if (rex)
      emit8(0x40 | rex);
    emit8(0x0f);
    emit8(0xc8 + (a->reg & 7));
    return true;
  }
  static struct { char *n; int pfx; int opc; } bitscan[] = {
      {"bsf", 0, 0xbc}, {"bsr", 0, 0xbd}, {"popcnt", 0xf3, 0xb8}, {"lzcnt", 0xf3, 0xbd},
      {"tzcnt", 0xf3, 0xbc},
  };
  for (size_t i = 0; i < sizeof(bitscan) / sizeof(*bitscan); i++)
    if (!strcmp(base, bitscan[i].n) && nops == 2 && is_rm(a) && is_reg(b)) {
      op[0] = 0x0f; op[1] = bitscan[i].opc;
      in.prefix = bitscan[i].pfx;
      in.op = op; in.oplen = 2; in.reg = b->reg; in.rm = a;
      in.opsize16 = b->size == 2; in.rexw = b->size == 8;
      emit_insn(&in);
      return true;
    }
  if (!strcmp(m, "ret") && nops == 1 && a->kind == OP_IMM) {
    emit8(0xc2);
    emit_imm(a, 2);
    return true;
  }
  if (!strcmp(m, "int") && nops == 1 && a->kind == OP_IMM) {
    emit8(0xcd);
    emit_imm(a, 1);
    return true;
  }
  return false;
}

/* ------------------------------------------------------- directives */

static void set_section(Section *s) {
  if (s != cur)
    prev_sec = cur;
  cur = s;
}

static char *parse_string(Buf *out) {
  skip_ws();
  if (*p != '"') {
    asm_error("expected a string");
    return NULL;
  }
  p++;
  while (*p && *p != '"') {
    if (*p == '\\') {
      p++;
      switch (*p) {
      case 'n': buf_putc(out, '\n'); p++; break;
      case 't': buf_putc(out, '\t'); p++; break;
      case 'r': buf_putc(out, '\r'); p++; break;
      case 'b': buf_putc(out, '\b'); p++; break;
      case 'f': buf_putc(out, '\f'); p++; break;
      case '"': buf_putc(out, '"'); p++; break;
      case '\\': buf_putc(out, '\\'); p++; break;
      case 'x': {
        p++;
        int v = 0;
        while (isxdigit((unsigned char)*p)) {
          v = v * 16 + (isdigit((unsigned char)*p) ? *p - '0' : tolower((unsigned char)*p) - 'a' + 10);
          p++;
        }
        buf_putc(out, v);
        break;
      }
      default:
        if (*p >= '0' && *p <= '7') {
          int v = 0;
          for (int k = 0; k < 3 && *p >= '0' && *p <= '7'; k++)
            v = v * 8 + (*p++ - '0');
          buf_putc(out, v);
        } else {
          buf_putc(out, *p++);
        }
      }
      continue;
    }
    buf_putc(out, *p++);
  }
  if (*p == '"')
    p++;
  return out->buf ? out->buf : "";
}

static void align_to_boundary(int64_t a, int fill, bool code) {
  if (a <= 0)
    return;
  if (a & (a - 1)) {
    asm_error("alignment is not a power of 2");
    return;
  }
  if ((uint64_t)a > cur->align)
    cur->align = a;
  while (cur_offset() % a)
    emit8(cur->type == SHT_NOBITS ? 0 : (fill >= 0 ? fill : code ? 0x90 : 0));
}

static void data_directive(int size) {
  for (;;) {
    Expr e;
    skip_ws();
    if (!*p)
      return;
    if (!parse_expr(&e))
      return;
    if (absolute(&e)) {
      emitn((uint64_t)e.val, size);
    } else {
      int64_t v;
      if (const_value(&e, &v))
        emitn((uint64_t)v, size);
      else {
        add_fixup(size, false, 0, &e);
        emitn(0, size);
      }
    }
    if (!consume_ch(','))
      return;
  }
}

static bool parse_sym_list(void (*fn)(Sym *)) {
  for (;;) {
    skip_ws();
    char *n = read_name();
    if (!n) {
      asm_error("expected a symbol name");
      return false;
    }
    fn(get_sym(n));
    if (!consume_ch(','))
      return true;
  }
}

static void mark_global(Sym *s) { s->global = true; }
static void mark_weak(Sym *s) { s->weak = true; s->global = true; }
static void mark_local(Sym *s) { s->local_decl = true; }
static void mark_hidden(Sym *s) { s->vis = STV_HIDDEN; }
static void mark_protected(Sym *s) { s->vis = STV_PROTECTED; }
static void mark_internal(Sym *s) { s->vis = STV_INTERNAL; }

static void directive(char *name) {
  skip_ws();
  if (!strcmp(name, ".text") || !strcmp(name, ".data") || !strcmp(name, ".bss")) {
    set_section(named_section(name));
    return;
  }
  if (!strcmp(name, ".section") || !strcmp(name, ".pushsection")) {
    char *sname;
    if (*p == '"') {
      sname = read_name();
    } else {
      char *st = p;
      while (*p && *p != ',' && !isspace((unsigned char)*p))
        p++;
      sname = p > st ? xstrndup(st, p - st) : NULL;
    }
    if (!sname) {
      asm_error("expected a section name");
      return;
    }
    Section *s;
    bool has_flags = false;
    uint64_t flags = 0;
    uint32_t type = SHT_PROGBITS;
    bool type_given = false;
    if (consume_ch(',')) {
      Buf fb = {0};
      skip_ws();
      if (*p == '"') {
        parse_string(&fb);
        has_flags = true;
        for (char *f = fb.buf ? fb.buf : ""; *f; f++) {
          switch (*f) {
          case 'a': flags |= SHF_ALLOC; break;
          case 'w': flags |= SHF_WRITE; break;
          case 'x': flags |= SHF_EXECINSTR; break;
          case 'T': flags |= SHF_TLS; break;
          case 'M': flags |= SHF_MERGE; break;
          case 'S': flags |= SHF_STRINGS; break;
          case 'G': flags |= SHF_GROUP; break;
          default: break;
          }
        }
      }
      if (consume_ch(',')) {
        skip_ws();
        if (*p == '@' || *p == '%')
          p++;
        char *t = read_name();
        type_given = t != NULL;
        if (t && !strcmp(t, "nobits"))
          type = SHT_NOBITS;
        else if (t && !strcmp(t, "note"))
          type = SHT_NOTE;
        else if (t && !strcmp(t, "init_array"))
          type = SHT_INIT_ARRAY;
        else if (t && !strcmp(t, "fini_array"))
          type = SHT_FINI_ARRAY;
        else if (t && !strcmp(t, "preinit_array"))
          type = SHT_PREINIT_ARRAY;
        else if (t && !strcmp(t, "unwind"))
          type = SHT_X86_64_UNWIND;
        while (*p)
          p++;  /* entity size, group name: ignored */
      }
    }
    if (has_flags) {
      s = NULL;
      for (int i = 0; i < nsections; i++)
        if (!strcmp(sections[i]->name, sname))
          s = sections[i];
      if (!s) {
        Section *known = named_section(sname);
        if (!type_given)
          type = known->type;
        s = known;
        s->type = type;
        s->flags = flags;
      }
    } else {
      s = named_section(sname);
    }
    set_section(s);
    return;
  }
  if (!strcmp(name, ".previous") || !strcmp(name, ".popsection")) {
    if (prev_sec) {
      Section *t = cur;
      cur = prev_sec;
      prev_sec = t;
    }
    return;
  }
  if (!strcmp(name, ".globl") || !strcmp(name, ".global")) {
    parse_sym_list(mark_global);
    return;
  }
  if (!strcmp(name, ".weak")) {
    parse_sym_list(mark_weak);
    return;
  }
  if (!strcmp(name, ".local")) {
    parse_sym_list(mark_local);
    return;
  }
  if (!strcmp(name, ".hidden")) {
    parse_sym_list(mark_hidden);
    return;
  }
  if (!strcmp(name, ".protected")) {
    parse_sym_list(mark_protected);
    return;
  }
  if (!strcmp(name, ".internal")) {
    parse_sym_list(mark_internal);
    return;
  }
  if (!strcmp(name, ".type")) {
    char *n = read_name();
    if (!n || !consume_ch(',')) {
      asm_error("bad .type directive");
      return;
    }
    skip_ws();
    if (*p == '@' || *p == '%' || *p == '"')
      p++;
    char *t = read_name();
    Sym *s = get_sym(n);
    if (!t) {
      asm_error("bad .type directive");
      return;
    }
    if (!strcmp(t, "function")) s->type = STT_FUNC;
    else if (!strcmp(t, "object")) s->type = STT_OBJECT;
    else if (!strcmp(t, "tls_object")) s->type = STT_TLS;
    else if (!strcmp(t, "notype")) s->type = STT_NOTYPE;
    else if (!strcmp(t, "gnu_indirect_function")) s->type = STT_GNU_IFUNC;
    else if (!strcmp(t, "common")) s->type = STT_COMMON;
    else
      asm_error("unknown symbol type '%s'", t);
    return;
  }
  if (!strcmp(name, ".size")) {
    char *n = read_name();
    if (!n || !consume_ch(',')) {
      asm_error("bad .size directive");
      return;
    }
    Expr *e = xcalloc(1, sizeof(Expr));
    if (!parse_expr(e))
      return;
    Sym *s = get_sym(n);
    s->size_expr = e;
    return;
  }
  if (!strcmp(name, ".set") || !strcmp(name, ".equ")) {
    char *n = read_name();
    if (!n || !consume_ch(',')) {
      asm_error("bad %s directive", name);
      return;
    }
    Expr *e = xcalloc(1, sizeof(Expr));
    if (!parse_expr(e))
      return;
    Sym *s = get_sym(n);
    s->set_expr = e;
    return;
  }
  if (!strcmp(name, ".align") || !strcmp(name, ".balign") || !strcmp(name, ".p2align")) {
    Expr e;
    if (!parse_expr(&e) || !absolute(&e)) {
      asm_error("bad alignment");
      return;
    }
    int fill = -1;
    if (consume_ch(',')) {
      skip_ws();
      if (*p != ',') {
        Expr f;
        if (parse_expr(&f) && absolute(&f))
          fill = (int)f.val & 0xff;
      }
      /* the maximum number of bytes to skip is ignored */
      while (*p)
        p++;
    }
    int64_t a = !strcmp(name, ".p2align") ? (int64_t)1 << e.val : e.val;
    align_to_boundary(a, fill, (cur->flags & SHF_EXECINSTR) != 0);
    return;
  }
  if (!strcmp(name, ".byte")) { data_directive(1); return; }
  if (!strcmp(name, ".short") || !strcmp(name, ".value") || !strcmp(name, ".word") ||
      !strcmp(name, ".2byte") || !strcmp(name, ".hword")) { data_directive(2); return; }
  if (!strcmp(name, ".long") || !strcmp(name, ".int") || !strcmp(name, ".4byte")) {
    data_directive(4);
    return;
  }
  if (!strcmp(name, ".quad") || !strcmp(name, ".8byte")) { data_directive(8); return; }
  if (!strcmp(name, ".zero") || !strcmp(name, ".skip") || !strcmp(name, ".space")) {
    Expr e;
    if (!parse_expr(&e) || !absolute(&e) || e.val < 0) {
      asm_error("bad size in %s", name);
      return;
    }
    int fill = 0;
    if (consume_ch(',')) {
      Expr f;
      if (parse_expr(&f) && absolute(&f))
        fill = (int)f.val & 0xff;
    }
    if (cur->type == SHT_NOBITS)
      cur->size += e.val;
    else
      for (int64_t i = 0; i < e.val; i++)
        emit8(fill);
    return;
  }
  if (!strcmp(name, ".ascii") || !strcmp(name, ".asciz") || !strcmp(name, ".string")) {
    for (;;) {
      Buf s = {0};
      if (!parse_string(&s))
        return;
      for (size_t i = 0; i < s.len; i++)
        emit8((unsigned char)s.buf[i]);
      if (strcmp(name, ".ascii"))
        emit8(0);
      if (!consume_ch(','))
        return;
    }
  }
  if (!strcmp(name, ".comm") || !strcmp(name, ".lcomm")) {
    char *n = read_name();
    Expr sz;
    if (!n || !consume_ch(',') || !parse_expr(&sz) || !absolute(&sz)) {
      asm_error("bad %s directive", name);
      return;
    }
    int64_t al = 1;
    if (consume_ch(',')) {
      Expr a;
      if (parse_expr(&a) && absolute(&a))
        al = a.val;
    }
    Sym *s = get_sym(n);
    if (name[1] == 'l') {
      /* local common: allocate in .bss */
      Section *save = cur;
      cur = named_section(".bss");
      align_to_boundary(al, 0, false);
      s->defined = true;
      s->sec = cur;
      s->value = cur_offset();
      cur->size += sz.val;
      cur = save;
      s->type = STT_OBJECT;
      s->size = sz.val;
      s->has_size = true;
    } else {
      s->is_common = true;
      s->global = true;
      s->size = sz.val;
      s->has_size = true;
      s->common_align = al;
      s->type = STT_OBJECT;
    }
    return;
  }
  if (!strcmp(name, ".file")) {
    skip_ws();
    if (*p == '"') {
      Buf s = {0};
      parse_string(&s);
      main_file = s.buf ? s.buf : "";
      return;
    }
    Expr n;
    if (!parse_expr(&n) || !absolute(&n)) {
      asm_error("bad .file directive");
      return;
    }
    Buf s = {0};
    parse_string(&s);
    int idx = (int)n.val;
    if (idx >= nfiles) {
      files = xrealloc(files, sizeof(char *) * (idx + 1));
      for (int i = nfiles; i <= idx; i++)
        files[i] = NULL;
      nfiles = idx + 1;
    }
    files[idx] = s.buf ? s.buf : "";
    return;
  }
  if (!strcmp(name, ".loc")) {
    Expr f, l;
    if (!parse_expr(&f) || !parse_expr(&l))
      return;
    while (*p)
      p++;
    if (!(cur->flags & SHF_EXECINSTR))
      return;
    if (nlines == caplines) {
      caplines = caplines ? caplines * 2 : 1024;
      lines = xrealloc(lines, sizeof(LineEntry) * caplines);
    }
    lines[nlines].file = (int)f.val;
    lines[nlines].line = (int)l.val;
    lines[nlines].offset = cur_offset();
    lines[nlines].sec = cur;
    nlines++;
    return;
  }
  if (!strcmp(name, ".code32") || !strcmp(name, ".code64")) {
    code32 = name[5] == '3';
    return;
  }
  if (!strcmp(name, ".code16")) {
    asm_error("16-bit code is not supported");
    return;
  }
  if (startswith(name, ".cfi_") || !strcmp(name, ".ident") ||
      !strcmp(name, ".att_syntax") || !strcmp(name, ".end") || !strcmp(name, ".loc_mark_labels")) {
    while (*p)
      p++;
    return;
  }
  if (!strcmp(name, ".intel_syntax")) {
    asm_error("Intel syntax is not supported");
    return;
  }
  asm_error("unknown directive '%s'", name);
}

/* ------------------------------------------------------------- macros */

/* .macro NAME [param[=default]]... / .endm: a textual macro; in the body,
   \param is the argument, \() separates, \@ counts expansions. */
typedef struct {
  char *name;
  char **params, **defaults;
  int nparams;
  Buf body;
} Macro;

static Macro *macros;
static int nmacros, capmacros, macro_count, macro_depth;
static Macro *macro_def;  /* the macro whose body is being collected */

static void assemble_source(char *src);

static Macro *find_macro(char *name) {
  for (int i = 0; i < nmacros; i++)
    if (!strcmp(macros[i].name, name))
      return &macros[i];
  return NULL;
}

static void define_macro(void) {
  char *name = read_name();
  if (!name) {
    asm_error(".macro: name expected");
    return;
  }
  if (nmacros == capmacros) {
    capmacros = capmacros ? capmacros * 2 : 16;
    macros = xrealloc(macros, sizeof(Macro) * capmacros);
  }
  Macro *mc = &macros[nmacros++];
  memset(mc, 0, sizeof(*mc));
  mc->name = name;
  for (;;) {
    skip_ws();
    consume_ch(',');
    skip_ws();
    if (!*p)
      break;
    char *pn = read_name();
    if (!pn) {
      asm_error(".macro: bad parameter '%.20s'", p);
      return;
    }
    char *def = NULL;
    skip_ws();
    if (*p == '=') {
      p++;
      skip_ws();
      char *st = p;
      while (*p && *p != ',' && *p != ' ' && *p != '\t')
        p++;
      def = xstrndup(st, p - st);
    } else if (*p == ':') {  /* :req, :vararg: accepted, not checked */
      p++;
      read_name();
    }
    mc->params = xrealloc(mc->params, sizeof(char *) * (mc->nparams + 1));
    mc->defaults = xrealloc(mc->defaults, sizeof(char *) * (mc->nparams + 1));
    mc->params[mc->nparams] = pn;
    mc->defaults[mc->nparams++] = def;
  }
  macro_def = mc;
}

/* While a macro body is being collected: true if stmt was taken */
static bool collect_macro(char *stmt) {
  if (!macro_def)
    return false;
  char *q = stmt;
  while (*q == ' ' || *q == '\t')
    q++;
  if (startswith(q, ".endm") && !isalnum((unsigned char)q[5]) && q[5] != '_') {
    macro_def = NULL;
    return true;
  }
  if (startswith(q, ".macro") && (q[6] == ' ' || q[6] == '\t')) {
    asm_error("nested .macro definitions are not supported");
    return true;
  }
  buf_puts(&macro_def->body, stmt);
  buf_putc(&macro_def->body, '\n');
  return true;
}

static void expand_macro(Macro *mc) {
  /* arguments: separated by commas, or by blanks if there are no commas */
  char **args = xcalloc(mc->nparams + 1, sizeof(char *));
  bool commas = strchr(p, ',') != NULL;
  for (int n = 0;; n++) {
    skip_ws();
    if (!*p)
      break;
    char *st = p;
    int depth = 0;
    while (*p && (depth || (*p != ',' && (commas || (*p != ' ' && *p != '\t'))))) {
      if (*p == '(')
        depth++;
      else if (*p == ')')
        depth--;
      p++;
    }
    char *e = p;
    while (e > st && (e[-1] == ' ' || e[-1] == '\t'))
      e--;
    if (n < mc->nparams)
      args[n] = xstrndup(st, e - st);
    else {
      asm_error("too many arguments to macro '%s'", mc->name);
      return;
    }
    skip_ws();
    if (*p == ',')
      p++;
  }
  Buf out = {0};
  char *b = mc->body.buf ? mc->body.buf : "";
  int count = macro_count++;
  while (*b) {
    if (*b != '\\') {
      buf_putc(&out, *b++);
      continue;
    }
    if (b[1] == '(' && b[2] == ')') {
      b += 3;
      continue;
    }
    if (b[1] == '@') {
      char num[24];
      snprintf(num, sizeof(num), "%d", count);
      buf_puts(&out, num);
      b += 2;
      continue;
    }
    int best = -1, bestlen = 0;
    for (int i = 0; i < mc->nparams; i++) {
      int l = strlen(mc->params[i]);
      if (!strncmp(b + 1, mc->params[i], l) && !is_sym2((unsigned char)b[1 + l]) &&
          l > bestlen)
        best = i, bestlen = l;
    }
    if (best < 0) {
      buf_putc(&out, *b++);
      continue;
    }
    char *v = args[best] ? args[best] : mc->defaults[best] ? mc->defaults[best] : "";
    buf_puts(&out, v);
    b += 1 + bestlen;
  }
  buf_putc(&out, '\0');
  if (macro_depth >= 100) {
    asm_error("macro '%s' nested too deeply", mc->name);
    return;
  }
  int save_line = lineno;
  macro_depth++;
  assemble_source(out.buf);
  macro_depth--;
  lineno = save_line;
}

/* --------------------------------------------------------- statements */

static void statement(char *stmt) {
  if (collect_macro(stmt))
    return;
  p = stmt;
  for (;;) {
    skip_ws();
    if (!*p)
      return;
    /* label? */
    char *save = p;
    if (isdigit((unsigned char)*p)) {
      char *q = p;
      while (isdigit((unsigned char)*q))
        q++;
      if (*q == ':' && q - p == 1) {
        int n = *p - '0';
        set_section(cur);
        char *nm = numlabel_name(n, numlabel_count[n]++);
        define_label(nm);
        p = q + 1;
        continue;
      }
      if (*q == ':') {
        asm_error("only single-digit local labels are supported");
        return;
      }
    }
    char *name = read_name();
    if (name) {
      skip_ws();
      if (*p == ':') {
        p++;
        define_label(name);
        continue;
      }
      if (*p == '=') {  /* sym = expr */
        p++;
        Expr *e = xcalloc(1, sizeof(Expr));
        if (parse_expr(e))
          get_sym(name)->set_expr = e;
        return;
      }
    } else {
      p = save;
      asm_error("syntax error at '%.20s'", p);
      return;
    }
    Macro *mc = find_macro(name);
    if (mc) {
      expand_macro(mc);
      return;
    }
    if (!strcmp(name, ".macro")) {
      define_macro();
      return;
    }
    if (!strcmp(name, ".endm")) {
      asm_error(".endm without .macro");
      return;
    }
    if (name[0] == '.') {
      directive(name);
      skip_ws();
      if (*p && !errors)
        asm_error("junk at end of line: '%.20s'", p);
      return;
    }

    /* prefixes */
    seg_prefix = 0;
    char *mn = name;
    for (;;) {
      char lower[16];
      snprintf(lower, sizeof(lower), "%s", mn);
      for (char *q = lower; *q; q++)
        *q = tolower((unsigned char)*q);
      int pfx = 0;
      if (!strcmp(lower, "lock")) pfx = 0xf0;
      else if (!strcmp(lower, "rep") || !strcmp(lower, "repe") || !strcmp(lower, "repz")) pfx = 0xf3;
      else if (!strcmp(lower, "repne") || !strcmp(lower, "repnz")) pfx = 0xf2;
      else if (!strcmp(lower, "data16")) pfx = 0x66;
      else if (!strcmp(lower, "rex64")) pfx = 0x48;
      if (!pfx)
        break;
      emit8(pfx);
      skip_ws();
      if (!*p)
        return;
      mn = read_name();
      if (!mn) {
        asm_error("expected an instruction after prefix");
        return;
      }
    }

    Operand ops[4];
    int nops = 0;
    skip_ws();
    while (*p) {
      if (nops == 4) {
        asm_error("too many operands");
        return;
      }
      if (!parse_operand(&ops[nops]))
        return;
      if (ops[nops].seg && !seg_prefix)
        seg_prefix = ops[nops].seg;
      nops++;
      skip_ws();
      if (!*p)
        break;
      if (*p != ',') {
        asm_error("junk after operand: '%.20s'", p);
        return;
      }
      p++;
    }
    for (int i = nops; i < 4; i++)
      memset(&ops[i], 0, sizeof(Operand));
    if (cur->type == SHT_NOBITS) {
      asm_error("instruction in a section without contents");
      return;
    }
    if (!assemble_insn(mn, ops, nops))
      asm_error("invalid instruction or operands: '%s'", stmt);
    seg_prefix = 0;
    return;
  }
}

/* Splits the source into statements: lines, ';' separators, comments. */
static void assemble_source(char *src) {
  Buf stmt = {0};
  char *s = src;
  lineno = 1;
  bool in_comment = false;
  while (*s) {
    stmt.len = 0;
    if (stmt.buf)
      stmt.buf[0] = '\0';
    bool in_str = false;
    while (*s && *s != '\n') {
      if (in_comment) {
        if (s[0] == '*' && s[1] == '/') {
          in_comment = false;
          s += 2;
          buf_putc(&stmt, ' ');
        } else {
          s++;
        }
        continue;
      }
      if (in_str) {
        if (*s == '\\' && s[1]) {
          buf_putc(&stmt, *s++);
          buf_putc(&stmt, *s++);
          continue;
        }
        if (*s == '"')
          in_str = false;
        buf_putc(&stmt, *s++);
        continue;
      }
      if (*s == '"') {
        in_str = true;
        buf_putc(&stmt, *s++);
        continue;
      }
      if (*s == '\'' && s[1] && s[1] != '\n') {
        /* character constant: keep the next char (and an escape) */
        buf_putc(&stmt, *s++);
        if (*s == '\\')
          buf_putc(&stmt, *s++);
        if (*s && *s != '\n')
          buf_putc(&stmt, *s++);
        continue;
      }
      if (*s == '#') {
        while (*s && *s != '\n')
          s++;
        break;
      }
      if (s[0] == '/' && s[1] == '*') {
        in_comment = true;
        s += 2;
        continue;
      }
      if (*s == ';') {
        s++;
        statement(stmt.buf ? stmt.buf : "");
        stmt.len = 0;
        stmt.buf[0] = '\0';
        continue;
      }
      buf_putc(&stmt, *s++);
    }
    statement(stmt.buf ? stmt.buf : "");
    if (*s == '\n') {
      s++;
      lineno++;
    }
  }
}

/* -------------------------------------------------------- resolution */

static void add_reloc(Section *s, int64_t offset, int type, Sym *sym, Section *target,
                      int64_t addend) {
  if (s->nrel == s->caprel) {
    s->caprel = s->caprel ? s->caprel * 2 : 64;
    s->rel = xrealloc(s->rel, sizeof(Reloc) * s->caprel);
  }
  Reloc *r = &s->rel[s->nrel++];
  r->offset = offset;
  r->type = type;
  r->sym = sym;
  r->sec = target;
  r->addend = addend;
}

static void patch(Fixup *f, int64_t v) {
  unsigned char *q = (unsigned char *)f->sec->data.buf + f->offset;
  switch (f->size) {
  case 1: q[0] = v; break;
  case 2: wr16(q, v); break;
  case 4: wr32(q, v); break;
  case 8: wr64(q, v); break;
  }
}

/* Resolves .set symbols to their targets. */
static Sym *resolve_alias(Sym *s, int64_t *addend) {
  for (int depth = 0; s->set_expr && depth < 100; depth++) {
    Expr *e = s->set_expr;
    if (e->op) {
      int64_t v;
      if (const_value(e, &v)) {
        s->is_abs = true;
        s->defined = true;
        s->value = v;
      }
      return s;
    }
    if (e->sub || !e->sym) {
      if (!e->sym && !e->sub) {
        s->is_abs = true;
        s->defined = true;
        s->value = e->val;
      }
      return s;
    }
    *addend += e->val;
    s = e->sym;
  }
  return s;
}

static void resolve_fixups(void) {
  for (int i = 0; i < nfixups; i++) {
    Fixup *f = &fixups[i];
    lineno = f->line;
    Expr e = f->e;
    if (e.op) {
      int64_t v;
      if (!const_value(&e, &v))
        asm_error("expression cannot be evaluated (labels in different sections?)");
      else
        patch(f, f->pcrel ? v - f->offset + f->adjust : v);
      continue;
    }
    int64_t addend = e.val + f->adjust;
    Sym *sym = e.sym ? resolve_alias(e.sym, &addend) : NULL;
    Sym *sub = e.sub ? resolve_alias(e.sub, &addend) : NULL;
    int64_t P = f->offset;

    if (sym && sym->is_abs && !sub && !f->pcrel) {
      patch(f, sym->value + addend);
      continue;
    }

    if (sub) {
      if (!sub->defined) {
        asm_error("undefined symbol '%s' in a difference", sub->name);
        continue;
      }
      if (sym && sym->defined && sym->sec == sub->sec && !e.mod) {
        int64_t v = sym->value - sub->value + addend;
        if (f->pcrel)
          v -= P;
        patch(f, v);
        continue;
      }
      if (sub->sec == f->sec && sym && !e.mod) {
        /* sym - L where L is in this section: pc-relative */
        int64_t a = addend + (P - sub->value);
        int type = f->size == 8 ? R_X86_64_PC64 : f->size == 4 ? R_X86_64_PC32 : 0;
        if (!type) {
          asm_error("unsupported relocation size");
          continue;
        }
        if (sym->defined && !sym->global && !sym->weak)
          add_reloc(f->sec, P, type, NULL, sym->sec, sym->value + a);
        else
          add_reloc(f->sec, P, type, sym, NULL, a);
        sym->used = true;
        continue;
      }
      asm_error("invalid difference of symbols in different sections");
      continue;
    }

    if (!sym) {
      patch(f, addend);
      continue;
    }
    sym->used = true;

    bool local_def = sym->defined && !sym->is_abs && !sym->global && !sym->weak && !sym->is_common;
    /* Branches and pc-relative references to a symbol defined in this
       section that cannot be preempted are resolved here. */
    if (f->pcrel && sym->defined && !sym->is_abs && sym->sec == f->sec && !sym->weak &&
        (e.mod == RM_NONE || e.mod == RM_PLT)) {
      patch(f, sym->value + addend - P);
      continue;
    }

    int type;
    switch (e.mod) {
    case RM_PLT: type = R_X86_64_PLT32; break;
    case RM_GOTPCREL: type = R_X86_64_GOTPCREL; break;
    case RM_TPOFF: type = f->size == 8 ? R_X86_64_TPOFF64 : R_X86_64_TPOFF32; break;
    case RM_GOTTPOFF: type = R_X86_64_GOTTPOFF; break;
    case RM_TLSGD: type = R_X86_64_TLSGD; break;
    case RM_TLSLD: type = R_X86_64_TLSLD; break;
    case RM_DTPOFF: type = f->size == 8 ? R_X86_64_DTPOFF64 : R_X86_64_DTPOFF32; break;
    case RM_GOTOFF: type = R_X86_64_GOTOFF64; break;
    case RM_GOT: type = R_X86_64_GOT32; break;
    default:
      if (f->pcrel) {
        type = f->size == 4 ? R_X86_64_PC32 : f->size == 8 ? R_X86_64_PC64 :
               f->size == 2 ? R_X86_64_PC16 : R_X86_64_PC8;
        if (f->is_call && !sym->defined)
          type = R_X86_64_PLT32;
      } else {
        type = f->size == 8 ? R_X86_64_64 : f->size == 4 ? (f->sign32 ? R_X86_64_32S : R_X86_64_32) :
               f->size == 2 ? R_X86_64_16 : R_X86_64_8;
      }
    }
    /* TLS and GOT relocations always name the symbol; others against a
       local symbol use its section. */
    bool keep_sym = e.mod == RM_TPOFF || e.mod == RM_GOTTPOFF || e.mod == RM_TLSGD ||
                    e.mod == RM_GOTPCREL || e.mod == RM_DTPOFF || e.mod == RM_GOT;
    if (local_def && !keep_sym)
      add_reloc(f->sec, P, type, NULL, sym->sec, sym->value + addend);
    else
      add_reloc(f->sec, P, type, sym, NULL, addend);
  }
}

/* ---------------------------------------------------- DWARF for -g */

static void uleb(Buf *b, uint64_t v) {
  do {
    int byte = v & 0x7f;
    v >>= 7;
    if (v)
      byte |= 0x80;
    buf_putc(b, byte);
  } while (v);
}

static void sleb(Buf *b, int64_t v) {
  for (;;) {
    int byte = v & 0x7f;
    v >>= 7;
    bool done = (v == 0 && !(byte & 0x40)) || (v == -1 && (byte & 0x40));
    if (!done)
      byte |= 0x80;
    buf_putc(b, byte);
    if (done)
      return;
  }
}

static void b16(Buf *b, uint16_t v) { buf_putc(b, v & 0xff); buf_putc(b, v >> 8); }
static void b32(Buf *b, uint32_t v) { b16(b, v & 0xffff); b16(b, v >> 16); }
static void b64(Buf *b, uint64_t v) { b32(b, (uint32_t)v); b32(b, (uint32_t)(v >> 32)); }

/* .debug_line (version 4), .debug_abbrev and a compile-unit entry in
   .debug_info, so that debuggers find the line table. */
static void make_debug_info(void) {
  Section *text = NULL;
  for (int i = 0; i < nlines; i++)
    if (!text)
      text = lines[i].sec;
  if (!text)
    return;

  Section *dl = get_section(".debug_line", SHT_PROGBITS, 0);
  Buf *b = &dl->data;
  size_t unit_start = b->len;
  b32(b, 0);              /* unit_length, patched */
  b16(b, 4);              /* version */
  size_t hlen_pos = b->len;
  b32(b, 0);              /* header_length, patched */
  size_t hstart = b->len;
  buf_putc(b, 1);         /* minimum_instruction_length */
  buf_putc(b, 1);         /* maximum_operations_per_instruction */
  buf_putc(b, 1);         /* default_is_stmt */
  buf_putc(b, (unsigned char)-5);  /* line_base */
  buf_putc(b, 14);        /* line_range */
  buf_putc(b, 13);        /* opcode_base */
  static unsigned char std_lengths[12] = {0, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1};
  for (int i = 0; i < 12; i++)
    buf_putc(b, std_lengths[i]);
  buf_putc(b, 0);         /* include_directories: none */
  for (int i = 1; i < nfiles; i++) {
    char *n = files[i] ? files[i] : "";
    buf_write(b, n, strlen(n) + 1);
    uleb(b, 0);
    uleb(b, 0);
    uleb(b, 0);
  }
  buf_putc(b, 0);
  wr32((unsigned char *)b->buf + hlen_pos, b->len - hstart);

  /* line program for the text section */
  buf_putc(b, 0);
  uleb(b, 9);
  buf_putc(b, 2);         /* DW_LNE_set_address */
  add_reloc(dl, b->len, R_X86_64_64, NULL, text, 0);
  b64(b, 0);
  int64_t addr = 0;
  int line = 1, file = 1;
  for (int i = 0; i < nlines; i++) {
    LineEntry *le = &lines[i];
    if (le->sec != text)
      continue;
    if (le->file != file) {
      buf_putc(b, 4);     /* DW_LNS_set_file */
      uleb(b, le->file);
      file = le->file;
    }
    if (le->offset != addr) {
      buf_putc(b, 2);     /* DW_LNS_advance_pc */
      uleb(b, le->offset - addr);
      addr = le->offset;
    }
    if (le->line != line) {
      buf_putc(b, 3);     /* DW_LNS_advance_line */
      sleb(b, le->line - line);
      line = le->line;
    }
    buf_putc(b, 1);       /* DW_LNS_copy */
  }
  int64_t end = text->data.len;
  if (end > addr) {
    buf_putc(b, 2);
    uleb(b, end - addr);
  }
  buf_putc(b, 0);
  uleb(b, 1);
  buf_putc(b, 1);         /* DW_LNE_end_sequence */
  wr32((unsigned char *)b->buf + unit_start, b->len - unit_start - 4);

  /* .debug_abbrev: one entry, DW_TAG_compile_unit */
  Section *da = get_section(".debug_abbrev", SHT_PROGBITS, 0);
  Buf *a = &da->data;
  uleb(a, 1);
  uleb(a, 0x11);          /* DW_TAG_compile_unit */
  buf_putc(a, 0);         /* no children */
  uleb(a, 0x10); uleb(a, 0x17);  /* DW_AT_stmt_list, DW_FORM_sec_offset */
  uleb(a, 0x11); uleb(a, 0x01);  /* DW_AT_low_pc, DW_FORM_addr */
  uleb(a, 0x12); uleb(a, 0x07);  /* DW_AT_high_pc, DW_FORM_data8 */
  uleb(a, 0x03); uleb(a, 0x08);  /* DW_AT_name, DW_FORM_string */
  uleb(a, 0x25); uleb(a, 0x08);  /* DW_AT_producer, DW_FORM_string */
  uleb(a, 0x13); uleb(a, 0x05);  /* DW_AT_language, DW_FORM_data2 */
  uleb(a, 0); uleb(a, 0);
  uleb(a, 0);

  Section *di = get_section(".debug_info", SHT_PROGBITS, 0);
  Buf *d = &di->data;
  size_t ustart = d->len;
  b32(d, 0);
  b16(d, 4);              /* version */
  add_reloc(di, d->len, R_X86_64_32, NULL, da, 0);
  b32(d, 0);              /* debug_abbrev_offset */
  buf_putc(d, 8);         /* address_size */
  uleb(d, 1);
  add_reloc(di, d->len, R_X86_64_32, NULL, dl, 0);
  b32(d, 0);              /* stmt_list */
  add_reloc(di, d->len, R_X86_64_64, NULL, text, 0);
  b64(d, 0);              /* low_pc */
  b64(d, text->data.len); /* high_pc (length) */
  char *nm = main_file ? main_file : (nfiles > 1 && files[1] ? files[1] : filename);
  buf_write(d, nm, strlen(nm) + 1);
  char *prod = "c17 " C17_VERSION;
  buf_write(d, prod, strlen(prod) + 1);
  b16(d, 0x0c);           /* DW_LANG_C99 (no C11/C17 code in DWARF 4) */
  wr32((unsigned char *)d->buf + ustart, d->len - ustart - 4);
}

/* ------------------------------------------------------ object file */

static void put_sym(Buf *b, uint32_t name, int info, int other, uint16_t shndx, uint64_t value,
                    uint64_t size) {
  b32(b, name);
  buf_putc(b, info);
  buf_putc(b, other);
  b16(b, shndx);
  b64(b, value);
  b64(b, size);
}

static uint32_t add_str(Buf *strtab, char *s) {
  uint32_t off = strtab->len;
  buf_write(strtab, s, strlen(s) + 1);
  return off;
}

static int cmp_syms(const void *a, const void *b) {
  Sym *x = *(Sym **)a, *y = *(Sym **)b;
  return strcmp(x->name, y->name);
}

static bool write_object(char *outpath) {
  /* Section indices: 0 null, then content sections, then .rela.*, .symtab,
     .strtab, .shstrtab */
  int idx = 1;
  for (int i = 0; i < nsections; i++)
    sections[i]->index = idx++;

  /* Symbols: null, sections, file, locals, then globals */
  Buf symtab = {0}, strtab = {0};
  buf_putc(&strtab, 0);
  put_sym(&symtab, 0, 0, 0, 0, 0, 0);
  int nsym = 1;
  if (main_file) {
    put_sym(&symtab, add_str(&strtab, main_file), STT_FILE, 0, SHN_ABS, 0, 0);
    nsym++;
  }
  for (int i = 0; i < nsections; i++) {
    sections[i]->symidx = nsym++;
    put_sym(&symtab, 0, STT_SECTION, 0, sections[i]->index, 0, 0);
  }

  /* Collect named symbols */
  Sym **all = NULL;
  int nall = 0;
  for (int i = 0; i < syms.capacity; i++) {
    HashEntry *ent = &syms.buckets[i];
    if (!ent->key || ent->key == (void *)-1)
      continue;
    Sym *s = ent->val;
    all = xrealloc(all, sizeof(Sym *) * (nall + 1));
    all[nall++] = s;
  }
  qsort(all, nall, sizeof(Sym *), cmp_syms);

  for (int pass = 0; pass < 2; pass++) {
    for (int i = 0; i < nall; i++) {
      Sym *s = all[i];
      int64_t add = 0;
      Sym *t = s->set_expr ? resolve_alias(s, &add) : s;
      bool global = s->global || s->weak;
      if (is_temp(s) && !global)
        continue;
      if (!global && !s->defined && !s->set_expr)
        continue;  /* referenced locally but undefined: becomes global undef */
      if (pass == 0 && global)
        continue;
      if (pass == 1 && !global)
        continue;
      if (!s->defined && !t->defined && !global && !s->used)
        continue;
      if (s->set_expr && t != s) {
        /* alias of another symbol */
        s->defined = t->defined;
        s->sec = t->sec;
        s->value = t->value + add;
        s->is_abs = t->is_abs;
        if (!s->type)
          s->type = t->type;
      }
      int bind = s->weak ? STB_WEAK : global ? STB_GLOBAL : STB_LOCAL;
      uint16_t shndx = s->is_common ? SHN_COMMON : s->is_abs ? SHN_ABS :
                       s->defined && s->sec ? s->sec->index : SHN_UNDEF;
      uint64_t value = s->is_common ? s->common_align : s->value;
      int64_t size = 0;
      if (s->size_expr) {
        int64_t v;
        if (const_value(s->size_expr, &v))
          size = v;
      } else if (s->has_size) {
        size = s->size;
      }
      int type = s->type;
      if (s->defined && s->sec && (s->sec->flags & SHF_TLS) && type == STT_NOTYPE)
        type = STT_TLS;
      s->index = nsym++;
      put_sym(&symtab, add_str(&strtab, s->name), bind << 4 | type, s->vis, shndx, value, size);
    }
  }
  int first_global = 0;
  /* Count locals: symbols with index < first global */
  {
    int n = 1 + (main_file ? 1 : 0) + nsections;
    for (int i = 0; i < nall; i++) {
      Sym *s = all[i];
      bool global = s->global || s->weak;
      if (!global && s->index)
        n++;
    }
    first_global = n;
  }
  /* Undefined symbols referenced but not declared global become global */
  for (int i = 0; i < nall; i++) {
    Sym *s = all[i];
    if (s->index || is_temp(s) || s->global || s->weak || s->set_expr)
      continue;
    if (!s->defined && s->used) {
      s->index = nsym++;
      put_sym(&symtab, add_str(&strtab, s->name), STB_GLOBAL << 4 | s->type, s->vis, SHN_UNDEF, 0, 0);
    }
  }
  for (int i = 0; i < nall; i++) {
    Sym *s = all[i];
    if (is_temp(s) && !s->defined && s->used && !s->set_expr) {
      lineno = 0;
      asm_error("undefined local symbol '%s'", s->name[0] == '\002' ? "numeric label" : s->name);
    }
  }
  if (errors)
    return false;

  /* Relocation sections */
  int nrelsec = 0;
  for (int i = 0; i < nsections; i++)
    if (sections[i]->nrel)
      nrelsec++;
  int symtab_idx = idx + nrelsec;
  int strtab_idx = symtab_idx + 1;
  int shstrtab_idx = strtab_idx + 1;
  int nshdr = shstrtab_idx + 1;

  Buf shstr = {0};
  buf_putc(&shstr, 0);

  /* Layout */
  Buf out = {0};
  for (int i = 0; i < EHDR_SIZE; i++)
    buf_putc(&out, 0);
  uint64_t *sec_off = xcalloc(nshdr, sizeof(uint64_t));
  uint64_t *sec_size = xcalloc(nshdr, sizeof(uint64_t));
  for (int i = 0; i < nsections; i++) {
    Section *s = sections[i];
    while (out.len % (s->align ? s->align : 1))
      buf_putc(&out, 0);
    sec_off[s->index] = out.len;
    if (s->type == SHT_NOBITS) {
      sec_size[s->index] = s->size;
    } else {
      buf_write(&out, s->data.buf ? s->data.buf : "", s->data.len);
      sec_size[s->index] = s->data.len;
    }
  }
  int ri = idx;
  int *rel_index = xcalloc(nsections, sizeof(int));
  for (int i = 0; i < nsections; i++) {
    Section *s = sections[i];
    if (!s->nrel)
      continue;
    while (out.len % 8)
      buf_putc(&out, 0);
    rel_index[i] = ri;
    sec_off[ri] = out.len;
    for (int k = 0; k < s->nrel; k++) {
      Reloc *r = &s->rel[k];
      int symi = r->sym ? r->sym->index : r->sec->symidx;
      if (r->sym && !r->sym->index) {
        /* a symbol only referenced via relocations (e.g. undefined) */
        r->sym->index = nsym++;
        put_sym(&symtab, add_str(&strtab, r->sym->name), STB_GLOBAL << 4 | r->sym->type, 0,
                SHN_UNDEF, 0, 0);
        symi = r->sym->index;
      }
      b64(&out, r->offset);
      b64(&out, (uint64_t)symi << 32 | (uint32_t)r->type);
      b64(&out, (uint64_t)r->addend);
    }
    sec_size[ri] = (uint64_t)s->nrel * RELA_SIZE;
    ri++;
  }
  while (out.len % 8)
    buf_putc(&out, 0);
  sec_off[symtab_idx] = out.len;
  buf_write(&out, symtab.buf, symtab.len);
  sec_size[symtab_idx] = symtab.len;
  sec_off[strtab_idx] = out.len;
  buf_write(&out, strtab.buf, strtab.len);
  sec_size[strtab_idx] = strtab.len;

  /* section names */
  uint32_t *name_off = xcalloc(nshdr, sizeof(uint32_t));
  for (int i = 0; i < nsections; i++)
    name_off[sections[i]->index] = add_str(&shstr, sections[i]->name);
  for (int i = 0; i < nsections; i++)
    if (sections[i]->nrel)
      name_off[rel_index[i]] = add_str(&shstr, format(".rela%s", sections[i]->name));
  name_off[symtab_idx] = add_str(&shstr, ".symtab");
  name_off[strtab_idx] = add_str(&shstr, ".strtab");
  name_off[shstrtab_idx] = add_str(&shstr, ".shstrtab");
  sec_off[shstrtab_idx] = out.len;
  buf_write(&out, shstr.buf, shstr.len);
  sec_size[shstrtab_idx] = shstr.len;

  while (out.len % 8)
    buf_putc(&out, 0);
  uint64_t shoff = out.len;
  /* section headers */
  for (int i = 0; i < nshdr; i++) {
    uint32_t type = 0;
    uint64_t flags = 0, align = 0, entsize = 0;
    uint32_t link = 0, info = 0;
    if (i == 0) {
      for (int k = 0; k < SHDR_SIZE; k++)
        buf_putc(&out, 0);
      continue;
    }
    if (i < idx) {
      Section *s = sections[i - 1];
      type = s->type;
      flags = s->flags;
      align = s->align;
    } else if (i < symtab_idx) {
      type = SHT_RELA;
      flags = SHF_INFO_LINK;
      align = 8;
      entsize = RELA_SIZE;
      link = symtab_idx;
      for (int k = 0; k < nsections; k++)
        if (rel_index[k] == i)
          info = sections[k]->index;
    } else if (i == symtab_idx) {
      type = SHT_SYMTAB;
      align = 8;
      entsize = SYM_SIZE;
      link = strtab_idx;
      info = first_global;
    } else {
      type = SHT_STRTAB;
      align = 1;
    }
    b32(&out, name_off[i]);
    b32(&out, type);
    b64(&out, flags);
    b64(&out, 0);
    b64(&out, sec_off[i]);
    b64(&out, sec_size[i]);
    b32(&out, link);
    b32(&out, info);
    b64(&out, align);
    b64(&out, entsize);
  }

  /* ELF header */
  unsigned char *h = (unsigned char *)out.buf;
  memcpy(h, "\177ELF", 4);
  h[4] = 2;  /* ELFCLASS64 */
  h[5] = 1;  /* ELFDATA2LSB */
  h[6] = 1;  /* EV_CURRENT */
  h[7] = 0;  /* ELFOSABI_NONE */
  wr16(h + 16, ET_REL);
  wr16(h + 18, EM_X86_64);
  wr32(h + 20, 1);
  wr64(h + 24, 0);
  wr64(h + 32, 0);
  wr64(h + 40, shoff);
  wr32(h + 48, 0);
  wr16(h + 52, EHDR_SIZE);
  wr16(h + 54, 0);
  wr16(h + 56, 0);
  wr16(h + 58, SHDR_SIZE);
  wr16(h + 60, nshdr);
  wr16(h + 62, shstrtab_idx);

  FILE *fp = fopen(outpath, "w");
  if (!fp) {
    fprintf(stderr, "%s: cannot create %s: %s\n", progname, outpath, strerror(errno));
    return false;
  }
  if (fwrite(out.buf, 1, out.len, fp) != out.len || fclose(fp) != 0) {
    fprintf(stderr, "%s: cannot write %s: %s\n", progname, outpath, strerror(errno));
    return false;
  }
  return true;
}

/* Assembles src into an ELF relocatable object at outpath. */
bool assemble(char *src, char *fname, char *outpath, bool debug) {
  filename = fname;
  syms = (HashMap){0};
  sections = NULL;
  nsections = 0;
  fixups = NULL;
  nfixups = capfixups = 0;
  errors = false;
  memset(numlabel_count, 0, sizeof(numlabel_count));
  lines = NULL;
  nlines = caplines = 0;
  files = NULL;
  nfiles = 0;
  main_file = NULL;
  code32 = false;
  nmacros = 0;
  macro_def = NULL;

  named_section(".text");
  named_section(".data");
  named_section(".bss");
  cur = sections[0];
  prev_sec = NULL;

  assemble_source(src);
  if (macro_def)
    asm_error("missing .endm");
  if (errors)
    return false;
  if (debug)
    make_debug_info();
  resolve_fixups();
  if (errors)
    return false;
  return write_object(outpath);
}
