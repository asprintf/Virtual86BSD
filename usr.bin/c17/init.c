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

/* init.c - initialization (6.7.9) */
#include "parse.h"

static void initializer2(Token **rest, Token *tok, Initializer *init, bool top);
static Initializer *new_initializer(Type *ty, bool is_flexible, Token *tok);

static int count_members(Type *ty) {
  int n = 0;
  for (Member *m = struct_members(ty); m; m = m->next)
    n++;
  return n;
}

static Initializer *new_initializer(Type *ty, bool is_flexible, Token *tok) {
  Initializer *init = xcalloc(1, sizeof(Initializer));
  init->ty = ty;
  init->tok = tok;
  if (ty->kind == TY_ARRAY) {
    if (ty->array_len < 0 || (is_flexible && ty->array_len == 0 && 0)) {
      init->is_flexible = true;
    }
    /* children grow on demand */
    return init;
  }
  if (is_su(ty)) {
    int n = count_members(ty);
    init->is_flexible = is_flexible;
    init->nchildren = n;
    init->children = xcalloc(n ? n : 1, sizeof(Initializer *));
    return init;
  }
  return init;
}

static Initializer *child(Initializer *init, int64_t i, bool is_flexible);

/* Child for a designator: an aggregate child previously initialized by a
   single expression is reset (GCC semantics for overriding). */
static Initializer *desg_child(Initializer *init, int64_t i, bool is_flexible) {
  Initializer *c = child(init, i, is_flexible);
  if (c->expr && is_aggregate(c->ty))
    c->expr = NULL;
  return c;
}

/* Returns (allocating if needed) the initializer for child i. */
static Initializer *child(Initializer *init, int64_t i, bool is_flexible) {
  Type *ty = init->ty;
  if (ty->kind == TY_ARRAY) {
    if (i >= init->nchildren) {
      int64_t n = i + 1;
      int64_t cap = 1;
      while (cap < n)
        cap *= 2;
      init->children = xrealloc(init->children, sizeof(Initializer *) * cap);
      for (int64_t k = init->nchildren; k < cap; k++)
        init->children[k] = NULL;
      init->nchildren = n;
    }
    if (!init->children[i])
      init->children[i] = new_initializer(ty->base, false, init->tok);
    return init->children[i];
  }
  /* struct/union: i is the member index */
  if (!init->children[i]) {
    Member *m = struct_members(ty);
    for (int k = 0; k < i; k++)
      m = m->next;
    Type *mty = m->ty;
    bool flex = is_flexible && su_def(ty)->is_flexible && !m->next && mty->kind == TY_ARRAY;
    if (flex) {
      mty = array_of(mty->base, -1);
    }
    init->children[i] = new_initializer(mty, flex, init->tok);
  }
  return init->children[i];
}

static Token *skip_excess_element(Token *tok) {
  if (equal(tok, "{")) {
    tok = skip_excess_element(tok->next);
    return skip(tok, "}");
  }
  assign(&tok, tok);
  return tok;
}

static bool is_end(Token *tok) {
  return equal(tok, "}") || (equal(tok, ",") && equal(tok->next, "}"));
}

static bool consume_end(Token **rest, Token *tok) {
  if (equal(tok, "}")) {
    *rest = tok->next;
    return true;
  }
  if (equal(tok, ",") && equal(tok->next, "}")) {
    *rest = tok->next->next;
    return true;
  }
  return false;
}

/* May a string literal of element type lit initialize an array of ty? */
static bool string_init_ok(Type *arr, Type *lit) {
  Type *e = unqual(arr->base);
  Type *l = lit->base;
  if (l->kind == TY_CHAR)
    return e->kind == TY_CHAR;
  return is_integer(e) && e->size == l->size && e->kind != TY_BOOL &&
         (e->is_unsigned == l->is_unsigned || e->kind == TY_ENUM);
}

static bool is_string_init(Token *tok, Type *ty) {
  return ty->kind == TY_ARRAY && tok->kind == TK_STR && is_integer(ty->base);
}

