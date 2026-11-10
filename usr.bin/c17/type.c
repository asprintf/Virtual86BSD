/* type.c - the C type system (6.2.5, 6.2.7, 6.3) and expression typing */
#include "c17.h"

static Type *new_type(TypeKind kind, int64_t size, int align) {
  Type *ty = xcalloc(1, sizeof(Type));
  ty->kind = kind;
  ty->size = size;
  ty->align = align;
  return ty;
}

static Type ty_void_s = {TY_VOID, 1, 1};
static Type ty_bool_s = {TY_BOOL, 1, 1, true};
static Type ty_char_s = {TY_CHAR, 1, 1, false, true};
static Type ty_schar_s = {TY_CHAR, 1, 1};
static Type ty_uchar_s = {TY_CHAR, 1, 1, true};
static Type ty_short_s = {TY_SHORT, 2, 2};
static Type ty_ushort_s = {TY_SHORT, 2, 2, true};
static Type ty_int_s = {TY_INT, 4, 4};
static Type ty_uint_s = {TY_INT, 4, 4, true};
static Type ty_long_s = {TY_LONG, 8, 8};
static Type ty_ulong_s = {TY_LONG, 8, 8, true};
static Type ty_llong_s = {TY_LLONG, 8, 8};
static Type ty_ullong_s = {TY_LLONG, 8, 8, true};
static Type ty_float_s = {TY_FLOAT, 4, 4};
static Type ty_double_s = {TY_DOUBLE, 8, 8};
static Type ty_ldouble_s = {TY_LDOUBLE, 16, 16};
static Type ty_cfloat_s = {TY_FLOAT, 8, 4, false, false, true};
static Type ty_cdouble_s = {TY_DOUBLE, 16, 8, false, false, true};
static Type ty_cldouble_s = {TY_LDOUBLE, 32, 16, false, false, true};

Type *ty_void = &ty_void_s;
Type *ty_bool = &ty_bool_s;
Type *ty_char = &ty_char_s;
Type *ty_schar = &ty_schar_s;
Type *ty_uchar = &ty_uchar_s;
Type *ty_short = &ty_short_s;
Type *ty_ushort = &ty_ushort_s;
Type *ty_int = &ty_int_s;
Type *ty_uint = &ty_uint_s;
Type *ty_long = &ty_long_s;
Type *ty_ulong = &ty_ulong_s;
Type *ty_llong = &ty_llong_s;
Type *ty_ullong = &ty_ullong_s;
Type *ty_float = &ty_float_s;
Type *ty_double = &ty_double_s;
Type *ty_ldouble = &ty_ldouble_s;
Type *ty_cfloat = &ty_cfloat_s;
Type *ty_cdouble = &ty_cdouble_s;
Type *ty_cldouble = &ty_cldouble_s;
Type *ty_wchar = &ty_int_s;
Type *ty_char16 = &ty_ushort_s;
Type *ty_char32 = &ty_uint_s;
Type *ty_size_t = &ty_ulong_s;
Type *ty_ptrdiff_t = &ty_long_s;

bool is_integer(Type *ty) {
  TypeKind k = ty->kind;
  return k == TY_BOOL || k == TY_CHAR || k == TY_SHORT || k == TY_INT ||
         k == TY_LONG || k == TY_LLONG || k == TY_ENUM;
}

bool is_flonum(Type *ty) {
  return (ty->kind == TY_FLOAT || ty->kind == TY_DOUBLE || ty->kind == TY_LDOUBLE) &&
         !ty->is_complex;
}

bool is_real(Type *ty) { return is_integer(ty) || is_flonum(ty); }

bool is_arith(Type *ty) {
  return is_integer(ty) || ty->kind == TY_FLOAT || ty->kind == TY_DOUBLE ||
         ty->kind == TY_LDOUBLE;
}

bool is_scalar(Type *ty) { return is_arith(ty) || ty->kind == TY_PTR; }
bool is_su(Type *ty) { return ty->kind == TY_STRUCT || ty->kind == TY_UNION; }
bool is_array(Type *ty) { return ty->kind == TY_ARRAY || ty->kind == TY_VLA; }
bool is_aggregate(Type *ty) { return is_su(ty) || is_array(ty); }
bool is_ptr(Type *ty) { return ty->kind == TY_PTR; }
bool is_func(Type *ty) { return ty->kind == TY_FUNC; }
bool is_void(Type *ty) { return ty->kind == TY_VOID; }

