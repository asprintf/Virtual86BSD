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

/*
 * c17.h - c17: declarations shared by the modules of the compiler, an
 * ISO/IEC 9899:2018 (C17) compiler for x86-64 (System V ABI, ELF, Linux)
 * with its own assembler and link editor.
 */
#define _POSIX_C_SOURCE 202405L
#define _XOPEN_SOURCE 800

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifndef C17_VERSION
#define C17_VERSION "1.0"
#endif

/* NORETURN, PRINTF; the interfaces of POSIX.1-2024 on older C libraries */
#include "port.h"

typedef struct Type Type;
typedef struct Node Node;
typedef struct Member Member;
typedef struct Relocation Relocation;
typedef struct Obj Obj;
typedef struct Token Token;
typedef struct File File;
typedef struct Param Param;

/* ------------------------------------------------------------------ util */

/* the tokenizer and preprocessor, and the utilities they share: cpp.c */
#include "cpp.h"

bool startswith(const char *p, const char *q);
bool endswith(const char *p, const char *q);
char *path_dirname(const char *path);
char *path_basename(const char *path);
char *replace_extn(char *tmpl, char *extn);
bool file_exists(const char *path);
bool is_regular_file(const char *path);


/* ---------------------------------------------------------- diagnostics */

extern bool opt_w;        /* suppress warnings */
extern int nwarnings;
extern jmp_buf *error_jmp; /* where a fatal translation error returns to */

NORETURN void fatal(char *fmt, ...) PRINTF(1, 2);
NORETURN void error_tok(Token *tok, char *fmt, ...) PRINTF(2, 3);
void warn_tok(Token *tok, char *fmt, ...) PRINTF(2, 3);
void note_tok(Token *tok, char *fmt, ...) PRINTF(2, 3);
NORETURN void internal_error(char *file, int line, char *msg);
#define unreachable() internal_error(__FILE__, __LINE__, "unreachable code reached")

/* ------------------------------------------------------------- tokenize */

typedef enum {
  TK_IDENT,   /* identifiers */
  TK_PUNCT,   /* punctuators */
  TK_KEYWORD, /* keywords (after phase 7 conversion) */
  TK_STR,     /* string literals */
  TK_NUM,     /* numeric literals (after conversion) */
  TK_PP_NUM,  /* preprocessing numbers */
  TK_CHAR,    /* character constants (raw, before conversion) */
  TK_OTHER,   /* each non-white-space character that cannot be one of the above */
  TK_EOF,
} TokenKind;

/* a source file, for diagnostics and -g */
struct File {
  char *name;
  char *contents;
  bool is_system;
};

struct Token {
  TokenKind kind;
  Token *next;
  int64_t val;     /* TK_NUM integer value */
  long double fval;/* TK_NUM floating value */
  char *loc;       /* original spelling */
  int len;
  char *punct;     /* canonical spelling of a punctuator */
  Type *ty;        /* TK_NUM / TK_STR type */
  char *str;       /* string literal contents incl. terminator */
  int str_len;     /* in bytes, including terminator */

  File *file;
  char *filename;  /* the presumed name (6.10.4) */
  int line_no;     /* the presumed line */
  int col_no;
  bool at_bol;     /* first token on a logical line */
  bool has_space;  /* preceded by white space */
  Token *origin;   /* if this came from macro expansion, the invocation */
};

Token *tokenize_string_literal(Token *tok, Type *basety);
bool equal(Token *tok, char *op);
Token *skip(Token *tok, char *op);
bool consume(Token **rest, Token *tok, char *str);
char *tok_text(Token *tok);
int tok_line(Token *tok);
char *tok_filename(Token *tok);

/* ----------------------------------------------------------- preprocess */

/* the preprocessed tokens of a translation unit: tokens of the compiler
   (phases 5 to 7 done: literals converted, keywords, strings joined) */
Token *convert_tokens(struct token *t);
/* the preprocessor's diagnostics, as the compiler's */
void cpp_diagnostic(void *arg, bool error, const struct token *t, const char *msg);

