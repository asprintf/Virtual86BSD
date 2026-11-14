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

/* decl.c - declarations (6.7), external definitions (6.9), scopes and
   linkage (6.2.1, 6.2.2) */
#include "parse.h"

Obj *locals;
Obj *globals;
Scope *scope;
Obj *current_fn;
Node *gotos;
Node *labels;
char *brk_label;
char *cont_label;
VlaChain *brk_vla;
VlaChain *cont_vla;
VlaChain *cur_vla;
Node *current_switch;
VlaChain *switch_vla;
HashMap linkage_map;
int scope_counter;
static Token *file_scope_end;

/* ---------------------------------------------------------- scopes */

void enter_scope(void) {
  Scope *sc = xcalloc(1, sizeof(Scope));
  sc->next = scope;
  sc->id = ++scope_counter;
  scope = sc;
}

void leave_scope(void) { scope = scope->next; }

VarScope *find_var(Token *tok) {
  for (Scope *sc = scope; sc; sc = sc->next) {
    VarScope *sc2 = hashmap_get2(&sc->vars, tok->loc, tok->len);
    if (sc2)
      return sc2;
  }
  return NULL;
}

static Type *find_tag(Token *tok, Scope **found) {
  for (Scope *sc = scope; sc; sc = sc->next) {
    Type *ty = hashmap_get2(&sc->tags, tok->loc, tok->len);
    if (ty) {
      if (found)
        *found = sc;
      return ty;
    }
  }
  return NULL;
}

VarScope *push_scope(char *name, Token *tok) {
  VarScope *sc = xcalloc(1, sizeof(VarScope));
  sc->tok = tok;
  hashmap_put(&scope->vars, name, sc);
  return sc;
}

static void push_tag_scope(Token *tok, Type *ty) {
  hashmap_put2(&scope->tags, tok->loc, tok->len, ty);
}

static VarScope *find_var_current(char *name) {
  return hashmap_get(&scope->vars, name);
}

Type *find_typedef(Token *tok) {
  if (tok->kind == TK_IDENT) {
    VarScope *sc = find_var(tok);
    if (sc)
      return sc->type_def;
  }
  return NULL;
}

bool is_typename(Token *tok) {
  static HashMap map;
  if (map.capacity == 0) {
    static char *kw[] = {
        "void", "_Bool", "char", "short", "int", "long", "struct", "union",
        "typedef", "enum", "static", "extern", "_Alignas", "signed",
        "unsigned", "const", "volatile", "auto", "register", "restrict",
        "_Noreturn", "float", "double", "inline", "_Thread_local",
        "_Atomic", "_Complex", "_Imaginary", "__typeof__", "__typeof",
        "__attribute__", "__attribute", "__inline", "__inline__",
        "__restrict", "__restrict__", "__signed", "__signed__", "__const",
        "__const__", "__volatile", "__volatile__", "__extension__",
        "__builtin_va_list", "__thread",
    };
    for (int i = 0; i < (int)(sizeof(kw) / sizeof(*kw)); i++)
      hashmap_put(&map, kw[i], (void *)1);
  }
  if (tok->kind == TK_KEYWORD)
    return hashmap_get2(&map, tok->loc, tok->len);
  return find_typedef(tok);
}

/* ------------------------------------------------------- objects */

char *new_unique_name(void) {
  static int id = 0;
  return format(".L..%d", id++);
}

static Obj *new_var(char *name, Type *ty) {
  Obj *var = xcalloc(1, sizeof(Obj));
  var->name = name;
  var->ty = ty;
  var->align = type_align(ty);
  return var;
}

Obj *new_lvar(char *name, Type *ty, Token *tok) {
  Obj *var = new_var(name, ty);
  var->is_local = true;
  var->tok = tok;
  var->next = locals;
  locals = var;
  if (name && *name)
    push_scope(name, tok)->var = var;
  return var;
}

static Obj *new_gvar(char *name, Type *ty) {
  Obj *var = new_var(name, ty);
  var->asm_name = name;
  var->next = globals;
  var->is_definition = true;
  globals = var;
  return var;
}

Obj *new_anon_gvar(Type *ty) {
  Obj *var = new_gvar(new_unique_name(), ty);
  var->is_static = true;
  return var;
}

Obj *new_string_literal(Token *tok) {
  Obj *var = new_anon_gvar(tok->ty);
  var->init_data = tok->str;
  var->is_string_lit = true;
  var->tok = tok;
  return var;
}

/* ------------------------------------------------- __attribute__ */

/* Attributes are parsed and mostly ignored; aligned/packed/noreturn
   are honored where they matter. */
static int attr_align;
static bool attr_packed;
static bool attr_noreturn;

/* Attributes of the objects and functions being declared: they apply to
   the declarators that follow them in the declaration. */
static struct {
  bool weak, used, tls_ie;
  char *alias, *visibility, *section;
  int ctor, dtor;
} gattr;

static char *attr_string(Token **rest, Token *tok, char *name) {
  tok = skip(tok, "(");
  if (tok->kind != TK_STR)
    error_tok(tok, "attribute '%s' takes a string", name);
  char *s = tok->str;
  *rest = skip(tok->next, ")");
  return s;
}

static int attr_priority(Token **rest, Token *tok) {
  if (!equal(tok, "(")) {
    *rest = tok;
    return 65536;
  }
  Token *t = tok->next;
  int64_t v = const_expr(&t, t);
  if (v < 0 || v > 65535)
    error_tok(tok, "constructor priority must be between 0 and 65535");
  *rest = skip(t, ")");
  return (int)v;
}

static bool attr_is(Token *tok, char *name) {
  return equal(tok, name) || equal(tok, format("__%s__", name));
}

/* Gives a declared object or function the attributes seen so far. */
static void apply_gattr(Obj *v) {
  if (gattr.weak)
    v->is_weak = true;
  if (gattr.used)
    v->attr_used = true;
  if (gattr.alias)
    v->alias = gattr.alias;
  if (gattr.visibility)
    v->visibility = gattr.visibility;
  if (gattr.tls_ie)
    v->tls_ie = true;
  if (gattr.section)
    v->section = gattr.section;
  if (gattr.ctor)
    v->ctor_prio = gattr.ctor;
  if (gattr.dtor)
    v->dtor_prio = gattr.dtor;
}

Token *skip_attributes(Token *tok) {
  for (;;) {
    if (equal(tok, "__attribute__") || equal(tok, "__attribute")) {
      tok = skip(tok->next, "(");
      tok = skip(tok, "(");
      int depth = 0;
      while (!(depth == 0 && equal(tok, ")"))) {
        if (tok->kind == TK_EOF)
          error_tok(tok, "unterminated __attribute__");
        if (depth == 0 && (equal(tok, "packed") || equal(tok, "__packed__")))
          attr_packed = true;
        if (depth == 0 && (equal(tok, "noreturn") || equal(tok, "__noreturn__")))
          attr_noreturn = true;
        if (depth == 0 && attr_is(tok, "weak")) {
          gattr.weak = true;
          tok = tok->next;
          continue;
        }
        if (depth == 0 && attr_is(tok, "used")) {
          gattr.used = true;
          tok = tok->next;
          continue;
        }
        if (depth == 0 && attr_is(tok, "alias")) {
          gattr.alias = attr_string(&tok, tok->next, "alias");
          continue;
        }
        if (depth == 0 && attr_is(tok, "section")) {
          gattr.section = attr_string(&tok, tok->next, "section");
          continue;
        }
        if (depth == 0 && attr_is(tok, "visibility")) {
          char *v = attr_string(&tok, tok->next, "visibility");
          if (strcmp(v, "default") && strcmp(v, "hidden") && strcmp(v, "protected") &&
              strcmp(v, "internal"))
            error_tok(tok, "unknown visibility '%s'", v);
          gattr.visibility = strcmp(v, "default") ? v : NULL;
          continue;
        }
        if (depth == 0 && attr_is(tok, "tls_model")) {
          /* initial-exec (or local-exec): the initial-exec sequence even
             with -G; the dynamic models are the default */
          char *m = attr_string(&tok, tok->next, "tls_model");
          if (strcmp(m, "global-dynamic") && strcmp(m, "local-dynamic") &&
              strcmp(m, "initial-exec") && strcmp(m, "local-exec"))
            error_tok(tok, "unknown TLS model '%s'", m);
          gattr.tls_ie = !strcmp(m, "initial-exec") || !strcmp(m, "local-exec");
          continue;
        }
        if (depth == 0 && attr_is(tok, "constructor")) {
          gattr.ctor = attr_priority(&tok, tok->next);
          continue;
        }
        if (depth == 0 && attr_is(tok, "destructor")) {
          gattr.dtor = attr_priority(&tok, tok->next);
          continue;
        }
        if (depth == 0 && (equal(tok, "aligned") || equal(tok, "__aligned__"))) {
          if (equal(tok->next, "(")) {
            Token *t = tok->next->next;
            attr_align = (int)const_expr(&t, t);
            tok = skip(t, ")");
            continue;
          }
          attr_align = 16;
        }
        if (equal(tok, "("))
          depth++;
        if (equal(tok, ")"))
          depth--;
        tok = tok->next;
      }
      tok = skip(tok, ")");
      tok = skip(tok, ")");
      continue;
    }
    if (equal(tok, "__extension__")) {
      tok = tok->next;
      continue;
    }
    return tok;
  }
}

/* --------------------------------------------------- _Static_assert */

