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

/* eval.c - constant expressions (6.6) */
#include "parse.h"

static int64_t eval_rval(Node *node, char ***label);

/* Truncates v to the integer type ty. */
static int64_t wrap(int64_t v, Type *ty) {
  if (!is_integer(ty) && ty->kind != TY_PTR)
    return v;
  if (ty->kind == TY_BOOL)
    return v != 0;
  switch (type_size(ty)) {
  case 1: return ty->is_unsigned ? (int64_t)(uint8_t)v : (int64_t)(int8_t)v;
  case 2: return ty->is_unsigned ? (int64_t)(uint16_t)v : (int64_t)(int16_t)v;
  case 4: return ty->is_unsigned ? (int64_t)(uint32_t)v : (int64_t)(int32_t)v;
  default: return v;
  }
}

int64_t eval(Node *node) { return eval2(node, NULL); }

static void need_no_label(Node *node, char ***label, char ***l2) {
  (void)label;
  if (l2 && *l2)
    error_tok(node->tok, "initializer element is not computable at load time");
}

/* Evaluates a constant expression: a number, or a label (address of a
   static object or function) plus an offset when label is non-NULL. */
int64_t eval2(Node *node, char ***label) {
  add_type(node);

  if ((is_flonum(node->ty) || node->ty->is_complex) && node->kind != ND_CAST)
    return (int64_t)eval_double(node);

  switch (node->kind) {
  case ND_ADD: {
    char **l1 = NULL, **l2 = NULL;
    int64_t a = eval2(node->lhs, label ? &l1 : NULL);
    int64_t b = eval2(node->rhs, label ? &l2 : NULL);
    if (l1 && l2)
      error_tok(node->tok, "initializer element is not computable at load time");
    if (label)
      *label = l1 ? l1 : l2;
    return wrap((int64_t)((uint64_t)a + (uint64_t)b), node->ty);
  }
  case ND_SUB: {
    char **l1 = NULL, **l2 = NULL;
    int64_t a = eval2(node->lhs, label ? &l1 : NULL);
    int64_t b = eval2(node->rhs, label ? &l2 : NULL);
    if (l2) {
      if (l1 && l1 == l2) {
        if (label)
          *label = NULL;
        return wrap(a - b, node->ty);
      }
      error_tok(node->tok, "initializer element is not computable at load time");
    }
    if (label)
      *label = l1;
    return wrap((int64_t)((uint64_t)a - (uint64_t)b), node->ty);
  }
  case ND_MUL:
    return wrap((int64_t)((uint64_t)eval(node->lhs) * (uint64_t)eval(node->rhs)), node->ty);
  case ND_DIV:
  case ND_MOD: {
    int64_t a = eval(node->lhs), b = eval(node->rhs);
    if (b == 0)
      error_tok(node->tok, "division by zero in constant expression");
    if (node->ty->is_unsigned) {
      uint64_t ua = node->ty->size == 4 ? (uint32_t)a : (uint64_t)a;
      uint64_t ub = node->ty->size == 4 ? (uint32_t)b : (uint64_t)b;
      return wrap(node->kind == ND_DIV ? (int64_t)(ua / ub) : (int64_t)(ua % ub), node->ty);
    }
    if (b == -1)
      return wrap(node->kind == ND_DIV ? (int64_t)(0 - (uint64_t)a) : 0, node->ty);
    return wrap(node->kind == ND_DIV ? a / b : a % b, node->ty);
  }
  case ND_NEG:
    return wrap((int64_t)(0 - (uint64_t)eval(node->lhs)), node->ty);
  case ND_BITAND:
    return wrap(eval(node->lhs) & eval(node->rhs), node->ty);
  case ND_BITOR:
    return wrap(eval(node->lhs) | eval(node->rhs), node->ty);
  case ND_BITXOR:
    return wrap(eval(node->lhs) ^ eval(node->rhs), node->ty);
  case ND_SHL: {
    int64_t b = eval(node->rhs);
    if (b < 0 || b >= node->ty->size * 8)
      warn_tok(node->tok, "shift count out of range");
    return wrap((int64_t)((uint64_t)eval(node->lhs) << (b & 63)), node->ty);
  }
  case ND_SHR: {
    int64_t a = eval(node->lhs), b = eval(node->rhs);
    if (b < 0 || b >= node->ty->size * 8)
      warn_tok(node->tok, "shift count out of range");
    b &= 63;
    if (node->ty->is_unsigned) {
      uint64_t ua = node->ty->size == 4 ? (uint32_t)a : (uint64_t)a;
      return wrap((int64_t)(ua >> b), node->ty);
    }
    return wrap(a >> b, node->ty);
  }
  case ND_EQ:
  case ND_NE:
  case ND_LT:
  case ND_LE: {
    Type *ot = node->lhs->ty;
    if (is_flonum(ot)) {
      long double a = eval_double(node->lhs), b = eval_double(node->rhs);
      switch (node->kind) {
      case ND_EQ: return a == b;
      case ND_NE: return a != b;
      case ND_LT: return a < b;
      default: return a <= b;
      }
    }
    char **l1 = NULL, **l2 = NULL;
    int64_t a = eval2(node->lhs, label ? &l1 : NULL);
    int64_t b = eval2(node->rhs, label ? &l2 : NULL);
    if (l1 || l2) {
      /* address constants: only == / != against null or same label */
      if (l1 == l2 && (node->kind == ND_EQ || node->kind == ND_NE)) {
        if (label) *label = NULL;
        return node->kind == ND_EQ ? a == b : a != b;
      }
      if ((node->kind == ND_EQ || node->kind == ND_NE) && (!l1 || !l2) && (l1 ? b : a) == 0) {
        if (label) *label = NULL;
        return node->kind == ND_NE;
      }
      error_tok(node->tok, "initializer element is not constant");
    }
    bool u = ot->is_unsigned || ot->kind == TY_PTR;
    switch (node->kind) {
    case ND_EQ: return a == b;
    case ND_NE: return a != b;
    case ND_LT: return u ? (uint64_t)a < (uint64_t)b : a < b;
    default: return u ? (uint64_t)a <= (uint64_t)b : a <= b;
    }
  }
  case ND_COND: {
    int64_t c = is_flonum(node->cond->ty) ? eval_double(node->cond) != 0 : eval(node->cond);
    return c ? eval2(node->then, label) : eval2(node->els, label);
  }
  case ND_COMMA:
    return eval2(node->rhs, label);
  case ND_NOT:
    if (is_flonum(node->lhs->ty))
      return !eval_double(node->lhs);
    return !eval(node->lhs);
  case ND_BITNOT:
    return wrap(~eval(node->lhs), node->ty);
  case ND_LOGAND: {
    int64_t a = is_flonum(node->lhs->ty) ? eval_double(node->lhs) != 0 : eval(node->lhs);
    if (!a)
      return 0;
    return is_flonum(node->rhs->ty) ? eval_double(node->rhs) != 0 : eval(node->rhs) != 0;
  }
  case ND_LOGOR: {
    int64_t a = is_flonum(node->lhs->ty) ? eval_double(node->lhs) != 0 : eval(node->lhs);
    if (a)
      return 1;
    return is_flonum(node->rhs->ty) ? eval_double(node->rhs) != 0 : eval(node->rhs) != 0;
  }
  case ND_CAST: {
    Type *from = node->lhs->ty;
    Type *to = node->ty;
    if (from->is_complex) {
      long double re, im;
      eval_complex(node->lhs, &re, &im);
      if (to->kind == TY_BOOL)
        return re != 0 || im != 0;
      if (to->is_unsigned && to->size == 8)
        return (int64_t)(uint64_t)re;
      return wrap((int64_t)re, to);
    }
    if (is_flonum(from)) {
      long double d = eval_double(node->lhs);
      if (to->kind == TY_BOOL)
        return d != 0;
      if (is_flonum(to))
        return (int64_t)d;
      if (to->is_unsigned && to->size == 8)
        return (int64_t)(uint64_t)d;
      return wrap((int64_t)d, to);
    }
    if (is_flonum(to))
      return (int64_t)eval_double(node);
    char **l = NULL;
    int64_t v = eval2(node->lhs, label ? &l : NULL);
    if (l) {
      if (type_size(to) != 8 && to->kind != TY_BOOL)
        error_tok(node->tok, "initializer element is not computable at load time");
      if (to->kind == TY_BOOL) {
        if (label) *label = NULL;
        return 1;
      }
      if (label)
        *label = l;
      return v;
    }
    if (to->kind == TY_VOID)
      return 0;
    return wrap(v, to);
  }
  case ND_ADDR:
    return eval_rval(node->lhs, label);
  case ND_MEMBER:
    if (!label)
      error_tok(node->tok, "not a compile-time constant");
    if (!is_array(node->ty))
      error_tok(node->tok, "invalid initializer");
    return eval_rval(node->lhs, label) + node->member->offset;
  case ND_VAR:
    if (!label)
      error_tok(node->tok, "not a compile-time constant");
    if (!is_array(node->var->ty) && !is_func(node->var->ty))
      error_tok(node->tok, "initializer element is not constant");
    if (node->var->is_local || node->var->is_tls)
      error_tok(node->tok, "initializer element is not constant");
    *label = &node->var->asm_name;
    return 0;
  case ND_NUM:
    return node->val;
  default:
    break;
  }
  (void)need_no_label;
  error_tok(node->tok, "initializer element is not constant");
}

