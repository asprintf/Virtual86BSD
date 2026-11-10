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

/* stmt.c - statements and blocks (6.8) */
#include "parse.h"

static Node *stmt(Token **rest, Token *tok);

/* The slot to restore rsp from when jumping from chain `from` to a
   point whose chain is `to` (an ancestor of from). */
static Obj *vla_restore(VlaChain *from, VlaChain *to) {
  Obj *slot = NULL;
  for (VlaChain *c = from; c && c != to; c = c->outer)
    slot = c->sp_slot;
  return slot;
}

static bool chain_contains(VlaChain *from, VlaChain *target) {
  if (!target)
    return true;
  for (VlaChain *c = from; c; c = c->outer)
    if (c == target)
      return true;
  return false;
}

void resolve_goto_labels(void) {
  for (Node *x = gotos; x; x = x->goto_next) {
    Node *y = labels;
    for (; y; y = y->goto_next)
      if (!strcmp(x->label, y->label))
        break;
    if (!y)
      error_tok(x->tok->next, "label '%s' used but not defined", x->label);
    x->unique_label = y->unique_label;
    if (!chain_contains(x->vchain, y->vchain))
      error_tok(x->tok, "jump into scope of identifier with variably modified type");
    x->target_vla_sp = vla_restore(x->vchain, y->vchain);
  }
  gotos = labels = NULL;
}

static Node *expr_stmt(Token **rest, Token *tok) {
  if (equal(tok, ";")) {
    *rest = tok->next;
    return new_node(ND_BLOCK, tok);
  }
  Node *node = new_node(ND_EXPR_STMT, tok);
  node->lhs = expr(&tok, tok);
  add_type(node->lhs);
  *rest = skip(tok, ";");
  return node;
}

static bool is_label_start(Token *tok) {
  return tok->kind == TK_IDENT && equal(tok->next, ":");
}

static Node *label_target_stmt(Token **rest, Token *tok, Token *label_tok) {
  /* In C17 a label must be followed by a statement. */
  if (equal(tok, "}"))
    error_tok(label_tok, "label at end of compound statement");
  if (is_typename(tok) && !(tok->kind == TK_IDENT && equal(tok->next, ":")) &&
      !equal(tok, "__extension__"))
    error_tok(tok, "a label can only be part of a statement and a declaration is not a statement");
  if (equal(tok, "_Static_assert"))
    error_tok(tok, "a label can only be part of a statement and a declaration is not a statement");
  return stmt(rest, tok);
}

static Node *cond_controlling(Token **rest, Token *tok) {
  Token *start = tok;
  Node *c = expr(rest, tok);
  return cond_expr_check(c, start);
}

static Node *asm_stmt(Token **rest, Token *tok) {
  Node *node = new_node(ND_ASM, tok);
  tok = tok->next;
  while (equal(tok, "volatile") || equal(tok, "__volatile__") || equal(tok, "__volatile") ||
         equal(tok, "inline") || equal(tok, "goto"))
    tok = tok->next;
  tok = skip(tok, "(");
  if (tok->kind != TK_STR || tok->ty->base->kind != TY_CHAR)
    error_tok(tok, "expected string literal");
  node->asm_str = tok->str;
  tok = tok->next;
  if (equal(tok, ":"))
    error_tok(tok, "extended asm operands are not supported");
  *rest = skip(skip(tok, ")"), ";");
  return node;
}