Token *static_assertion(Token *tok) {
  Token *start = tok;
  tok = skip(tok->next, "(");
  Node *e = conditional(&tok, tok);
  int64_t val = ice_value(e, start, "static assertion");
  char *msg = NULL;
  if (equal(tok, ",")) {
    tok = tok->next;
    if (tok->kind != TK_STR)
      error_tok(tok, "expected string literal");
    msg = tok->str;
    tok = tok->next;
  } else {
    /* The one-argument form is a C2x feature. */
    warn_tok(tok, "static_assert with no message is a C2x extension");
  }
  tok = skip(tok, ")");
  tok = skip(tok, ";");
  if (!val)
    error_tok(start, "static assertion failed%s%s%s", msg ? ": \"" : "", msg ? msg : "",
              msg ? "\"" : "");
  return tok;
}

/* ----------------------------------------------- declaration specifiers */

static Type *struct_union_decl(Token **rest, Token *tok, TypeKind kind);
static Type *enum_specifier(Token **rest, Token *tok);
static Type *typeof_specifier(Token **rest, Token *tok);

static int parse_alignas(Token **rest, Token *tok) {
  Token *start = tok;
  tok = skip(tok->next, "(");
  int64_t val;
  if (is_typename(tok)) {
    Type *ty = typename(&tok, tok);
    if (!is_object_complete(ty))
      error_tok(start, "_Alignas applied to an incomplete type");
    val = type_align(ty);
  } else {
    Node *e = conditional(&tok, tok);
    val = ice_value(e, start, "_Alignas");
    if (val < 0 || (val & (val - 1)))
      error_tok(start, "requested alignment is not a non-negative power of 2");
    if (val > (1 << 28))
      error_tok(start, "requested alignment is too large");
  }
  *rest = skip(tok, ")");
  return (int)val;
}

/* 6.7.2: type specifiers in any order, validated against the list of
   permitted multisets. */
Type *declspec(Token **rest, Token *tok, VarAttr *attr) {
  enum {
    VOID = 1 << 0,
    BOOL = 1 << 2,
    CHAR = 1 << 4,
    SHORT = 1 << 6,
    INT = 1 << 8,
    LONG = 1 << 10,
    FLOAT = 1 << 12,
    DOUBLE = 1 << 14,
    OTHER = 1 << 16,
    SIGNED = 1 << 17,
    UNSIGNED = 1 << 18,
    COMPLEX = 1 << 20,
  };

  Type *ty = ty_int;
  int counter = 0;
  int qual = 0;
  Token *start = tok;
  bool has_alignas = false;
  int align = 0;

  while (is_typename(tok)) {
    if (equal(tok, "__attribute__") || equal(tok, "__attribute") || equal(tok, "__extension__")) {
      tok = skip_attributes(tok);
      continue;
    }

    /* Storage-class specifiers */
    if (equal(tok, "typedef") || equal(tok, "static") || equal(tok, "extern") ||
        equal(tok, "inline") || equal(tok, "__inline") || equal(tok, "__inline__") ||
        equal(tok, "_Thread_local") || equal(tok, "__thread") || equal(tok, "_Noreturn") ||
        equal(tok, "auto") || equal(tok, "register")) {
      if (!attr)
        error_tok(tok, "storage class specifier is not allowed in this context");

      bool is_sc = !equal(tok, "inline") && !equal(tok, "__inline") &&
                   !equal(tok, "__inline__") && !equal(tok, "_Noreturn") &&
                   !equal(tok, "_Thread_local") && !equal(tok, "__thread");
      if (is_sc) {
        if (attr->is_typedef || attr->is_static || attr->is_extern || attr->is_auto ||
            attr->is_register)
          error_tok(tok, "multiple storage classes in declaration specifiers");
        attr->storage_tok = tok;
      }

      if (equal(tok, "typedef"))
        attr->is_typedef = true;
      else if (equal(tok, "static"))
        attr->is_static = true;
      else if (equal(tok, "extern"))
        attr->is_extern = true;
      else if (equal(tok, "auto"))
        attr->is_auto = true;
      else if (equal(tok, "register"))
        attr->is_register = true;
      else if (equal(tok, "_Noreturn"))
        attr->is_noreturn = true;
      else if (equal(tok, "_Thread_local") || equal(tok, "__thread")) {
        if (attr->is_tls)
          error_tok(tok, "duplicate '_Thread_local'");
        attr->is_tls = true;
      } else
        attr->is_inline = true;

      if (attr->is_tls && (attr->is_typedef || attr->is_auto || attr->is_register))
        error_tok(tok, "'_Thread_local' used with '%s'",
                  attr->is_typedef ? "typedef" : attr->is_auto ? "auto" : "register");
      tok = tok->next;
      continue;
    }

    /* Type qualifiers */
    if (equal(tok, "const") || equal(tok, "__const") || equal(tok, "__const__")) {
      qual |= Q_CONST;
      tok = tok->next;
      continue;
    }
    if (equal(tok, "volatile") || equal(tok, "__volatile") || equal(tok, "__volatile__")) {
      qual |= Q_VOLATILE;
      tok = tok->next;
      continue;
    }
    if (equal(tok, "restrict") || equal(tok, "__restrict") || equal(tok, "__restrict__")) {
      qual |= Q_RESTRICT;
      tok = tok->next;
      continue;
    }
    if (equal(tok, "_Atomic") && !equal(tok->next, "(")) {
      qual |= Q_ATOMIC;
      tok = tok->next;
      continue;
    }

    if (equal(tok, "_Alignas")) {
      if (!attr)
        error_tok(tok, "_Alignas is not allowed in this context");
      int a = parse_alignas(&tok, tok);
      if (a > align)
        align = a;
      has_alignas = true;
      continue;
    }

    /* Typedef names and tag types: only if no other type specifier. */
    Type *ty2 = find_typedef(tok);
    if (equal(tok, "struct") || equal(tok, "union") || equal(tok, "enum") ||
        equal(tok, "__typeof__") || equal(tok, "__typeof") || equal(tok, "_Atomic") ||
        equal(tok, "__builtin_va_list") || ty2) {
      if (counter)
        break;

      if (equal(tok, "struct")) {
        ty = struct_union_decl(&tok, tok->next, TY_STRUCT);
      } else if (equal(tok, "union")) {
        ty = struct_union_decl(&tok, tok->next, TY_UNION);
      } else if (equal(tok, "enum")) {
        ty = enum_specifier(&tok, tok->next);
      } else if (equal(tok, "__typeof__") || equal(tok, "__typeof")) {
        ty = typeof_specifier(&tok, tok->next);
      } else if (equal(tok, "_Atomic")) {
        /* _Atomic ( type-name ) */
        Token *at = tok;
        tok = skip(tok->next, "(");
        ty = typename(&tok, tok);
        tok = skip(tok, ")");
        if (is_array(ty) || is_func(ty) || ty->qual || ty->kind == TY_VOID)
          error_tok(at, "_Atomic cannot be applied to type '%s'", type_name(ty));
        ty = qualify(ty, Q_ATOMIC);
      } else if (equal(tok, "__builtin_va_list")) {
        /* struct __va_list_tag[1] */
        static Type *va_list_ty;
        if (!va_list_ty) {
          Type *st = struct_type();
          st->size = 24;
          st->align = 8;
          st->is_complete = true;
          va_list_ty = array_of(st, 1);
        }
        ty = va_list_ty;
        tok = tok->next;
      } else {
        ty = ty2;
        tok = tok->next;
      }
      counter += OTHER;
      continue;
    }

    /* Built-in type specifiers */
    if (equal(tok, "void"))
      counter += VOID;
    else if (equal(tok, "_Bool"))
      counter += BOOL;
    else if (equal(tok, "char"))
      counter += CHAR;
    else if (equal(tok, "short"))
      counter += SHORT;
    else if (equal(tok, "int"))
      counter += INT;
    else if (equal(tok, "long"))
      counter += LONG;
    else if (equal(tok, "float"))
      counter += FLOAT;
    else if (equal(tok, "double"))
      counter += DOUBLE;
    else if (equal(tok, "signed") || equal(tok, "__signed") || equal(tok, "__signed__"))
      counter |= (counter & SIGNED) ? (error_tok(tok, "duplicate 'signed'"), 0) : SIGNED;
    else if (equal(tok, "unsigned"))
      counter |= (counter & UNSIGNED) ? (error_tok(tok, "duplicate 'unsigned'"), 0) : UNSIGNED;
    else if (equal(tok, "_Complex"))
      counter |= (counter & COMPLEX) ? (error_tok(tok, "duplicate '_Complex'"), 0) : COMPLEX;
    else if (equal(tok, "_Imaginary"))
      error_tok(tok, "imaginary types are not supported");
    else
      unreachable();

    switch (counter) {
    case VOID: ty = ty_void; break;
    case BOOL: ty = ty_bool; break;
    case CHAR: ty = ty_char; break;
    case SIGNED + CHAR: ty = ty_schar; break;
    case UNSIGNED + CHAR: ty = ty_uchar; break;
    case SHORT:
    case SHORT + INT:
    case SIGNED + SHORT:
    case SIGNED + SHORT + INT: ty = ty_short; break;
    case UNSIGNED + SHORT:
    case UNSIGNED + SHORT + INT: ty = ty_ushort; break;
    case INT:
    case SIGNED:
    case SIGNED + INT: ty = ty_int; break;
    case UNSIGNED:
    case UNSIGNED + INT: ty = ty_uint; break;
    case LONG:
    case LONG + INT:
    case SIGNED + LONG:
    case SIGNED + LONG + INT: ty = ty_long; break;
    case LONG + LONG:
    case LONG + LONG + INT:
    case SIGNED + LONG + LONG:
    case SIGNED + LONG + LONG + INT: ty = ty_llong; break;
    case UNSIGNED + LONG:
    case UNSIGNED + LONG + INT: ty = ty_ulong; break;
    case UNSIGNED + LONG + LONG:
    case UNSIGNED + LONG + LONG + INT: ty = ty_ullong; break;
    case FLOAT: ty = ty_float; break;
    case DOUBLE: ty = ty_double; break;
    case LONG + DOUBLE: ty = ty_ldouble; break;
    case COMPLEX + FLOAT: ty = ty_cfloat; break;
    case COMPLEX + DOUBLE: ty = ty_cdouble; break;
    case COMPLEX + LONG + DOUBLE: ty = ty_cldouble; break;
    default:
      /* _Complex alone or with long is a prefix of a valid set. */
      if (counter == COMPLEX || counter == COMPLEX + LONG)
        break;
      error_tok(tok, "invalid combination of type specifiers");
    }
    tok = tok->next;
  }

  if (counter == COMPLEX || counter == COMPLEX + LONG)
    error_tok(start, counter == COMPLEX ? "_Complex requires a floating type" : "invalid combination of type specifiers");
  if (counter == 0) {
    /* 6.7.2p2: at least one type specifier shall be given. */
    if (tok == start)
      error_tok(tok, "expected declaration specifiers");
    warn_tok(start, "type specifier missing, defaults to 'int'");
  }

  if (attr && has_alignas) {
    attr->align = align;
  }
  if (attr && attr->is_noreturn) {
  }

  if (qual & Q_ATOMIC) {
    if (is_array(ty) || is_func(ty))
      error_tok(start, "_Atomic cannot be applied to type '%s'", type_name(ty));
  }
  if ((qual & Q_RESTRICT) && !(ty->kind == TY_PTR && !is_func(ty->base)))
    error_tok(start, "restrict requires a pointer type");

  ty = qualify(ty, qual);
  *rest = tok;
  return ty;
}