static void string_initializer(Token **rest, Token *tok, Initializer *init) {
  Type *ty = init->ty;
  if (!string_init_ok(ty, tok->ty))
    error_tok(tok, "array of '%s' initialized from string literal of '%s'",
              type_name(ty->base), type_name(tok->ty));
  int esz = tok->ty->base->size;
  int64_t len = tok->ty->array_len;
  if (init->is_flexible) {
    init->ty = array_of(ty->base, len);
    ty = init->ty;
  } else if (ty->array_len < len - 1) {
    warn_tok(tok, "initializer-string for char array is too long");
  }
  int64_t n = ty->array_len < len ? ty->array_len : len;
  for (int64_t i = 0; i < n; i++) {
    int64_t v;
    switch (esz) {
    case 1: v = tok->str[i]; break;
    case 2: v = ((uint16_t *)tok->str)[i]; break;
    default: v = ((uint32_t *)tok->str)[i]; break;
    }
    Initializer *c = child(init, i, false);
    c->expr = new_cast(new_num(v, tok), ty->base);
  }
  *rest = tok->next;
}

/* Designator lookup: finds member "name" in ty, descending into
   anonymous members; returns the path as a list. */
static Member *find_designated_member(Type *ty, Token *name, Member **path, int *depth) {
  for (Member *m = struct_members(ty); m; m = m->next) {
    if (m->name && m->name->len == name->len && !memcmp(m->name->loc, name->loc, name->len)) {
      path[(*depth)++] = m;
      return m;
    }
    if (m->is_anon) {
      int d = *depth;
      path[(*depth)++] = m;
      if (find_designated_member(m->ty, name, path, depth))
        return m;
      *depth = d;
    }
  }
  return NULL;
}

static int member_index(Type *ty, Member *mem) {
  int i = 0;
  for (Member *m = struct_members(ty); m; m = m->next, i++)
    if (m == mem)
      return i;
  return -1;
}

static int64_t array_designator(Token **rest, Token *tok, Type *ty, int64_t *end) {
  Token *start = tok;
  Node *e = conditional(&tok, tok->next);
  int64_t i = ice_value(e, start->next, "array index in initializer");
  *end = i;
  if (equal(tok, "...")) {
    Token *t2 = tok->next;
    Node *e2 = conditional(&tok, tok->next);
    *end = ice_value(e2, t2, "array index in initializer");
    if (*end < i)
      error_tok(t2, "empty index range in initializer");
  }
  if (i < 0 || (ty->array_len >= 0 && *end >= ty->array_len))
    error_tok(start->next, "array index in initializer exceeds array bounds");
  *rest = skip(tok, "]");
  return i;
}

static void struct_initializer2(Token **rest, Token *tok, Initializer *init, int start_idx,
                                bool braced);
static void array_initializer2(Token **rest, Token *tok, Initializer *init, int64_t i,
                               bool braced);