Type *su_def(Type *ty) {
  if (is_su(ty) && ty->tag_type)
    return ty->tag_type;
  return ty;
}

Member *struct_members(Type *ty) { return su_def(ty)->members; }

bool is_complete(Type *ty) {
  switch (ty->kind) {
  case TY_VOID:
    return false;
  case TY_ARRAY:
    return ty->array_len >= 0 && is_complete(ty->base);
  case TY_VLA:
    return true;
  case TY_STRUCT:
  case TY_UNION:
    return su_def(ty)->is_complete;
  case TY_ENUM:
    return ty->is_complete || (ty->tag_type && ty->tag_type->is_complete);
  case TY_FUNC:
    return false;
  default:
    return true;
  }
}

bool is_object_complete(Type *ty) { return ty->kind != TY_FUNC && is_complete(ty); }

int64_t type_size(Type *ty) {
  if (is_su(ty))
    return su_def(ty)->size;
  return ty->size;
}

int type_align(Type *ty) {
  if (is_su(ty))
    return su_def(ty)->align;
  return ty->align;
}

/* Variably modified (6.7.6p3) */
bool is_vm(Type *ty) {
  for (; ty; ty = ty->base) {
    if (ty->kind == TY_VLA)
      return true;
    if (ty->kind == TY_FUNC)
      return false;
    if (ty->kind != TY_PTR && ty->kind != TY_ARRAY)
      return false;
  }
  return false;
}

Type *copy_type(Type *ty) {
  Type *ret = xmalloc(sizeof(Type));
  *ret = *ty;
  ret->origin = ty;
  return ret;
}

/* Returns ty with the given qualifiers added.  Qualifying an array type
   qualifies its element type (6.7.3p10). */
Type *qualify(Type *ty, int qual) {
  if (!qual || (ty->qual & qual) == qual)
    return ty;
  if (ty->kind == TY_ARRAY) {
    Type *t = array_of(qualify(ty->base, qual), ty->array_len);
    t->array_static = ty->array_static;
    return t;
  }
  if (ty->kind == TY_VLA) {
    Type *t = vla_of(qualify(ty->base, qual), ty->vla_len);
    t->vla_size = ty->vla_size;
    return t;
  }
  int q = ty->qual | qual;
  if (is_su(ty)) {
    Type *canon = su_def(ty);
    if (!canon->variants)
      canon->variants = xcalloc(16, sizeof(Type *));
    if (!canon->variants[q]) {
      Type *t = copy_type(canon);
      t->qual = q;
      t->variants = NULL;
      t->tag_type = canon;
      canon->variants[q] = t;
    }
    return canon->variants[q];
  }
  Type *t = copy_type(ty);
  t->qual = q;
  return t;
}

Type *unqual(Type *ty) {
  if (is_su(ty))
    return su_def(ty);
  if (!ty->qual)
    return ty;
  Type *t = copy_type(ty);
  t->qual = 0;
  return t;
}

Type *pointer_to(Type *base) {
  Type *ty = new_type(TY_PTR, 8, 8);
  ty->base = base;
  ty->is_unsigned = true;
  return ty;
}

Type *func_type(Type *return_ty) {
  Type *ty = new_type(TY_FUNC, 1, 1);
  ty->return_ty = return_ty;
  return ty;
}

Type *array_of(Type *base, int64_t len) {
  int64_t esz = type_size(base);
  Type *ty = new_type(TY_ARRAY, len < 0 || esz < 0 ? -1 : esz * len, type_align(base));
  ty->base = base;
  ty->array_len = len;
  return ty;
}

Type *vla_of(Type *base, Node *len) {
  Type *ty = new_type(TY_VLA, 8, 8);
  ty->base = base;
  ty->vla_len = len;
  ty->align = type_align(base);
  return ty;
}

Type *enum_type(void) {
  Type *ty = new_type(TY_ENUM, 4, 4);
  ty->is_unsigned = true;  /* until a negative enumerator is seen */
  ty->enum_int = ty_uint;
  return ty;
}