/* ---------------------------------------------------------------- types */

typedef enum {
  TY_VOID,
  TY_BOOL,
  TY_CHAR,
  TY_SHORT,
  TY_INT,
  TY_LONG,
  TY_LLONG,
  TY_FLOAT,
  TY_DOUBLE,
  TY_LDOUBLE,
  TY_ENUM,
  TY_PTR,
  TY_FUNC,
  TY_ARRAY,
  TY_VLA,
  TY_STRUCT,
  TY_UNION,
} TypeKind;

enum {
  Q_CONST = 1,
  Q_VOLATILE = 2,
  Q_RESTRICT = 4,
  Q_ATOMIC = 8,
};

struct Type {
  TypeKind kind;
  int64_t size;   /* sizeof; -1 if incomplete */
  int align;
  bool is_unsigned;
  bool is_plain_char;  /* `char`, distinct from signed/unsigned char */
  bool is_complex;     /* _Complex float/double/long double */
  int qual;            /* Q_* */
  Type *origin;        /* unqualified type this is a qualified/named copy of */

  /* Pointer-to or array-of */
  Type *base;

  /* Array */
  int64_t array_len;   /* -1 for incomplete array */
  bool array_static;   /* [static n] in parameter */
  int array_qual;      /* qualifiers inside [] in parameter */

  /* Variable-length array */
  Node *vla_len;
  Obj *vla_size;

  /* Struct/union/enum */
  Member *members;
  bool is_flexible;
  bool is_packed;
  bool is_complete;    /* for tags */
  Type *tag_type;      /* canonical tag type object shared by all copies */
  Token *tag;
  int tag_scope_id;
  Type *enum_int;      /* compatible integer type of an enum */

  /* Qualified variants of a struct/union (indexed by Q_* bits) */
  Type **variants;

  /* Function */
  Type *return_ty;
  Param *params;
  bool is_variadic;
  bool has_proto;      /* prototype (vs. old-style identifier list) */
  bool is_noreturn;
};

struct Param {
  Param *next;
  Type *ty;        /* adjusted parameter type */
  Token *name;
  Token *pos;
  Type *decl_ty;   /* as declared, before adjustment */
  Obj *var;        /* its object, which later parameters' types may use */
};

struct Member {
  Member *next;
  Type *ty;
  Token *tok;
  Token *name;
  int idx;
  int align;
  int64_t offset;

  bool is_bitfield;
  int bit_offset;
  int bit_width;
  bool is_anon;       /* anonymous struct/union member */
};

extern Type *ty_void;
extern Type *ty_bool;
extern Type *ty_char;
extern Type *ty_schar;
extern Type *ty_uchar;
extern Type *ty_short;
extern Type *ty_ushort;
extern Type *ty_int;
extern Type *ty_uint;
extern Type *ty_long;
extern Type *ty_ulong;
extern Type *ty_llong;
extern Type *ty_ullong;
extern Type *ty_float;
extern Type *ty_double;
extern Type *ty_ldouble;
extern Type *ty_cfloat;
extern Type *ty_cdouble;
extern Type *ty_cldouble;
extern Type *ty_wchar;   /* int */
extern Type *ty_char16;  /* unsigned short (uint_least16_t) */
extern Type *ty_char32;  /* unsigned int (uint_least32_t) */
extern Type *ty_size_t;
extern Type *ty_ptrdiff_t;