/* designation-initializer for the current object at a designator */
static void designation(Token **rest, Token *tok, Initializer *init) {
  Type *ty = init->ty;
  if (equal(tok, "[")) {
    if (ty->kind != TY_ARRAY)
      error_tok(tok, "array index in non-array initializer");
    int64_t end;
    int64_t i = array_designator(&tok, tok, ty, &end);
    for (int64_t k = i; k <= end; k++) {
      Token *t2 = tok;
      Initializer *c = equal(tok, "[") || equal(tok, ".") ? desg_child(init, k, false) : child(init, k, false);
      if (equal(t2, "[") || equal(t2, "."))
        designation(&t2, t2, c);
      else {
        t2 = skip(t2, "=");
        initializer2(&t2, t2, c, false);
      }
      if (k == end)
        tok = t2;
    }
    /* continue with following elements */
    array_initializer2(rest, tok, init, end + 1, false);
    return;
  }

  if (equal(tok, ".")) {
    if (!is_su(ty))
      error_tok(tok, "field name not in record or union initializer");
    Token *name = tok->next;
    if (name->kind != TK_IDENT)
      error_tok(name, "expected a field name");
    Member *path[64];
    int depth = 0;
    if (!find_designated_member(ty, name, path, &depth))
      error_tok(name, "no member named '%.*s' in '%s'", name->len, name->loc, type_name(ty));
    tok = name->next;

    /* Walk the path through anonymous members. */
    Initializer *cur = init;
    Type *cty = ty;
    for (int d = 0; d < depth; d++) {
      Member *m = path[d];
      int idx = member_index(cty, m);
      if (cty->kind == TY_UNION) {
        /* Selecting a different union member discards the old one. */
        if (cur->mem != m) {
          for (int k = 0; k < cur->nchildren; k++)
            cur->children[k] = NULL;
        }
        cur->mem = m;
      }
      Initializer *c = (d < depth - 1 || equal(tok, "[") || equal(tok, "."))
                           ? desg_child(cur, idx, init->is_flexible || cur->is_flexible)
                           : child(cur, idx, init->is_flexible || cur->is_flexible);
      if (d == depth - 1) {
        if (equal(tok, "[") || equal(tok, "."))
          designation(&tok, tok, c);
        else {
          tok = skip(tok, "=");
          initializer2(&tok, tok, c, false);
        }
        /* Following initializers continue after this member at this level. */
        if (cty->kind == TY_STRUCT && d == 0) {
          struct_initializer2(rest, tok, init, idx + 1, false);
          return;
        }
        if (d == 0) {
          *rest = tok;
          return;
        }
        /* inside an anonymous member: continue there, then outer */
        if (cty->kind == TY_STRUCT)
          struct_initializer2(&tok, tok, cur, idx + 1, false);
        *rest = tok;
        return;
      }
      cur = c;
      cty = m->ty;
    }
    *rest = tok;
    return;
  }

  error_tok(tok, "expected designator");
}

/* After a designated initializer, initialization continues with the
   following elements of the same array (6.7.9p17). */
static void array_initializer2(Token **rest, Token *tok, Initializer *init, int64_t i,
                               bool braced) {
  Type *ty = init->ty;
  (void)braced;
  for (; ty->array_len < 0 || i < ty->array_len; i++) {
    if (!equal(tok, ",") || is_end(tok) || equal(tok->next, "[") || equal(tok->next, "."))
      break;
    tok = tok->next;
    initializer2(&tok, tok, child(init, i, false), false);
  }
  *rest = tok;
}

/* The core of a braced list for an array: handles designators. */
static void array_initializer1(Token **rest, Token *tok, Initializer *init) {
  tok = skip(tok, "{");
  Type *ty = init->ty;
  bool first = true;
  int64_t i = 0;
  int64_t maxlen = 0;

  while (!consume_end(rest, tok)) {
    if (!first)
      tok = skip(tok, ",");
    first = false;

    if (equal(tok, "[")) {
      int64_t end;
      int64_t k0 = array_designator(&tok, tok, ty, &end);
      for (int64_t k = k0; k <= end; k++) {
        Token *t2 = tok;
        Initializer *c = equal(tok, "[") || equal(tok, ".") ? desg_child(init, k, false) : child(init, k, false);
        if (equal(t2, "[") || equal(t2, "."))
          designation(&t2, t2, c);
        else {
          consume(&t2, t2, "="); /* GNU: "[i] value" without '=' */
          initializer2(&t2, t2, c, false);
        }
        if (k == end)
          tok = t2;
      }
      i = end + 1;
      if (i > maxlen)
        maxlen = i;
      continue;
    }
    if (equal(tok, "."))
      error_tok(tok, "field name not in record or union initializer");

    if (ty->array_len >= 0 && i >= ty->array_len) {
      warn_tok(tok, "excess elements in array initializer");
      tok = skip_excess_element(tok);
      i++;
      continue;
    }
    Initializer *c = child(init, i, false);
    initializer2(&tok, tok, c, false);
    i++;
    if (i > maxlen)
      maxlen = i;
  }

  if (init->is_flexible) {
    init->ty = array_of(ty->base, maxlen);
  }
}