Type *struct_type(void) {
  Type *ty = new_type(TY_STRUCT, -1, 1);
  ty->tag_type = ty;
  return ty;
}

Type *complex_of(Type *real) {
  switch (real->kind) {
  case TY_FLOAT: return ty_cfloat;
  case TY_DOUBLE: return ty_cdouble;
  case TY_LDOUBLE: return ty_cldouble;
  default: return ty_cdouble;
  }
}

Type *complex_base(Type *ty) {
  switch (ty->kind) {
  case TY_FLOAT: return ty_float;
  case TY_DOUBLE: return ty_double;
  case TY_LDOUBLE: return ty_ldouble;
  default: return ty;
  }
}

/* Complex types are laid out as struct { T re, im; } (6.2.5p13); give
   them synthetic members so the ABI classification code can treat them
   like that struct. */
static Member cx_mem[3][2];

void init_types(void) {
  Type *cx[3] = {ty_cfloat, ty_cdouble, ty_cldouble};
  Type *re[3] = {ty_float, ty_double, ty_ldouble};
  for (int i = 0; i < 3; i++) {
    cx_mem[i][0].ty = re[i];
    cx_mem[i][0].offset = 0;
    cx_mem[i][0].align = re[i]->align;
    cx_mem[i][0].next = &cx_mem[i][1];
    cx_mem[i][1].ty = re[i];
    cx_mem[i][1].offset = re[i]->size;
    cx_mem[i][1].align = re[i]->align;
    cx_mem[i][1].idx = 1;
    cx[i]->members = &cx_mem[i][0];
  }
}

Type *imaginary_type_hook(Type *ty) {
  return complex_of(ty);
}

/* Array and function types decay to pointers (6.3.2.1). */
Type *decay(Type *ty) {
  if (is_array(ty))
    return pointer_to(ty->base);
  if (ty->kind == TY_FUNC)
    return pointer_to(ty);
  return ty;
}

/* Integer conversion rank (6.3.1.1) */
int int_rank(Type *ty) {
  switch (ty->kind) {
  case TY_BOOL: return 1;
  case TY_CHAR: return 2;
  case TY_SHORT: return 3;
  case TY_INT: return 4;
  case TY_ENUM: return 4;
  case TY_LONG: return 5;
  case TY_LLONG: return 6;
  default: return 0;
  }
}

/* Integer promotions (6.3.1.1p2) */
Type *int_promote(Type *ty) {
  if (!is_integer(ty))
    return ty;
  if (int_rank(ty) < 4)
    return ty_int;  /* all of bool/char/short fit in int */
  if (ty->kind == TY_ENUM)
    return ty->is_unsigned ? ty_uint : ty_int;
  return unqual(ty);
}

static Type *canonical_int(Type *ty) {
  if (ty->kind == TY_ENUM)
    return ty->is_unsigned ? ty_uint : ty_int;
  return ty;
}

/* Usual arithmetic conversions (6.3.1.8) on real types; complex types
   take the floating domain of the wider real type. */
Type *usual_arith_conv(Type *a, Type *b) {
  bool cplx = a->is_complex || b->is_complex;
  if (a->kind == TY_LDOUBLE || b->kind == TY_LDOUBLE)
    return cplx ? ty_cldouble : ty_ldouble;
  if (a->kind == TY_DOUBLE || b->kind == TY_DOUBLE)
    return cplx ? ty_cdouble : ty_double;
  if (a->kind == TY_FLOAT || b->kind == TY_FLOAT)
    return cplx ? ty_cfloat : ty_float;

  a = canonical_int(int_promote(a));
  b = canonical_int(int_promote(b));
  if (a->kind == b->kind && a->is_unsigned == b->is_unsigned)
    return unqual(a);
  if (a->is_unsigned == b->is_unsigned)
    return int_rank(a) > int_rank(b) ? unqual(a) : unqual(b);
  Type *u = a->is_unsigned ? a : b;
  Type *s = a->is_unsigned ? b : a;
  if (int_rank(u) >= int_rank(s))
    return unqual(u);
  if (s->size > u->size)
    return unqual(s);
  /* signed type's unsigned counterpart */
  switch (s->kind) {
  case TY_LONG: return ty_ulong;
  case TY_LLONG: return ty_ullong;
  default: return ty_uint;
  }
}