static Type *typeof_specifier(Token **rest, Token *tok) {
  tok = skip(tok, "(");
  Type *ty;
  if (is_typename(tok)) {
    ty = typename(&tok, tok);
  } else {
    Node *node = expr(&tok, tok);
    add_type(node);
    ty = node->ty;
    if (node->kind == ND_MEMBER && node->member->is_bitfield)
      error_tok(tok, "typeof applied to a bit-field");
  }
  *rest = skip(tok, ")");
  return ty;
}

/* ---------------------------------------------------------- declarators */

static Type *func_params(Token **rest, Token *tok, Type *ty, bool *is_oldstyle);

/* type-qualifier-list after '*' or in '[' */
static int read_qualifiers(Token **rest, Token *tok) {
  int q = 0;
  for (;;) {
    tok = skip_attributes(tok);
    if (equal(tok, "const") || equal(tok, "__const") || equal(tok, "__const__"))
      q |= Q_CONST;
    else if (equal(tok, "volatile") || equal(tok, "__volatile") || equal(tok, "__volatile__"))
      q |= Q_VOLATILE;
    else if (equal(tok, "restrict") || equal(tok, "__restrict") || equal(tok, "__restrict__"))
      q |= Q_RESTRICT;
    else if (equal(tok, "_Atomic"))
      q |= Q_ATOMIC;
    else
      break;
    tok = tok->next;
  }
  *rest = tok;
  return q;
}

static Type *pointers(Token **rest, Token *tok, Type *ty) {
  while (consume(&tok, tok, "*")) {
    ty = pointer_to(ty);
    int q = read_qualifiers(&tok, tok);
    if ((q & Q_RESTRICT) && is_func(ty->base))
      error_tok(tok, "restrict requires a pointer to an object type");
    ty = qualify(ty, q);
  }
  *rest = tok;
  return ty;
}

static bool in_param_decl;  /* parsing a parameter's declarator */
static int declarator_depth;

/* array-dimensions: "[" qualifiers static? expr? "]" */
static Type *array_dimensions(Token **rest, Token *tok, Type *ty, bool outermost_param) {
  Token *start = tok;
  bool is_static = false;
  int q = 0;
  for (;;) {
    if (equal(tok, "static")) {
      is_static = true;
      tok = tok->next;
      continue;
    }
    int q2 = read_qualifiers(&tok, tok);
    if (!q2)
      break;
    q |= q2;
  }
  if ((is_static || q) && !outermost_param)
    error_tok(start, "static or type qualifiers in non-parameter array declarator");

  Node *len = NULL;
  bool star = false;
  if (equal(tok, "*") && equal(tok->next, "]")) {
    if (!in_param_decl)
      error_tok(tok, "'[*]' not allowed in other than function prototype scope");
    star = true;
    tok = tok->next;
  } else if (!equal(tok, "]")) {
    len = assign(&tok, tok);
  } else if (is_static) {
    error_tok(tok, "'static' may not be used without an array size");
  }
  tok = skip(tok, "]");

  /* Parse remaining suffixes first: the element type. */
  Type *elem;
  if (equal(tok, "[")) {
    elem = array_dimensions(&tok, tok->next, ty, false);
  } else if (equal(tok, "(")) {
    error_tok(tok, "declaration of array of functions");
  } else {
    elem = ty;
  }
  *rest = tok;

  if (is_func(elem))
    error_tok(start, "declaration of array of functions");
  if (!is_object_complete(elem))
    error_tok(start, "array type has incomplete element type '%s'", type_name(elem));
  if (is_su(elem) && su_def(elem)->is_flexible)
    warn_tok(start, "invalid use of structure with flexible array member");

  Type *arr;
  if (star) {
    arr = vla_of(elem, NULL);
  } else if (!len) {
    arr = array_of(elem, -1);
  } else {
    add_type(len);
    len = rval(len);
    if (!is_integer(len->ty))
      error_tok(start, "size of array has non-integer type '%s'", type_name(len->ty));
    if (is_ice(len) && !is_vm(elem)) {
      int64_t n = eval(len);
      if (n < 0 || (len->ty->is_unsigned && (uint64_t)n > (1ULL << 60)))
        error_tok(start, "size of array is negative");
      if (n == 0)
        warn_tok(start, "zero-size array");
      if (type_size(elem) > 0 && (uint64_t)n > (uint64_t)INT64_MAX / (uint64_t)type_size(elem))
        error_tok(start, "size of array is too large");
      arr = array_of(elem, n);
    } else {
      arr = vla_of(elem, len);
    }
  }
  if (outermost_param) {
    arr->array_static = is_static;
    arr->array_qual = q;
  }
  return arr;
}

static Type *type_suffix(Token **rest, Token *tok, Type *ty, bool *oldstyle) {
  if (equal(tok, "("))
    return func_params(rest, tok->next, ty, oldstyle);
  if (equal(tok, "["))
    return array_dimensions(rest, tok->next, ty, in_param_decl && declarator_depth == 1);
  *rest = tok;
  return ty;
}

/* Parameter list or identifier list. */
static Type *func_params(Token **rest, Token *tok, Type *ty, bool *is_oldstyle) {
  Token *start = tok;
  if (is_func(ty))
    error_tok(start, "function cannot return a function type");
  if (is_array(ty))
    error_tok(start, "function cannot return an array type");

  Type *fn = func_type(ty);

  /* () - no information about parameters */
  if (equal(tok, ")")) {
    *rest = tok->next;
    fn->has_proto = false;
    return fn;
  }

  /* (void) */
  if (equal(tok, "void") && equal(tok->next, ")")) {
    *rest = tok->next->next;
    fn->has_proto = true;
    return fn;
  }

  /* Identifier list (old style) */
  if (tok->kind == TK_IDENT && !find_typedef(tok)) {
    Param head = {0}, *cur = &head;
    while (!equal(tok, ")")) {
      if (cur != &head)
        tok = skip(tok, ",");
      if (tok->kind != TK_IDENT || find_typedef(tok))
        error_tok(tok, "expected identifier");
      for (Param *p = head.next; p; p = p->next)
        if (p->name->len == tok->len && !memcmp(p->name->loc, tok->loc, tok->len))
          error_tok(tok, "redefinition of parameter '%.*s'", tok->len, tok->loc);
      Param *p = xcalloc(1, sizeof(Param));
      p->name = tok;
      p->pos = tok;
      p->ty = NULL;  /* filled in by the declaration list */
      cur = cur->next = p;
      tok = tok->next;
    }
    *rest = tok->next;
    fn->params = head.next;
    fn->has_proto = false;
    if (is_oldstyle)
      *is_oldstyle = true;
    return fn;
  }

  /* Parameter type list in function prototype scope */
  enter_scope();
  scope->is_proto = true;
  Param head = {0}, *cur = &head;
  bool saved_in_param = in_param_decl;
  int saved_depth = declarator_depth;
  while (!equal(tok, ")")) {
    if (cur != &head)
      tok = skip(tok, ",");

    if (equal(tok, "...")) {
      if (cur == &head)
        error_tok(tok, "ISO C requires a named parameter before '...'");
      fn->is_variadic = true;
      tok = skip(tok->next, ")");
      goto done;
    }

    VarAttr attr = {0};
    Token *pstart = tok;
    in_param_decl = false;
    Type *ty2 = declspec(&tok, tok, &attr);
    if (attr.is_typedef || attr.is_static || attr.is_extern || attr.is_auto || attr.is_tls ||
        attr.is_inline || attr.is_noreturn)
      error_tok(attr.storage_tok ? attr.storage_tok : pstart,
                "invalid storage class for parameter");
    if (attr.align)
      error_tok(pstart, "_Alignas cannot be applied to a parameter");
    in_param_decl = true;
    declarator_depth = 0;
    Token *name = NULL, *pos = NULL;
    ty2 = declarator(&tok, tok, ty2, &name, &pos);
    in_param_decl = false;

    Param *p = xcalloc(1, sizeof(Param));
    p->decl_ty = ty2;
    p->name = name;
    p->pos = pos ? pos : pstart;

    /* Adjustments (6.7.6.3p7-8) */
    if (is_array(ty2)) {
      Type *pt = pointer_to(ty2->base);
      pt = qualify(pt, ty2->array_qual);
      ty2 = pt;
    } else if (is_func(ty2)) {
      ty2 = pointer_to(ty2);
    }
    if (is_void(ty2)) {
      if (name || ty2->qual || cur != &head || !equal(tok, ")"))
        error_tok(pstart, "'void' must be the only parameter");
    }
    p->ty = ty2;
    if (name) {
      char *nm = tok_text(name);
      if (find_var_current(nm))
        error_tok(name, "redefinition of parameter '%s'", nm);
      /* in scope for the parameters that follow (int a[n]); becomes the
         parameter's object if this is a definition */
      Obj *v = new_var(nm, ty2);
      v->is_local = true;
      v->tok = name;
      push_scope(nm, name)->var = v;
      p->var = v;
    }
    cur = cur->next = p;
  }
  tok = tok->next;
done:
  in_param_decl = saved_in_param;
  declarator_depth = saved_depth;
  leave_scope();
  fn->params = head.next;
  fn->has_proto = true;
  *rest = tok;
  return fn;
}