/* Without braces: consumes as many initializers as the array needs. */
static void array_initializer_nobrace(Token **rest, Token *tok, Initializer *init) {
  Type *ty = init->ty;
  if (init->is_flexible) {
    /* GNU: a flexible array member initialized by brace elision takes
       the remaining initializers of the list. */
    int64_t i = 0;
    for (; !is_end(tok); i++) {
      if (i > 0) {
        if (!equal(tok, ",") || equal(tok->next, "[") || equal(tok->next, "."))
          break;
        tok = tok->next;
      }
      initializer2(&tok, tok, child(init, i, false), false);
    }
    init->ty = array_of(ty->base, i);
    *rest = tok;
    return;
  }
  for (int64_t i = 0; i < ty->array_len && !is_end(tok); i++) {
    if (i > 0) {
      if (!equal(tok, ","))
        break;
      if (equal(tok->next, "[") || equal(tok->next, "."))
        break;
      tok = tok->next;
    }
    Initializer *c = child(init, i, false);
    initializer2(&tok, tok, c, false);
  }
  *rest = tok;
}

static Member *first_named_member(Type *ty, int *idx) {
  int i = 0;
  for (Member *m = struct_members(ty); m; m = m->next, i++) {
    if (m->name || m->is_anon) {
      *idx = i;
      return m;
    }
  }
  return NULL;
}

/* After a designated member, initialization continues with the
   following members of the same struct. */
static void struct_initializer2(Token **rest, Token *tok, Initializer *init, int start_idx,
                                bool braced) {
  Type *ty = init->ty;
  (void)braced;
  int idx = 0;
  Member *mem = struct_members(ty);
  for (; mem && idx < start_idx; mem = mem->next)
    idx++;
  for (; mem; mem = mem->next, idx++) {
    if (mem->is_bitfield && !mem->name)
      continue;
    if (!equal(tok, ",") || is_end(tok) || equal(tok->next, ".") || equal(tok->next, "["))
      break;
    tok = tok->next;
    initializer2(&tok, tok, child(init, idx, init->is_flexible), false);
  }
  *rest = tok;
}

static void struct_initializer1(Token **rest, Token *tok, Initializer *init) {
  tok = skip(tok, "{");
  Type *ty = init->ty;
  Member *mem = struct_members(ty);
  int idx = 0;
  bool first = true;

  while (!consume_end(rest, tok)) {
    if (!first)
      tok = skip(tok, ",");
    first = false;

    if (equal(tok, ".") ) {
      Token *name = tok->next;
      if (name->kind != TK_IDENT)
        error_tok(name, "expected a field name");
      Member *path[64];
      int depth = 0;
      if (!find_designated_member(ty, name, path, &depth))
        error_tok(name, "no member named '%.*s' in '%s'", name->len, name->loc, type_name(ty));
      /* Designate within the path. */
      Initializer *cur = init;
      Type *cty = ty;
      tok = name->next;
      for (int d = 0; d < depth; d++) {
        Member *m = path[d];
        int k = member_index(cty, m);
        Initializer *c = (d < depth - 1 || equal(tok, "[") || equal(tok, "."))
                             ? desg_child(cur, k, cur->is_flexible)
                             : child(cur, k, cur->is_flexible);
        if (d == 0) {
          mem = m;
          idx = k;
        }
        if (d == depth - 1) {
          if (equal(tok, "[") || equal(tok, "."))
            designation(&tok, tok, c);
          else {
            tok = skip(tok, "=");
            initializer2(&tok, tok, c, false);
          }
          /* Subsequent non-designated initializers continue within the
             innermost anonymous member, then the outer struct. */
          if (d > 0 && cty->kind == TY_STRUCT)
            struct_initializer2(&tok, tok, cur, k + 1, false);
        } else {
          cur = c;
          cty = m->ty;
        }
      }
      mem = mem->next;
      idx++;
      continue;
    }
    if (equal(tok, "["))
      error_tok(tok, "array index in non-array initializer");

    /* Skip unnamed bit-fields */
    while (mem && mem->is_bitfield && !mem->name) {
      mem = mem->next;
      idx++;
    }
    if (!mem) {
      warn_tok(tok, "excess elements in struct initializer");
      tok = skip_excess_element(tok);
      continue;
    }
    Initializer *c = child(init, idx, init->is_flexible);
    initializer2(&tok, tok, c, false);
    mem = mem->next;
    idx++;
  }
}