/* --------------------------------------------------- compatibility */

static bool params_compatible_with_promotions(Type *fn) {
  if (fn->is_variadic)
    return false;
  for (Param *p = fn->params; p; p = p->next) {
    Type *t = unqual(p->ty);
    if (t->kind == TY_FLOAT || (int_rank(t) && int_rank(t) < 4))
      return false;
  }
  return true;
}

static bool compat(Type *t1, Type *t2, bool check_qual) {
  if (t1 == t2)
    return true;
  if (check_qual && (t1->qual & ~Q_RESTRICT) != (t2->qual & ~Q_RESTRICT))
    return false;
  if (check_qual && (t1->qual & Q_RESTRICT) != (t2->qual & Q_RESTRICT))
    return false;

  /* An enumerated type is compatible with its underlying integer type. */
  if (t1->kind == TY_ENUM && t2->kind != TY_ENUM && is_integer(t2))
    return t2->kind == TY_INT && t1->is_unsigned == t2->is_unsigned;
  if (t2->kind == TY_ENUM && t1->kind != TY_ENUM && is_integer(t1))
    return t1->kind == TY_INT && t1->is_unsigned == t2->is_unsigned;

  if (t1->kind != t2->kind) {
    if (is_array(t1) && is_array(t2))
      return compat(t1->base, t2->base, true);
    return false;
  }

  switch (t1->kind) {
  case TY_VOID:
    return true;
  case TY_CHAR:
    return t1->is_unsigned == t2->is_unsigned && t1->is_plain_char == t2->is_plain_char;
  case TY_BOOL:
  case TY_SHORT:
  case TY_INT:
  case TY_LONG:
  case TY_LLONG:
    return t1->is_unsigned == t2->is_unsigned;
  case TY_FLOAT:
  case TY_DOUBLE:
  case TY_LDOUBLE:
    return t1->is_complex == t2->is_complex;
  case TY_ENUM:
    return (t1->tag_type ? t1->tag_type : t1) == (t2->tag_type ? t2->tag_type : t2) ||
           (t1->origin && t1->origin == t2->origin);
  case TY_PTR:
    return compat(t1->base, t2->base, true);
  case TY_ARRAY:
    if (!compat(t1->base, t2->base, true))
      return false;
    return t1->array_len < 0 || t2->array_len < 0 || t1->array_len == t2->array_len;
  case TY_VLA:
    return compat(t1->base, t2->base, true);
  case TY_STRUCT:
  case TY_UNION:
    return su_def(t1) == su_def(t2);
  case TY_FUNC: {
    if (!compat(t1->return_ty, t2->return_ty, true))
      return false;
    if (t1->has_proto && t2->has_proto) {
      if (t1->is_variadic != t2->is_variadic)
        return false;
      Param *p1 = t1->params, *p2 = t2->params;
      for (; p1 && p2; p1 = p1->next, p2 = p2->next)
        if (!compat(unqual(p1->ty), unqual(p2->ty), true))
          return false;
      return p1 == NULL && p2 == NULL;
    }
    if (t1->has_proto)
      return params_compatible_with_promotions(t1);
    if (t2->has_proto)
      return params_compatible_with_promotions(t2);
    return true;
  }
  default:
    return false;
  }
}

bool is_compatible(Type *t1, Type *t2) { return compat(t1, t2, true); }

bool is_compatible_unqual(Type *t1, Type *t2) {
  return compat(unqual(t1), unqual(t2), true);
}