/* declarator = pointers ("(" ident ")" | "(" declarator ")" | ident) type-suffix */
static Type *declarator2(Token **rest, Token *tok, Type *ty, Token **name, Token **pos,
                         bool abstract, bool *oldstyle) {
  tok = skip_attributes(tok);
  ty = pointers(&tok, tok, ty);
  tok = skip_attributes(tok);
  declarator_depth++;

  /* Nested declarator "(" declarator ")" - but in an abstract
     declarator, "(" followed by a type name or ")" begins a parameter list. */
  Token *after_paren = equal(tok, "(") ? skip_attributes(tok->next) : NULL;
  bool attr_only = after_paren && after_paren != tok->next;
  if (equal(tok, "(") &&
      !(abstract && ((is_typename(after_paren) && !attr_only) || equal(after_paren, ")"))) &&
      !(in_param_decl && is_typename(after_paren) && !attr_only)) {
    Token *start = tok;
    declarator2(&tok, start->next, ty_int, NULL, NULL, abstract, NULL);
    tok = skip(tok, ")");
    ty = type_suffix(rest, tok, ty, oldstyle);
    declarator_depth = 0;
    Type *r = declarator2(&tok, start->next, ty, name, pos, abstract, oldstyle);
    return r;
  }

  if (tok->kind == TK_IDENT && !abstract) {
    if (name)
      *name = tok;
    if (pos)
      *pos = tok;
    tok = tok->next;
  } else if (tok->kind == TK_IDENT && abstract) {
    /* In a parameter, a declarator may or may not have a name. */
    if (in_param_decl && !find_typedef(tok)) {
      if (name)
        *name = tok;
      if (pos)
        *pos = tok;
      tok = tok->next;
    }
  } else if (!abstract && !in_param_decl) {
    if (tok->kind == TK_KEYWORD)
      error_tok(tok, "expected identifier before '%.*s'", tok->len, tok->loc);
    error_tok(tok, "expected identifier or '('");
  }
  if (pos && !*pos)
    *pos = tok;
  tok = skip_attributes(tok);
  ty = type_suffix(rest, tok, ty, oldstyle);
  tok = *rest;
  if (tok->kind == TK_KEYWORD && (equal(tok, "__asm__") || equal(tok, "__asm"))) {
  }
  return ty;
}

static Token *current_asm_label;

Type *declarator(Token **rest, Token *tok, Type *ty, Token **name, Token **pos) {
  if (name)
    *name = NULL;
  if (pos)
    *pos = NULL;
  int saved = declarator_depth;
  declarator_depth = 0;
  Type *r = declarator2(rest, tok, ty, name, pos, in_param_decl, NULL);
  declarator_depth = saved;
  /* asm label */
  tok = *rest;
  current_asm_label = NULL;
  if (equal(tok, "__asm__") || equal(tok, "__asm")) {
    tok = skip(tok->next, "(");
    if (tok->kind != TK_STR)
      error_tok(tok, "expected string literal");
    current_asm_label = tok;
    tok = skip(tok->next, ")");
  }
  *rest = skip_attributes(tok);
  return r;
}

static bool last_decl_oldstyle;

static Type *declarator_fn(Token **rest, Token *tok, Type *ty, Token **name, Token **pos) {
  *name = *pos = NULL;
  last_decl_oldstyle = false;
  declarator_depth = 0;
  Type *r = declarator2(rest, tok, ty, name, pos, false, &last_decl_oldstyle);
  tok = *rest;
  current_asm_label = NULL;
  if (equal(tok, "__asm__") || equal(tok, "__asm")) {
    tok = skip(tok->next, "(");
    if (tok->kind != TK_STR)
      error_tok(tok, "expected string literal");
    current_asm_label = tok;
    tok = skip(tok->next, ")");
  }
  *rest = skip_attributes(tok);
  return r;
}

Type *abstract_declarator(Token **rest, Token *tok, Type *ty) {
  bool saved = in_param_decl;
  in_param_decl = false;
  int saved_depth = declarator_depth;
  declarator_depth = 0;
  Type *r = declarator2(rest, tok, ty, NULL, NULL, true, NULL);
  in_param_decl = saved;
  declarator_depth = saved_depth;
  return r;
}

Type *typename(Token **rest, Token *tok) {
  Token *start = tok;
  Type *ty = declspec(&tok, tok, NULL);
  ty = abstract_declarator(rest, tok, ty);
  (void)start;
  return ty;
}

/* -------------------------------------------------- struct and union */

static bool is_end(Token *tok) {
  return equal(tok, "}") || (equal(tok, ",") && equal(tok->next, "}"));
}

static void struct_members_parse(Token **rest, Token *tok, Type *ty) {
  Member head = {0};
  Member *cur = &head;
  int idx = 0;

  while (!equal(tok, "}")) {
    if (equal(tok, "_Static_assert")) {
      tok = static_assertion(tok);
      continue;
    }
    if (equal(tok, ";")) {
      warn_tok(tok, "extra semicolon in struct or union");
      tok = tok->next;
      continue;
    }

    VarAttr attr = {0};
    Token *start = tok;
    Type *basety = declspec(&tok, tok, &attr);
    if (attr.is_typedef || attr.is_static || attr.is_extern || attr.is_auto ||
        attr.is_register || attr.is_tls || attr.is_inline || attr.is_noreturn)
      error_tok(start, "storage class specified for a member");

    /* Anonymous struct/union member (6.7.2.1p13) */
    if (is_su(basety) && equal(tok, ";")) {
      Type *d = su_def(basety);
      if (!d->tag || 1) {
        Member *mem = xcalloc(1, sizeof(Member));
        mem->ty = basety;
        mem->idx = idx++;
        mem->align = attr.align ? attr.align : type_align(basety);
        mem->is_anon = true;
        mem->tok = start;
        if (d->tag)
          warn_tok(start, "declaration does not declare anything");
        cur = cur->next = mem;
        tok = tok->next;
        continue;
      }
    }
    if (equal(tok, ";")) {
      warn_tok(start, "declaration does not declare anything");
      tok = tok->next;
      continue;
    }

    bool first = true;
    while (!consume(&tok, tok, ";")) {
      if (!first)
        tok = skip(tok, ",");
      first = false;

      Member *mem = xcalloc(1, sizeof(Member));
      Token *name = NULL, *pos = NULL;
      if (equal(tok, ":")) {
        mem->ty = basety;
        pos = tok;
      } else {
        mem->ty = declarator(&tok, tok, basety, &name, &pos);
      }
      mem->name = name;
      mem->tok = pos ? pos : start;
      mem->idx = idx++;
      mem->align = attr.align ? attr.align : type_align(mem->ty);
      if (attr.align && attr.align < type_align(mem->ty))
        error_tok(start, "_Alignas cannot reduce alignment");

      if (is_func(mem->ty))
        error_tok(mem->tok, "field '%s' declared as a function", name ? tok_text(name) : "");
      if (is_vm(mem->ty))
        error_tok(mem->tok, "a member of a structure or union cannot have a variably modified type");

      if (consume(&tok, tok, ":")) {
        Token *wtok = tok;
        Node *w = conditional(&tok, tok);
        int64_t width = ice_value(w, wtok, "bit-field width");
        Type *bt = mem->ty;
        if (!is_integer(bt))
          error_tok(mem->tok, "bit-field has invalid type '%s'", type_name(bt));
        if (width < 0)
          error_tok(wtok, "negative width in bit-field");
        if (width > type_size(bt) * 8 || (bt->kind == TY_BOOL && width > 1))
          error_tok(wtok, "width of bit-field exceeds its type");
        if (width == 0 && name)
          error_tok(wtok, "zero width for bit-field '%s'", tok_text(name));
        if (attr.align)
          error_tok(start, "_Alignas cannot be applied to a bit-field");
        mem->is_bitfield = true;
        mem->bit_width = (int)width;
      } else if (!name) {
        error_tok(tok, "expected member name");
      }

      if (name)
        for (Member *m = head.next; m; m = m->next)
          if (m->name && m->name->len == name->len && !memcmp(m->name->loc, name->loc, name->len))
            error_tok(name, "duplicate member '%.*s'", name->len, name->loc);

      cur = cur->next = mem;
      tok = skip_attributes(tok);
    }
  }

  /* Flexible array member (6.7.2.1p18) */
  for (Member *m = head.next; m; m = m->next) {
    if (m->ty->kind == TY_ARRAY && m->ty->array_len < 0) {
      if (m->next)
        error_tok(m->tok, "flexible array member not at end of struct");
      if (ty->kind == TY_UNION)
        error_tok(m->tok, "flexible array member in union");
      if (m == head.next)
        error_tok(m->tok, "flexible array member in a struct with no named members");
      m->ty = array_of(m->ty->base, 0);
      ty->is_flexible = true;
    } else if (!is_object_complete(m->ty)) {
      error_tok(m->tok, "field has incomplete type '%s'", type_name(m->ty));
    } else if (is_su(m->ty) && su_def(m->ty)->is_flexible) {
      /* C17 6.7.2.1p3: not a member of a structure; a union may have one,
         anywhere (SQLite: union { SrcList s; u8 space[N]; }) */
      if (ty->kind == TY_STRUCT)
        warn_tok(m->tok, "invalid use of structure with flexible array member");
    }
  }

  *rest = tok->next;
  ty->members = head.next;
}