static void struct_initializer_nobrace(Token **rest, Token *tok, Initializer *init) {
  Type *ty = init->ty;
  bool first = true;
  int idx = 0;
  for (Member *mem = struct_members(ty); mem; mem = mem->next, idx++) {
    if (mem->is_bitfield && !mem->name)
      continue;
    if (is_end(tok))
      break;
    if (!first) {
      if (!equal(tok, ",") || equal(tok->next, ".") || equal(tok->next, "["))
        break;
      tok = tok->next;
    }
    first = false;
    Initializer *c = child(init, idx, false);
    initializer2(&tok, tok, c, false);
  }
  *rest = tok;
}

static void union_initializer(Token **rest, Token *tok, Initializer *init) {
  Type *ty = init->ty;
  if (equal(tok, "{")) {
    tok = tok->next;
    bool first = true;
    while (!consume_end(rest, tok)) {
      if (!first)
        tok = skip(tok, ",");
      if (equal(tok, ".")) {
        designation(&tok, tok, init);
        first = false;
        continue;
      }
      if (!first) {
        warn_tok(tok, "excess elements in union initializer");
        tok = skip_excess_element(tok);
        continue;
      }
      first = false;
      int idx;
      Member *m = first_named_member(ty, &idx);
      if (!m)
        error_tok(tok, "union has no named members");
      init->mem = m;
      initializer2(&tok, tok, child(init, idx, false), false);
    }
    return;
  }
  int idx;
  Member *m = first_named_member(ty, &idx);
  if (!m)
    error_tok(tok, "union has no named members");
  init->mem = m;
  initializer2(rest, tok, child(init, idx, false), false);
}

/* Is the next initializer an expression whose type is ty itself
   (e.g. a struct value)?  Requires a lookahead parse. */
static Node *try_aggregate_expr(Token **rest, Token *tok, Type *ty) {
  if (equal(tok, "{") || equal(tok, ".") || equal(tok, "["))
    return NULL;
  Token *t2;
  Node *e = assign(&t2, tok);
  add_type(e);
  if (is_su(e->ty) && is_compatible_unqual(e->ty, ty)) {
    *rest = t2;
    return e;
  }
  return NULL;
}

static void initializer2(Token **rest, Token *tok, Initializer *init, bool top) {
  Type *ty = init->ty;

  if (is_string_init(tok, ty)) {
    string_initializer(rest, tok, init);
    return;
  }
  if (ty->kind == TY_ARRAY && equal(tok, "{") && is_string_init(tok->next, ty) &&
      (equal(tok->next->next, "}") ||
       (equal(tok->next->next, ",") && equal(tok->next->next->next, "}")))) {
    string_initializer(&tok, tok->next, init);
    consume_end(rest, tok);
    return;
  }

  if (ty->kind == TY_ARRAY) {
    if (equal(tok, "{"))
      array_initializer1(rest, tok, init);
    else {
      if (top)
        error_tok(tok, "array initializer must be an initializer list");
      array_initializer_nobrace(rest, tok, init);
    }
    return;
  }

  if (ty->kind == TY_STRUCT) {
    if (equal(tok, "{")) {
      struct_initializer1(rest, tok, init);
      return;
    }
    /* A struct can be initialized with another struct value. */
    Node *e = NULL;
    Token *save = tok;
    jmp_buf *saved = error_jmp;
    (void)saved;
    if (!top || 1) {
      Token *t2;
      Node *expr0;
      /* Parse an assignment expression and check its type. */
      expr0 = assign(&t2, tok);
      add_type(expr0);
      if (is_su(expr0->ty) && is_compatible_unqual(expr0->ty, ty)) {
        e = expr0;
        tok = t2;
      }
    }
    if (e) {
      init->expr = e;
      *rest = tok;
      return;
    }
    tok = save;
    if (top)
      error_tok(tok, "invalid initializer");
    struct_initializer_nobrace(rest, tok, init);
    return;
  }

  if (ty->kind == TY_UNION) {
    if (!equal(tok, "{")) {
      Node *e = try_aggregate_expr(rest, tok, ty);
      if (e) {
        init->expr = e;
        return;
      }
      if (top)
        error_tok(tok, "invalid initializer");
    }
    union_initializer(rest, tok, init);
    return;
  }

  /* Scalar */
  if (equal(tok, "{")) {
    if (equal(tok->next, "}"))
      error_tok(tok, "empty scalar initializer");
    if (equal(tok->next, "{"))
      warn_tok(tok, "braces around scalar initializer");
    initializer2(&tok, tok->next, init, false);
    if (!consume_end(rest, tok)) {
      warn_tok(tok, "excess elements in scalar initializer");
      while (!consume_end(rest, tok)) {
        tok = skip(tok, ",");
        tok = skip_excess_element(tok);
      }
    }
    return;
  }
  if (equal(tok, ".") || equal(tok, "["))
    error_tok(tok, "designator in scalar initializer");
  Token *etok = tok;
  Node *e = assign(rest, tok);
  init->expr = convert_for_assign(e, unqual(ty), etok, "initializing");
}