/* Composite type (6.2.7p3); assumes the types are compatible. */
Type *composite_type(Type *t1, Type *t2) {
  if (t1 == t2)
    return t1;
  if (t1->kind == TY_ARRAY && t2->kind == TY_ARRAY) {
    Type *base = composite_type(t1->base, t2->base);
    int64_t len = t1->array_len >= 0 ? t1->array_len : t2->array_len;
    if (base == t1->base && len == t1->array_len)
      return t1;
    return array_of(base, len);
  }
  if (t1->kind == TY_VLA)
    return t1;
  if (t2->kind == TY_VLA)
    return t2;
  if (t1->kind == TY_PTR && t2->kind == TY_PTR) {
    Type *base = composite_type(t1->base, t2->base);
    if (base == t1->base)
      return t1;
    Type *t = pointer_to(base);
    t->qual = t1->qual;
    return t;
  }
  if (t1->kind == TY_FUNC && t2->kind == TY_FUNC) {
    if (!t1->has_proto && t2->has_proto)
      return t2;
    if (!t2->has_proto)
      return t1;
    /* Both prototypes: compose parameter types. */
    Type *t = copy_type(t1);
    t->return_ty = composite_type(t1->return_ty, t2->return_ty);
    Param head = {0}, *cur = &head;
    Param *p1 = t1->params, *p2 = t2->params;
    for (; p1 && p2; p1 = p1->next, p2 = p2->next) {
      Param *p = xcalloc(1, sizeof(Param));
      *p = *p1;
      p->next = NULL;
      p->ty = composite_type(p1->ty, p2->ty);
      cur = cur->next = p;
    }
    t->params = head.next;
    t->is_noreturn = t1->is_noreturn || t2->is_noreturn;
    return t;
  }
  return t1;
}

/* --------------------------------------------------------- names */

static void type_name2(Buf *b, Type *ty, char *inner);

static void quals(Buf *b, int q) {
  if (q & Q_CONST) buf_puts(b, "const ");
  if (q & Q_VOLATILE) buf_puts(b, "volatile ");
  if (q & Q_RESTRICT) buf_puts(b, "restrict ");
  if (q & Q_ATOMIC) buf_puts(b, "_Atomic ");
}

static char *base_name(Type *ty) {
  switch (ty->kind) {
  case TY_VOID: return "void";
  case TY_BOOL: return "_Bool";
  case TY_CHAR: return ty->is_plain_char ? "char" : ty->is_unsigned ? "unsigned char" : "signed char";
  case TY_SHORT: return ty->is_unsigned ? "unsigned short" : "short";
  case TY_INT: return ty->is_unsigned ? "unsigned int" : "int";
  case TY_LONG: return ty->is_unsigned ? "unsigned long" : "long";
  case TY_LLONG: return ty->is_unsigned ? "unsigned long long" : "long long";
  case TY_FLOAT: return ty->is_complex ? "_Complex float" : "float";
  case TY_DOUBLE: return ty->is_complex ? "_Complex double" : "double";
  case TY_LDOUBLE: return ty->is_complex ? "_Complex long double" : "long double";
  case TY_ENUM:
    return ty->tag ? format("enum %.*s", ty->tag->len, ty->tag->loc) : "enum <anonymous>";
  case TY_STRUCT:
  case TY_UNION: {
    Type *d = su_def(ty);
    char *kw = ty->kind == TY_STRUCT ? "struct" : "union";
    return d->tag ? format("%s %.*s", kw, d->tag->len, d->tag->loc) : format("%s <anonymous>", kw);
  }
  default:
    return "?";
  }
}

static void type_name2(Buf *b, Type *ty, char *inner) {
  switch (ty->kind) {
  case TY_PTR: {
    Buf in = {0};
    buf_putc(&in, '*');
    if (ty->qual) {
      Buf q = {0};
      quals(&q, ty->qual);
      q.buf[q.len - 1] = '\0';
      buf_puts(&in, q.buf);
      if (*inner)
        buf_putc(&in, ' ');
    }
    buf_puts(&in, inner);
    if (ty->base->kind == TY_ARRAY || ty->base->kind == TY_VLA || ty->base->kind == TY_FUNC)
      type_name2(b, ty->base, format("(%s)", in.buf));
    else
      type_name2(b, ty->base, in.buf);
    return;
  }
  case TY_ARRAY:
    if (ty->array_len >= 0)
      type_name2(b, ty->base, format("%s[%" PRId64 "]", inner, ty->array_len));
    else
      type_name2(b, ty->base, format("%s[]", inner));
    return;
  case TY_VLA:
    type_name2(b, ty->base, format("%s[*]", inner));
    return;
  case TY_FUNC: {
    Buf p = {0};
    buf_puts(&p, inner);
    buf_putc(&p, '(');
    for (Param *pa = ty->params; pa; pa = pa->next) {
      if (pa != ty->params)
        buf_puts(&p, ", ");
      buf_puts(&p, type_name(pa->ty));
    }
    if (ty->is_variadic)
      buf_puts(&p, ty->params ? ", ..." : "...");
    else if (!ty->params && ty->has_proto)
      buf_puts(&p, "void");
    buf_putc(&p, ')');
    type_name2(b, ty->return_ty, p.buf);
    return;
  }
  default:
    quals(b, ty->qual);
    buf_puts(b, base_name(ty));
    if (*inner) {
      buf_putc(b, ' ');
      buf_puts(b, inner);
    }
  }
}