/* Does a struct have any named members (directly or via anonymous ones)? */
static bool has_named_member(Type *ty) {
  for (Member *m = su_def(ty)->members; m; m = m->next) {
    if (m->name)
      return true;
    if (m->is_anon && has_named_member(m->ty))
      return true;
  }
  return false;
}

static void layout_struct(Type *ty, Token *tok) {
  int64_t bits = 0;
  int align = 1;
  for (Member *mem = ty->members; mem; mem = mem->next) {
    int64_t sz = type_size(mem->ty);
    if (mem->is_bitfield && mem->bit_width == 0) {
      bits = align_to64(bits, sz * 8);
      mem->offset = bits / 8;
      continue;
    }
    if (mem->is_bitfield) {
      if (ty->is_packed) {
        mem->offset = bits / 8;
        mem->bit_offset = bits % 8;
        /* make sure the load unit is wide enough */
        mem->offset = (bits / 8);
        if (mem->bit_offset + mem->bit_width > sz * 8) {
          mem->offset = align_to64(bits, 8) / 8 - (mem->bit_offset ? 1 : 0);
        }
        mem->bit_offset = bits - mem->offset * 8;
        bits += mem->bit_width;
        continue;
      }
      int64_t unit = sz * 8;
      if (bits / unit != (bits + mem->bit_width - 1) / unit)
        bits = align_to64(bits, unit);
      mem->offset = (bits / unit) * sz;
      mem->bit_offset = bits % unit;
      bits += mem->bit_width;
      if (mem->name && mem->align > align)
        align = mem->align;
      continue;
    }
    int a = ty->is_packed ? 1 : mem->align;
    bits = align_to64(bits, (int64_t)a * 8);
    mem->offset = bits / 8;
    bits += sz * 8;
    if (a > align)
      align = a;
  }
  if (attr_align > align)
    align = attr_align;
  ty->size = align_to64(bits, (int64_t)align * 8) / 8;
  ty->align = align;
  (void)tok;
}

static void layout_union(Type *ty) {
  int64_t size = 0;
  int align = 1;
  for (Member *mem = ty->members; mem; mem = mem->next) {
    int64_t sz = type_size(mem->ty);
    if (mem->is_bitfield) {
      sz = (mem->bit_width + 7) / 8;
      if (mem->name && mem->align > align)
        align = mem->align;
    } else if (mem->align > align && !ty->is_packed) {
      align = mem->align;
    }
    if (sz > size)
      size = sz;
  }
  if (attr_align > align)
    align = attr_align;
  ty->size = align_to64(size, align);
  ty->align = align;
}

/* Completing a struct updates the canonical type and its variants. */
static void complete_su(Type *canon) {
  if (canon->variants)
    for (int q = 0; q < 16; q++) {
      Type *v = canon->variants[q];
      if (!v)
        continue;
      v->size = canon->size;
      v->align = canon->align;
      v->members = canon->members;
      v->is_flexible = canon->is_flexible;
      v->is_complete = true;
    }
}

static Type *struct_union_decl(Token **rest, Token *tok, TypeKind kind) {
  attr_align = 0;
  attr_packed = false;
  tok = skip_attributes(tok);
  bool packed = attr_packed;
  int a_align = attr_align;

  Token *tag = NULL;
  if (tok->kind == TK_IDENT) {
    tag = tok;
    tok = tok->next;
  }

  char *kw = kind == TY_STRUCT ? "struct" : "union";

  if (tag && !equal(tok, "{")) {
    /* "struct S;" declares a new type in the current scope even if an
       outer S exists (6.7.2.3p7). */
    bool is_decl = equal(tok, ";");
    Scope *found = NULL;
    Type *ty = find_tag(tag, &found);
    if (ty && (!is_decl || found == scope)) {
      if (ty->kind != kind)
        error_tok(tag, "'%.*s' defined as wrong kind of tag", tag->len, tag->loc);
      *rest = tok;
      return ty;
    }
    ty = struct_type();
    ty->kind = kind;
    ty->size = -1;
    ty->tag = tag;
    ty->tag_scope_id = scope->id;
    push_tag_scope(tag, ty);
    *rest = tok;
    return ty;
  }

  Token *lbrace = tok;
  tok = skip(tok, "{");

  Type *ty = NULL;
  if (tag) {
    Type *old = hashmap_get2(&scope->tags, tag->loc, tag->len);
    if (old) {
      if (old->kind != kind)
        error_tok(tag, "'%.*s' defined as wrong kind of tag", tag->len, tag->loc);
      if (su_def(old)->is_complete || su_def(old)->members)
        error_tok(tag, "redefinition of '%s %.*s'", kw, tag->len, tag->loc);
      ty = su_def(old);
    }
  }
  if (!ty) {
    ty = struct_type();
    ty->kind = kind;
    ty->tag = tag;
    ty->tag_scope_id = scope->id;
    if (tag)
      push_tag_scope(tag, ty);
  }

  /* Mark as being defined (nested redefinition check). */
  struct_members_parse(&tok, tok, ty);
  attr_align = 0;
  attr_packed = false;
  tok = skip_attributes(tok);
  ty->is_packed = packed || attr_packed;
  int outer_align = attr_align;
  attr_align = a_align > outer_align ? a_align : outer_align;

  if (!has_named_member(ty))
    warn_tok(lbrace, "%s has no named members", kw);

  if (kind == TY_STRUCT)
    layout_struct(ty, lbrace);
  else
    layout_union(ty);
  attr_align = 0;
  ty->is_complete = true;
  complete_su(ty);
  *rest = tok;
  return ty;
}

/* ------------------------------------------------------------ enum */

static Type *enum_specifier(Token **rest, Token *tok) {
  tok = skip_attributes(tok);
  Token *tag = NULL;
  if (tok->kind == TK_IDENT) {
    tag = tok;
    tok = tok->next;
  }

  if (tag && !equal(tok, "{")) {
    Type *ty = find_tag(tag, NULL);
    if (!ty) {
      /* Forward reference to an enum is not standard C; accept as an
         incomplete type like GCC. */
      warn_tok(tag, "ISO C forbids forward references to 'enum' types");
      ty = enum_type();
      ty->tag = tag;
      ty->is_complete = false;
      push_tag_scope(tag, ty);
    } else if (ty->kind != TY_ENUM) {
      error_tok(tag, "'%.*s' defined as wrong kind of tag", tag->len, tag->loc);
    }
    *rest = tok;
    return ty;
  }

  Token *lbrace = tok;
  tok = skip(tok, "{");
  if (equal(tok, "}"))
    error_tok(tok, "empty enum is invalid");

  Type *ty = NULL;
  if (tag) {
    Type *old = hashmap_get2(&scope->tags, tag->loc, tag->len);
    if (old) {
      if (old->kind != TY_ENUM)
        error_tok(tag, "'%.*s' defined as wrong kind of tag", tag->len, tag->loc);
      if (old->is_complete)
        error_tok(tag, "redefinition of 'enum %.*s'", tag->len, tag->loc);
      ty = old;
    }
  }
  if (!ty) {
    ty = enum_type();
    ty->tag = tag;
    if (tag)
      push_tag_scope(tag, ty);
  }
  ty->tag_type = ty;

  /* Enumerators */
  int64_t val = 0;
  int64_t minv = 0, maxv = 0;
  bool first = true;
  bool overflowed = false;
  StrArray names = {0};
  VarScope **scs = NULL;
  int nscs = 0;
  while (!is_end(tok)) {
    if (!first)
      tok = skip(tok, ",");
    if (tok->kind != TK_IDENT)
      error_tok(tok, "expected identifier");
    Token *name = tok;
    char *nm = tok_text(tok);
    tok = skip_attributes(tok->next);

    if (equal(tok, "=")) {
      Token *etok = tok->next;
      Node *e = conditional(&tok, tok->next);
      val = ice_value(e, etok, "enumerator value");
      add_type(e);
      if (val < INT32_MIN || val > INT32_MAX || (e->ty->is_unsigned && e->ty->size == 8 && val < 0))
        warn_tok(etok, "ISO C restricts enumerator values to range of 'int'");
    } else if (overflowed) {
      error_tok(name, "overflow in enumeration values");
    }

    VarScope *old = find_var_current(nm);
    if (old)
      error_tok(name, "redeclaration of '%s'", nm);
    VarScope *sc = push_scope(nm, name);
    sc->enum_ty = ty_int;
    sc->enum_val = val;
    scs = xrealloc(scs, sizeof(VarScope *) * (nscs + 1));
    scs[nscs++] = sc;
    strarray_push(&names, nm);
    if (first || val < minv)
      minv = val;
    if (first || val > maxv)
      maxv = val;
    if (val == INT64_MAX)
      overflowed = true;
    val++;
    first = false;
  }
  if (equal(tok, ","))
    tok = tok->next;
  *rest = skip(tok, "}");
  (void)lbrace;

  /* Compatible integer type: unsigned int if no negative values, as GCC. */
  if (minv < 0) {
    ty->is_unsigned = false;
    ty->enum_int = ty_int;
    if (minv < INT32_MIN || maxv > INT32_MAX) {
      ty->size = ty->align = 8;
      ty->enum_int = ty_long;
    }
  } else {
    ty->is_unsigned = true;
    ty->enum_int = ty_uint;
    if (maxv > UINT32_MAX) {
      ty->size = ty->align = 8;
      ty->enum_int = ty_ulong;
    }
  }
  /* Enumeration constants have type int when representable (6.7.2.2p3). */
  for (int i = 0; i < nscs; i++) {
    int64_t v = scs[i]->enum_val;
    if (v >= INT32_MIN && v <= INT32_MAX)
      scs[i]->enum_ty = ty_int;
    else
      scs[i]->enum_ty = ty->size == 8 ? ty->enum_int : (v > INT32_MAX ? ty_uint : ty_int);
  }
  ty->is_complete = true;
  return ty;
}