static Node *stmt(Token **rest, Token *tok) {
  if (equal(tok, "return")) {
    Node *node = new_node(ND_RETURN, tok);
    Type *rty = current_fn->ty->return_ty;
    if (consume(rest, tok->next, ";")) {
      if (!is_void(rty))
        warn_tok(tok, "'return' with no value, in function returning non-void");
      return node;
    }
    Token *etok = tok->next;
    Node *exp = expr(&tok, tok->next);
    *rest = skip(tok, ";");
    add_type(exp);
    if (is_void(rty)) {
      if (!is_void(exp->ty))
        warn_tok(etok, "'return' with a value, in function returning void");
      node->lhs = exp;
      return node;
    }
    node->lhs = convert_for_assign(exp, unqual(rty), etok, "returning");
    return node;
  }

  if (equal(tok, "if")) {
    Node *node = new_node(ND_IF, tok);
    tok = skip(tok->next, "(");
    node->cond = cond_controlling(&tok, tok);
    tok = skip(tok, ")");
    /* Selection statements are blocks (6.8.4p3). */
    enter_scope();
    VlaChain *saved = cur_vla;
    node->then = stmt(&tok, tok);
    cur_vla = saved;
    leave_scope();
    if (equal(tok, "else")) {
      enter_scope();
      node->els = stmt(&tok, tok->next);
      cur_vla = saved;
      leave_scope();
    }
    *rest = tok;
    return node;
  }

  if (equal(tok, "switch")) {
    Node *node = new_node(ND_SWITCH, tok);
    tok = skip(tok->next, "(");
    Token *ctok = tok;
    Node *c = expr(&tok, tok);
    add_type(c);
    c = rval(c);
    if (!is_integer(c->ty))
      error_tok(ctok, "switch quantity not an integer");
    node->cond = new_cast(c, int_promote(c->ty));
    tok = skip(tok, ")");

    Node *sw = current_switch;
    current_switch = node;
    VlaChain *sw_vla = switch_vla;
    switch_vla = cur_vla;

    char *brk = brk_label;
    VlaChain *bv = brk_vla;
    brk_label = node->brk_label = new_unique_name();
    brk_vla = cur_vla;

    enter_scope();
    VlaChain *saved = cur_vla;
    node->then = stmt(rest, tok);
    cur_vla = saved;
    leave_scope();

    current_switch = sw;
    switch_vla = sw_vla;
    brk_label = brk;
    brk_vla = bv;
    return node;
  }

  if (equal(tok, "case")) {
    if (!current_switch)
      error_tok(tok, "case label not within a switch statement");
    if (cur_vla != switch_vla)
      error_tok(tok, "switch jumps into scope of identifier with variably modified type");

    Node *node = new_node(ND_CASE, tok);
    Token *vtok = tok->next;
    Node *e = conditional(&tok, tok->next);
    int64_t begin = ice_value(e, vtok, "case label");
    int64_t end = begin;
    if (equal(tok, "...")) {
      Token *t2 = tok->next;
      Node *e2 = conditional(&tok, tok->next);
      end = ice_value(e2, t2, "case label");
      if (end < begin)
        warn_tok(t2, "empty range specified");
    }
    tok = skip(tok, ":");

    /* Convert to the promoted type of the controlling expression. */
    Type *cty = current_switch->cond->ty;
    if (cty->size == 4) {
      begin = cty->is_unsigned ? (int64_t)(uint32_t)begin : (int64_t)(int32_t)begin;
      end = cty->is_unsigned ? (int64_t)(uint32_t)end : (int64_t)(int32_t)end;
    }
    for (Node *n = current_switch->case_next; n; n = n->case_next)
      if (!(end < n->begin || begin > n->end))
        error_tok(vtok, "duplicate case value");

    node->label = new_unique_name();
    node->begin = begin;
    node->end = end;
    node->case_next = current_switch->case_next;
    current_switch->case_next = node;
    node->lhs = label_target_stmt(rest, tok, node->tok);
    return node;
  }

  if (equal(tok, "default")) {
    if (!current_switch)
      error_tok(tok, "'default' label not within a switch statement");
    if (current_switch->default_case)
      error_tok(tok, "multiple default labels in one switch");
    if (cur_vla != switch_vla)
      error_tok(tok, "switch jumps into scope of identifier with variably modified type");
    Node *node = new_node(ND_CASE, tok);
    tok = skip(tok->next, ":");
    node->label = new_unique_name();
    current_switch->default_case = node;
    node->lhs = label_target_stmt(rest, tok, node->tok);
    return node;
  }

  if (equal(tok, "for")) {
    Node *node = new_node(ND_FOR, tok);
    tok = skip(tok->next, "(");

    enter_scope();
    VlaChain *saved_vla = cur_vla;

    char *brk = brk_label;
    char *cont = cont_label;
    VlaChain *bv = brk_vla, *cv = cont_vla;
    brk_label = node->brk_label = new_unique_name();
    cont_label = node->cont_label = new_unique_name();

    if (equal(tok, "_Static_assert")) {
      error_tok(tok, "expected expression or declaration of an object");
    } else if (is_typename(tok)) {
      VarAttr attr = {0};
      Token *dstart = tok;
      Type *basety = declspec(&tok, tok, &attr);
      if (attr.is_typedef || attr.is_static || attr.is_extern || attr.is_tls)
        error_tok(dstart, "declaration of non-variable in 'for' loop initial declaration");
      node->init = declaration(&tok, tok, basety, &attr);
    } else {
      node->init = expr_stmt(&tok, tok);
    }

    brk_vla = cur_vla;
    cont_vla = cur_vla;

    if (!equal(tok, ";"))
      node->cond = cond_controlling(&tok, tok);
    tok = skip(tok, ";");

    if (!equal(tok, ")")) {
      node->inc = expr(&tok, tok);
      add_type(node->inc);
    }
    tok = skip(tok, ")");

    enter_scope();
    VlaChain *body_vla = cur_vla;
    node->then = stmt(rest, tok);
    cur_vla = body_vla;
    leave_scope();

    /* Leaving the loop releases VLAs declared in its init clause. */
    node->vla_sp = vla_restore(cur_vla, saved_vla);
    cur_vla = saved_vla;
    leave_scope();
    brk_label = brk;
    cont_label = cont;
    brk_vla = bv;
    cont_vla = cv;
    return node;
  }

  if (equal(tok, "while")) {
    Node *node = new_node(ND_FOR, tok);
    tok = skip(tok->next, "(");
    node->cond = cond_controlling(&tok, tok);
    tok = skip(tok, ")");

    char *brk = brk_label;
    char *cont = cont_label;
    VlaChain *bv = brk_vla, *cv = cont_vla;
    brk_label = node->brk_label = new_unique_name();
    cont_label = node->cont_label = new_unique_name();
    brk_vla = cont_vla = cur_vla;

    enter_scope();
    VlaChain *saved = cur_vla;
    node->then = stmt(rest, tok);
    cur_vla = saved;
    leave_scope();

    brk_label = brk;
    cont_label = cont;
    brk_vla = bv;
    cont_vla = cv;
    return node;
  }

  if (equal(tok, "do")) {
    Node *node = new_node(ND_DO, tok);

    char *brk = brk_label;
    char *cont = cont_label;
    VlaChain *bv = brk_vla, *cv = cont_vla;
    brk_label = node->brk_label = new_unique_name();
    cont_label = node->cont_label = new_unique_name();
    brk_vla = cont_vla = cur_vla;

    enter_scope();
    VlaChain *saved = cur_vla;
    node->then = stmt(&tok, tok->next);
    cur_vla = saved;
    leave_scope();

    brk_label = brk;
    cont_label = cont;
    brk_vla = bv;
    cont_vla = cv;

    tok = skip(tok, "while");
    tok = skip(tok, "(");
    node->cond = cond_controlling(&tok, tok);
    tok = skip(tok, ")");
    *rest = skip(tok, ";");
    return node;
  }

  if (equal(tok, "__asm__") || equal(tok, "__asm"))
    return asm_stmt(rest, tok);

  if (equal(tok, "goto")) {
    if (equal(tok->next, "*"))
      error_tok(tok->next, "computed goto is not supported");
    Node *node = new_node(ND_GOTO, tok);
    if (tok->next->kind != TK_IDENT)
      error_tok(tok->next, "expected identifier");
    node->label = tok_text(tok->next);
    node->vchain = cur_vla;
    node->goto_next = gotos;
    gotos = node;
    *rest = skip(tok->next->next, ";");
    return node;
  }

  if (equal(tok, "break")) {
    if (!brk_label)
      error_tok(tok, "break statement not within loop or switch");
    Node *node = new_node(ND_GOTO, tok);
    node->unique_label = brk_label;
    node->target_vla_sp = vla_restore(cur_vla, brk_vla);
    *rest = skip(tok->next, ";");
    return node;
  }

  if (equal(tok, "continue")) {
    if (!cont_label)
      error_tok(tok, "continue statement not within a loop");
    Node *node = new_node(ND_GOTO, tok);
    node->unique_label = cont_label;
    node->target_vla_sp = vla_restore(cur_vla, cont_vla);
    *rest = skip(tok->next, ";");
    return node;
  }

  if (is_label_start(tok)) {
    Node *node = new_node(ND_LABEL, tok);
    node->label = tok_text(tok);
    for (Node *l = labels; l; l = l->goto_next)
      if (!strcmp(l->label, node->label))
        error_tok(tok, "duplicate label '%s'", node->label);
    node->unique_label = new_unique_name();
    node->vchain = cur_vla;
    node->goto_next = labels;
    labels = node;
    Token *t = skip_attributes(tok->next->next);
    node->lhs = label_target_stmt(rest, t, tok);
    return node;
  }

  if (equal(tok, "{"))
    return compound_stmt(rest, tok->next, true);

  return expr_stmt(rest, tok);
}