char *type_name(Type *ty) {
  Buf b = {0};
  type_name2(&b, ty, "");
  return b.buf;
}

/* ---------------------------------------------------- expression types */

static Node *conv(Node *expr, Type *ty) { return new_cast(expr, ty); }

/* Converts both operands of an arithmetic binary operator. */
static void usual_arith_conv_node(Node **lhs, Node **rhs) {
  Type *ty = usual_arith_conv((*lhs)->ty, (*rhs)->ty);
  *lhs = conv(*lhs, ty);
  *rhs = conv(*rhs, ty);
}

void add_type(Node *node) {
  if (!node || node->ty)
    return;

  add_type(node->lhs);
  add_type(node->rhs);
  add_type(node->cond);
  add_type(node->then);
  add_type(node->els);
  add_type(node->init);
  add_type(node->inc);

  for (Node *n = node->body; n; n = n->next)
    add_type(n);
  for (Node *n = node->args; n; n = n->next)
    add_type(n);

  switch (node->kind) {
  case ND_NUM:
    node->ty = ty_int;
    return;
  case ND_ADD:
  case ND_SUB:
  case ND_MUL:
  case ND_DIV:
  case ND_MOD:
  case ND_BITAND:
  case ND_BITOR:
  case ND_BITXOR:
    usual_arith_conv_node(&node->lhs, &node->rhs);
    node->ty = node->lhs->ty;
    return;
  case ND_NEG: {
    Type *ty = int_promote(node->lhs->ty);
    node->lhs = conv(node->lhs, ty);
    node->ty = ty;
    return;
  }
  case ND_ASSIGN:
    node->ty = unqual(node->lhs->ty);
    if (!is_su(node->lhs->ty))
      node->rhs = conv(node->rhs, node->ty);
    return;
  case ND_EQ:
  case ND_NE:
  case ND_LT:
  case ND_LE:
    if (is_arith(node->lhs->ty) && is_arith(node->rhs->ty))
      usual_arith_conv_node(&node->lhs, &node->rhs);
    node->ty = ty_int;
    return;
  case ND_FUNCALL:
    node->ty = ty_long;
    return;
  case ND_NOT:
  case ND_LOGOR:
  case ND_LOGAND:
    node->ty = ty_int;
    return;
  case ND_BITNOT: {
    Type *ty = int_promote(node->lhs->ty);
    node->lhs = conv(node->lhs, ty);
    node->ty = ty;
    return;
  }
  case ND_SHL:
  case ND_SHR:
    node->lhs = conv(node->lhs, int_promote(node->lhs->ty));
    node->rhs = conv(node->rhs, int_promote(node->rhs->ty));
    node->ty = node->lhs->ty;
    return;
  case ND_VAR:
  case ND_VLA_PTR:
    node->ty = node->var->ty;
    return;
  case ND_COND:
    if (is_void(node->then->ty) || is_void(node->els->ty)) {
      node->ty = ty_void;
    } else if (is_arith(node->then->ty) && is_arith(node->els->ty)) {
      usual_arith_conv_node(&node->then, &node->els);
      node->ty = node->then->ty;
    } else {
      node->ty = node->then->ty;
    }
    return;
  case ND_COMMA:
    node->ty = node->rhs->ty;
    return;
  case ND_MEMBER:
    node->ty = node->member->ty;
    return;
  case ND_ADDR:
    node->ty = pointer_to(node->lhs->ty);
    return;
  case ND_DEREF:
    node->ty = node->lhs->ty->base;
    return;
  case ND_STMT_EXPR:
    if (node->body) {
      Node *stmt = node->body;
      while (stmt->next)
        stmt = stmt->next;
      if (stmt->kind == ND_EXPR_STMT) {
        node->ty = stmt->lhs->ty;
        return;
      }
    }
    node->ty = ty_void;
    return;
  default:
    return;
  }
}
