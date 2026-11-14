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

/* expr.c - expressions (6.5) */
#include "parse.h"

/* the values of __builtin_inf and __builtin_nan, computed here (when c17
   runs, where floating-point exceptions do not matter) */
static long double fp_inf_value(void) {
  volatile long double zero = 0;
  return 1 / zero;
}
static long double fp_nan_value(void) {
  volatile long double zero = 0;
  long double n = zero / zero;
  return signbit(n) ? -n : n;   /* positive, as GCC's */
}

void add_fn_ref(Obj *fn);

static Node *cast(Token **rest, Token *tok);
static Node *unary(Token **rest, Token *tok);
static Node *postfix(Token **rest, Token *tok);
static Node *cx_tmp(Node *n);
static Node *primary(Token **rest, Token *tok);
static Node *mul(Token **rest, Token *tok);
static Node *logor(Token **rest, Token *tok);
Node *postfix_rest_hook(Token **rest, Token *tok, Node *node);

/* ------------------------------------------------------- node makers */

Node *new_node(NodeKind kind, Token *tok) {
  Node *node = xcalloc(1, sizeof(Node));
  node->kind = kind;
  node->tok = tok;
  return node;
}

Node *new_binary(NodeKind kind, Node *lhs, Node *rhs, Token *tok) {
  Node *node = new_node(kind, tok);
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

Node *new_unary(NodeKind kind, Node *expr, Token *tok) {
  Node *node = new_node(kind, tok);
  node->lhs = expr;
  return node;
}

Node *new_num(int64_t val, Token *tok) {
  Node *node = new_node(ND_NUM, tok);
  node->val = val;
  node->ty = ty_int;
  return node;
}

Node *new_long(int64_t val, Token *tok) {
  Node *node = new_node(ND_NUM, tok);
  node->val = val;
  node->ty = ty_long;
  return node;
}

Node *new_ulong(int64_t val, Token *tok) {
  Node *node = new_node(ND_NUM, tok);
  node->val = val;
  node->ty = ty_ulong;
  return node;
}

Node *new_var_node(Obj *var, Token *tok) {
  Node *node = new_node(ND_VAR, tok);
  node->var = var;
  node->ty = var->ty;
  return node;
}

Node *new_vla_ptr(Obj *var, Token *tok) {
  Node *node = new_node(ND_VLA_PTR, tok);
  node->var = var;
  return node;
}

Node *new_cast(Node *expr, Type *ty) {
  add_type(expr);
  if (expr->kind == ND_NUM && is_integer(expr->ty) && is_integer(ty) && ty->kind != TY_ENUM) {
    Node *n = new_num(expr->val, expr->tok);
    n->ty = ty->qual ? unqual(ty) : ty;
    Node tmp = {0};
    tmp.kind = ND_CAST;
    tmp.lhs = expr;
    tmp.ty = n->ty;
    tmp.tok = expr->tok;
    n->val = eval(&tmp);
    return n;
  }
  Node *node = xcalloc(1, sizeof(Node));
  node->kind = ND_CAST;
  node->tok = expr->tok;
  node->lhs = expr;
  node->ty = ty->qual && !is_su(ty) ? unqual(ty) : ty;
  if (node->ty->is_complex) {
    if (expr->ty && expr->ty->is_complex && expr->ty->kind == node->ty->kind)
      return expr;  /* same complex type */
    cx_tmp(node);
  }
  return node;
}

/* Lock-free atomics: objects of size 1, 2, 4 or 8. */
static void check_atomic_size(Type *ty, Token *tok) {
  int64_t sz = type_size(ty);
  if (sz != 1 && sz != 2 && sz != 4 && sz != 8)
    error_tok(tok, "atomic operations on '%s' (size %" PRId64 ") are not supported", type_name(ty), sz);
}

static bool is_atomic_lvalue(Node *n) { return n->ty && (n->ty->qual & Q_ATOMIC); }

/* Atomic read-modify-write: *addr = newexpr(old), retried with
   compare-and-swap.  `old` is the variable newexpr refers to. */
static Node *new_atomic_rmw(Node *addr, Obj *old, Node *newexpr, bool fetch_first, Token *tok) {
  Node *n = new_node(ND_ATOMIC_RMW, tok);
  n->cas_addr = addr;
  n->var = old;
  n->lhs = newexpr;
  n->rmw_fetch_first = fetch_first;
  n->ty = old->ty;
  return n;
}

/* Complex-valued nodes compute their result into a temporary. */
static Node *cx_tmp(Node *n) {
  if (n->ty && n->ty->is_complex && current_fn && !n->var)
    n->var = new_lvar("", n->ty, n->tok);
  return n;
}

/* Folds an integer operation whose operands are constants (keeps ICE
   semantics: the result is still an integer constant node). */
static Node *fold(Node *n) {
  if (!n->ty || !is_integer(n->ty))
    return n;
  switch (n->kind) {
  case ND_ADD: case ND_SUB: case ND_MUL: case ND_BITAND: case ND_BITOR: case ND_BITXOR:
  case ND_EQ: case ND_NE: case ND_LT: case ND_LE:
    if (n->lhs->kind != ND_NUM || n->rhs->kind != ND_NUM || !is_integer(n->lhs->ty) ||
        !is_integer(n->rhs->ty))
      return n;
    break;
  case ND_DIV: case ND_MOD:
    if (n->lhs->kind != ND_NUM || n->rhs->kind != ND_NUM || !is_integer(n->lhs->ty) ||
        n->rhs->val == 0)
      return n;
    break;
  case ND_SHL: case ND_SHR:
    if (n->lhs->kind != ND_NUM || n->rhs->kind != ND_NUM || n->rhs->val < 0 ||
        n->rhs->val >= type_size(n->ty) * 8)
      return n;
    break;
  case ND_NEG: case ND_BITNOT: case ND_NOT:
    if (n->lhs->kind != ND_NUM || !is_integer(n->lhs->ty))
      return n;
    break;
  default:
    return n;
  }
  Node *r = new_num(eval(n), n->tok);
  r->ty = n->ty;
  return r;
}

/* ---------------------------------------------------- properties */

bool is_lvalue(Node *node) {
  switch (node->kind) {
  case ND_VAR:
    return !node->var->is_function;
  case ND_DEREF:
    return !is_func(node->ty);
  case ND_MEMBER:
    return true;
  case ND_COMMA:
    /* (init, var) produced for a compound literal */
    return node->var && node->var->is_compound_lit;
  default:
    return false;
  }
}

/* A member access is an lvalue iff its struct operand is (6.5.2.3). */
static bool member_is_lvalue(Node *node) {
  while (node->kind == ND_MEMBER)
    node = node->lhs;
  if (node->kind == ND_DEREF)
    return true;
  return is_lvalue(node);
}

static bool is_modifiable_lvalue(Node *node, Token *tok, char *what) {
  bool lv = node->kind == ND_MEMBER ? member_is_lvalue(node) : is_lvalue(node);
  if (!lv)
    error_tok(tok, "lvalue required as %s", what);
  Type *ty = node->ty;
  if (is_array(ty))
    error_tok(tok, "array type '%s' is not assignable", type_name(ty));
  if (!is_object_complete(ty))
    error_tok(tok, "%s of object with incomplete type '%s'", what, type_name(ty));
  if (is_const_or_has_const_member(ty)) {
    if (node->kind == ND_VAR)
      error_tok(tok, "%s of read-only variable '%s'", what, node->var->name);
    error_tok(tok, "%s of read-only location", what);
  }
  return true;
}

bool is_ice(Node *node) {
  if (!node)
    return false;
  add_type(node);
  switch (node->kind) {
  case ND_NUM:
    return is_integer(node->ty);
  case ND_CAST:
    if (!is_integer(node->ty))
      return false;
    if (node->lhs->kind == ND_NUM)
      return true;  /* floating constants may be cast operands */
    return is_ice(node->lhs);
  case ND_ADD: case ND_SUB: case ND_MUL: case ND_DIV: case ND_MOD:
  case ND_BITAND: case ND_BITOR: case ND_BITXOR: case ND_SHL: case ND_SHR:
  case ND_EQ: case ND_NE: case ND_LT: case ND_LE: case ND_LOGAND: case ND_LOGOR:
    return is_integer(node->ty) && is_ice(node->lhs) && is_ice(node->rhs);
  case ND_NEG: case ND_BITNOT: case ND_NOT:
    return is_ice(node->lhs);
  case ND_COND:
    return is_integer(node->ty) && is_ice(node->cond) && is_ice(node->then) &&
           is_ice(node->els);
  default:
    return false;
  }
}

int64_t ice_value(Node *node, Token *tok, char *what) {
  add_type(node);
  if (!is_ice(node))
    error_tok(tok, "%s is not an integer constant expression", what);
  return eval(node);
}

bool is_null_ptr_const(Node *node) {
  add_type(node);
  if (is_integer(node->ty) && is_ice(node))
    return eval(node) == 0;
  if (node->kind == ND_CAST && node->ty->kind == TY_PTR && node->ty->base->kind == TY_VOID &&
      !node->ty->base->qual && is_integer(node->lhs->ty) && is_ice(node->lhs))
    return eval(node->lhs) == 0;
  return false;
}

/* ---------------------------------------------------- conversions */

/* Lvalue conversion, array-to-pointer and function-to-pointer decay. */
Node *rval(Node *node) {
  add_type(node);
  if (is_array(node->ty)) {
    if (node->kind == ND_VAR && node->var->is_register)
      error_tok(node->tok, "address of register variable requested");
    Node *n = new_unary(ND_ADDR, node, node->tok);
    n->ty = pointer_to(node->ty->base);
    return n;
  }
  if (is_func(node->ty)) {
    Node *n = new_unary(ND_ADDR, node, node->tok);
    n->ty = pointer_to(node->ty);
    return n;
  }
  if (node->kind == ND_MEMBER && node->member->is_bitfield && is_integer(node->ty)) {
    Member *m = node->member;
    if (m->bit_width < 32 || (m->bit_width == 32 && !node->ty->is_unsigned))
      if (node->ty->kind != TY_BOOL)
        return new_cast(node, ty_int);
    if (m->bit_width == 32 && node->ty->size > 4)
      return new_cast(node, ty_uint);
  }
  return node;
}

/* 6.5.16.1: conversion as if by assignment, with constraint checks. */
Node *convert_for_assign(Node *rhs, Type *ty, Token *tok, char *what) {
  rhs = rval(rhs);
  Type *rt = rhs->ty;

  if (is_void(rt))
    error_tok(tok, "void value not ignored as it ought to be");

  if (is_arith(ty) && is_arith(rt))
    return new_cast(rhs, ty);

  if (is_su(ty)) {
    if (is_su(rt) && is_compatible_unqual(ty, rt))
      return rhs;
    error_tok(tok, "incompatible types when %s type '%s' using type '%s'", what, type_name(ty),
              type_name(rt));
  }

  if (ty->kind == TY_BOOL && rt->kind == TY_PTR)
    return new_cast(rhs, ty);

  if (ty->kind == TY_PTR) {
    if (is_null_ptr_const(rhs))
      return new_cast(rhs, ty);
    if (rt->kind == TY_PTR) {
      Type *lb = ty->base, *rb = rt->base;
      bool ok;
      if (is_void(lb) || is_void(rb))
        ok = !is_func(lb) && !is_func(rb);
      else
        ok = is_compatible_unqual(lb, rb);
      if (!ok)
        warn_tok(tok, "incompatible pointer types %s '%s' from '%s'", what, type_name(ty),
                 type_name(rt));
      else if ((rb->qual & ~lb->qual) & (Q_CONST | Q_VOLATILE | Q_RESTRICT | Q_ATOMIC))
        warn_tok(tok, "%s '%s' from '%s' discards qualifiers from pointer target type", what,
                 type_name(ty), type_name(rt));
      return new_cast(rhs, ty);
    }
    if (is_integer(rt)) {
      warn_tok(tok, "%s '%s' from '%s' makes pointer from integer without a cast", what,
               type_name(ty), type_name(rt));
      return new_cast(rhs, ty);
    }
  }

  if (is_integer(ty) && rt->kind == TY_PTR) {
    warn_tok(tok, "%s '%s' from '%s' makes integer from pointer without a cast", what,
             type_name(ty), type_name(rt));
    return new_cast(rhs, ty);
  }

  error_tok(tok, "incompatible types when %s type '%s' using type '%s'", what, type_name(ty),
            type_name(rt));
}

Node *to_bool(Node *node) { return new_cast(node, ty_bool); }

/* Controlling expressions and operands of !, &&, || must be scalar. */
Node *cond_expr_check(Node *cond, Token *tok) {
  cond = rval(cond);
  if (!is_scalar(cond->ty))
    error_tok(tok, "used %s type value where scalar is required",
              is_su(cond->ty) ? "struct" : type_name(cond->ty));
  if (cond->ty->is_complex)
    cond = new_cast(cond, ty_bool);
  return cond;
}

static Node *arith_operand(Node *n, Token *tok, char *op) {
  n = rval(n);
  if (!is_arith(n->ty))
    error_tok(tok, "invalid operand to binary %s (have '%s')", op, type_name(n->ty));
  return n;
}

static Node *int_operand(Node *n, Token *tok, char *op) {
  n = rval(n);
  if (!is_integer(n->ty))
    error_tok(tok, "invalid operand to %s (have '%s')", op, type_name(n->ty));
  return n;
}

static Node *new_arith(NodeKind kind, Node *lhs, Node *rhs, Token *tok, char *op) {
  lhs = arith_operand(lhs, tok, op);
  rhs = arith_operand(rhs, tok, op);
  if ((kind == ND_MOD || kind == ND_BITAND || kind == ND_BITOR || kind == ND_BITXOR) &&
      (!is_integer(lhs->ty) || !is_integer(rhs->ty)))
    error_tok(tok, "invalid operands to binary %s (have '%s' and '%s')", op, type_name(lhs->ty),
              type_name(rhs->ty));
  Type *ty = usual_arith_conv(lhs->ty, rhs->ty);
  Node *n = new_binary(kind, new_cast(lhs, ty), new_cast(rhs, ty), tok);
  n->ty = ty;
  cx_tmp(n);
  return fold(n);
}

static Node *new_shift(NodeKind kind, Node *lhs, Node *rhs, Token *tok, char *op) {
  lhs = int_operand(lhs, tok, op);
  rhs = int_operand(rhs, tok, op);
  Type *lt = int_promote(lhs->ty);
  Node *n = new_binary(kind, new_cast(lhs, lt), new_cast(rhs, int_promote(rhs->ty)), tok);
  n->ty = lt;
  return fold(n);
}

/* Size of the pointed-to type, possibly computed at run time. */
static Node *pointee_size(Type *ptr, Token *tok) {
  Type *base = ptr->base;
  if (base->kind == TY_VLA) {
    if (!base->vla_size)
      error_tok(tok, "pointer arithmetic on an unspecified variable length array");
    return new_var_node(base->vla_size, tok);
  }
  if (is_void(base) || is_func(base)) {
    warn_tok(tok, "pointer of type '%s' used in arithmetic", type_name(ptr));
    return new_long(1, tok);
  }
  if (!is_complete(base))
    error_tok(tok, "arithmetic on a pointer to an incomplete type '%s'", type_name(base));
  return new_long(type_size(base), tok);
}

Node *new_add(Node *lhs, Node *rhs, Token *tok) {
  lhs = rval(lhs);
  rhs = rval(rhs);

  if (is_arith(lhs->ty) && is_arith(rhs->ty))
    return new_arith(ND_ADD, lhs, rhs, tok, "+");

  if (lhs->ty->kind == TY_PTR && rhs->ty->kind == TY_PTR)
    error_tok(tok, "invalid operands to binary + (have '%s' and '%s')", type_name(lhs->ty),
              type_name(rhs->ty));

  if (rhs->ty->kind == TY_PTR) {
    Node *t = lhs;
    lhs = rhs;
    rhs = t;
  }
  if (lhs->ty->kind != TY_PTR || !is_integer(rhs->ty))
    error_tok(tok, "invalid operands to binary + (have '%s' and '%s')", type_name(lhs->ty),
              type_name(rhs->ty));

  /* ptr + num */
  Node *off = new_binary(ND_MUL, new_cast(rhs, ty_long), pointee_size(lhs->ty, tok), tok);
  off->ty = ty_long;
  off = fold(off);
  Node *n = new_binary(ND_ADD, lhs, off, tok);
  n->ty = lhs->ty;
  return n;
}

Node *new_sub(Node *lhs, Node *rhs, Token *tok) {
  lhs = rval(lhs);
  rhs = rval(rhs);

  if (is_arith(lhs->ty) && is_arith(rhs->ty))
    return new_arith(ND_SUB, lhs, rhs, tok, "-");

  /* ptr - num */
  if (lhs->ty->kind == TY_PTR && is_integer(rhs->ty)) {
    Node *off = new_binary(ND_MUL, new_cast(rhs, ty_long), pointee_size(lhs->ty, tok), tok);
    off->ty = ty_long;
    off = fold(off);
    Node *n = new_binary(ND_SUB, lhs, off, tok);
    n->ty = lhs->ty;
    return n;
  }

  /* ptr - ptr: the number of elements between */
  if (lhs->ty->kind == TY_PTR && rhs->ty->kind == TY_PTR) {
    if (!is_compatible_unqual(lhs->ty->base, rhs->ty->base))
      warn_tok(tok, "subtraction of pointers to incompatible types '%s' and '%s'",
               type_name(lhs->ty), type_name(rhs->ty));
    Node *diff = new_binary(ND_SUB, new_cast(lhs, ty_long), new_cast(rhs, ty_long), tok);
    diff->ty = ty_long;
    Node *n = new_binary(ND_DIV, diff, pointee_size(lhs->ty, tok), tok);
    n->ty = ty_long;
    return n;
  }

  error_tok(tok, "invalid operands to binary - (have '%s' and '%s')", type_name(lhs->ty),
            type_name(rhs->ty));
}

/* Equality and relational operators */
static Node *new_compare(NodeKind kind, Node *lhs, Node *rhs, Token *tok, bool equality,
                         char *op) {
  lhs = rval(lhs);
  rhs = rval(rhs);
  Type *lt = lhs->ty, *rt = rhs->ty;

  if (is_arith(lt) && is_arith(rt)) {
    if (!equality && (lt->is_complex || rt->is_complex))
      error_tok(tok, "invalid operands to binary %s (complex)", op);
    Type *ty = usual_arith_conv(lt, rt);
    Node *n = new_binary(kind, new_cast(lhs, ty), new_cast(rhs, ty), tok);
    n->ty = ty_int;
    return fold(n);
  }

  if (lt->kind == TY_PTR && rt->kind == TY_PTR) {
    Type *lb = lt->base, *rb = rt->base;
    if (equality) {
      bool ok = is_compatible_unqual(lb, rb) ||
                ((is_void(lb) || is_void(rb)) && !is_func(lb) && !is_func(rb)) ||
                is_null_ptr_const(lhs) || is_null_ptr_const(rhs);
      if (!ok)
        warn_tok(tok, "comparison of distinct pointer types ('%s' and '%s')", type_name(lt),
                 type_name(rt));
    } else {
      if (!is_compatible_unqual(lb, rb))
        warn_tok(tok, "comparison of distinct pointer types ('%s' and '%s')", type_name(lt),
                 type_name(rt));
      if (is_func(lb))
        warn_tok(tok, "ordered comparison of function pointers");
    }
  } else if (lt->kind == TY_PTR && is_integer(rt)) {
    if (!(equality && is_null_ptr_const(rhs)))
      warn_tok(tok, "comparison between pointer and integer");
  } else if (rt->kind == TY_PTR && is_integer(lt)) {
    if (!(equality && is_null_ptr_const(lhs)))
      warn_tok(tok, "comparison between pointer and integer");
  } else {
    error_tok(tok, "invalid operands to binary %s (have '%s' and '%s')", op, type_name(lt),
              type_name(rt));
  }
  Node *n = new_binary(kind, new_cast(lhs, ty_ulong), new_cast(rhs, ty_ulong), tok);
  n->ty = ty_int;
  return n;
}

/* ------------------------------------------------------ assignment */

/* Returns an lvalue equivalent to `node` whose evaluation has no side
   effects, plus code (in *pre) to set up the temporaries it needs. */
static Node *stable_lvalue(Node *node, Node **pre, Token *tok) {
  if (node->kind == ND_VAR) {
    *pre = new_node(ND_NULL_EXPR, tok);
    return node;
  }
  if (node->kind == ND_MEMBER) {
    Node *base;
    Node *inner = stable_lvalue(node->lhs, pre, tok);
    base = inner;
    Node *n = new_unary(ND_MEMBER, base, tok);
    n->member = node->member;
    n->ty = node->ty;
    return n;
  }
  /* *(tmp = &node) */
  Obj *var = new_lvar("", pointer_to(node->ty), tok);
  Node *addr = new_unary(ND_ADDR, node, tok);
  addr->ty = pointer_to(node->ty);
  *pre = new_binary(ND_ASSIGN, new_var_node(var, tok), addr, tok);
  (*pre)->ty = var->ty;
  Node *d = new_unary(ND_DEREF, new_var_node(var, tok), tok);
  d->ty = node->ty;
  return d;
}

Node *new_assign(Node *lhs, Node *rhs, Token *tok) {
  add_type(lhs);
  is_modifiable_lvalue(lhs, tok, "left operand of assignment");
  if (is_atomic_lvalue(lhs))
    check_atomic_size(lhs->ty, tok);
  Node *r = convert_for_assign(rhs, unqual(lhs->ty), tok, "assigning to");
  Node *n = new_binary(ND_ASSIGN, lhs, r, tok);
  n->ty = unqual(lhs->ty);
  return n;
}

static Node *atomic_compound(Node *lhs, Node *rhs, NodeKind kind, Token *tok, char *op,
                             bool fetch_first);

/* A op= B  ==>  (pre, L = L op B) */
static Node *compound_assign(Node *lhs, Node *rhs, NodeKind kind, Token *tok, char *op) {
  add_type(lhs);
  is_modifiable_lvalue(lhs, tok, "left operand of assignment");
  if (is_atomic_lvalue(lhs))
    return atomic_compound(lhs, rhs, kind, tok, op, false);
  Node *pre;
  Node *l = stable_lvalue(lhs, &pre, tok);
  Node *val;
  Type *lt = lhs->ty;
  switch (kind) {
  case ND_ADD:
    if (lt->kind == TY_PTR) {
      if (!is_integer(rval(rhs)->ty))
        error_tok(tok, "invalid operands to %s", op);
    } else if (!is_arith(lt)) {
      error_tok(tok, "invalid operands to %s", op);
    }
    val = new_add(l, rhs, tok);
    break;
  case ND_SUB:
    if (lt->kind == TY_PTR) {
      if (!is_integer(rval(rhs)->ty))
        error_tok(tok, "invalid operands to %s", op);
    } else if (!is_arith(lt)) {
      error_tok(tok, "invalid operands to %s", op);
    }
    val = new_sub(l, rhs, tok);
    break;
  case ND_SHL:
  case ND_SHR:
    val = new_shift(kind, l, rhs, tok, op);
    break;
  default:
    val = new_arith(kind, l, rhs, tok, op);
  }
  Node *a = new_binary(ND_ASSIGN, l, new_cast(val, unqual(lt)), tok);
  a->ty = unqual(lt);
  Node *n = new_binary(ND_COMMA, pre, a, tok);
  n->ty = a->ty;
  return n;
}

/* E1 op= E2 on an atomic object is a single atomic read-modify-write
   (6.5.16.2p3); likewise ++/--.  fetch_first: yield the old value. */
static Node *atomic_compound(Node *lhs, Node *rhs, NodeKind kind, Token *tok, char *op,
                             bool fetch_first) {
  Type *ty = unqual(lhs->ty);
  ty = copy_type(ty);
  ty->qual = 0;
  check_atomic_size(ty, tok);
  Obj *addr = new_lvar("", pointer_to(lhs->ty), tok);
  Obj *val = new_lvar("", rval(rhs)->ty, tok);
  Obj *old = new_lvar("", ty, tok);
  Node *a = new_unary(ND_ADDR, lhs, tok);
  a->ty = pointer_to(lhs->ty);
  Node *set_addr = new_binary(ND_ASSIGN, new_var_node(addr, tok), a, tok);
  set_addr->ty = addr->ty;
  Node *set_val = new_binary(ND_ASSIGN, new_var_node(val, tok), rval(rhs), tok);
  set_val->ty = val->ty;
  Node *o = new_var_node(old, tok);
  Node *v = new_var_node(val, tok);
  Node *nv;
  switch (kind) {
  case ND_ADD:
    if (ty->kind != TY_PTR && !is_arith(ty))
      error_tok(tok, "invalid operands to %s", op);
    nv = new_add(o, v, tok);
    break;
  case ND_SUB:
    if (ty->kind != TY_PTR && !is_arith(ty))
      error_tok(tok, "invalid operands to %s", op);
    nv = new_sub(o, v, tok);
    break;
  case ND_SHL:
  case ND_SHR:
    nv = new_shift(kind, o, v, tok, op);
    break;
  default:
    nv = new_arith(kind, o, v, tok, op);
  }
  nv = new_cast(nv, ty);
  Node *rmw = new_atomic_rmw(new_var_node(addr, tok), old, nv, fetch_first, tok);
  Node *n = new_binary(ND_COMMA, set_addr, set_val, tok);
  n = new_binary(ND_COMMA, n, rmw, tok);
  n->ty = ty;
  return n;
}

/* Prefix ++/-- */
static Node *new_preinc(Node *lhs, int delta, Token *tok) {
  add_type(lhs);
  if (!is_real(lhs->ty) && lhs->ty->kind != TY_PTR)
    error_tok(tok, "wrong type argument to %s", delta > 0 ? "increment" : "decrement");
  if (is_atomic_lvalue(lhs)) {
    is_modifiable_lvalue(lhs, tok, delta > 0 ? "increment operand" : "decrement operand");
    return atomic_compound(lhs, new_num(1, tok), delta > 0 ? ND_ADD : ND_SUB, tok, "++", false);
  }
  return compound_assign(lhs, new_num(1, tok), delta > 0 ? ND_ADD : ND_SUB, tok,
                         delta > 0 ? "++" : "--");
}

/* Postfix ++/--:  (pre, old = L, L = old +- 1, old) */
static Node *new_postinc(Node *lhs, int delta, Token *tok) {
  add_type(lhs);
  if (!is_real(lhs->ty) && lhs->ty->kind != TY_PTR)
    error_tok(tok, "wrong type argument to %s", delta > 0 ? "increment" : "decrement");
  is_modifiable_lvalue(lhs, tok, delta > 0 ? "increment operand" : "decrement operand");
  if (is_atomic_lvalue(lhs))
    return atomic_compound(lhs, new_num(1, tok), delta > 0 ? ND_ADD : ND_SUB, tok, "++", true);
  Node *pre;
  Node *l = stable_lvalue(lhs, &pre, tok);
  Type *ty = unqual(lhs->ty);
  Obj *old = new_lvar("", ty, tok);
  Node *save = new_binary(ND_ASSIGN, new_var_node(old, tok), new_cast(l, ty), tok);
  save->ty = ty;
  Node *val = delta > 0 ? new_add(new_var_node(old, tok), new_num(1, tok), tok)
                        : new_sub(new_var_node(old, tok), new_num(1, tok), tok);
  Node *store = new_binary(ND_ASSIGN, l, new_cast(val, ty), tok);
  store->ty = ty;
  Node *n = new_binary(ND_COMMA, pre, save, tok);
  n = new_binary(ND_COMMA, n, store, tok);
  n = new_binary(ND_COMMA, n, new_var_node(old, tok), tok);
  n->ty = ty;
  /* Equivalent prefix form, used when the value is discarded */
  n->init = new_preinc(lhs, delta, tok);
  return n;
}

/* --------------------------------------------------------- grammar */

Node *expr(Token **rest, Token *tok) {
  Node *node = assign(&tok, tok);
  if (equal(tok, ",")) {
    Token *op = tok;
    Node *rhs = expr(rest, tok->next);
    add_type(node);
    Node *r = rval(rhs);
    Node *n = new_binary(ND_COMMA, node, r, op);
    n->ty = r->ty;
    return n;
  }
  *rest = tok;
  return node;
}

Node *assign(Token **rest, Token *tok) {
  Node *node = conditional(&tok, tok);

  if (equal(tok, "="))
    return new_assign(node, assign(rest, tok->next), tok);

  static struct {
    char *op;
    NodeKind kind;
  } ops[] = {
      {"+=", ND_ADD}, {"-=", ND_SUB}, {"*=", ND_MUL}, {"/=", ND_DIV},
      {"%=", ND_MOD}, {"&=", ND_BITAND}, {"|=", ND_BITOR}, {"^=", ND_BITXOR},
      {"<<=", ND_SHL}, {">>=", ND_SHR},
  };
  for (int i = 0; i < 10; i++)
    if (equal(tok, ops[i].op))
      return compound_assign(node, assign(rest, tok->next), ops[i].kind, tok, ops[i].op);

  *rest = tok;
  return node;
}

/* conditional = logor ("?" expr ":" conditional)? */
Node *conditional(Token **rest, Token *tok) {
  Node *cond = logor(&tok, tok);
  if (!equal(tok, "?")) {
    *rest = tok;
    return cond;
  }
  Token *qtok = tok;
  cond = cond_expr_check(cond, qtok);

  Node *then;
  if (equal(tok->next, ":")) {
    /* GNU extension "a ?: b" == "a ? a : b" with a evaluated once */
    Node *els = conditional(rest, tok->next->next);
    els = rval(els);
    if (!(is_arith(cond->ty) && is_arith(els->ty)) &&
        !(cond->ty->kind == TY_PTR && (els->ty->kind == TY_PTR || is_null_ptr_const(els))))
      error_tok(qtok, "type mismatch in conditional expression");
    Type *ty = is_arith(cond->ty) ? usual_arith_conv(cond->ty, els->ty) : cond->ty;
    Obj *tmp = new_lvar("", ty, qtok);
    Node *set = new_binary(ND_ASSIGN, new_var_node(tmp, qtok), new_cast(cond, ty), qtok);
    set->ty = ty;
    Node *node = new_node(ND_COND, qtok);
    node->cond = set;
    node->then = new_var_node(tmp, qtok);
    node->els = new_cast(els, ty);
    node->ty = ty;
    return node;
  }
  then = expr(&tok, tok->next);
  tok = skip(tok, ":");
  Node *els = conditional(rest, tok);

  then = rval(then);
  els = rval(els);
  Type *t1 = then->ty, *t2 = els->ty;
  Type *ty;

  if (is_arith(t1) && is_arith(t2)) {
    ty = usual_arith_conv(t1, t2);
    then = new_cast(then, ty);
    els = new_cast(els, ty);
  } else if (is_su(t1) && is_su(t2) && is_compatible_unqual(t1, t2)) {
    ty = unqual(t1);
  } else if (is_void(t1) && is_void(t2)) {
    ty = ty_void;
  } else if (is_void(t1) || is_void(t2)) {
    warn_tok(qtok, "only one operand of conditional expression is void");
    ty = ty_void;
  } else if (t1->kind == TY_PTR && t2->kind == TY_PTR) {
    Type *b1 = t1->base, *b2 = t2->base;
    int q = b1->qual | b2->qual;
    if (is_null_ptr_const(els)) {
      ty = t1;
    } else if (is_null_ptr_const(then)) {
      ty = t2;
    } else if (is_void(b1) || is_void(b2)) {
      ty = pointer_to(qualify(ty_void, q));
    } else if (is_compatible_unqual(b1, b2)) {
      ty = pointer_to(qualify(composite_type(unqual(b1), unqual(b2)), q));
    } else {
      warn_tok(qtok, "pointer type mismatch in conditional expression");
      ty = pointer_to(qualify(ty_void, q));
    }
    then = new_cast(then, ty);
    els = new_cast(els, ty);
  } else if (t1->kind == TY_PTR && is_integer(t2)) {
    if (!is_null_ptr_const(els))
      warn_tok(qtok, "pointer/integer type mismatch in conditional expression");
    ty = t1;
    els = new_cast(els, ty);
  } else if (t2->kind == TY_PTR && is_integer(t1)) {
    if (!is_null_ptr_const(then))
      warn_tok(qtok, "pointer/integer type mismatch in conditional expression");
    ty = t2;
    then = new_cast(then, ty);
  } else {
    error_tok(qtok, "type mismatch in conditional expression ('%s' and '%s')", type_name(t1),
              type_name(t2));
  }

  Node *node = new_node(ND_COND, qtok);
  node->cond = cond;
  node->then = then;
  node->els = els;
  node->ty = ty;
  return node;
}

static Node *logand(Token **rest, Token *tok);
static Node *bitor_(Token **rest, Token *tok);
static Node *bitxor_(Token **rest, Token *tok);
static Node *bitand_(Token **rest, Token *tok);
static Node *equality(Token **rest, Token *tok);
static Node *relational(Token **rest, Token *tok);
static Node *shift(Token **rest, Token *tok);
static Node *add(Token **rest, Token *tok);

static Node *logor(Token **rest, Token *tok) {
  Node *node = logand(&tok, tok);
  while (equal(tok, "||")) {
    Token *start = tok;
    Node *rhs = logand(&tok, tok->next);
    node = new_binary(ND_LOGOR, cond_expr_check(node, start), cond_expr_check(rhs, start), start);
    node->ty = ty_int;
  }
  *rest = tok;
  return node;
}

static Node *logand(Token **rest, Token *tok) {
  Node *node = bitor_(&tok, tok);
  while (equal(tok, "&&")) {
    Token *start = tok;
    Node *rhs = bitor_(&tok, tok->next);
    node = new_binary(ND_LOGAND, cond_expr_check(node, start), cond_expr_check(rhs, start), start);
    node->ty = ty_int;
  }
  *rest = tok;
  return node;
}

static Node *bitor_(Token **rest, Token *tok) {
  Node *node = bitxor_(&tok, tok);
  while (equal(tok, "|")) {
    Token *start = tok;
    node = new_arith(ND_BITOR, node, bitxor_(&tok, tok->next), start, "|");
  }
  *rest = tok;
  return node;
}

static Node *bitxor_(Token **rest, Token *tok) {
  Node *node = bitand_(&tok, tok);
  while (equal(tok, "^")) {
    Token *start = tok;
    node = new_arith(ND_BITXOR, node, bitand_(&tok, tok->next), start, "^");
  }
  *rest = tok;
  return node;
}

static Node *bitand_(Token **rest, Token *tok) {
  Node *node = equality(&tok, tok);
  while (equal(tok, "&")) {
    Token *start = tok;
    node = new_arith(ND_BITAND, node, equality(&tok, tok->next), start, "&");
  }
  *rest = tok;
  return node;
}

static Node *equality(Token **rest, Token *tok) {
  Node *node = relational(&tok, tok);
  for (;;) {
    Token *start = tok;
    if (equal(tok, "==")) {
      node = new_compare(ND_EQ, node, relational(&tok, tok->next), start, true, "==");
      continue;
    }
    if (equal(tok, "!=")) {
      node = new_compare(ND_NE, node, relational(&tok, tok->next), start, true, "!=");
      continue;
    }
    *rest = tok;
    return node;
  }
}

static Node *relational(Token **rest, Token *tok) {
  Node *node = shift(&tok, tok);
  for (;;) {
    Token *start = tok;
    if (equal(tok, "<")) {
      node = new_compare(ND_LT, node, shift(&tok, tok->next), start, false, "<");
      continue;
    }
    if (equal(tok, "<=")) {
      node = new_compare(ND_LE, node, shift(&tok, tok->next), start, false, "<=");
      continue;
    }
    if (equal(tok, ">")) {
      Node *rhs = shift(&tok, tok->next);
      node = new_compare(ND_LT, rhs, node, start, false, ">");
      continue;
    }
    if (equal(tok, ">=")) {
      Node *rhs = shift(&tok, tok->next);
      node = new_compare(ND_LE, rhs, node, start, false, ">=");
      continue;
    }
    *rest = tok;
    return node;
  }
}

static Node *shift(Token **rest, Token *tok) {
  Node *node = add(&tok, tok);
  for (;;) {
    Token *start = tok;
    if (equal(tok, "<<")) {
      node = new_shift(ND_SHL, node, add(&tok, tok->next), start, "<<");
      continue;
    }
    if (equal(tok, ">>")) {
      node = new_shift(ND_SHR, node, add(&tok, tok->next), start, ">>");
      continue;
    }
    *rest = tok;
    return node;
  }
}

static Node *add(Token **rest, Token *tok) {
  Node *node = mul(&tok, tok);
  for (;;) {
    Token *start = tok;
    if (equal(tok, "+")) {
      node = new_add(node, mul(&tok, tok->next), start);
      continue;
    }
    if (equal(tok, "-")) {
      node = new_sub(node, mul(&tok, tok->next), start);
      continue;
    }
    *rest = tok;
    return node;
  }
}

static Node *mul(Token **rest, Token *tok) {
  Node *node = cast(&tok, tok);
  for (;;) {
    Token *start = tok;
    if (equal(tok, "*")) {
      node = new_arith(ND_MUL, node, cast(&tok, tok->next), start, "*");
      continue;
    }
    if (equal(tok, "/")) {
      node = new_arith(ND_DIV, node, cast(&tok, tok->next), start, "/");
      continue;
    }
    if (equal(tok, "%")) {
      node = new_arith(ND_MOD, node, cast(&tok, tok->next), start, "%");
      continue;
    }
    *rest = tok;
    return node;
  }
}

static Node *compound_literal(Token **rest, Token *tok, Type *ty, Token *start);

/* An explicit cast (6.5.4) */
static Node *explicit_cast(Node *e, Type *ty, Token *tok) {
  e = rval(e);
  if (is_void(ty)) {
    Node *n = new_cast(e, ty_void);
    return n;
  }
  if (is_su(ty) && is_su(e->ty) && is_compatible_unqual(ty, e->ty)) {
    warn_tok(tok, "ISO C forbids casting nonscalar to the same type");
    return e;
  }
  if (!is_scalar(ty))
    error_tok(tok, "conversion to non-scalar type '%s' requested", type_name(ty));
  if (!is_scalar(e->ty))
    error_tok(tok, "%s type '%s' where scalar is required",
              is_void(e->ty) ? "void value of" : "aggregate value of", type_name(e->ty));
  if ((ty->kind == TY_PTR && is_flonum(e->ty)) || (is_flonum(ty) && e->ty->kind == TY_PTR) ||
      (ty->kind == TY_PTR && e->ty->is_complex) || (ty->is_complex && e->ty->kind == TY_PTR))
    error_tok(tok, "invalid cast between pointer and floating type");
  Node *n = new_cast(e, ty);
  /* A cast is never an lvalue and drops qualifiers */
  return n;
}

static Node *cast(Token **rest, Token *tok) {
  if (equal(tok, "(") && is_typename(tok->next)) {
    Token *start = tok;
    Type *ty = typename(&tok, tok->next);
    tok = skip(tok, ")");

    /* compound literal */
    if (equal(tok, "{"))
      return postfix(rest, start);

    Token *etok = tok;
    Node *e = cast(rest, tok);
    if (is_vm(ty)) {
      Node *code = compute_vla_size(ty, start);
      Node *n = explicit_cast(e, ty, etok);
      Node *c = new_binary(ND_COMMA, code, n, start);
      c->ty = n->ty;
      return c;
    }
    return explicit_cast(e, ty, start);
  }
  return unary(rest, tok);
}

static Node *new_deref(Node *e, Token *tok) {
  e = rval(e);
  if (e->ty->kind != TY_PTR)
    error_tok(tok, "invalid type argument of unary '*' (have '%s')", type_name(e->ty));
  Node *n = new_unary(ND_DEREF, e, tok);
  n->ty = e->ty->base;
  return n;
}

static Node *new_addr(Node *e, Token *tok) {
  add_type(e);
  /* &*E is E (6.5.3.2p3), &a[i] is a+i (without the dereference) */
  if (e->kind == ND_DEREF) {
    Node *p = e->lhs;
    Node *n = new_cast(p, pointer_to(e->ty));
    return n;
  }
  if (e->kind == ND_MEMBER && e->member->is_bitfield)
    error_tok(tok, "cannot take address of bit-field '%.*s'", e->member->name->len,
              e->member->name->loc);
  if (e->kind == ND_VAR && e->var->is_register)
    error_tok(tok, "address of register variable '%s' requested", e->var->name);
  bool lv = e->kind == ND_MEMBER ? member_is_lvalue(e) : is_lvalue(e);
  if (!lv && !is_func(e->ty))
    error_tok(tok, "lvalue required as unary '&' operand");
  if (e->kind == ND_MEMBER) {
    Node *b = e;
    while (b->kind == ND_MEMBER)
      b = b->lhs;
    if (b->kind == ND_VAR && b->var->is_register)
      error_tok(tok, "address of register variable '%s' requested", b->var->name);
  }
  if (e->kind == ND_VAR) {
    e->var->is_addr_taken = true;
    if (e->var->is_function)
      add_fn_ref(e->var);
  }
  Node *n = new_unary(ND_ADDR, e, tok);
  n->ty = pointer_to(e->ty);
  return n;
}

static Node *sizeof_type(Type *ty, Token *tok, Node *pre) {
  if (is_func(ty))
    error_tok(tok, "invalid application of 'sizeof' to a function type");
  if (ty->kind == TY_VLA) {
    if (!ty->vla_size)
      error_tok(tok, "invalid application of 'sizeof' to a variable length array of unspecified size");
    Node *n = new_var_node(ty->vla_size, tok);
    if (pre) {
      Node *c = new_binary(ND_COMMA, pre, n, tok);
      c->ty = ty_ulong;
      return c;
    }
    return n;
  }
  if (!is_complete(ty))
    error_tok(tok, "invalid application of 'sizeof' to incomplete type '%s'", type_name(ty));
  return new_ulong(type_size(ty), tok);
}

static Node *unary(Token **rest, Token *tok) {
  if (equal(tok, "+")) {
    Node *e = arith_operand(cast(rest, tok->next), tok, "unary +");
    return new_cast(e, int_promote(e->ty));
  }

  if (equal(tok, "-")) {
    Node *e = arith_operand(cast(rest, tok->next), tok, "unary -");
    Type *ty = int_promote(e->ty);
    Node *n = new_unary(ND_NEG, new_cast(e, ty), tok);
    n->ty = ty;
    cx_tmp(n);
    return fold(n);
  }

  if (equal(tok, "&"))
    return new_addr(cast(rest, tok->next), tok);

  if (equal(tok, "*")) {
    Node *e = cast(rest, tok->next);
    return new_deref(e, tok);
  }

  if (equal(tok, "!")) {
    Node *e = cond_expr_check(cast(rest, tok->next), tok);
    Node *n = new_unary(ND_NOT, e, tok);
    n->ty = ty_int;
    return fold(n);
  }

  if (equal(tok, "~")) {
    Node *e = int_operand(cast(rest, tok->next), tok, "unary ~");
    Type *ty = int_promote(e->ty);
    Node *n = new_unary(ND_BITNOT, new_cast(e, ty), tok);
    n->ty = ty;
    return fold(n);
  }

  if (equal(tok, "++"))
    return new_preinc(unary(rest, tok->next), 1, tok);
  if (equal(tok, "--"))
    return new_preinc(unary(rest, tok->next), -1, tok);

  if (equal(tok, "sizeof")) {
    if (equal(tok->next, "(") && is_typename(tok->next->next)) {
      Token *start = tok->next;
      Type *ty = typename(&tok, tok->next->next);
      tok = skip(tok, ")");
      if (equal(tok, "{")) {
        /* sizeof (T){...} */
        Node *cl = compound_literal(&tok, tok, ty, start);
        cl = postfix_rest_hook(&tok, tok, cl);
        *rest = tok;
        add_type(cl);
        return sizeof_type(cl->ty, start, NULL);
      }
      *rest = tok;
      Node *pre = is_vm(ty) ? compute_vla_size(ty, start) : NULL;
      return sizeof_type(ty, start, pre);
    }
    Token *start = tok;
    Node *e = unary(rest, tok->next);
    add_type(e);
    if (e->kind == ND_MEMBER && e->member->is_bitfield)
      error_tok(start, "'sizeof' applied to a bit-field");
    if (e->ty->kind == TY_VLA) {
      /* The operand is evaluated (6.5.3.4p2). */
      Node *c = new_binary(ND_COMMA, new_cast(e->kind == ND_VAR ? new_num(0, start) : rval(e), ty_void),
                           sizeof_type(e->ty, start, NULL), start);
      c->ty = ty_ulong;
      return c;
    }
    return sizeof_type(e->ty, start, NULL);
  }

  if (equal(tok, "_Alignof") || equal(tok, "__alignof__") || equal(tok, "__alignof")) {
    Token *start = tok;
    if (equal(tok->next, "(") && is_typename(tok->next->next)) {
      Type *ty = typename(&tok, tok->next->next);
      *rest = skip(tok, ")");
      if (is_func(ty))
        error_tok(start, "invalid application of '_Alignof' to a function type");
      if (!is_complete(ty))
        error_tok(start, "invalid application of '_Alignof' to incomplete type '%s'",
                  type_name(ty));
      while (is_array(ty))
        ty = ty->base;
      return new_ulong(type_align(ty), start);
    }
    if (equal(tok, "_Alignof"))
      warn_tok(tok->next, "ISO C does not allow '_Alignof (expression)'");
    Node *e = unary(rest, tok->next);
    add_type(e);
    if (e->kind == ND_VAR)
      return new_ulong(e->var->align, start);
    return new_ulong(type_align(e->ty), start);
  }

  if (equal(tok, "__extension__"))
    return cast(rest, tok->next);

  if (equal(tok, "__real__") || equal(tok, "__real") || equal(tok, "__imag__") ||
      equal(tok, "__imag")) {
    bool imag = equal(tok, "__imag__") || equal(tok, "__imag");
    Node *e = cast(rest, tok->next);
    add_type(e);
    if (!is_arith(e->ty))
      error_tok(tok, "wrong type argument to %s", imag ? "__imag__" : "__real__");
    if (!e->ty->is_complex) {
      e = rval(e);
      if (imag) {
        Node *z = new_cast(new_num(0, tok), e->ty);
        Node *c = new_binary(ND_COMMA, new_cast(e, ty_void), z, tok);
        c->ty = z->ty;
        return c;
      }
      return new_cast(e, e->ty);
    }
    /* A member access to the synthetic re/im members */
    Member *m = struct_members(e->ty);
    if (imag)
      m = m->next;
    Node *n = new_unary(ND_MEMBER, e, tok);
    n->member = m;
    n->ty = qualify(m->ty, e->ty->qual);
    return n;
  }

  return postfix(rest, tok);
}

/* ------------------------------------------------- struct members */

Member *get_struct_member(Type *ty, Token *tok) {
  for (Member *mem = struct_members(ty); mem; mem = mem->next) {
    if (mem->is_anon) {
      if (get_struct_member(mem->ty, tok))
        return mem;
      continue;
    }
    if (mem->name && mem->name->len == tok->len && !strncmp(mem->name->loc, tok->loc, tok->len))
      return mem;
  }
  return NULL;
}

/* Builds a member access, going through anonymous members. */
Node *struct_ref(Node *node, Token *tok) {
  add_type(node);
  if (!is_su(node->ty))
    error_tok(tok, "request for member '%.*s' in something not a structure or union", tok->len,
              tok->loc);
  if (!is_complete(node->ty))
    error_tok(tok, "member access into incomplete type '%s'", type_name(node->ty));

  Type *ty = node->ty;
  int q = node->ty->qual;
  for (;;) {
    Member *mem = get_struct_member(ty, tok);
    if (!mem)
      error_tok(tok, "'%s' has no member named '%.*s'", type_name(unqual(node->ty)), tok->len,
                tok->loc);
    node = new_unary(ND_MEMBER, node, tok);
    node->member = mem;
    node->ty = qualify(mem->ty, q);
    q |= mem->ty->qual;
    if (mem->name)
      break;
    ty = mem->ty;
  }
  return node;
}

/* --------------------------------------------------------- calls */

static Node *default_promote(Node *arg) {
  arg = rval(arg);
  if (arg->ty->kind == TY_FLOAT && !arg->ty->is_complex)
    return new_cast(arg, ty_double);
  if (is_integer(arg->ty))
    return new_cast(arg, int_promote(arg->ty));
  return arg;
}

static Node *funcall(Token **rest, Token *tok, Node *fn) {
  Token *start = tok;
  fn = rval(fn);
  if (fn->ty->kind != TY_PTR || !is_func(fn->ty->base))
    error_tok(fn->tok, "called object is not a function or function pointer");

  Type *ty = fn->ty->base;
  Param *param = ty->params;
  Node head = {0};
  Node *cur = &head;
  int nargs = 0;
  int nparams = 0;
  for (Param *p = ty->params; p; p = p->next)
    nparams++;

  tok = tok->next;
  while (!equal(tok, ")")) {
    if (cur != &head)
      tok = skip(tok, ",");
    Token *atok = tok;
    Node *arg = assign(&tok, tok);
    add_type(arg);
    nargs++;

    if (ty->has_proto && param) {
      Type *pt = unqual(param->ty);
      char *what = "passing argument to parameter";
      arg = convert_for_assign(arg, pt, atok, what);
      param = param->next;
    } else {
      if (ty->has_proto && !ty->is_variadic)
        error_tok(atok, "too many arguments to function");
      arg = rval(arg);
      if (is_void(arg->ty))
        error_tok(atok, "invalid use of void expression");
      if (!is_complete(arg->ty))
        error_tok(atok, "argument has incomplete type");
      arg = default_promote(arg);
    }
    cur = cur->next = arg;
  }
  if (ty->has_proto && param)
    error_tok(start, "too few arguments to function (expected %d, have %d)", nparams, nargs);

  *rest = skip(tok, ")");

  Type *rty = ty->return_ty;
  if (!is_void(rty) && !is_object_complete(rty))
    error_tok(start, "calling function with incomplete return type '%s'", type_name(rty));

  Node *node = new_unary(ND_FUNCALL, fn, start);
  node->func_ty = ty;
  node->ty = unqual(rty);
  node->args = head.next;

  /* A buffer for a struct/union/complex return value */
  if (is_su(node->ty) || node->ty->is_complex)
    node->ret_buffer = new_lvar("", node->ty, start);
  return node;
}

/* ------------------------------------------------------ _Generic */

static Node *generic_selection(Token **rest, Token *tok) {
  Token *start = tok;
  tok = skip(tok->next, "(");

  Node *ctrl = assign(&tok, tok);
  add_type(ctrl);
  Type *t1 = ctrl->ty;
  /* lvalue conversion: drop qualifiers; decay arrays/functions */
  if (is_array(t1) || is_func(t1))
    t1 = decay(t1);
  t1 = unqual(t1);

  Node *ret = NULL;
  Node *def = NULL;
  Type **seen = NULL;
  int nseen = 0;
  while (!equal(tok, ")")) {
    tok = skip(tok, ",");
    if (equal(tok, "default")) {
      if (def)
        error_tok(tok, "duplicate default generic association");
      tok = skip(tok->next, ":");
      def = assign(&tok, tok);
      continue;
    }
    Token *ttok = tok;
    Type *t2 = typename(&tok, tok);
    if (!is_object_complete(t2) || is_vm(t2))
      error_tok(ttok, "'_Generic' association has incomplete or variably modified type '%s'",
                type_name(t2));
    for (int i = 0; i < nseen; i++)
      if (is_compatible(seen[i], t2))
        error_tok(ttok, "'_Generic' specifies two compatible types");
    seen = xrealloc(seen, sizeof(Type *) * (nseen + 1));
    seen[nseen++] = t2;
    tok = skip(tok, ":");
    Node *node = assign(&tok, tok);
    if (is_compatible(t1, t2))
      ret = node;
  }
  *rest = tok->next;

  if (!ret)
    ret = def;
  if (!ret)
    error_tok(start, "'_Generic' selector of type '%s' is not compatible with any association",
              type_name(t1));
  return ret;
}

/* --------------------------------------------- compound literals */

static Node *compound_literal(Token **rest, Token *tok, Type *ty, Token *start) {
  if (is_vm(ty) || ty->kind == TY_VLA)
    error_tok(start, "compound literal has variable size");
  if (is_func(ty))
    error_tok(start, "compound literal of function type");
  if (!is_object_complete(ty) && !(ty->kind == TY_ARRAY && ty->array_len < 0))
    error_tok(start, "compound literal has incomplete type");

  if (!current_fn || scope->is_file) {
    Obj *var = new_anon_gvar(ty);
    var->is_compound_lit = true;
    var->tok = start;
    gvar_initializer(rest, tok, var);
    var->is_definition = true;
    return new_var_node(var, start);
  }

  Obj *var = new_lvar("", ty, start);
  var->is_compound_lit = true;
  Node *lhs = lvar_initializer(rest, tok, var);
  Node *rhs = new_var_node(var, start);
  Node *n = new_binary(ND_COMMA, lhs, rhs, start);
  n->ty = var->ty;
  /* keep it an lvalue: (init, var) - expose var for & and member access */
  n->var = var;
  return n;
}

/* ----------------------------------------------------- postfix */

static Node *postfix_ops(Token **rest, Token *tok, Node *node);

Node *postfix_rest_hook(Token **rest, Token *tok, Node *node) {
  return postfix_ops(rest, tok, node);
}

static Node *postfix_ops(Token **rest, Token *tok, Node *node) {
  for (;;) {
    if (equal(tok, "(")) {
      node = funcall(&tok, tok, node);
      continue;
    }

    if (equal(tok, "[")) {
      /* x[y] is short for *(x+y) */
      Token *start = tok;
      Node *idx = expr(&tok, tok->next);
      tok = skip(tok, "]");
      Node *b = rval(node);
      Node *i = rval(idx);
      if (!(b->ty->kind == TY_PTR && is_integer(i->ty)) &&
          !(i->ty->kind == TY_PTR && is_integer(b->ty)))
        error_tok(start, "subscripted value is neither array nor pointer");
      Node *p = new_add(b, i, start);
      if (!is_object_complete(p->ty->base) && p->ty->base->kind != TY_VLA)
        error_tok(start, "subscript of pointer to incomplete type '%s'", type_name(p->ty->base));
      node = new_deref(p, start);
      continue;
    }

    if (equal(tok, ".")) {
      if (tok->next->kind != TK_IDENT)
        error_tok(tok->next, "expected identifier after '.'");
      node = struct_ref(node, tok->next);
      tok = tok->next->next;
      continue;
    }

    if (equal(tok, "->")) {
      Node *p = rval(node);
      if (p->ty->kind != TY_PTR || !is_su(p->ty->base))
        error_tok(tok, "invalid type argument of '->' (have '%s')", type_name(p->ty));
      if (tok->next->kind != TK_IDENT)
        error_tok(tok->next, "expected identifier after '->'");
      node = new_deref(p, tok);
      node = struct_ref(node, tok->next);
      tok = tok->next->next;
      continue;
    }

    if (equal(tok, "++")) {
      node = new_postinc(node, 1, tok);
      tok = tok->next;
      continue;
    }

    if (equal(tok, "--")) {
      node = new_postinc(node, -1, tok);
      tok = tok->next;
      continue;
    }

    *rest = tok;
    return node;
  }
}

static Node *postfix(Token **rest, Token *tok) {
  if (equal(tok, "(") && is_typename(tok->next)) {
    /* Compound literal */
    Token *start = tok;
    Type *ty = typename(&tok, tok->next);
    tok = skip(tok, ")");
    if (!equal(tok, "{"))
      error_tok(tok, "expected '{'");
    Node *node = compound_literal(&tok, tok, ty, start);
    return postfix_ops(rest, tok, node);
  }
  Node *node = primary(&tok, tok);
  return postfix_ops(rest, tok, node);
}

/* ----------------------------------------------------- primary */

static Node *builtin_va_start(Token **rest, Token *tok) {
  Token *start = tok;
  tok = skip(tok->next, "(");
  Node *ap = assign(&tok, tok);
  /* the last named parameter is not needed (C23 allows leaving it out) */
  if (consume(&tok, tok, ","))
    assign(&tok, tok);
  *rest = skip(tok, ")");
  if (!current_fn || !current_fn->ty->is_variadic)
    error_tok(start, "'va_start' used in function with fixed arguments");
  Node *n = new_unary(ND_VA_START, rval(ap), start);
  n->ty = ty_void;
  return n;
}

static Node *builtin_va_arg(Token **rest, Token *tok) {
  Token *start = tok;
  tok = skip(tok->next, "(");
  Node *ap = assign(&tok, tok);
  tok = skip(tok, ",");
  Type *ty = typename(&tok, tok);
  *rest = skip(tok, ")");
  if (!is_object_complete(ty))
    error_tok(start, "second argument to 'va_arg' is of incomplete type");
  if (is_array(ty) || is_func(ty))
    error_tok(start, "second argument to 'va_arg' is of invalid type");
  Node *n = new_unary(ND_VA_ARG, rval(ap), start);
  n->ty = unqual(ty);
  if (is_su(ty))  /* temporary for register-passed parts: whole eightbytes */
    n->var = new_lvar("", array_of(ty_long, (type_size(ty) + 7) / 8 + 1), start);
  /* va_arg produces an rvalue; for aggregates, the node yields an address */
  return n;
}

static Node *builtin_offsetof(Token **rest, Token *tok) {
  Token *start = tok;
  tok = skip(tok->next, "(");
  Type *ty = typename(&tok, tok);
  tok = skip(tok, ",");
  if (!is_su(ty))
    error_tok(start, "offsetof requires a struct or union type");
  if (!is_complete(ty))
    error_tok(start, "offsetof of incomplete type");
  int64_t off = 0;
  Type *cur = ty;
  bool first = true;
  for (;;) {
    if (first || equal(tok, ".")) {
      if (!first)
        tok = tok->next;
      first = false;
      if (tok->kind != TK_IDENT)
        error_tok(tok, "expected identifier");
      if (!is_su(cur))
        error_tok(tok, "request for member in something not a structure or union");
      Type *t = cur;
      for (;;) {
        Member *m = get_struct_member(t, tok);
        if (!m)
          error_tok(tok, "no member named '%.*s'", tok->len, tok->loc);
        if (m->is_bitfield)
          error_tok(tok, "cannot apply offsetof to a bit-field");
        off += m->offset;
        t = m->ty;
        if (m->name)
          break;
      }
      cur = t;
      tok = tok->next;
      continue;
    }
    if (equal(tok, "[")) {
      if (!is_array(cur))
        error_tok(tok, "subscripted value is not an array");
      Token *itok = tok->next;
      Node *e = expr(&tok, tok->next);
      int64_t i = ice_value(e, itok, "array index in offsetof");
      tok = skip(tok, "]");
      off += i * type_size(cur->base);
      cur = cur->base;
      continue;
    }
    break;
  }
  *rest = skip(tok, ")");
  return new_ulong(off, start);
}

/* <stdatomic.h> generic functions (all sequentially consistent) */
static Node *atomic_builtin(Token **rest, Token *tok) {
  Token *start = tok;
  char *name = tok_text(tok) + strlen("__c17_atomic_");
  tok = skip(tok->next, "(");
  Node *args[3] = {0};
  int n = 0;
  while (!equal(tok, ")")) {
    if (n)
      tok = skip(tok, ",");
    if (n == 3)
      error_tok(tok, "too many arguments to %s", tok_text(start));
    args[n++] = assign(&tok, tok);
  }
  *rest = skip(tok, ")");

  if (!strcmp(name, "fence") || !strcmp(name, "signal_fence")) {
    if (n != 1)
      error_tok(start, "wrong number of arguments");
    Node *f = new_node(ND_FENCE, start);
    f->lhs = new_cast(rval(args[0]), ty_void);
    f->val = !strcmp(name, "fence");
    f->ty = ty_void;
    return f;
  }

  if (n < 1)
    error_tok(start, "too few arguments to %s", tok_text(start));
  Node *p = rval(args[0]);
  if (p->ty->kind != TY_PTR || is_func(p->ty->base) || is_void(p->ty->base))
    error_tok(start, "first argument of %s must be a pointer to an atomic object", tok_text(start));
  Type *at = p->ty->base;
  Type *ty = copy_type(unqual(at));
  ty->qual = 0;
  check_atomic_size(ty, start);

  if (!strcmp(name, "load")) {
    if (n != 1)
      error_tok(start, "wrong number of arguments");
    Node *r = new_unary(ND_ATOMIC_LOAD, p, start);
    r->ty = ty;
    return r;
  }
  if (!strcmp(name, "store") || !strcmp(name, "exchange")) {
    if (n != 2)
      error_tok(start, "wrong number of arguments");
    Node *r = new_binary(name[0] == 's' ? ND_ATOMIC_STORE : ND_EXCH, p,
                         convert_for_assign(args[1], ty, start, "passing"), start);
    r->ty = name[0] == 's' ? ty_void : ty;
    return r;
  }
  if (!strcmp(name, "compare_exchange")) {
    if (n != 3)
      error_tok(start, "wrong number of arguments");
    Node *exp = rval(args[1]);
    if (exp->ty->kind != TY_PTR || type_size(exp->ty->base) != type_size(ty))
      error_tok(start, "second argument of compare_exchange must point to the expected value");
    Node *r = new_node(ND_CAS, start);
    r->cas_addr = p;
    r->cas_old = exp;
    r->cas_new = convert_for_assign(args[2], ty, start, "passing");
    r->ty = ty_bool;
    return r;
  }
  NodeKind kind;
  if (!strcmp(name, "fetch_add")) kind = ND_ADD;
  else if (!strcmp(name, "fetch_sub")) kind = ND_SUB;
  else if (!strcmp(name, "fetch_and")) kind = ND_BITAND;
  else if (!strcmp(name, "fetch_or")) kind = ND_BITOR;
  else if (!strcmp(name, "fetch_xor")) kind = ND_BITXOR;
  else
    error_tok(start, "unknown atomic builtin %s", tok_text(start));
  if (n != 2)
    error_tok(start, "wrong number of arguments");
  if (!is_integer(ty) && !(ty->kind == TY_PTR && (kind == ND_ADD || kind == ND_SUB)))
    error_tok(start, "%s requires an atomic integer type", tok_text(start));
  Node *d = new_unary(ND_DEREF, p, start);
  d->ty = qualify(ty, Q_ATOMIC);
  return atomic_compound(d, args[1], kind, start, name, true);
}

static Obj *func_name_var(Token *tok) {
  /* static const char __func__[] = "function-name"; */
  static Obj *last_fn;
  static Obj *last_var;
  if (last_fn == current_fn && last_var)
    return last_var;
  char *nm = current_fn->name;
  int len = strlen(nm);
  Type *ty = array_of(qualify(ty_char, Q_CONST), len + 1);
  Obj *var = new_anon_gvar(ty);
  var->init_data = xstrdup(nm);
  var->tok = tok;
  var->is_string_lit = true;
  last_fn = current_fn;
  last_var = var;
  return var;
}

static Node *primary(Token **rest, Token *tok) {
  Token *start = tok;

  if (equal(tok, "(") && equal(tok->next, "{")) {
    /* GNU statement expression */
    if (!current_fn)
      error_tok(tok, "braced-group within expression allowed only inside a function");
    Node *node = new_node(ND_STMT_EXPR, tok);
    node->body = compound_stmt(&tok, tok->next->next, true)->body;
    *rest = skip(tok, ")");
    add_type(node);
    if (node->ty && is_array(node->ty))
      node->ty = decay(node->ty);
    return node;
  }

  if (equal(tok, "(")) {
    Node *node = expr(&tok, tok->next);
    *rest = skip(tok, ")");
    return node;
  }

  if (equal(tok, "_Generic"))
    return generic_selection(rest, tok);

  if (equal(tok, "__builtin_va_start"))
    return builtin_va_start(rest, tok);
  if (equal(tok, "__builtin_va_arg"))
    return builtin_va_arg(rest, tok);
  if (equal(tok, "__builtin_va_end")) {
    tok = skip(tok->next, "(");
    Node *ap = assign(&tok, tok);
    *rest = skip(tok, ")");
    return new_cast(rval(ap), ty_void);
  }
  if (equal(tok, "__builtin_va_copy")) {
    tok = skip(tok->next, "(");
    Node *dst = assign(&tok, tok);
    tok = skip(tok, ",");
    Node *src = assign(&tok, tok);
    *rest = skip(tok, ")");
    Node *n = new_binary(ND_VA_COPY, rval(dst), rval(src), start);
    n->ty = ty_void;
    return n;
  }
  if (equal(tok, "__builtin_offsetof"))
    return builtin_offsetof(rest, tok);
  if (tok->kind == TK_IDENT && startswith(tok->loc, "__c17_atomic_") && equal(tok->next, "("))
    return atomic_builtin(rest, tok);
  if (equal(tok, "__builtin_complex")) {
    tok = skip(tok->next, "(");
    Node *re = rval(assign(&tok, tok));
    tok = skip(tok, ",");
    Node *im = rval(assign(&tok, tok));
    *rest = skip(tok, ")");
    if (!is_flonum(re->ty) || !is_flonum(im->ty) || re->ty->kind != im->ty->kind)
      error_tok(start, "__builtin_complex requires two real floating arguments of the same type");
    Node *n = new_binary(ND_MKCOMPLEX, re, im, start);
    n->ty = complex_of(re->ty);
    return cx_tmp(n);
  }
  if (equal(tok, "__builtin_types_compatible_p")) {
    tok = skip(tok->next, "(");
    Type *t1 = typename(&tok, tok);
    tok = skip(tok, ",");
    Type *t2 = typename(&tok, tok);
    *rest = skip(tok, ")");
    return new_num(is_compatible_unqual(t1, t2), start);
  }

  if (tok->kind == TK_IDENT) {
    VarScope *sc = find_var(tok);
    *rest = tok->next;

    if (sc) {
      if (sc->var) {
        Obj *v = sc->var;
        v->is_used = true;
        if (v->is_function)
          add_fn_ref(v);
        Node *n = new_var_node(v, tok);
        return n;
      }
      if (sc->enum_ty) {
        Node *n = new_num(sc->enum_val, tok);
        n->ty = sc->enum_ty;
        return n;
      }
      if (sc->type_def)
        error_tok(tok, "unexpected type name '%.*s': expected expression", tok->len, tok->loc);
    }

    if (equal(tok, "__func__") || equal(tok, "__FUNCTION__")) {
      if (!current_fn)
        error_tok(tok, "'__func__' is not defined outside of function scope");
      return new_var_node(func_name_var(tok), tok);
    }

    /* A few GCC builtins as plain identifiers */
    if (equal(tok, "__builtin_expect") && equal(tok->next, "(")) {
      tok = tok->next->next;
      Node *e = assign(&tok, tok);
      tok = skip(tok, ",");
      assign(&tok, tok);
      *rest = skip(tok, ")");
      return e;
    }
    /* frames: c17 keeps a frame pointer, so level n is n links up the
       chain of saved %rbp; the return address is the word above */
    if ((equal(tok, "__builtin_return_address") || equal(tok, "__builtin_frame_address")) &&
        equal(tok->next, "(")) {
      bool ret = equal(tok, "__builtin_return_address");
      Token *t = tok->next->next;
      int64_t level = const_expr(&t, t);
      if (level < 0)
        error_tok(tok, "the level must not be negative");
      *rest = skip(t, ")");
      Node *n = new_node(ND_FRAMEADDR, tok);
      n->val = level;
      n->rmw_op = ret;
      n->ty = pointer_to(ty_void);
      return n;
    }
    if (equal(tok, "__builtin_unreachable") && equal(tok->next, "(")) {
      *rest = skip(skip(tok->next, "("), ")");
      Node *n = new_node(ND_UNREACHABLE, tok);
      n->ty = ty_void;
      return n;
    }
    if (equal(tok, "__builtin_constant_p") && equal(tok->next, "(")) {
      tok = tok->next->next;
      Node *e = assign(&tok, tok);
      *rest = skip(tok, ")");
      return new_num(is_ice(e) || is_const_expr(e), start);
    }
    /* infinities and quiet NaNs as constants (for <math.h>'s INFINITY,
       NAN and HUGE_VAL): unlike 1.0/0.0 evaluated when run, they raise no
       floating-point exception */
    {
      static const struct { const char *name; int kind; int ty; } fb[] = {
          {"__builtin_inf", 0, 1},       {"__builtin_inff", 0, 0},
          {"__builtin_infl", 0, 2},      {"__builtin_huge_val", 0, 1},
          {"__builtin_huge_valf", 0, 0}, {"__builtin_huge_vall", 0, 2},
          {"__builtin_nan", 1, 1},       {"__builtin_nanf", 1, 0},
          {"__builtin_nanl", 1, 2},
      };
      for (size_t i = 0; i < sizeof fb / sizeof fb[0]; i++) {
        if (!equal(tok, (char *)fb[i].name) || !equal(tok->next, "("))
          continue;
        Token *t = tok->next->next;
        if (fb[i].kind) {   /* the argument: a string; only "" is supported */
          if (t->kind != TK_STR)
            error_tok(t, "%s: expected a string literal", fb[i].name);
          if (t->ty->array_len > 1)
            error_tok(t, "%s: only an empty string is supported", fb[i].name);
          t = t->next;
        }
        *rest = skip(t, ")");
        Node *n = new_node(ND_NUM, start);
        n->fval = fb[i].kind ? fp_nan_value() : fp_inf_value();
        n->ty = fb[i].ty == 0 ? ty_float : fb[i].ty == 1 ? ty_double : ty_ldouble;
        return n;
      }
    }
    if (equal(tok, "__builtin_alloca") && equal(tok->next, "(")) {
      tok = tok->next->next;
      Node *e = assign(&tok, tok);
      *rest = skip(tok, ")");
      Node *n = new_unary(ND_ALLOCA, new_cast(rval(e), ty_ulong), start);
      n->ty = pointer_to(ty_void);
      return n;
    }

    if (equal(tok->next, "("))
      error_tok(tok, "implicit declaration of function '%.*s'", tok->len, tok->loc);
    error_tok(tok, "'%.*s' undeclared", tok->len, tok->loc);
  }

  if (tok->kind == TK_STR) {
    Obj *var = new_string_literal(tok);
    *rest = tok->next;
    return new_var_node(var, tok);
  }

  if (tok->kind == TK_NUM) {
    Node *node;
    if (tok->ty->is_complex) {
      /* GNU imaginary constant: 0 + fval*i */
      node = new_node(ND_NUM, tok);
      node->fval = 0;
      node->fimag = tok->fval;
      node->ty = tok->ty;
      *rest = tok->next;
      return cx_tmp(node);
    } else if (is_flonum(tok->ty)) {
      node = new_node(ND_NUM, tok);
      node->fval = tok->fval;
    } else {
      node = new_num(tok->val, tok);
    }
    node->ty = tok->ty;
    *rest = tok->next;
    return node;
  }

  if (tok->kind == TK_EOF)
    error_tok(tok, "expected expression at end of input");
  error_tok(tok, "expected expression before '%.*s'", tok->len, tok->loc);
}

/* For the preprocessor-independent uses (array sizes etc.) */
int64_t const_expr(Token **rest, Token *tok) {
  Token *start = tok;
  Node *node = conditional(rest, tok);
  return ice_value(node, start, "expression");
}