/* ------------------------------------------------------------ VLAs */

/* Generates code to compute the byte size of a VLA type, storing it in
   ty->vla_size; returns the code (or NULL). */
Node *compute_vla_size(Type *ty, Token *tok) {
  Node *node = new_node(ND_NULL_EXPR, tok);
  if (ty->base)
    node = new_binary(ND_COMMA, node, compute_vla_size(ty->base, tok), tok);

  if (ty->kind != TY_VLA)
    return node;
  if (!ty->vla_len) {
    /* [*] outside of a prototype is caught earlier; treat as unknown */
    return node;
  }

  Node *base_sz;
  if (ty->base->kind == TY_VLA)
    base_sz = new_var_node(ty->base->vla_size, tok);
  else
    base_sz = new_ulong(type_size(ty->base), tok);

  ty->vla_size = new_lvar("", ty_ulong, tok);
  Node *len = new_cast(ty->vla_len, ty_ulong);
  Node *expr = new_binary(ND_ASSIGN, new_var_node(ty->vla_size, tok),
                          new_binary(ND_MUL, len, base_sz, tok), tok);
  return new_binary(ND_COMMA, node, expr, tok);
}

/* ------------------------------------------------------ declarations */

static bool is_function_def(Token *tok, Type *basety);

static char *asm_label_text(void) {
  if (!current_asm_label)
    return NULL;
  return current_asm_label->str;
}

/* Declares or redeclares an identifier with linkage. */
static Obj *declare_linked(char *name, Type *ty, Token *tok, bool is_static_link,
                           bool is_func) {
  Obj *var = hashmap_get(&linkage_map, name);
  if (var) {
    if (var->is_function != is_func)
      error_tok(tok, "'%s' redeclared as different kind of symbol", name);
    if (!is_compatible(var->ty, ty)) {
      note_tok(var->tok, "previous declaration of '%s' was here", name);
      error_tok(tok, "conflicting types for '%s'", name);
    }
    var->ty = composite_type(var->ty, ty);
    if (is_static_link && !var->is_static)
      error_tok(tok, "static declaration of '%s' follows non-static declaration", name);
    return var;
  }
  var = new_var(name, ty);
  var->asm_name = name;
  var->tok = tok;
  var->is_function = is_func;
  var->is_static = is_static_link;
  var->next = globals;
  globals = var;
  hashmap_put(&linkage_map, name, var);
  return var;
}

/* Linkage of an identifier declared with "extern" or as a function at
   block scope: the prior visible declaration's linkage if any. */
static bool prior_internal_linkage(char *name, Token *tok) {
  VarScope *sc = find_var(tok);
  if (sc && sc->var && !sc->var->is_local)
    return sc->var->is_static;
  Obj *o = hashmap_get(&linkage_map, name);
  (void)o;
  return false;
}

/* Checks a declarator's type is valid for an object declaration. */
static void check_object_type(Type *ty, Token *tok, bool is_def_needed) {
  if (is_void(ty))
    error_tok(tok, "variable has incomplete type 'void'");
  if (is_def_needed && !is_object_complete(ty))
    error_tok(tok, "variable has incomplete type '%s'", type_name(ty));
}

bool is_const_or_has_const_member(Type *ty) {
  if (ty->qual & Q_CONST)
    return true;
  if (is_su(ty))
    for (Member *m = struct_members(ty); m; m = m->next)
      if (is_const_or_has_const_member(m->ty))
        return true;
  return false;
}

static void set_tls(Obj *var, VarAttr *attr) {
  if (attr->is_tls)
    var->is_tls = true;
}

/* Block-scope declaration (returns initialization code). */
Node *declaration(Token **rest, Token *tok, Type *basety, VarAttr *attr) {
  Node head = {0};
  Node *cur = &head;
  int i = 0;
  Token *start = tok;

  if (equal(tok, ";")) {
    /* Tag declaration only */
    bool declares_tag = (is_su(basety) || basety->kind == TY_ENUM);
    if (!declares_tag)
      warn_tok(start, "declaration does not declare anything");
    *rest = tok->next;
    return new_node(ND_BLOCK, tok);
  }

  while (!equal(tok, ";")) {
    if (i++ > 0)
      tok = skip(tok, ",");

    Token *name = NULL, *pos = NULL;
    Type *ty = declarator(&tok, tok, basety, &name, &pos);
    char *asm_label = asm_label_text();
    if (!name)
      error_tok(pos ? pos : tok, "variable name omitted");
    char *nm = tok_text(name);

    if (attr->is_typedef) {
      VarScope *old = find_var_current(nm);
      if (old && (!old->type_def || !is_compatible(old->type_def, ty)))
        error_tok(name, "redefinition of '%s' as a different kind of symbol", nm);
      if (old && (is_vm(ty) || is_vm(old->type_def)))
        error_tok(name, "redefinition of typedef '%s' with variably modified type", nm);
      if (attr->align)
        error_tok(name, "_Alignas in typedef");
      push_scope(nm, name)->type_def = ty;
      if (is_vm(ty)) {
        /* Size expressions of a VM typedef are evaluated at the declaration. */
        Node *code = compute_vla_size(ty, name);
        cur = cur->next = new_unary(ND_EXPR_STMT, code, name);
      }
      if (equal(tok, "="))
        error_tok(tok, "typedef '%s' is initialized", nm);
      continue;
    }

    if (is_func(ty)) {
      /* Block-scope function declaration: external unless prior internal. */
      if (attr->is_static || attr->is_auto || attr->is_register || attr->is_tls)
        error_tok(name, "invalid storage class for function '%s'", nm);
      if (equal(tok, "{"))
        error_tok(tok, "function definition is not allowed here");
      VarScope *old = find_var_current(nm);
      if (old && (!old->var || !old->var->is_function))
        error_tok(name, "redeclaration of '%s' as a different kind of symbol", nm);
      bool internal = prior_internal_linkage(nm, name);
      Obj *fn = declare_linked(nm, ty, name, internal, true);
      if (!fn->tok)
        fn->tok = name;
      if (attr->is_inline)
        error_tok(name, "inline specifier in block-scope function declaration");
      if (asm_label)
        fn->asm_name = asm_label;
      push_scope(nm, name)->var = fn;
      if (equal(tok, "="))
        error_tok(tok, "function '%s' is initialized like a variable", nm);
      continue;
    }

    if (attr->is_inline || attr->is_noreturn)
      error_tok(name, "'%s' can only appear on functions", attr->is_inline ? "inline" : "_Noreturn");

    if (attr->align && attr->align < type_align(ty))
      error_tok(name, "_Alignas cannot reduce the alignment of '%s'", nm);
    if (attr->align && attr->is_register)
      error_tok(name, "_Alignas on a register object");

    VarScope *old = find_var_current(nm);

    if (attr->is_extern) {
      if (equal(tok, "="))
        error_tok(tok, "'%s' has both 'extern' and initializer", nm);
      if (is_vm(ty))
        error_tok(name, "object with variably modified type must have no linkage");
      if (old && !(old->var && !old->var->is_local))
        error_tok(name, "extern declaration of '%s' follows declaration with no linkage", nm);
      bool internal = prior_internal_linkage(nm, name);
      Obj *var = declare_linked(nm, ty, name, internal, false);
      if (!var->tok)
        var->tok = name;
      var->is_extern_decl = true;
      if (attr->is_tls)
        var->is_tls = true;
      if (asm_label)
        var->asm_name = asm_label;
      push_scope(nm, name)->var = var;
      continue;
    }

    if (old)
      error_tok(name, "redefinition of '%s'", nm);

    if (attr->is_tls && !attr->is_static)
      error_tok(name, "'_Thread_local' at block scope requires 'static' or 'extern'");

    if (attr->is_static) {
      /* Static local: a global with a unique name, no linkage. */
      if (is_vm(ty))
        error_tok(name, "static variable '%s' cannot have a variably modified type", nm);
      Obj *var = new_anon_gvar(ty);
      var->asm_name = format("%s.%d", nm, scope_counter * 1000 + i);
      var->name = nm;
      var->tok = name;
      var->align = attr->align > var->align ? attr->align : var->align;
      set_tls(var, attr);
      push_scope(nm, name)->var = var;
      if (equal(tok, "=")) {
        gvar_initializer(&tok, tok->next, var);
      } else {
        check_object_type(var->ty, name, true);
        var->is_tentative = true;
      }
      continue;
    }

    /* Variable-length arrays */
    if (is_vm(ty)) {
      if (equal(tok, "="))
        error_tok(tok, "variable-sized object may not be initialized");

      Node *code = compute_vla_size(ty, name);
      cur = cur->next = new_unary(ND_EXPR_STMT, code, name);

      if (ty->kind == TY_VLA) {
        /* Save the stack pointer, then allocate. */
        VlaChain *vc = xcalloc(1, sizeof(VlaChain));
        vc->outer = cur_vla;
        vc->sp_slot = new_lvar("", ty_ulong, name);
        cur_vla = vc;

        Obj *var = new_lvar(nm, ty, name);
        var->is_vla_ptr = true;
        var->is_register = attr->is_register;
        Node *save = new_node(ND_VLA_PTR, name); /* placeholder kind */
        save->kind = ND_ALLOCA;
        save->var = var;
        save->vla_sp = vc->sp_slot;
        save->lhs = new_var_node(ty->vla_size, name);
        save->ty = ty_void;
        cur = cur->next = new_unary(ND_EXPR_STMT, save, name);
      } else {
        Obj *var = new_lvar(nm, ty, name);
        var->is_register = attr->is_register;
      }
      continue;
    }

    Obj *var = new_lvar(nm, ty, name);
    var->is_register = attr->is_register;
    if (attr->align)
      var->align = attr->align;

    if (equal(tok, "=")) {
      Node *expr = lvar_initializer(&tok, tok->next, var);
      cur = cur->next = new_unary(ND_EXPR_STMT, expr, tok);
    }

    check_object_type(var->ty, name, true);
  }

  Node *node = new_node(ND_BLOCK, start);
  node->body = head.next;
  *rest = tok->next;
  return node;
}