static Type *copy_struct_type(Type *ty) {
  ty = copy_type(su_def(ty));
  ty->variants = NULL;
  ty->tag_type = ty;
  Member head = {0};
  Member *cur = &head;
  for (Member *mem = ty->members; mem; mem = mem->next) {
    Member *m = xmalloc(sizeof(Member));
    *m = *mem;
    cur = cur->next = m;
  }
  ty->members = head.next;
  return ty;
}

static Initializer *initializer(Token **rest, Token *tok, Type *ty, Type **new_ty) {
  Initializer *init = new_initializer(ty, true, tok);
  initializer2(rest, tok, init, true);

  if (is_su(ty) && su_def(ty)->is_flexible) {
    /* Size the flexible array member from the initializer. */
    Member *last = struct_members(ty);
    int n = 0;
    while (last->next) {
      last = last->next;
      n++;
    }
    Initializer *fi = init->children[n];
    if (fi && fi->ty->array_len > 0) {
      Type *t = copy_struct_type(ty);
      Member *m = t->members;
      while (m->next)
        m = m->next;
      m->ty = fi->ty;
      t->size += type_size(fi->ty);
      *new_ty = t;
      return init;
    }
    *new_ty = ty;
    return init;
  }

  *new_ty = init->ty;
  return init;
}

/* ------------------------------------------------- local variables */

static Node *init_desg_expr(InitDesg *desg, Token *tok) {
  if (desg->var)
    return new_var_node(desg->var, tok);
  if (desg->member) {
    Node *node = new_unary(ND_MEMBER, init_desg_expr(desg->next, tok), tok);
    node->member = desg->member;
    node->ty = desg->member->ty;
    return node;
  }
  Node *lhs = init_desg_expr(desg->next, tok);
  Node *ptr = new_add(rval(lhs), new_long(desg->idx, tok), tok);
  Node *d = new_unary(ND_DEREF, ptr, tok);
  add_type(d);
  return d;
}