static int64_t eval_rval(Node *node, char ***label) {
  switch (node->kind) {
  case ND_VAR:
    if (node->var->is_local)
      error_tok(node->tok, "initializer element is not constant");
    if (node->var->is_tls)
      error_tok(node->tok, "initializer element is not constant (thread-local)");
    if (!label)
      error_tok(node->tok, "not a compile-time constant");
    *label = &node->var->asm_name;
    return 0;
  case ND_DEREF:
    return eval2(node->lhs, label);
  case ND_MEMBER:
    return eval_rval(node->lhs, label) + node->member->offset;
  case ND_COMMA:
    if (node->var && node->var->is_compound_lit && !node->var->is_local)
      return eval_rval(node->rhs, label);
    break;
  case ND_CAST:
    return eval_rval(node->lhs, label);
  default:
    break;
  }
  error_tok(node->tok, "invalid initializer");
}

/* Floating constant evaluation; each operation is carried out in the
   precision of its type so results match run-time arithmetic. */
static long double round_to(long double v, Type *ty) {
  switch (ty->kind) {
  case TY_FLOAT: return (float)v;
  case TY_DOUBLE: return (double)v;
  default: return v;
  }
}

long double eval_double(Node *node) {
  add_type(node);

  if (is_integer(node->ty)) {
    if (node->ty->is_unsigned)
      return (long double)(uint64_t)eval(node);
    return (long double)eval(node);
  }

  Type *ty = node->ty;
  switch (node->kind) {
  case ND_ADD:
  case ND_SUB:
  case ND_MUL:
  case ND_DIV: {
    long double a = eval_double(node->lhs), b = eval_double(node->rhs);
    if (ty->kind == TY_FLOAT) {
      float x = a, y = b;
      switch (node->kind) {
      case ND_ADD: return x + y;
      case ND_SUB: return x - y;
      case ND_MUL: return x * y;
      default: return x / y;
      }
    }
    if (ty->kind == TY_DOUBLE) {
      double x = a, y = b;
      switch (node->kind) {
      case ND_ADD: return x + y;
      case ND_SUB: return x - y;
      case ND_MUL: return x * y;
      default: return x / y;
      }
    }
    switch (node->kind) {
    case ND_ADD: return a + b;
    case ND_SUB: return a - b;
    case ND_MUL: return a * b;
    default: return a / b;
    }
  }
  case ND_NEG:
    return -eval_double(node->lhs);
  case ND_COND: {
    bool c = is_flonum(node->cond->ty) ? eval_double(node->cond) != 0 : eval(node->cond) != 0;
    return c ? eval_double(node->then) : eval_double(node->els);
  }
  case ND_COMMA:
    return eval_double(node->rhs);
  case ND_CAST:
    if (node->lhs->ty->is_complex) {
      long double re, im;
      eval_complex(node->lhs, &re, &im);
      return round_to(re, ty);
    }
    if (is_flonum(node->lhs->ty))
      return round_to(eval_double(node->lhs), ty);
    if (node->lhs->ty->is_unsigned && type_size(node->lhs->ty) == 8)
      return round_to((long double)(uint64_t)eval(node->lhs), ty);
    if (node->lhs->ty->kind == TY_PTR)
      error_tok(node->tok, "initializer element is not constant");
    return round_to((long double)eval(node->lhs), ty);
  case ND_NUM:
    return node->fval;
  default:
    break;
  }
  error_tok(node->tok, "initializer element is not a constant expression");
}