/* compound-stmt = (declaration | stmt)* "}" */
Node *compound_stmt(Token **rest, Token *tok, bool new_scope) {
  Node *node = new_node(ND_BLOCK, tok);
  Node head = {0};
  Node *cur = &head;

  if (new_scope)
    enter_scope();
  VlaChain *saved_vla = cur_vla;

  while (!equal(tok, "}")) {
    if (tok->kind == TK_EOF)
      error_tok(tok, "expected '}' at end of input");

    if (equal(tok, "_Static_assert")) {
      tok = static_assertion(tok);
      continue;
    }

    if (is_typename(tok) && !is_label_start(tok)) {
      /* __extension__ before an expression statement */
      if (equal(tok, "__extension__")) {
        Token *t = skip_attributes(tok);
        if (!is_typename(t)) {
          cur = cur->next = stmt(&tok, t);
          add_type(cur);
          continue;
        }
      }
      VarAttr attr = {0};
      Type *basety = declspec(&tok, tok, &attr);
      cur = cur->next = declaration(&tok, tok, basety, &attr);
      continue;
    }

    cur = cur->next = stmt(&tok, tok);
    add_type(cur);
  }

  node->vla_sp = vla_restore(cur_vla, saved_vla);
  cur_vla = saved_vla;
  if (new_scope)
    leave_scope();

  node->body = head.next;
  *rest = tok->next;
  return node;
}