static Node *create_lvar_init(Initializer *init, Type *ty, InitDesg *desg, Token *tok) {
  if (ty->kind == TY_ARRAY) {
    Node *node = new_node(ND_NULL_EXPR, tok);
    for (int64_t i = 0; i < init->nchildren; i++) {
      if (!init->children[i])
        continue;
      InitDesg desg2 = {desg, i};
      Node *rhs = create_lvar_init(init->children[i], ty->base, &desg2, tok);
      node = new_binary(ND_COMMA, node, rhs, tok);
    }
    return node;
  }

  if (is_su(ty) && !init->expr) {
    Node *node = new_node(ND_NULL_EXPR, tok);
    int i = 0;
    for (Member *mem = struct_members(ty); mem; mem = mem->next, i++) {
      if (ty->kind == TY_UNION && init->mem && mem != init->mem)
        continue;
      if (!init->children[i])
        continue;
      InitDesg desg2 = {desg, 0, mem};
      Node *rhs = create_lvar_init(init->children[i], mem->ty, &desg2, tok);
      node = new_binary(ND_COMMA, node, rhs, tok);
      if (ty->kind == TY_UNION)
        break;
    }
    return node;
  }

  if (!init->expr)
    return new_node(ND_NULL_EXPR, tok);

  Node *lhs = init_desg_expr(desg, tok);
  Node *n = new_binary(ND_ASSIGN, lhs, init->expr, tok);
  n->ty = unqual(lhs->ty);
  return n;
}

/* A local "T x = init;" becomes: memzero(x); then element stores. */
Node *lvar_initializer(Token **rest, Token *tok, Obj *var) {
  Token *start = tok;
  Initializer *init = initializer(rest, tok, var->ty, &var->ty);
  if (is_su(var->ty) && su_def(var->ty)->is_flexible && init->children) {
    Member *last = struct_members(var->ty);
    int n = 0;
    while (last->next) {
      last = last->next;
      n++;
    }
    if (init->children[n] && init->children[n]->nchildren > 0)
      error_tok(start, "initialization of a flexible array member in a non-static object");
  }
  var->align = type_align(var->ty) > var->align ? type_align(var->ty) : var->align;
  InitDesg desg = {NULL, 0, NULL, var};

  Node *rhs = create_lvar_init(init, var->ty, &desg, tok);
  /* A scalar is fully initialized by its expression. */
  if (is_scalar(var->ty) && init->expr)
    return rhs;
  /* Elements not explicitly initialized are zero (6.7.9p21). */
  Node *lhs = new_node(ND_MEMZERO, tok);
  lhs->var = var;
  return new_binary(ND_COMMA, lhs, rhs, tok);
}

/* --------------------------------------------- static storage duration */

static void write_buf(char *buf, uint64_t val, int sz) {
  switch (sz) {
  case 1: *buf = val; return;
  case 2: *(uint16_t *)buf = val; return;
  case 4: *(uint32_t *)buf = val; return;
  case 8: *(uint64_t *)buf = val; return;
  default: unreachable();
  }
}