/* ------------------------------------------------ function definitions */

static void create_param_lvars(Param *param) {
  if (!param)
    return;
  create_param_lvars(param->next);
  char *nm = param->name ? tok_text(param->name) : "";
  Obj *var;
  if (param->var) {
    var = param->var;
    var->ty = param->ty;
    var->next = locals;
    locals = var;
    push_scope(nm, param->name)->var = var;
  } else {
    var = new_lvar(nm, param->ty, param->name ? param->name : param->pos);
  }
  var->is_param = true;
}

/* Old-style parameter declarations: "int f(a, b) int a; char *b; {" */
static void oldstyle_param_decls(Token **rest, Token *tok, Type *fn_ty) {
  enter_scope();
  HashMap declared = {0};
  while (!equal(tok, "{")) {
    VarAttr attr = {0};
    Token *start = tok;
    Type *basety = declspec(&tok, tok, &attr);
    if (attr.is_typedef || attr.is_static || attr.is_extern || attr.is_auto || attr.is_tls ||
        attr.is_inline)
      error_tok(start, "invalid storage class for parameter");
    int i = 0;
    while (!consume(&tok, tok, ";")) {
      if (i++ > 0)
        tok = skip(tok, ",");
      Token *name = NULL, *pos = NULL;
      Type *ty = declarator(&tok, tok, basety, &name, &pos);
      if (!name)
        error_tok(tok, "expected identifier");
      Param *p = fn_ty->params;
      for (; p; p = p->next)
        if (p->name->len == name->len && !memcmp(p->name->loc, name->loc, name->len))
          break;
      if (!p)
        error_tok(name, "declaration for parameter '%.*s' but no such parameter", name->len,
                  name->loc);
      if (hashmap_get2(&declared, name->loc, name->len))
        error_tok(name, "redefinition of parameter '%.*s'", name->len, name->loc);
      hashmap_put2(&declared, name->loc, name->len, (void *)1);
      if (equal(tok, "="))
        error_tok(tok, "parameter '%.*s' is initialized", name->len, name->loc);
      p->decl_ty = ty;
      if (is_array(ty))
        ty = qualify(pointer_to(ty->base), ty->array_qual);
      else if (is_func(ty))
        ty = pointer_to(ty);
      p->ty = ty;
    }
  }
  /* Undeclared parameters default to int (6.9.1p6: shall be declared -
     C99 removed implicit int; diagnose). */
  for (Param *p = fn_ty->params; p; p = p->next) {
    if (!p->ty) {
      warn_tok(p->name, "type of '%.*s' defaults to 'int'", p->name->len, p->name->loc);
      p->ty = ty_int;
      p->decl_ty = ty_int;
    }
  }
  leave_scope();
  *rest = tok;
}

static Obj *builtin_alloca_fn;

static Token *function_def(Token *tok, Type *ty, Token *name, VarAttr *attr) {
  char *nm = tok_text(name);
  char *asm_label = asm_label_text();

  bool oldstyle = last_decl_oldstyle, knr_va = false;
  if (oldstyle) {
    oldstyle_param_decls(&tok, tok, ty);
    /* <varargs.h>: "f(a, va_alist) va_dcl" is f(a, ...) */
    for (Param **pp = &ty->params; *pp; pp = &(*pp)->next) {
      Token *pn = (*pp)->name;
      if (!(*pp)->next && pn->len == sizeof("__builtin_va_alist") - 1 && !memcmp(pn->loc, "__builtin_va_alist", pn->len)) {
        *pp = NULL;
        ty->is_variadic = knr_va = true;
        break;
      }
    }
  } else if (!equal(tok, "{"))
    error_tok(tok, "expected '{'");

  if (ty->params && !ty->has_proto && !oldstyle)
    error_tok(name, "parameter list without types in function definition");

  if (attr->is_extern && attr->is_static)
    error_tok(name, "multiple storage classes");
  if (attr->is_tls || attr->is_auto || attr->is_register)
    error_tok(attr->storage_tok ? attr->storage_tok : name, "invalid storage class for function '%s'", nm);

  /* Linkage */
  bool internal = attr->is_static;
  if (!internal) {
    Obj *prev = hashmap_get(&linkage_map, nm);
    if (prev && prev->is_static)
      internal = true;
  }

  /* For an old-style definition, the function type has no prototype;
     record parameter types for the definition itself. */
  Obj *fn = declare_linked(nm, ty, name, attr->is_static, true);
  if (fn->is_definition && fn->body)
    error_tok(name, "redefinition of '%s'", nm);
  if (knr_va && !fn->ty->is_variadic)
    fn->ty = ty;  /* (a declaration without a prototype came first) */
  fn->is_static = internal;
  fn->tok = name;
  if (asm_label)
    fn->asm_name = asm_label;

  /* Inline semantics (6.7.4p7) */
  if (attr->is_inline)
    fn->is_inline = true;
  if (!attr->is_inline || attr->is_extern)
    fn->is_extern_inline = true;  /* marks: an external definition is required */
  if (attr->is_noreturn || attr_noreturn)
    fn->is_noreturn = true;
  apply_gattr(fn);

  if (!is_void(ty->return_ty) && !is_object_complete(ty->return_ty))
    error_tok(name, "return type is an incomplete type");
  if (!strcmp(nm, "main") && ty->return_ty->kind != TY_INT)
    warn_tok(name, "return type of 'main' is not 'int'");

  fn->is_definition = true;
  /* The function is visible in its own body. */
  VarScope *sc = hashmap_get(&scope->vars, nm);
  if (!sc)
    sc = push_scope(nm, name);
  sc->var = fn;

  current_fn = fn;
  locals = NULL;
  enter_scope();
  for (Param *p = ty->params; p; p = p->next) {
    if (!p->name && ty->has_proto)
      error_tok(p->pos, "parameter name omitted");
    if (!is_object_complete(p->ty))
      error_tok(p->pos, "parameter has incomplete type '%s'", type_name(p->ty));
  }
  create_param_lvars(ty->params);

  /* A buffer for a struct/union return value is passed as a hidden
     first parameter in the codegen. */
  fn->params = locals;

  if (ty->is_variadic)
    fn->va_area = new_lvar("__va_area__", array_of(ty_char, 176), name);
  fn->alloca_bottom = new_lvar("__alloca_size__", pointer_to(ty_char), name);

  /* VLA parameter sizes are evaluated on entry */
  Node *vla_code = NULL;
  for (Param *p = ty->params; p; p = p->next) {
    if (p->decl_ty && is_vm(p->decl_ty)) {
      Node *code = compute_vla_size(p->decl_ty, p->pos);
      if (!vla_code)
        vla_code = new_unary(ND_EXPR_STMT, code, name);
      else {
        Node *c = new_unary(ND_EXPR_STMT, code, name);
        c->next = vla_code;
        vla_code = c;
      }
    }
    if (is_vm(p->ty)) {
      Node *code = compute_vla_size(p->ty, p->pos);
      Node *c = new_unary(ND_EXPR_STMT, code, name);
      c->next = vla_code;
      vla_code = c;
    }
  }

  gotos = labels = NULL;
  cur_vla = NULL;
  Token *body_start = tok;
  Node *body = compound_stmt(&tok, tok->next, false);
  if (vla_code) {
    Node *last = vla_code;
    while (last->next)
      last = last->next;
    last->next = body->body;
    body->body = vla_code;
  }
  fn->body = body;
  (void)body_start;
  fn->locals = locals;
  leave_scope();
  resolve_goto_labels();
  current_fn = NULL;
  return tok;
}

/* Is "declspec declarator" followed by a function body? */
static bool is_function_def(Token *tok, Type *basety) {
  if (equal(tok, ";"))
    return false;
  Token *name = NULL, *pos = NULL;
  Scope *saved = scope;
  Type *ty = declarator_fn(&tok, tok, basety, &name, &pos);
  scope = saved;
  if (ty->kind != TY_FUNC)
    return false;
  if (equal(tok, "{"))
    return true;
  return last_decl_oldstyle && !equal(tok, ";") && !equal(tok, ",") && !equal(tok, "=");
}

