/* parse.h - state shared by the parser's translation units */
#include "c17.h"

/* An ordinary identifier binding in a scope. */
typedef struct {
  Obj *var;
  Type *type_def;
  Type *enum_ty;
  int64_t enum_val;
  Token *tok;  /* declaration site */
} VarScope;

typedef struct Scope Scope;
struct Scope {
  Scope *next;
  HashMap vars;  /* ordinary identifiers */
  HashMap tags;  /* struct/union/enum tags */
  int id;
  bool is_proto;  /* function prototype scope */
  bool is_file;
};

/* Storage-class and function specifiers */
typedef struct {
  bool is_typedef;
  bool is_static;
  bool is_extern;
  bool is_inline;
  bool is_tls;
  bool is_noreturn;
  bool is_register;
  bool is_auto;
  int align;        /* _Alignas; 0 if none */
  Token *storage_tok;
} VarAttr;

/* A chain of VLA objects in scope, innermost first. */
typedef struct VlaChain VlaChain;
struct VlaChain {
  VlaChain *outer;
  Obj *sp_slot;   /* saved stack pointer before allocating this VLA */
};

/* Initializer tree */
typedef struct Initializer Initializer;
struct Initializer {
  Initializer *next;
  Type *ty;
  Token *tok;
  bool is_flexible;
  Node *expr;               /* scalar initializer */
  Initializer **children;   /* aggregate */
  Member *mem;              /* union: initialized member */
  int64_t nchildren;
  bool zero_fill;           /* children not explicitly initialized */
};

typedef struct InitDesg InitDesg;
struct InitDesg {
  InitDesg *next;
  int64_t idx;
  Member *member;
  Obj *var;
};

/* Parser state */
extern Obj *locals;
extern Obj *globals;
extern Scope *scope;
extern Obj *current_fn;
extern Node *gotos;
extern Node *labels;
extern char *brk_label;
extern char *cont_label;
extern VlaChain *brk_vla;
extern VlaChain *cont_vla;
extern VlaChain *cur_vla;
extern Node *current_switch;
extern VlaChain *switch_vla;
extern HashMap linkage_map;  /* name -> Obj for identifiers with linkage */
extern int scope_counter;

/* decl.c */
void enter_scope(void);
void leave_scope(void);
VarScope *find_var(Token *tok);
VarScope *push_scope(char *name, Token *tok);
Type *find_typedef(Token *tok);
bool is_typename(Token *tok);
Type *declspec(Token **rest, Token *tok, VarAttr *attr);
Type *declarator(Token **rest, Token *tok, Type *ty, Token **name, Token **pos);
Type *abstract_declarator(Token **rest, Token *tok, Type *ty);
Type *typename(Token **rest, Token *tok);
Node *declaration(Token **rest, Token *tok, Type *basety, VarAttr *attr);
Obj *new_lvar(char *name, Type *ty, Token *tok);
Obj *new_anon_gvar(Type *ty);
Obj *new_string_literal(Token *tok);
char *new_unique_name(void);
Token *skip_attributes(Token *tok);
Token *static_assertion(Token *tok);
Type *finish_vla_sizes(Type *ty, Node **code, Token *tok);
Node *compute_vla_size(Type *ty, Token *tok);
bool is_const_or_has_const_member(Type *ty);

/* init.c */
Node *lvar_initializer(Token **rest, Token *tok, Obj *var);
void gvar_initializer(Token **rest, Token *tok, Obj *var);
Type *init_compound_type(Token *tok, Type *ty, Token **end);

/* stmt.c */
Node *compound_stmt(Token **rest, Token *tok, bool new_scope);
void resolve_goto_labels(void);

/* expr.c */
Node *expr(Token **rest, Token *tok);
Node *assign(Token **rest, Token *tok);
Node *conditional(Token **rest, Token *tok);
Node *new_node(NodeKind kind, Token *tok);
Node *new_binary(NodeKind kind, Node *lhs, Node *rhs, Token *tok);
Node *new_unary(NodeKind kind, Node *expr, Token *tok);
Node *new_num(int64_t val, Token *tok);
Node *new_long(int64_t val, Token *tok);
Node *new_ulong(int64_t val, Token *tok);
Node *new_var_node(Obj *var, Token *tok);
Node *new_vla_ptr(Obj *var, Token *tok);
Node *rval(Node *node);
Node *convert_for_assign(Node *rhs, Type *ty, Token *tok, char *what);
Node *new_assign(Node *lhs, Node *rhs, Token *tok);
Node *to_bool(Node *node);
Node *new_add(Node *lhs, Node *rhs, Token *tok);
Node *new_sub(Node *lhs, Node *rhs, Token *tok);
bool is_lvalue(Node *node);
bool is_null_ptr_const(Node *node);
bool is_ice(Node *node);
int64_t ice_value(Node *node, Token *tok, char *what);
Node *struct_ref(Node *node, Token *tok);
Node *cond_expr_check(Node *cond, Token *tok);
Member *get_struct_member(Type *ty, Token *tok);

/* eval.c */
int64_t eval2(Node *node, char ***label);
long double eval_double(Node *node);
void eval_complex(Node *node, long double *re, long double *im);
bool is_const_expr(Node *node);
bool is_arith_const(Node *node);
