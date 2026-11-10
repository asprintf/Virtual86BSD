/*
 * cpp.h - a C tokenizer and preprocessor (translation phases 1 to 4 of
 * C17), for c17, cflow, ctags and cxref
 */

#ifndef CPP_H
#define CPP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* ---- utilities, shared with the programs that use the preprocessor ---- */

extern const char *progname;
void *xmalloc(size_t n);
void *xcalloc(size_t n, size_t m);
void *xrealloc(void *p, size_t n);
char *xstrdup(const char *s);
char *xstrndup(const char *s, size_t n);
char *format(const char *fmt, ...);

typedef struct {
  char *buf;
  size_t len, cap;
} Buf;
void buf_putc(Buf *b, int c);
void buf_puts(Buf *b, const char *s);
void buf_write(Buf *b, const char *s, size_t n);
void buf_printf(Buf *b, const char *fmt, ...);

typedef struct {
  char **data;
  int capacity;
  int len;
} StrArray;
void strarray_push(StrArray *arr, char *s);

typedef struct {
  char *key;
  int keylen;
  void *val;
} HashEntry;
typedef struct {
  HashEntry *buckets;
  int capacity;
  int used;
} HashMap;
void *hashmap_get(HashMap *map, const char *key);
void *hashmap_get2(HashMap *map, const char *key, int keylen);
void hashmap_put(HashMap *map, const char *key, void *val);
void hashmap_put2(HashMap *map, const char *key, int keylen, void *val);
void hashmap_delete(HashMap *map, const char *key);
/* a file's contents ("-": the standard input), ending with a newline;
   NULL (errno set) if it cannot be read */
char *read_file(const char *path);

/* ---- source files ---- */

struct linechange {
  int from;          /* the physical line from which it applies */
  int delta;         /* added to the physical line */
  const char *name;  /* the presumed name of the file */
};

struct srcfile {
  char *path;        /* as opened ("<built-in>/stddef.h" for built-in headers) */
  char *display;     /* the name before any #line */
  char *contents;    /* after translation phases 1 and 2 (lines kept) */
  int number;        /* 1, 2, ... in the order read */
  int dirindex;      /* where it was found in the search list (-1: elsewhere) */
  bool system;       /* from a system directory, or #pragma GCC system_header */
  char *quotedir;    /* where #include "..." looks first (NULL: its own directory) */
  struct linechange *lc;
  int nlc, caplc;
};

/* ---- tokens ---- */

enum { CPP_IDENT, CPP_PUNCT, CPP_NUM, CPP_CHAR, CPP_STR, CPP_OTHER, CPP_EOF };

struct hideset;

struct token {
  int kind;               /* CPP_NUM: a preprocessing number */
  char *text;             /* NUL-terminated; for punctuators the canonical
                             spelling ("[" for "<:") */
  char *loc;              /* the spelling as written: in the file's contents, or
                             in a buffer of its own for made tokens */
  int len;                /* of the spelling */
  struct srcfile *sf;     /* the file it is from (NULL: made up) */
  int line, col;          /* physical line and display column there */
  bool bol;               /* first on its line */
  bool space;             /* white space (or a comment) before it */
  struct hideset *hs;     /* macros not to expand it as */
  struct token *origin;   /* made by expanding the macro invocation here */
  struct token *next;
  /* after cpp_run (or cpp_tokenize): where it is, after #line */
  const char *file;
  int oline;
};

/* the presumed file and line of a token (6.10.4), of the invocation if it
   comes from a macro expansion */
void cpp_where(const struct token *t, const char **file, int *line);
bool tok_is(const struct token *t, const char *s);
/* the source line of a token, if it has one in a file: its start and length */
bool cpp_source_line(const struct token *t, const char **line, int *len);

/* ---- the preprocessor ---- */

struct cpp_hooks {
  /* a macro defined: the name token of the #define */
  void (*define)(void *arg, const struct token *name);
  /* a macro used: expanded, or tested by #ifdef, #ifndef, defined */
  void (*use)(void *arg, const struct token *name);
  /* a diagnostic: error true for violations of the syntax rules and
     constraints of the standard, false for warnings.  May not return
     for an error.  Without it, errors are written as
     "progname: file:line: message" and the preprocessor carries on;
     warnings are not written. */
  void (*diag)(void *arg, bool error, const struct token *t, const char *msg);
  void *arg;
};

struct cpp;

/* a warning on the standard error: "progname: file:line: message" */
void cpp_warn(const struct token *t, const char *fmt, ...);

/* the tokens of a file, not preprocessed (directives are tokens too);
   NULL (errno set) if it cannot be read; display: the name to give it */
struct token *cpp_tokenize(const char *path, const char *display);

struct cpp *cpp_new(void);
/* -D name[=def], -U name, in the order of the command line; -D name(x)=def
   too */
void cpp_define(struct cpp *p, const char *arg);
void cpp_undef(struct cpp *p, const char *name);
/* -I dir, in order */
void cpp_include_dir(struct cpp *p, const char *dir);
/* the system's directories, in order; without any, the usual ones */
void cpp_system_dir(struct cpp *p, const char *dir);
void cpp_set_hooks(struct cpp *p, const struct cpp_hooks *h);
/* the directory where #include "..." in the file given to cpp_run looks
   first (NULL: the file's own directory) */
void cpp_set_quote_dir(struct cpp *p, const char *dir);
/* the input is the output of the preprocessor (a .i file): only line
   markers and #pragma are understood */
void cpp_set_preprocessed(struct cpp *p, bool on);
/* keep #pragma lines in the output, as tokens "#pragma ..." (for -E) */
void cpp_keep_pragmas(struct cpp *p, bool on);
/* the preprocessed tokens of a file (a translation unit), ending with a
   CPP_EOF token; NULL (errno set) if the file cannot be read.  Macros
   defined by a translation unit are forgotten after it. */
struct token *cpp_run(struct cpp *p, const char *path, const char *display);
/* the preprocessed tokens as text, with line markers (c17 -E) */
void cpp_print(struct token *t, FILE *out);
/* the macros defined at the end of the last cpp_run (c17 -E -dM) */
void cpp_print_macros(struct cpp *p, FILE *out);
/* is it a "#pragma ..." token of cpp_keep_pragmas? */
bool cpp_is_pragma(const struct token *t);
/* the main file of the last cpp_run */
const char *cpp_base_file(struct cpp *p);

/* the encoding of characters in identifiers (C17 annex D) */
int encode_utf8(char *buf, uint32_t c);
uint32_t decode_utf8(char **new_pos, char *p);
bool is_ident1(uint32_t c);
bool is_ident2(uint32_t c);
/* the columns of len bytes of text (tabs to multiples of 8) */
int display_width(const char *p, int len);

#endif