/* Complex constant evaluation: *re + *im i, rounded per operation. */
void eval_complex(Node *node, long double *re, long double *im) {
  add_type(node);
  Type *ty = node->ty;
  if (!ty->is_complex) {
    *re = eval_double(node);
    *im = 0;
    return;
  }
  Type *base = complex_base(ty);
  long double a, b, c, d;
  switch (node->kind) {
  case ND_NUM:
    *re = node->fval;
    *im = node->fimag;
    return;
  case ND_CAST:
    eval_complex(node->lhs, &a, &b);
    *re = round_to(a, base);
    *im = round_to(b, base);
    return;
  case ND_MKCOMPLEX:
    *re = round_to(eval_double(node->lhs), base);
    *im = round_to(eval_double(node->rhs), base);
    return;
  case ND_NEG:
    eval_complex(node->lhs, &a, &b);
    *re = -a;
    *im = -b;
    return;
  case ND_ADD:
  case ND_SUB:
  case ND_MUL:
  case ND_DIV:
    eval_complex(node->lhs, &a, &b);
    eval_complex(node->rhs, &c, &d);
    switch (node->kind) {
    case ND_ADD: *re = a + c; *im = b + d; break;
    case ND_SUB: *re = a - c; *im = b - d; break;
    case ND_MUL: *re = a * c - b * d; *im = a * d + b * c; break;
    default: {
      long double den = c * c + d * d;
      *re = (a * c + b * d) / den;
      *im = (b * c - a * d) / den;
    }
    }
    *re = round_to(*re, base);
    *im = round_to(*im, base);
    return;
  case ND_COMMA:
    eval_complex(node->rhs, re, im);
    return;
  case ND_COND: {
    bool cv = is_flonum(node->cond->ty) ? eval_double(node->cond) != 0 : eval(node->cond) != 0;
    eval_complex(cv ? node->then : node->els, re, im);
    return;
  }
  default:
    error_tok(node->tok, "initializer element is not a constant expression");
  }
}