bool is_integer(Type *ty);
bool is_flonum(Type *ty);     /* real floating */
bool is_real(Type *ty);       /* integer or real floating */
bool is_arith(Type *ty);      /* includes complex */
bool is_scalar(Type *ty);
bool is_aggregate(Type *ty);
bool is_su(Type *ty);         /* struct or union */
bool is_array(Type *ty);
bool is_ptr(Type *ty);
bool is_func(Type *ty);
bool is_void(Type *ty);
bool is_complete(Type *ty);
bool is_object_complete(Type *ty);
bool is_vm(Type *ty);         /* variably-modified */
bool is_compatible(Type *t1, Type *t2);
bool is_compatible_unqual(Type *t1, Type *t2);
Type *composite_type(Type *t1, Type *t2);
Type *copy_type(Type *ty);
Type *unqual(Type *ty);
Type *qualify(Type *ty, int qual);
Type *pointer_to(Type *base);
Type *func_type(Type *return_ty);
Type *array_of(Type *base, int64_t size);
Type *vla_of(Type *base, Node *expr);
Type *enum_type(void);
Type *struct_type(void);
Type *complex_of(Type *real);
Type *complex_base(Type *ty);
void init_types(void);
Type *int_promote(Type *ty);
Type *usual_arith_conv(Type *a, Type *b);
int int_rank(Type *ty);
void add_type(Node *node);
char *type_name(Type *ty);
Type *decay(Type *ty);
int64_t type_size(Type *ty);
int type_align(Type *ty);
Member *struct_members(Type *ty);
Type *su_def(Type *ty);

/* ----------------------------------------------------------------- AST */

struct Obj {
  Obj *next;
  char *name;
  char *asm_name;   /* symbol name used in the assembly output */
  Type *ty;
  Token *tok;
  bool is_local;
  int align;

  /* Local variable */
  int64_t offset;
  bool is_param;
  bool is_vla_ptr;

  /* Global variable or function */
  bool is_function;
  bool is_definition;
  bool is_static;      /* internal linkage */
  bool is_extern_decl;
  bool is_tentative;
  bool is_tls;
  bool is_used;        /* referenced */
  bool is_addr_taken;
  bool is_inline;
  bool is_extern_inline; /* an inline definition with an external declaration */
  bool is_noreturn;
  bool is_register;
  bool is_compound_lit;
  bool is_string_lit;

  /* GNU attributes of objects and functions (an extension) */
  bool is_weak;
  char *alias;         /* alias("target") */
  char *visibility;    /* "hidden", "protected", "internal" */
  bool tls_ie;         /* tls_model("initial-exec") */
  char *section;       /* section("name") */
  int ctor_prio;       /* constructor: 0 none, 65536 without priority */
  int dtor_prio;
  bool attr_used;

  /* Global variable */
  char *init_data;
  int64_t data_size;   /* if larger than the type (initialized flexible array) */
  Relocation *rel;

  /* Function */
  Obj *params;
  Node *body;
  Obj *locals;
  Obj *va_area;
  Obj *alloca_bottom;
  int64_t stack_size;
  bool has_vla;
  Obj *struct_ret;     /* hidden pointer for MEMORY-class returns */
  StrArray refs;       /* functions referenced from this one */
  bool is_live;
};

/* Global variable can be initialized either by a constant expression
   or a pointer to another global variable plus an addend. */
struct Relocation {
  Relocation *next;
  int64_t offset;
  char **label;
  int64_t addend;
  int size;
};