/* File-scope declaration */
static Token *global_declaration(Token *tok, Type *basety, VarAttr *attr) {
  bool first = true;
  Token *start = tok;

  if (equal(tok, ";")) {
    bool declares_tag = is_su(basety) || basety->kind == TY_ENUM;
    if (!declares_tag)
      warn_tok(start, "declaration does not declare anything");
    return tok->next;
  }

  if (attr->is_auto || attr->is_register)
    error_tok(attr->storage_tok, "file-scope declaration specifies '%s'",
              attr->is_auto ? "auto" : "register");

  while (!consume(&tok, tok, ";")) {
    if (!first)
      tok = skip(tok, ",");
    first = false;

    Token *name = NULL, *pos = NULL;
    Type *ty = declarator_fn(&tok, tok, basety, &name, &pos);
    char *asm_label = asm_label_text();
    if (!name)
      error_tok(pos ? pos : tok, "variable name omitted");
    char *nm = tok_text(name);

    if (attr->is_typedef) {
      VarScope *old = find_var_current(nm);
      if (old && (!old->type_def || !is_compatible(old->type_def, ty)))
        error_tok(name, "conflicting types for typedef '%s'", nm);
      if (is_vm(ty))
        error_tok(name, "variably modified '%s' at file scope", nm);
      push_scope(nm, name)->type_def = ty;
      if (equal(tok, "="))
        error_tok(tok, "typedef '%s' is initialized", nm);
      continue;
    }

    {
      VarScope *old = find_var_current(nm);
      if (old && (old->type_def || old->enum_ty))
        error_tok(name, "'%s' redeclared as different kind of symbol", nm);
    }

    if (is_func(ty)) {
      if (attr->is_tls)
        error_tok(name, "function '%s' declared '_Thread_local'", nm);
      if (equal(tok, "{"))
        error_tok(tok, "function definition in a list of declarators");
      Obj *prev = hashmap_get(&linkage_map, nm);
      bool internal = attr->is_static || (prev && prev->is_static && !attr->is_static);
      if (attr->is_static && prev && !prev->is_static)
        error_tok(name, "static declaration of '%s' follows non-static declaration", nm);
      Obj *fn = declare_linked(nm, ty, name, attr->is_static, true);
      fn->is_static = internal;
      if (!fn->tok)
        fn->tok = name;
      if (asm_label)
        fn->asm_name = asm_label;
      if (!attr->is_inline || attr->is_extern)
        fn->is_extern_inline = true;
      if (attr->is_inline)
        fn->is_inline = true;
      if (attr->is_noreturn || attr_noreturn)
        fn->is_noreturn = true;
      tok = skip_attributes(tok);
      apply_gattr(fn);
      push_scope(nm, name)->var = fn;
      continue;
    }

    if (attr->is_inline || attr->is_noreturn)
      error_tok(name, "'%s' can only appear on functions", attr->is_inline ? "inline" : "_Noreturn");
    if (is_vm(ty))
      error_tok(name, "variably modified '%s' at file scope", nm);
    if (attr->align && attr->align < type_align(ty))
      error_tok(name, "_Alignas cannot reduce the alignment of '%s'", nm);

    Obj *prev = hashmap_get(&linkage_map, nm);
    if (attr->is_static && prev && !prev->is_static)
      error_tok(name, "static declaration of '%s' follows non-static declaration", nm);
    if (!attr->is_static && !attr->is_extern && prev && prev->is_static)
      error_tok(name, "non-static declaration of '%s' follows static declaration", nm);
    bool internal = attr->is_static || (prev && prev->is_static);
    Obj *var = declare_linked(nm, ty, name, attr->is_static, false);
    var->is_static = internal;
    if (attr->align && attr->align > var->align)
      var->align = attr->align;
    if (asm_label)
      var->asm_name = asm_label;
    if (prev && prev->is_tls != attr->is_tls)
      error_tok(name, "thread-local declaration of '%s' follows non-thread-local declaration", nm);
    set_tls(var, attr);
    tok = skip_attributes(tok);
    apply_gattr(var);
    if (var->alias) {
      if (equal(tok, "="))
        error_tok(name, "alias '%s' cannot have an initializer", nm);
      var->is_extern_decl = false;
    }
    push_scope(nm, name)->var = var;

    if (equal(tok, "=")) {
      if (var->init_data)
        error_tok(name, "redefinition of '%s'", nm);
      if (attr->is_extern)
        warn_tok(name, "'%s' initialized and declared 'extern'", nm);
      var->is_extern_decl = false;
      var->is_tentative = false;
      gvar_initializer(&tok, tok->next, var);
      var->is_definition = true;
      continue;
    }
    if (attr->is_extern) {
      if (!var->init_data && !var->is_tentative)
        var->is_extern_decl = true;
      continue;
    }
    /* Tentative definition (6.9.2) */
    if (!var->init_data) {
      var->is_tentative = true;
      var->is_extern_decl = false;
    }
  }
  return tok;
}

/* Marks static functions that are referenced, transitively. */
static void mark_live(Obj *prog) {
  HashMap map = {0};
  for (Obj *v = prog; v; v = v->next)
    if (v->is_function)
      hashmap_put(&map, v->name, v);
  StrArray work = {0};
  for (Obj *v = prog; v; v = v->next) {
    if (!v->is_function || !v->body)
      continue;
    bool root = !v->is_static && !(v->is_inline && !v->is_extern_inline);
    if (v->ctor_prio || v->dtor_prio || v->attr_used)
      root = true;
    if (root || v->is_addr_taken) {
      if (!v->is_live) {
        v->is_live = true;
        strarray_push(&work, v->name);
      }
    }
  }
  /* Aliases keep their targets */
  for (Obj *v = prog; v; v = v->next)
    if (v->alias) {
      Obj *f = hashmap_get(&map, v->alias);
      if (f && !f->is_live) {
        f->is_live = true;
        strarray_push(&work, f->name);
      }
    }
  /* Globals' initializers may reference functions */
  for (Obj *v = prog; v; v = v->next)
    for (Relocation *r = v->rel; r; r = r->next) {
      Obj *f = hashmap_get(&map, *r->label);
      if (f && !f->is_live) {
        f->is_live = true;
        strarray_push(&work, f->name);
      }
    }
  while (work.len) {
    char *nm = work.data[--work.len];
    Obj *f = hashmap_get(&map, nm);
    for (int i = 0; i < f->refs.len; i++) {
      Obj *g = hashmap_get(&map, f->refs.data[i]);
      if (g && !g->is_live) {
        g->is_live = true;
        strarray_push(&work, g->name);
      }
    }
  }
}

static void finish_globals(void) {
  for (Obj *v = globals; v; v = v->next) {
    if (v->is_function) {
      if (!v->body) {
        v->is_definition = false;
        if (v->is_static && v->is_used && !v->alias)
          warn_tok(v->tok, "'%s' used but never defined", v->name);
      } else if (v->is_inline && !v->is_extern_inline && !v->is_static) {
        /* An inline definition provides no external definition. */
        v->is_definition = false;
      }
      continue;
    }
    if (v->is_string_lit || v->is_compound_lit || !v->asm_name)
      continue;
    if (v->alias) {
      v->is_definition = false;
      continue;
    }
    if (v->init_data) {
      v->is_definition = true;
      continue;
    }
    if (v->is_tentative) {
      /* Zero-initialized; an incomplete array gets one element. */
      if (v->ty->kind == TY_ARRAY && v->ty->array_len < 0) {
        warn_tok(v->tok, "array '%s' assumed to have one element", v->name);
        v->ty = array_of(v->ty->base, 1);
      }
      if (!is_object_complete(v->ty))
        error_tok(v->tok, "storage size of '%s' isn't known", v->name);
      v->is_definition = true;
      continue;
    }
    v->is_definition = false;
  }
}

static void declare_builtins(void) {
  builtin_alloca_fn = NULL;
}

char *base_file;

Obj *parse(Token *tok) {
  init_types();
  locals = globals = NULL;
  scope = NULL;
  linkage_map = (HashMap){0};
  scope_counter = 0;
  enter_scope();
  scope->is_file = true;
  declare_builtins();

  if (tok->kind == TK_EOF)
    warn_tok(tok, "ISO C requires a translation unit to contain at least one declaration");

  while (tok->kind != TK_EOF) {
    if (equal(tok, "_Static_assert")) {
      tok = static_assertion(tok);
      continue;
    }
    if (equal(tok, ";")) {
      warn_tok(tok, "ISO C does not allow extra ';' outside of a function");
      tok = tok->next;
      continue;
    }
    if (equal(tok, "__asm__") || equal(tok, "__asm")) {
      /* file-scope asm: pass through */
      Token *start = tok;
      tok = skip(tok->next, "(");
      if (tok->kind != TK_STR)
        error_tok(tok, "expected string literal");
      Obj *v = new_anon_gvar(ty_void);
      v->is_function = false;
      v->is_definition = false;
      v->init_data = tok->str;
      v->asm_name = NULL;
      v->name = "__toplevel_asm__";
      v->tok = start;
      tok = skip(tok->next, ")");
      tok = skip(tok, ";");
      continue;
    }

    VarAttr attr = {0};
    attr_noreturn = false;
    memset(&gattr, 0, sizeof(gattr));
    Token *start = tok;
    Type *basety = declspec(&tok, tok, &attr);
    (void)start;

    if (!attr.is_typedef && is_function_def(tok, basety)) {
      Token *name = NULL, *pos = NULL;
      Type *ty = declarator_fn(&tok, tok, basety, &name, &pos);
      if (!name)
        error_tok(tok, "function name omitted");
      tok = function_def(tok, ty, name, &attr);
      continue;
    }

    tok = global_declaration(tok, basety, &attr);
  }
  file_scope_end = tok;

  finish_globals();

  /* Reverse into source order */
  Obj *prog = NULL;
  for (Obj *v = globals; v;) {
    Obj *next = v->next;
    v->next = prog;
    prog = v;
    v = next;
  }
  mark_live(prog);
  return prog;
}

/* Exposed for the codegen's "refs" tracking */
void add_fn_ref(Obj *fn) {
  if (current_fn && fn->is_function && fn != current_fn)
    strarray_push(&current_fn->refs, fn->name);
  else if (!current_fn && fn->is_function)
    fn->is_addr_taken = true;
}