/* Arithmetic constant expression (6.6p8) */
bool is_arith_const(Node *node) {
  add_type(node);
  switch (node->kind) {
  case ND_NUM:
    return true;
  case ND_MKCOMPLEX:
    return is_arith_const(node->lhs) && is_arith_const(node->rhs);
  case ND_COMMA:
    warn_tok(node->tok, "comma operator in a constant expression");
    return is_arith_const(node->rhs);
  case ND_CAST:
    return is_arith(node->ty) && is_arith(node->lhs->ty) && is_arith_const(node->lhs);
  case ND_ADD: case ND_SUB: case ND_MUL: case ND_DIV: case ND_MOD:
  case ND_BITAND: case ND_BITOR: case ND_BITXOR: case ND_SHL: case ND_SHR:
  case ND_EQ: case ND_NE: case ND_LT: case ND_LE: case ND_LOGAND: case ND_LOGOR:
    return is_arith_const(node->lhs) && is_arith_const(node->rhs);
  case ND_NEG: case ND_BITNOT: case ND_NOT:
    return is_arith_const(node->lhs);
  case ND_COND:
    return is_arith_const(node->cond) && is_arith_const(node->then) &&
           is_arith_const(node->els);
  default:
    return false;
  }
}

static bool is_addr_const(Node *node) {
  switch (node->kind) {
  case ND_VAR:
    return !node->var->is_local && !node->var->is_tls;
  case ND_DEREF:
    return is_const_expr(node->lhs);
  case ND_MEMBER:
    return is_addr_const(node->lhs);
  case ND_COMMA:
    return node->var && node->var->is_compound_lit && !node->var->is_local;
  case ND_CAST:
    return is_addr_const(node->lhs);
  default:
    return false;
  }
}

/* Constant expression usable in a static initializer (6.6p7) */
bool is_const_expr(Node *node) {
  add_type(node);
  switch (node->kind) {
  case ND_NUM:
    return true;
  case ND_COMMA:
    if (node->var)
      return false;
    warn_tok(node->tok, "comma operator in a constant expression");
    return is_const_expr(node->rhs);
  case ND_ADD: case ND_SUB: case ND_MUL: case ND_DIV: case ND_MOD:
  case ND_BITAND: case ND_BITOR: case ND_BITXOR: case ND_SHL: case ND_SHR:
  case ND_EQ: case ND_NE: case ND_LT: case ND_LE: case ND_LOGAND: case ND_LOGOR:
    return is_const_expr(node->lhs) && is_const_expr(node->rhs);
  case ND_NEG: case ND_BITNOT: case ND_NOT:
    return is_const_expr(node->lhs);
  case ND_COND:
    return is_const_expr(node->cond) && is_const_expr(node->then) &&
           is_const_expr(node->els);
  case ND_CAST:
    return is_const_expr(node->lhs);
  case ND_ADDR:
    return is_addr_const(node->lhs);
  case ND_MEMBER:
    return is_array(node->ty) && is_addr_const(node->lhs);
  case ND_VAR:
    return (is_array(node->ty) || is_func(node->ty)) && !node->var->is_local &&
           !node->var->is_tls;
  default:
    return false;
  }
}