static Relocation *write_gvar_data(Relocation *cur, Initializer *init, Type *ty, char *buf,
                                   int64_t offset) {
  if (ty->kind == TY_ARRAY) {
    int64_t sz = type_size(ty->base);
    for (int64_t i = 0; i < init->nchildren; i++)
      if (init->children[i])
        cur = write_gvar_data(cur, init->children[i], ty->base, buf, offset + sz * i);
    return cur;
  }

  if (is_su(ty) && !init->expr) {
    int i = 0;
    for (Member *mem = struct_members(ty); mem; mem = mem->next, i++) {
      if (ty->kind == TY_UNION && init->mem && mem != init->mem)
        continue;
      Initializer *c = init->children[i];
      if (!c)
        continue;
      if (mem->is_bitfield) {
        Node *expr = c->expr;
        if (!expr)
          continue;
        char **label = NULL;
        int64_t v = eval2(expr, &label);
        if (label)
          error_tok(expr->tok, "initializer element is not computable at load time");
        char *loc = buf + offset + mem->offset;
        int64_t sz = type_size(mem->ty);
        uint64_t oldval = 0;
        memcpy(&oldval, loc, sz);
        uint64_t mask = mem->bit_width >= 64 ? ~0ULL : ((1ULL << mem->bit_width) - 1);
        uint64_t newval = oldval & ~(mask << mem->bit_offset);
        newval |= ((uint64_t)v & mask) << mem->bit_offset;
        memcpy(loc, &newval, sz);
      } else {
        Type *mty = c->ty->kind == TY_ARRAY && mem->ty->kind == TY_ARRAY ? c->ty : mem->ty;
        cur = write_gvar_data(cur, c, mty, buf, offset + mem->offset);
      }
      if (ty->kind == TY_UNION)
        break;
    }
    return cur;
  }

  if (!init->expr)
    return cur;

  Node *expr = init->expr;
  if (is_su(ty)) {
    /* A struct initialized from a file-scope compound literal (GNU
       extension; not a constant expression in C17): copy its data. */
    Node *e = expr;
    while (e->kind == ND_CAST)
      e = e->lhs;
    if (e->kind == ND_VAR && e->var->is_compound_lit && !e->var->is_local && e->var->init_data) {
      memcpy(buf + offset, e->var->init_data, type_size(ty));
      for (Relocation *r = e->var->rel; r; r = r->next) {
        Relocation *rel = xcalloc(1, sizeof(Relocation));
        *rel = *r;
        rel->next = NULL;
        rel->offset += offset;
        cur = cur->next = rel;
      }
      return cur;
    }
    error_tok(expr->tok, "initializer element is not constant");
  }

  if (ty->is_complex) {
    if (!is_arith_const(expr))
      error_tok(expr->tok, "initializer element is not a constant expression");
    long double re, im;
    eval_complex(expr, &re, &im);
    switch (ty->kind) {
    case TY_FLOAT:
      ((float *)(buf + offset))[0] = re;
      ((float *)(buf + offset))[1] = im;
      break;
    case TY_DOUBLE:
      ((double *)(buf + offset))[0] = re;
      ((double *)(buf + offset))[1] = im;
      break;
    default:
      memset(buf + offset, 0, 32);
      *(long double *)(buf + offset) = re;
      *(long double *)(buf + offset + 16) = im;
    }
    return cur;
  }

  if (ty->kind == TY_FLOAT || ty->kind == TY_DOUBLE || ty->kind == TY_LDOUBLE) {
    if (!is_arith_const(expr))
      error_tok(expr->tok, "initializer element is not a constant expression");
    long double v = eval_double(expr);
    if (ty->kind == TY_FLOAT)
      *(float *)(buf + offset) = (float)v;
    else if (ty->kind == TY_DOUBLE)
      *(double *)(buf + offset) = (double)v;
    else {
      memset(buf + offset, 0, 16);
      *(long double *)(buf + offset) = v;
    }
    return cur;
  }

  if (!is_const_expr(expr))
    error_tok(expr->tok, "initializer element is not a constant expression");
  char **label = NULL;
  int64_t val = eval2(expr, &label);

  if (!label) {
    write_buf(buf + offset, val, type_size(ty));
    return cur;
  }
  if (type_size(ty) != 8)
    error_tok(expr->tok, "initializer element is not computable at load time");

  Relocation *rel = xcalloc(1, sizeof(Relocation));
  rel->offset = offset;
  rel->label = label;
  rel->addend = val;
  rel->size = 8;
  cur->next = rel;
  return cur->next;
}

void gvar_initializer(Token **rest, Token *tok, Obj *var) {
  Type *declared = var->ty;
  Type *ty;
  Initializer *init = initializer(rest, tok, var->ty, &ty);
  if (!is_object_complete(ty))
    error_tok(var->tok, "variable '%s' has incomplete type '%s'", var->name, type_name(ty));
  /* A struct with an initialized flexible array member keeps its type;
     only the storage is larger (as GCC). */
  if (is_su(declared) && su_def(declared)->is_flexible && ty != declared) {
    var->ty = declared;
    var->data_size = type_size(ty);
  } else {
    var->ty = ty;
  }
  var->align = type_align(var->ty) > var->align ? type_align(var->ty) : var->align;

  Relocation head = {0};
  int64_t sz = type_size(ty);
  char *buf = xcalloc(1, sz ? sz : 1);
  write_gvar_data(&head, init, ty, buf, 0);
  var->init_data = buf;
  var->rel = head.next;
}

/* Compound literal types: parses the initializer to complete the type. */
Type *init_compound_type(Token *tok, Type *ty, Token **end) {
  Type *new_ty;
  initializer(end, tok, ty, &new_ty);
  return new_ty;
}