typedef enum {
  ND_NULL_EXPR, // Do nothing
  ND_ADD,       // +
  ND_SUB,       // -
  ND_MUL,       // *
  ND_DIV,       // /
  ND_NEG,       // unary -
  ND_MOD,       // %
  ND_BITAND,    // &
  ND_BITOR,     // |
  ND_BITXOR,    // ^
  ND_SHL,       // <<
  ND_SHR,       // >>
  ND_EQ,        // ==
  ND_NE,        // !=
  ND_LT,        // <
  ND_LE,        // <=
  ND_ASSIGN,    // =
  ND_COND,      // ?:
  ND_COMMA,     // ,
  ND_MEMBER,    // . (struct member access)
  ND_ADDR,      // unary &
  ND_DEREF,     // unary *
  ND_NOT,       // !
  ND_BITNOT,    // ~
  ND_LOGAND,    // &&
  ND_LOGOR,     // ||
  ND_RETURN,    // "return"
  ND_IF,        // "if"
  ND_FOR,       // "for" or "while"
  ND_DO,        // "do"
  ND_SWITCH,    // "switch"
  ND_CASE,      // "case"
  ND_BLOCK,     // { ... }
  ND_GOTO,      // "goto"
  ND_LABEL,     // Labeled statement
  ND_FUNCALL,   // Function call
  ND_EXPR_STMT, // Expression statement
  ND_STMT_EXPR, // Statement expression (internal)
  ND_VAR,       // Variable
  ND_VLA_PTR,   // VLA designator
  ND_NUM,       // Integer/float constant
  ND_CAST,      // Type cast
  ND_MEMZERO,   // Zero-clear a stack variable
  ND_ASM,       // "asm"
  ND_CAS,       // Atomic compare-and-swap
  ND_EXCH,      // Atomic exchange
  ND_ATOMIC_RMW,// Atomic read-modify-write (op in node->rmw_op)
  ND_ATOMIC_LOAD,
  ND_ATOMIC_STORE,
  ND_FENCE,
  ND_VA_START,
  ND_VA_ARG,
  ND_VA_COPY,
  ND_ALLOCA,
  ND_FRAMEADDR, // __builtin_frame_address / __builtin_return_address (val: level; rmw_op 1: return)
  ND_REAL,      // __real__ (complex)
  ND_IMAG,      // __imag__ (complex)
  ND_MKCOMPLEX, // build complex from (lhs, rhs)
  ND_UNREACHABLE,
} NodeKind;

struct Node {
  NodeKind kind;
  Node *next;
  Type *ty;
  Token *tok;

  Node *lhs;
  Node *rhs;

  /* "if" or "for" */
  Node *cond;
  Node *then;
  Node *els;
  Node *init;
  Node *inc;

  /* "break" and "continue" labels */
  char *brk_label;
  char *cont_label;

  /* Block or statement expression */
  Node *body;

  /* Struct member access */
  Member *member;

  /* Function call */
  Type *func_ty;
  Node *args;
  bool pass_by_stack;
  int64_t stack_off;   /* offset in the outgoing argument area */
  Obj *ret_buffer;

  /* Goto or labeled statement */
  char *label;
  char *unique_label;
  Node *goto_next;

  /* Switch */
  Node *case_next;
  Node *default_case;

  /* Case */
  int64_t begin;
  int64_t end;

  /* "asm" string literal */
  char *asm_str;

  /* Atomic ops */
  Node *cas_addr;
  Node *cas_old;
  Node *cas_new;
  NodeKind rmw_op;
  bool rmw_fetch_first; /* return old value (fetch_op) rather than new */

  /* Variable */
  Obj *var;

  /* Numeric literal */
  int64_t val;
  long double fval;
  long double fimag;   /* imaginary part of a complex constant */

  /* Block-scope VLA cleanup: stack pointer to restore when leaving */
  Obj *vla_sp;
  Obj *target_vla_sp;
  void *vchain;        /* VLA chain in scope at this node (goto/label) */
};

Node *new_cast(Node *expr, Type *ty);
int64_t const_expr(Token **rest, Token *tok);
Obj *parse(Token *tok);
bool is_const_expr(Node *node);
int64_t eval(Node *node);
extern char *base_file;

/* ------------------------------------------------------------- codegen */

typedef enum {
  CLASS_NONE,
  CLASS_INTEGER,
  CLASS_SSE,
  CLASS_SSEUP,
  CLASS_X87,
  CLASS_X87UP,
  CLASS_COMPLEX_X87,
  CLASS_MEMORY,
} ArgClass;

void codegen(Obj *prog, FILE *out);
int align_to(int64_t n, int64_t align);
int64_t align_to64(int64_t n, int64_t align);

/* --------------------------------------------------------------- driver */

extern bool opt_pic;    /* -G: position-independent code */
extern bool opt_g;      /* -g */
extern int opt_O;       /* optimization level */
extern bool opt_E;
extern bool opt_fcommon;
extern StrArray input_paths;
extern char *libdir;    /* where our headers & libs live */
