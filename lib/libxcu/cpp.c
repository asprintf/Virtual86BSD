/*
 * cpp.c - a C tokenizer and preprocessor, for c17, cflow, ctags and cxref
 *
 * The translation phases 1 to 4 of C17: trigraphs, line splices,
 * comments, preprocessing tokens (with universal character names and
 * UTF-8 in identifiers); #include and #include_next, #define and #undef
 * (object-like and function-like macros, variadic ones, # and ##), #if,
 * #ifdef, #ifndef, #elif, #else, #endif (with defined and __has_include),
 * #line (and the "# N "file"" line markers of c17 -E output), #error,
 * #warning, #pragma (once, push_macro, pop_macro, GCC system_header; the
 * rest kept for -E or ignored) and _Pragma, and the predefined macros.
 *
 * Macro expansion follows Dave Prosser's algorithm: every token carries
 * a "hide set" of macro names that must not be expanded again, which
 * gives exactly the rescanning and nested-replacement rules of 6.10.3.4;
 * placemarkers make ## with empty arguments work as 6.10.3.3 says.
 *
 * Choices: the headers a compiler provides (stddef.h, stdarg.h, float.h,
 * stdbool.h, stdalign.h, stdnoreturn.h, iso646.h, stdatomic.h, and a
 * complex.h that adds CMPLX to the C library's) are built in, searched
 * after the -I directories and before the system's.  The predefined
 * macros are those of the standard and of the system (__linux__,
 * __x86_64__, the sizes of the types), not those of GCC, so that system
 * headers declare in standard C.  Violations of the syntax rules and
 * constraints are errors when the user of the preprocessor wants them
 * (c17); otherwise they are written as warnings and the preprocessor
 * carries on with what can be made of the input (cflow, ctags, cxref).
 */

#define _XOPEN_SOURCE 800

#include "cpp.h"
#include "port.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

/* ================================================================ utilities */

const char *progname = "cpp";

static void out_of_memory(void) {
  fprintf(stderr, "%s: out of memory\n", progname);
  exit(2);
}

void *xmalloc(size_t n) {
  void *p = malloc(n ? n : 1);
  if (!p)
    out_of_memory();
  return p;
}

void *xcalloc(size_t n, size_t m) {
  void *p = calloc(n ? n : 1, m ? m : 1);
  if (!p)
    out_of_memory();
  return p;
}

void *xrealloc(void *p, size_t n) {
  p = realloc(p, n ? n : 1);
  if (!p)
    out_of_memory();
  return p;
}

char *xstrndup(const char *s, size_t n) {
  char *p = xmalloc(n + 1);
  memcpy(p, s, n);
  p[n] = '\0';
  return p;
}

char *xstrdup(const char *s) { return xstrndup(s, strlen(s)); }

static char *vformat(const char *fmt, va_list ap) {
  va_list ap2;
  va_copy(ap2, ap);
  int n = vsnprintf(NULL, 0, fmt, ap);
  char *buf = xmalloc(n + 1);
  vsnprintf(buf, n + 1, fmt, ap2);
  va_end(ap2);
  return buf;
}

char *format(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  char *s = vformat(fmt, ap);
  va_end(ap);
  return s;
}

static void buf_grow(Buf *b, size_t n) {
  if (b->len + n + 1 > b->cap) {
    size_t cap = b->cap ? b->cap : 64;
    while (cap < b->len + n + 1)
      cap *= 2;
    b->buf = xrealloc(b->buf, cap);
    b->cap = cap;
  }
}

void buf_putc(Buf *b, int c) {
  buf_grow(b, 1);
  b->buf[b->len++] = c;
  b->buf[b->len] = '\0';
}

void buf_write(Buf *b, const char *s, size_t n) {
  buf_grow(b, n);
  memcpy(b->buf + b->len, s, n);
  b->len += n;
  b->buf[b->len] = '\0';
}

void buf_puts(Buf *b, const char *s) { buf_write(b, s, strlen(s)); }

void buf_printf(Buf *b, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  char *s = vformat(fmt, ap);
  va_end(ap);
  buf_puts(b, s);
  free(s);
}

void strarray_push(StrArray *arr, char *s) {
  if (arr->len == arr->capacity) {
    arr->capacity = arr->capacity ? arr->capacity * 2 : 8;
    arr->data = xrealloc(arr->data, sizeof(char *) * arr->capacity);
  }
  arr->data[arr->len++] = s;
}

/* open addressing with linear probing */
#define TOMBSTONE ((char *)-1)

static uint64_t fnv_hash(const char *s, int len) {
  uint64_t hash = 0xcbf29ce484222325;
  for (int i = 0; i < len; i++) {
    hash *= 0x100000001b3;
    hash ^= (unsigned char)s[i];
  }
  return hash;
}

static bool match(HashEntry *ent, const char *key, int keylen) {
  return ent->key && ent->key != TOMBSTONE && ent->keylen == keylen &&
         memcmp(ent->key, key, keylen) == 0;
}

static void rehash(HashMap *map) {
  int nkeys = 0;
  for (int i = 0; i < map->capacity; i++)
    if (map->buckets[i].key && map->buckets[i].key != TOMBSTONE)
      nkeys++;
  int cap = map->capacity;
  while ((nkeys * 100) / cap >= 50)
    cap *= 2;
  HashMap map2 = {0};
  map2.buckets = xcalloc(cap, sizeof(HashEntry));
  map2.capacity = cap;
  for (int i = 0; i < map->capacity; i++) {
    HashEntry *ent = &map->buckets[i];
    if (ent->key && ent->key != TOMBSTONE)
      hashmap_put2(&map2, ent->key, ent->keylen, ent->val);
  }
  free(map->buckets);
  *map = map2;
}

static HashEntry *get_entry(HashMap *map, const char *key, int keylen) {
  if (!map->buckets)
    return NULL;
  uint64_t hash = fnv_hash(key, keylen);
  for (int i = 0; i < map->capacity; i++) {
    HashEntry *ent = &map->buckets[(hash + i) % map->capacity];
    if (match(ent, key, keylen))
      return ent;
    if (!ent->key)
      return NULL;
  }
  return NULL;
}

void *hashmap_get2(HashMap *map, const char *key, int keylen) {
  HashEntry *ent = get_entry(map, key, keylen);
  return ent ? ent->val : NULL;
}

void *hashmap_get(HashMap *map, const char *key) { return hashmap_get2(map, key, strlen(key)); }

void hashmap_put2(HashMap *map, const char *key, int keylen, void *val) {
  if (!map->buckets) {
    map->buckets = xcalloc(16, sizeof(HashEntry));
    map->capacity = 16;
  } else if ((map->used * 100) / map->capacity >= 70) {
    rehash(map);
  }
  uint64_t hash = fnv_hash(key, keylen);
  for (int i = 0; i < map->capacity; i++) {
    HashEntry *ent = &map->buckets[(hash + i) % map->capacity];
    if (match(ent, key, keylen)) {
      ent->val = val;
      return;
    }
    if (ent->key == TOMBSTONE || !ent->key) {
      if (!ent->key)
        map->used++;
      ent->key = (char *)key;
      ent->keylen = keylen;
      ent->val = val;
      return;
    }
  }
}

void hashmap_put(HashMap *map, const char *key, void *val) {
  hashmap_put2(map, key, strlen(key), val);
}

void hashmap_delete(HashMap *map, const char *key) {
  HashEntry *ent = get_entry(map, key, strlen(key));
  if (ent)
    ent->key = TOMBSTONE;
}

static bool is_regular_file(const char *path) {
  struct stat st;
  return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static bool startswith(const char *p, const char *q) { return strncmp(p, q, strlen(q)) == 0; }

/* the texts of tokens, allocated in large blocks */
static char *text_alloc(const char *s, size_t n) {
  static char *block;
  static size_t left;
  if (n + 1 > left) {
    size_t size = n + 1 > 65536 ? n + 1 : 65536;
    block = xmalloc(size);
    left = size;
  }
  char *p = block;
  memcpy(p, s, n);
  p[n] = '\0';
  block += n + 1;
  left -= n + 1;
  return p;
}

/* ================================================================ UTF-8 */

int encode_utf8(char *buf, uint32_t c) {
  if (c <= 0x7F) {
    buf[0] = c;
    return 1;
  }
  if (c <= 0x7FF) {
    buf[0] = 0xC0 | (c >> 6);
    buf[1] = 0x80 | (c & 0x3F);
    return 2;
  }
  if (c <= 0xFFFF) {
    buf[0] = 0xE0 | (c >> 12);
    buf[1] = 0x80 | ((c >> 6) & 0x3F);
    buf[2] = 0x80 | (c & 0x3F);
    return 3;
  }
  buf[0] = 0xF0 | (c >> 18);
  buf[1] = 0x80 | ((c >> 12) & 0x3F);
  buf[2] = 0x80 | ((c >> 6) & 0x3F);
  buf[3] = 0x80 | (c & 0x3F);
  return 4;
}

/* one UTF-8 sequence; an invalid byte stands for itself */
uint32_t decode_utf8(char **new_pos, char *p) {
  unsigned char *s = (unsigned char *)p;
  if (s[0] < 128) {
    *new_pos = p + 1;
    return s[0];
  }
  int len;
  uint32_t c;
  if (s[0] >= 0xF0 && s[0] < 0xF8) {
    len = 4;
    c = s[0] & 0x07;
  } else if (s[0] >= 0xE0) {
    len = 3;
    c = s[0] & 0x0F;
  } else if (s[0] >= 0xC0) {
    len = 2;
    c = s[0] & 0x1F;
  } else {
    *new_pos = p + 1;
    return s[0];
  }
  for (int i = 1; i < len; i++) {
    if ((s[i] >> 6) != 0x02) {
      *new_pos = p + 1;
      return s[0];
    }
    c = (c << 6) | (s[i] & 0x3F);
  }
  *new_pos = p + len;
  return c;
}

static bool in_range(const uint32_t *range, uint32_t c) {
  for (int i = 0; range[i] != (uint32_t)-1; i += 2)
    if (range[i] <= c && c <= range[i + 1])
      return true;
  return false;
}

/* C17 annex D, and $ (implementation-defined) */
bool is_ident1(uint32_t c) {
  static const uint32_t range[] = {
      '_',     '_',     'a',     'z',     'A',     'Z',     '$',     '$',
      0x00A8,  0x00A8,  0x00AA,  0x00AA,  0x00AD,  0x00AD,  0x00AF,  0x00AF,
      0x00B2,  0x00B5,  0x00B7,  0x00BA,  0x00BC,  0x00BE,  0x00C0,  0x00D6,
      0x00D8,  0x00F6,  0x00F8,  0x00FF,  0x0100,  0x02FF,  0x0370,  0x167F,
      0x1681,  0x180D,  0x180F,  0x1DBF,  0x1E00,  0x1FFF,  0x200B,  0x200D,
      0x202A,  0x202E,  0x203F,  0x2040,  0x2054,  0x2054,  0x2060,  0x206F,
      0x2070,  0x20CF,  0x2100,  0x218F,  0x2460,  0x24FF,  0x2776,  0x2793,
      0x2C00,  0x2DFF,  0x2E80,  0x2FFF,  0x3004,  0x3007,  0x3021,  0x302F,
      0x3031,  0x303F,  0x3040,  0xD7FF,  0xF900,  0xFD3D,  0xFD40,  0xFDCF,
      0xFDF0,  0xFE1F,  0xFE30,  0xFE44,  0xFE47,  0xFFFD,  0x10000, 0x1FFFD,
      0x20000, 0x2FFFD, 0x30000, 0x3FFFD, 0x40000, 0x4FFFD, 0x50000, 0x5FFFD,
      0x60000, 0x6FFFD, 0x70000, 0x7FFFD, 0x80000, 0x8FFFD, 0x90000, 0x9FFFD,
      0xA0000, 0xAFFFD, 0xB0000, 0xBFFFD, 0xC0000, 0xCFFFD, 0xD0000, 0xDFFFD,
      0xE0000, 0xEFFFD, (uint32_t)-1,
  };
  return in_range(range, c);
}

bool is_ident2(uint32_t c) {
  static const uint32_t range[] = {
      '0', '9', 0x0300, 0x036F, 0x1DC0, 0x1DFF, 0x20D0, 0x20FF, 0xFE20, 0xFE2F, (uint32_t)-1,
  };
  return is_ident1(c) || in_range(range, c);
}

int display_width(const char *p, int len) {
  char *s = (char *)p;
  int w = 0;
  while (s - p < len) {
    uint32_t c = decode_utf8(&s, s);
    if (c == '\t')
      w = (w + 8) & ~7;
    else
      w++;
  }
  return w;
}

/* ================================================== files, positions, diagnostics */

static int nfiles;

static struct srcfile *new_srcfile(const char *path, const char *display, char *contents,
                                   int dirindex) {
  struct srcfile *sf = xcalloc(1, sizeof *sf);
  sf->path = xstrdup(path);
  sf->display = xstrdup(display);
  sf->contents = contents;
  sf->dirindex = dirindex;
  sf->number = ++nfiles;
  return sf;
}

static void add_linechange(struct srcfile *sf, int from, int delta, const char *name) {
  if (sf->nlc == sf->caplc) {
    sf->caplc = sf->caplc ? 2 * sf->caplc : 8;
    sf->lc = xrealloc(sf->lc, sf->caplc * sizeof *sf->lc);
  }
  sf->lc[sf->nlc++] = (struct linechange){from, delta, name};
}

/* where physical line `line` of sf is, after #line */
static void where(const struct srcfile *sf, int line, const char **file, int *oline) {
  if (!sf) {
    *file = "<built-in>";
    *oline = line;
    return;
  }
  *file = sf->display;
  *oline = line;
  int lo = 0, hi = sf->nlc;
  while (lo < hi) {  /* the last change with from <= line */
    int mid = (lo + hi) / 2;
    if (sf->lc[mid].from <= line)
      lo = mid + 1;
    else
      hi = mid;
  }
  if (lo > 0) {
    *file = sf->lc[lo - 1].name;
    *oline = line + sf->lc[lo - 1].delta;
  }
}

void cpp_where(const struct token *t, const char **file, int *line) {
  while (t->origin)
    t = t->origin;
  where(t->sf, t->line, file, line);
}

static void set_where(struct token *t) { cpp_where(t, &t->file, &t->oline); }

bool cpp_source_line(const struct token *t, const char **line, int *len) {
  if (!t->sf || !t->loc || !t->sf->contents)
    return false;
  char *input = t->sf->contents;
  if (t->loc < input || t->loc > input + strlen(input))
    return false;
  char *s = t->loc, *e = t->loc;
  while (s > input && s[-1] != '\n')
    s--;
  while (*e && *e != '\n')
    e++;
  *line = s;
  *len = e - s;
  return true;
}

void cpp_warn(const struct token *t, const char *fmt, ...) {
  const char *f = "?";
  int l = 0;
  if (t)
    cpp_where(t, &f, &l);
  fprintf(stderr, "%s: %s:%d: ", progname, f, l);
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
}

bool tok_is(const struct token *t, const char *s) {
  return t->kind != CPP_EOF && t->kind != CPP_STR && t->kind != CPP_CHAR && !strcmp(t->text, s);
}

/* the preprocessor's diagnostics: through the hooks, else (errors only) as
   warnings, after which an error goes back to the innermost place that
   can carry on */
static struct cpp_hooks *diag_hooks;
static jmp_buf *recover;

static void report(bool error, const struct token *t, const char *fmt, va_list ap) {
  char *msg = vformat(fmt, ap);
  if (diag_hooks && diag_hooks->diag)
    diag_hooks->diag(diag_hooks->arg, error, t, msg);
  else if (error)
    cpp_warn(t, "%s", msg);
  free(msg);
}

static NORETURN void error_tok(const struct token *t, const char *fmt, ...) PRINTF(2, 3);
static void error_tok(const struct token *t, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  report(true, t, fmt, ap);
  va_end(ap);
  if (recover)
    longjmp(*recover, 1);
  exit(1);
}

/* an error after which the preprocessor goes on */
static void error_soft(const struct token *t, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  report(true, t, fmt, ap);
  va_end(ap);
}

static void warn_tok(const struct token *t, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  report(false, t, fmt, ap);
  va_end(ap);
}

/* ================================================================ the tokenizer */

static bool eq(const struct token *t, const char *s) {
  return t->kind != CPP_STR && t->kind != CPP_EOF && !strcmp(t->text, s);
}

static int from_hex(char c) {
  if ('0' <= c && c <= '9')
    return c - '0';
  if ('a' <= c && c <= 'f')
    return c - 'a' + 10;
  return c - 'A' + 10;
}

/* \uXXXX or \UXXXXXXXX at p */
static uint32_t read_ucn(char **rest, char *p, bool *ok) {
  int n = p[1] == 'u' ? 4 : 8;
  uint32_t c = 0;
  for (int i = 0; i < n; i++) {
    if (!isxdigit((unsigned char)p[2 + i])) {
      *ok = false;
      *rest = p + 2;
      return 0;
    }
    c = (c << 4) | from_hex(p[2 + i]);
  }
  *ok = true;
  *rest = p + 2 + n;
  return c;
}

/* the length of an identifier character (a UCN, UTF-8) at p, or 0 */
static int ident_char_len(char *p, bool first) {
  char *q;
  uint32_t c;
  if (*p == '\\' && (p[1] == 'u' || p[1] == 'U')) {
    bool ok;
    c = read_ucn(&q, p, &ok);
    if (!ok)
      return 0;
  } else {
    c = decode_utf8(&q, p);
  }
  if (first ? !is_ident1(c) : !is_ident2(c))
    return 0;
  return q - p;
}

static int read_ident(char *start) {
  char *p = start;
  int n = ident_char_len(p, true);
  if (!n)
    return 0;
  for (p += n; (n = ident_char_len(p, false)) > 0; p += n)
    ;
  return p - start;
}

static const char *punct_list[] = {
    "%:%:", "<<=", ">>=", "...", "==", "!=", "<=", ">=", "->", "+=", "-=",
    "*=",   "/=",  "++",  "--",  "%=", "&=", "|=", "^=", "&&", "||", "<<",
    ">>",   "##",  "<:",  ":>",  "<%", "%>", "%:",
};

/* the canonical spelling of a punctuator ("[" for "<:") */
static char *canonical_punct(const char *s, int len) {
  static const char *digraph_from[] = {"<:", ":>", "<%", "%>", "%:", "%:%:"};
  static char *digraph_to[] = {"[", "]", "{", "}", "#", "##"};
  for (int i = 0; i < 6; i++)
    if ((int)strlen(digraph_from[i]) == len && !memcmp(s, digraph_from[i], len))
      return digraph_to[i];
  for (size_t i = 0; i < sizeof punct_list / sizeof *punct_list; i++)
    if ((int)strlen(punct_list[i]) == len && !memcmp(s, punct_list[i], len))
      return (char *)punct_list[i];
  static char single[256][2];
  unsigned char c = *s;
  single[c][0] = c;
  return single[c];
}

static int read_punct(const char *p) {
  for (size_t i = 0; i < sizeof punct_list / sizeof *punct_list; i++)
    if (startswith(p, punct_list[i]))
      return strlen(punct_list[i]);
  return *p && strchr("[](){}.&*+-~!/%<>^|?:;=,#", *p) ? 1 : 0;
}

/* the closing quote of a literal, NULL if unterminated */
static char *literal_end(char *p, char quote) {
  for (; *p != quote; p++) {
    if (*p == '\n' || *p == '\0')
      return NULL;
    if (*p == '\\') {
      p++;
      if (*p == '\n' || *p == '\0')
        return NULL;
    }
  }
  return p;
}

static struct token *new_token(int kind, char *start, char *end, struct srcfile *sf, int line,
                               char *line_start) {
  struct token *t = xcalloc(1, sizeof *t);
  t->kind = kind;
  t->loc = start;
  t->len = end - start;
  t->text = kind == CPP_PUNCT ? canonical_punct(start, end - start) : text_alloc(start, end - start);
  t->sf = sf;
  t->line = line;
  t->col = display_width(line_start, start - line_start) + 1;
  return t;
}

/* the tokens of a buffer (after phases 1 and 2), ending with CPP_EOF */
static struct token *tokenize_buf(char *buf, struct srcfile *sf) {
  struct token head = {0}, *cur = &head;
  char *p = buf, *line_start = buf;
  int line = 1;
  bool bol = true, space = false;
#define ADD(kind, s, e)                                       \
  do {                                                        \
    cur = cur->next = new_token(kind, s, e, sf, line, line_start); \
    cur->bol = bol;                                           \
    cur->space = space;                                       \
    bol = space = false;                                      \
  } while (0)

  while (*p) {
    if (p[0] == '/' && p[1] == '/') {
      while (*p && *p != '\n')
        p++;
      space = true;
      continue;
    }
    if (p[0] == '/' && p[1] == '*') {
      char *q = p + 2;
      while (*q && !(q[0] == '*' && q[1] == '/')) {
        if (*q == '\n') {
          line++;
          line_start = q + 1;
        }
        q++;
      }
      if (!*q) {
        struct token tmp = {0};
        tmp.sf = sf;
        tmp.line = line;
        tmp.loc = p;
        error_soft(&tmp, "unterminated comment");
        p = q;
        break;
      }
      p = q + 2;
      space = true;
      continue;
    }
    if (*p == '\n') {
      p++;
      line++;
      line_start = p;
      bol = true;
      space = false;
      continue;
    }
    if (*p == ' ' || *p == '\t' || *p == '\v' || *p == '\f' || *p == '\r') {
      p++;
      space = true;
      continue;
    }
    /* a preprocessing number */
    if (isdigit((unsigned char)*p) || (*p == '.' && isdigit((unsigned char)p[1]))) {
      char *q = p++;
      for (;;) {
        if (p[0] && p[1] && strchr("eEpP", p[0]) && strchr("+-", p[1])) {
          p += 2;
        } else if (isalnum((unsigned char)*p) || *p == '.' || *p == '_') {
          p++;
        } else {
          int n = ident_char_len(p, false);
          if (n > 0 && ((unsigned char)*p >= 0x80 || *p == '\\'))
            p += n;
          else
            break;
        }
      }
      ADD(CPP_NUM, q, p);
      continue;
    }
    /* string and character literals, with an encoding prefix */
    int plen = -1;
    if (startswith(p, "u8\""))
      plen = 2;
    else if ((*p == 'u' || *p == 'U' || *p == 'L') && (p[1] == '"' || p[1] == '\''))
      plen = 1;
    else if (*p == '"' || *p == '\'')
      plen = 0;
    if (plen >= 0) {
      char quote = p[plen];
      char *end = literal_end(p + plen + 1, quote);
      if (end) {
        ADD(quote == '"' ? CPP_STR : CPP_CHAR, p, end + 1);
        p = end + 1;
        continue;
      }
      if (plen == 0) {  /* a lone ' or ": an "other" token */
        ADD(CPP_OTHER, p, p + 1);
        p++;
        continue;
      }
    }
    int n = read_ident(p);
    if (n) {
      ADD(CPP_IDENT, p, p + n);
      p += n;
      continue;
    }
    n = read_punct(p);
    if (n) {
      ADD(CPP_PUNCT, p, p + n);
      p += n;
      continue;
    }
    char *q;
    decode_utf8(&q, p);
    ADD(CPP_OTHER, p, q);
    p = q;
  }
  ADD(CPP_EOF, p, p);
  cur->bol = true;
#undef ADD
  return head.next;
}

/* a file's contents ("-": the standard input), ending with a newline;
   NULL (errno set) if it cannot be read */
char *read_file(const char *path) {
  FILE *fp = strcmp(path, "-") == 0 ? stdin : fopen(path, "r");
  if (!fp)
    return NULL;
  Buf b = {0};
  char tmp[65536];
  size_t n;
  while ((n = fread(tmp, 1, sizeof tmp, fp)) > 0)
    buf_write(&b, tmp, n);
  int err = ferror(fp) ? errno : 0;
  if (fp != stdin)
    fclose(fp);
  if (err) {
    free(b.buf);
    errno = err;
    return NULL;
  }
  if (b.len == 0 || b.buf[b.len - 1] != '\n')
    buf_putc(&b, '\n');
  return b.buf;
}

/* phase 1: CR LF and CR to LF, trigraphs; phase 2: splices, the newlines
   put back after the logical line so that lines keep their numbers */
static char *translate12(char *p) {
  if (!memcmp(p, "\xef\xbb\xbf", 3))  /* a UTF-8 byte-order mark */
    p += 3;
  char *q = p, *s = p;
  while (*s) {
    if (s[0] == '\r') {
      *q++ = '\n';
      s += s[1] == '\n' ? 2 : 1;
      continue;
    }
    static const char tri[] = "=(/)'<!>-", rep[] = "#[\\]^{|}~";
    const char *k;
    if (s[0] == '?' && s[1] == '?' && s[2] && (k = strchr(tri, s[2]))) {
      *q++ = rep[k - tri];
      s += 3;
      continue;
    }
    *q++ = *s++;
  }
  *q = '\0';
  int i = 0, j = 0, n = 0;
  while (p[i]) {
    if (p[i] == '\\' && p[i + 1] == '\n') {
      i += 2;
      n++;
    } else if (p[i] == '\n') {
      p[j++] = p[i++];
      for (; n > 0; n--)
        p[j++] = '\n';
    } else {
      p[j++] = p[i++];
    }
  }
  for (; n > 0; n--)
    p[j++] = '\n';
  p[j] = '\0';
  return p;
}

static struct token *tokenize_file(const char *path, const char *display, int dirindex,
                                   const char *builtin) {
  char *buf = builtin ? xstrdup(builtin) : read_file(path);
  if (!buf)
    return NULL;
  struct srcfile *sf = new_srcfile(path, display, translate12(buf), dirindex);
  return tokenize_buf(sf->contents, sf);
}

struct token *cpp_tokenize(const char *path, const char *display) {
  struct token *t = tokenize_file(path, display, -1, NULL);
  for (struct token *u = t; u; u = u->next)
    set_where(u);
  return t;
}

/* tokens made by the preprocessor from text, at the place of tmpl */
static struct token *tokenize_text(const char *text, const struct token *tmpl) {
  struct token *t = tokenize_buf(format("%s\n", text), NULL);
  for (struct token *u = t; u; u = u->next) {
    u->sf = tmpl->sf;
    u->line = tmpl->line;
    u->col = tmpl->col;
    u->origin = tmpl->origin;
    u->bol = u->kind == CPP_EOF;
  }
  return t;
}

/* ================================================================ built-in headers */

static const struct {
  const char *name, *text;
} builtin_headers[] = {
    {"complex.h",
     "/* <complex.h> for c17: the C library's header, plus the C11 CMPLX macros,\n"
     "   where it does not define them (glibc defines them only for GCC). */\n"
     "#include_next <complex.h>\n"
     "\n"
     "#ifndef CMPLX\n"
     "#define CMPLX(x, y) __builtin_complex((double)(x), (double)(y))\n"
     "#endif\n"
     "#ifndef CMPLXF\n"
     "#define CMPLXF(x, y) __builtin_complex((float)(x), (float)(y))\n"
     "#endif\n"
     "#ifndef CMPLXL\n"
     "#define CMPLXL(x, y) __builtin_complex((long double)(x), (long double)(y))\n"
     "#endif\n"},
    {"float.h",
     "/* <float.h> - characteristics of floating types (C17 7.7, 5.2.4.2.2)\n"
     "   float/double: IEC 60559 binary32/binary64; long double: x87 80-bit. */\n"
     "#ifndef __c17_float_h\n"
     "#define __c17_float_h\n"
     "\n"
     "#define FLT_ROUNDS 1\n"
     "#define FLT_EVAL_METHOD 0\n"
     "#define DECIMAL_DIG 21\n"
     "#define FLT_RADIX 2\n"
     "\n"
     "#define FLT_HAS_SUBNORM 1\n"
     "#define DBL_HAS_SUBNORM 1\n"
     "#define LDBL_HAS_SUBNORM 1\n"
     "\n"
     "#define FLT_MANT_DIG 24\n"
     "#define DBL_MANT_DIG 53\n"
     "#define LDBL_MANT_DIG 64\n"
     "\n"
     "#define FLT_DECIMAL_DIG 9\n"
     "#define DBL_DECIMAL_DIG 17\n"
     "#define LDBL_DECIMAL_DIG 21\n"
     "\n"
     "#define FLT_DIG 6\n"
     "#define DBL_DIG 15\n"
     "#define LDBL_DIG 18\n"
     "\n"
     "#define FLT_MIN_EXP (-125)\n"
     "#define DBL_MIN_EXP (-1021)\n"
     "#define LDBL_MIN_EXP (-16381)\n"
     "\n"
     "#define FLT_MIN_10_EXP (-37)\n"
     "#define DBL_MIN_10_EXP (-307)\n"
     "#define LDBL_MIN_10_EXP (-4931)\n"
     "\n"
     "#define FLT_MAX_EXP 128\n"
     "#define DBL_MAX_EXP 1024\n"
     "#define LDBL_MAX_EXP 16384\n"
     "\n"
     "#define FLT_MAX_10_EXP 38\n"
     "#define DBL_MAX_10_EXP 308\n"
     "#define LDBL_MAX_10_EXP 4932\n"
     "\n"
     "#define FLT_MAX 0x1.fffffep+127F\n"
     "#define DBL_MAX 0x1.fffffffffffffp+1023\n"
     "#define LDBL_MAX 0xf.fffffffffffffffp+16380L\n"
     "\n"
     "#define FLT_EPSILON 0x1p-23F\n"
     "#define DBL_EPSILON 0x1p-52\n"
     "#define LDBL_EPSILON 0x1p-63L\n"
     "\n"
     "#define FLT_MIN 0x1p-126F\n"
     "#define DBL_MIN 0x1p-1022\n"
     "#define LDBL_MIN 0x8p-16385L\n"
     "\n"
     "#define FLT_TRUE_MIN 0x1p-149F\n"
     "#define DBL_TRUE_MIN 0x0.0000000000001p-1022\n"
     "#define LDBL_TRUE_MIN 0x0.000000000000001p-16385L\n"
     "\n"
     "#endif\n"},
    {"iso646.h",
     "/* <iso646.h> - alternative spellings (C17 7.9) */\n"
     "#ifndef __c17_iso646_h\n"
     "#define __c17_iso646_h\n"
     "#define and &&\n"
     "#define and_eq &=\n"
     "#define bitand &\n"
     "#define bitor |\n"
     "#define compl ~\n"
     "#define not !\n"
     "#define not_eq !=\n"
     "#define or ||\n"
     "#define or_eq |=\n"
     "#define xor ^\n"
     "#define xor_eq ^=\n"
     "#endif\n"},
    {"stdalign.h",
     "/* <stdalign.h> - alignment (C17 7.15) */\n"
     "#ifndef __c17_stdalign_h\n"
     "#define __c17_stdalign_h\n"
     "#define alignas _Alignas\n"
     "#define alignof _Alignof\n"
     "#define __alignas_is_defined 1\n"
     "#define __alignof_is_defined 1\n"
     "#endif\n"},
    {"stdarg.h",
     "/* <stdarg.h> - variable arguments (C17 7.16) for c17 */\n"
     "#ifndef __c17_gnuc_va_list\n"
     "#define __c17_gnuc_va_list\n"
     "typedef __builtin_va_list __gnuc_va_list;\n"
     "#endif\n"
     "\n"
     "#ifndef __need___va_list\n"
     "#ifndef __c17_stdarg_h\n"
     "#define __c17_stdarg_h\n"
     "#ifndef _VA_LIST_DEFINED\n"
     "#define _VA_LIST_DEFINED\n"
     "typedef __gnuc_va_list va_list;\n"
     "#endif\n"
     "#define _VA_LIST\n"
     "#define _VA_LIST_T_H\n"
     "#define __va_list__\n"
     "#define va_start(ap, parmN) __builtin_va_start(ap, parmN)\n"
     "#define va_arg(ap, type) __builtin_va_arg(ap, type)\n"
     "#define va_end(ap) __builtin_va_end(ap)\n"
     "#define va_copy(dest, src) __builtin_va_copy(dest, src)\n"
     "#define __va_copy(dest, src) __builtin_va_copy(dest, src)\n"
     "#endif\n"
     "#endif\n"
     "#undef __need___va_list\n"},
    {"stdatomic.h",
     "/* <stdatomic.h> - atomics (C17 7.17) for c17.\n"
     "   All operations are performed with sequentially consistent ordering,\n"
     "   which is permitted for every memory_order argument.  Atomic objects\n"
     "   of size 1, 2, 4 and 8 bytes are supported (always lock-free). */\n"
     "#ifndef __c17_stdatomic_h\n"
     "#define __c17_stdatomic_h\n"
     "\n"
     "/* no <stddef.h> or <stdint.h>, whose macros are not to be seen here:\n"
     "   the types of x86-64 (LP64); int_fast16_t and int_fast32_t are those\n"
     "   of the C library (long for V86BSD's and glibc, int for musl: its\n"
     "   profile defines __c17_int_fast16) */\n"
     "#ifndef __c17_int_fast16\n"
     "#define __c17_int_fast16 long\n"
     "#endif\n"
     "\n"
     "typedef enum memory_order {\n"
     "  memory_order_relaxed,\n"
     "  memory_order_consume,\n"
     "  memory_order_acquire,\n"
     "  memory_order_release,\n"
     "  memory_order_acq_rel,\n"
     "  memory_order_seq_cst\n"
     "} memory_order;\n"
     "\n"
     "#define ATOMIC_BOOL_LOCK_FREE 2\n"
     "#define ATOMIC_CHAR_LOCK_FREE 2\n"
     "#define ATOMIC_CHAR16_T_LOCK_FREE 2\n"
     "#define ATOMIC_CHAR32_T_LOCK_FREE 2\n"
     "#define ATOMIC_WCHAR_T_LOCK_FREE 2\n"
     "#define ATOMIC_SHORT_LOCK_FREE 2\n"
     "#define ATOMIC_INT_LOCK_FREE 2\n"
     "#define ATOMIC_LONG_LOCK_FREE 2\n"
     "#define ATOMIC_LLONG_LOCK_FREE 2\n"
     "#define ATOMIC_POINTER_LOCK_FREE 2\n"
     "\n"
     "#define ATOMIC_VAR_INIT(value) (value)\n"
     "#define atomic_init(obj, value) ((void)(*(obj) = (value)))\n"
     "#define kill_dependency(y) (y)\n"
     "\n"
     "/* the non-generic functions are also functions (C17 7.1.4), in\n"
     "   libc17rt.a */\n"
     "void atomic_thread_fence(memory_order);\n"
     "void atomic_signal_fence(memory_order);\n"
     "#define atomic_thread_fence(order) __c17_atomic_fence(order)\n"
     "#define atomic_signal_fence(order) __c17_atomic_signal_fence(order)\n"
     "\n"
     "#define atomic_is_lock_free(obj) \\\n"
     "  ((void)(obj), sizeof(*(obj)) == 1 || sizeof(*(obj)) == 2 || sizeof(*(obj)) == 4 || \\\n"
     "                    sizeof(*(obj)) == 8)\n"
     "\n"
     "typedef _Atomic _Bool atomic_bool;\n"
     "typedef _Atomic char atomic_char;\n"
     "typedef _Atomic signed char atomic_schar;\n"
     "typedef _Atomic unsigned char atomic_uchar;\n"
     "typedef _Atomic short atomic_short;\n"
     "typedef _Atomic unsigned short atomic_ushort;\n"
     "typedef _Atomic int atomic_int;\n"
     "typedef _Atomic unsigned int atomic_uint;\n"
     "typedef _Atomic long atomic_long;\n"
     "typedef _Atomic unsigned long atomic_ulong;\n"
     "typedef _Atomic long long atomic_llong;\n"
     "typedef _Atomic unsigned long long atomic_ullong;\n"
     "typedef _Atomic unsigned short atomic_char16_t;\n"
     "typedef _Atomic unsigned int atomic_char32_t;\n"
     "typedef _Atomic int atomic_wchar_t;\n"
     "typedef _Atomic signed char atomic_int_least8_t;\n"
     "typedef _Atomic unsigned char atomic_uint_least8_t;\n"
     "typedef _Atomic short atomic_int_least16_t;\n"
     "typedef _Atomic unsigned short atomic_uint_least16_t;\n"
     "typedef _Atomic int atomic_int_least32_t;\n"
     "typedef _Atomic unsigned int atomic_uint_least32_t;\n"
     "typedef _Atomic long atomic_int_least64_t;\n"
     "typedef _Atomic unsigned long atomic_uint_least64_t;\n"
     "typedef _Atomic signed char atomic_int_fast8_t;\n"
     "typedef _Atomic unsigned char atomic_uint_fast8_t;\n"
     "typedef _Atomic __c17_int_fast16 atomic_int_fast16_t;\n"
     "typedef _Atomic unsigned __c17_int_fast16 atomic_uint_fast16_t;\n"
     "typedef _Atomic __c17_int_fast16 atomic_int_fast32_t;\n"
     "typedef _Atomic unsigned __c17_int_fast16 atomic_uint_fast32_t;\n"
     "typedef _Atomic long atomic_int_fast64_t;\n"
     "typedef _Atomic unsigned long atomic_uint_fast64_t;\n"
     "typedef _Atomic long atomic_intptr_t;\n"
     "typedef _Atomic unsigned long atomic_uintptr_t;\n"
     "typedef _Atomic unsigned long atomic_size_t;\n"
     "typedef _Atomic long atomic_ptrdiff_t;\n"
     "typedef _Atomic long atomic_intmax_t;\n"
     "typedef _Atomic unsigned long atomic_uintmax_t;\n"
     "\n"
     "#define atomic_store(obj, desired) __c17_atomic_store(obj, desired)\n"
     "#define atomic_store_explicit(obj, desired, order) \\\n"
     "  ((void)(order), __c17_atomic_store(obj, desired))\n"
     "#define atomic_load(obj) __c17_atomic_load(obj)\n"
     "#define atomic_load_explicit(obj, order) ((void)(order), __c17_atomic_load(obj))\n"
     "#define atomic_exchange(obj, desired) __c17_atomic_exchange(obj, desired)\n"
     "#define atomic_exchange_explicit(obj, desired, order) \\\n"
     "  ((void)(order), __c17_atomic_exchange(obj, desired))\n"
     "#define atomic_compare_exchange_strong(obj, expected, desired) \\\n"
     "  __c17_atomic_compare_exchange(obj, expected, desired)\n"
     "#define atomic_compare_exchange_strong_explicit(obj, expected, desired, succ, fail) \\\n"
     "  ((void)(succ), (void)(fail), __c17_atomic_compare_exchange(obj, expected, desired))\n"
     "#define atomic_compare_exchange_weak(obj, expected, desired) \\\n"
     "  __c17_atomic_compare_exchange(obj, expected, desired)\n"
     "#define atomic_compare_exchange_weak_explicit(obj, expected, desired, succ, fail) \\\n"
     "  ((void)(succ), (void)(fail), __c17_atomic_compare_exchange(obj, expected, desired))\n"
     "#define atomic_fetch_add(obj, arg) __c17_atomic_fetch_add(obj, arg)\n"
     "#define atomic_fetch_add_explicit(obj, arg, order) ((void)(order), __c17_atomic_fetch_add(obj, arg))\n"
     "#define atomic_fetch_sub(obj, arg) __c17_atomic_fetch_sub(obj, arg)\n"
     "#define atomic_fetch_sub_explicit(obj, arg, order) ((void)(order), __c17_atomic_fetch_sub(obj, arg))\n"
     "#define atomic_fetch_or(obj, arg) __c17_atomic_fetch_or(obj, arg)\n"
     "#define atomic_fetch_or_explicit(obj, arg, order) ((void)(order), __c17_atomic_fetch_or(obj, arg))\n"
     "#define atomic_fetch_xor(obj, arg) __c17_atomic_fetch_xor(obj, arg)\n"
     "#define atomic_fetch_xor_explicit(obj, arg, order) ((void)(order), __c17_atomic_fetch_xor(obj, arg))\n"
     "#define atomic_fetch_and(obj, arg) __c17_atomic_fetch_and(obj, arg)\n"
     "#define atomic_fetch_and_explicit(obj, arg, order) ((void)(order), __c17_atomic_fetch_and(obj, arg))\n"
     "\n"
     "typedef struct atomic_flag {\n"
     "  atomic_bool __value;\n"
     "} atomic_flag;\n"
     "\n"
     "#define ATOMIC_FLAG_INIT { 0 }\n"
     "_Bool atomic_flag_test_and_set(volatile atomic_flag *);\n"
     "_Bool atomic_flag_test_and_set_explicit(volatile atomic_flag *, memory_order);\n"
     "void atomic_flag_clear(volatile atomic_flag *);\n"
     "void atomic_flag_clear_explicit(volatile atomic_flag *, memory_order);\n"
     "#define atomic_flag_test_and_set(obj) __c17_atomic_exchange(&(obj)->__value, 1)\n"
     "#define atomic_flag_test_and_set_explicit(obj, order) \\\n"
     "  ((void)(order), __c17_atomic_exchange(&(obj)->__value, 1))\n"
     "#define atomic_flag_clear(obj) __c17_atomic_store(&(obj)->__value, 0)\n"
     "#define atomic_flag_clear_explicit(obj, order) \\\n"
     "  ((void)(order), __c17_atomic_store(&(obj)->__value, 0))\n"
     "\n"
     "#endif\n"},
    {"stdbool.h",
     "/* <stdbool.h> - boolean type and values (C17 7.18) */\n"
     "#ifndef __c17_stdbool_h\n"
     "#define __c17_stdbool_h\n"
     "#define bool _Bool\n"
     "#define true 1\n"
     "#define false 0\n"
     "#define __bool_true_false_are_defined 1\n"
     "#endif\n"},
    {"stddef.h",
     "/* <stddef.h> - common definitions (C17 7.19) for c17 */\n"
     "#if !defined(__need_size_t) && !defined(__need_ptrdiff_t) && !defined(__need_wchar_t) && \\\n"
     "    !defined(__need_NULL) && !defined(__need_wint_t)\n"
     "#define __c17_stddef_all\n"
     "#endif\n"
     "\n"
     "#if defined(__c17_stddef_all) || defined(__need_size_t)\n"
     "#ifndef __c17_size_t\n"
     "#define __c17_size_t\n"
     "typedef unsigned long size_t;\n"
     "#endif\n"
     "#undef __need_size_t\n"
     "#endif\n"
     "\n"
     "#if defined(__c17_stddef_all) || defined(__need_ptrdiff_t)\n"
     "#ifndef __c17_ptrdiff_t\n"
     "#define __c17_ptrdiff_t\n"
     "typedef long ptrdiff_t;\n"
     "#endif\n"
     "#undef __need_ptrdiff_t\n"
     "#endif\n"
     "\n"
     "#if defined(__c17_stddef_all) || defined(__need_wchar_t)\n"
     "#ifndef __c17_wchar_t\n"
     "#define __c17_wchar_t\n"
     "typedef int wchar_t;\n"
     "#endif\n"
     "#undef __need_wchar_t\n"
     "#endif\n"
     "\n"
     "#if defined(__need_wint_t)\n"
     "#ifndef __c17_wint_t\n"
     "#define __c17_wint_t\n"
     "typedef unsigned int wint_t;\n"
     "#endif\n"
     "#undef __need_wint_t\n"
     "#endif\n"
     "\n"
     "#if defined(__c17_stddef_all) || defined(__need_NULL)\n"
     "#undef NULL\n"
     "#define NULL ((void *)0)\n"
     "#undef __need_NULL\n"
     "#endif\n"
     "\n"
     "#ifdef __c17_stddef_all\n"
     "#undef __c17_stddef_all\n"
     "#ifndef __c17_stddef_rest\n"
     "#define __c17_stddef_rest\n"
     "typedef struct {\n"
     "  long long __max_align_ll;\n"
     "  long double __max_align_ld;\n"
     "} max_align_t;\n"
     "#define offsetof(type, member) __builtin_offsetof(type, member)\n"
     "#endif\n"
     "#endif\n"},
    {"stdnoreturn.h",
     "/* <stdnoreturn.h> - _Noreturn (C17 7.23) */\n"
     "#ifndef __c17_stdnoreturn_h\n"
     "#define __c17_stdnoreturn_h\n"
     "#define noreturn _Noreturn\n"
     "#endif\n"},
};

/* ================================================================ macros */

struct hideset {
  struct hideset *next;
  char *name;
};

typedef struct MacroParam {
  struct MacroParam *next;
  char *name;
} MacroParam;

typedef struct MacroArg {
  struct MacroArg *next;
  char *name;
  bool is_va_args;
  bool omitted;            /* variable arguments entirely absent */
  struct token *tok;       /* unexpanded */
  struct token *expanded;
} MacroArg;

struct cpp;
typedef struct token *macro_handler_fn(struct cpp *, struct token *);

typedef struct Macro {
  char *name;
  bool is_objlike;
  MacroParam *params;
  char *va_args_name;
  struct token *body;
  macro_handler_fn *handler;
  bool is_predefined;      /* may not be #defined or #undefined */
} Macro;

typedef struct CondIncl {
  struct CondIncl *next;
  enum { IN_THEN, IN_ELIF, IN_ELSE } ctx;
  struct token *tok;
  bool included;
} CondIncl;

struct defop {
  bool undef;
  char *arg;
};

struct cpp {
  HashMap macros;
  CondIncl *cond;
  HashMap pragma_once;
  HashMap guards;          /* path -> guard macro */
  HashMap pushed;          /* #pragma push_macro stacks */
  int depth;               /* of #include */
  int counter;
  StrArray idirs, sdirs;   /* -I, system */
  struct defop *ops;
  int nops;
  struct cpp_hooks hooks;
  char *quotedir;
  bool preprocessed, keep_pragmas;
  char *base_file;
};

static struct token *preprocess2(struct cpp *p, struct token *tok);

static bool is_hash(const struct token *t) { return t->bol && eq(t, "#"); }

/* the tokens of a directive's line after it: warns of any */
static struct token *skip_line(struct token *t) {
  if (t->bol)
    return t;
  warn_tok(t, "extra tokens at end of directive");
  while (!t->bol)
    t = t->next;
  return t;
}

static struct token *copy_token(const struct token *t) {
  struct token *u = xmalloc(sizeof *u);
  *u = *t;
  u->next = NULL;
  return u;
}

static struct token *new_eof(const struct token *t) {
  struct token *u = copy_token(t);
  u->kind = CPP_EOF;
  u->text = "";
  u->len = 0;
  return u;
}

static struct hideset *new_hideset(char *name) {
  struct hideset *hs = xcalloc(1, sizeof *hs);
  hs->name = name;
  return hs;
}

static struct hideset *hideset_union(struct hideset *a, struct hideset *b) {
  struct hideset head = {0}, *cur = &head;
  for (; a; a = a->next)
    cur = cur->next = new_hideset(a->name);
  cur->next = b;
  return head.next;
}

static bool hideset_contains(struct hideset *hs, const char *s) {
  for (; hs; hs = hs->next)
    if (!strcmp(hs->name, s))
      return true;
  return false;
}

static struct hideset *hideset_intersection(struct hideset *a, struct hideset *b) {
  struct hideset head = {0}, *cur = &head;
  for (; a; a = a->next)
    if (hideset_contains(b, a->name))
      cur = cur->next = new_hideset(a->name);
  return head.next;
}

static struct token *add_hideset(struct token *t, struct hideset *hs) {
  struct token head = {0}, *cur = &head;
  for (; t; t = t->next) {
    struct token *u = copy_token(t);
    u->hs = hideset_union(u->hs, hs);
    cur = cur->next = u;
  }
  return head.next;
}

/* a (ending with EOF, copied) followed by b */
static struct token *append(struct token *a, struct token *b) {
  if (a->kind == CPP_EOF)
    return b;
  struct token head = {0}, *cur = &head;
  for (; a->kind != CPP_EOF; a = a->next)
    cur = cur->next = copy_token(a);
  cur->next = b;
  return head.next;
}

static struct token *skip_cond_incl2(struct token *t) {
  while (t->kind != CPP_EOF) {
    if (is_hash(t) && (eq(t->next, "if") || eq(t->next, "ifdef") || eq(t->next, "ifndef"))) {
      t = skip_cond_incl2(t->next->next);
      continue;
    }
    if (is_hash(t) && eq(t->next, "endif"))
      return t->next->next;
    t = t->next;
  }
  return t;
}

/* skips a group to the next #elif, #else or #endif of the same level */
static struct token *skip_cond_incl(struct token *t) {
  while (t->kind != CPP_EOF) {
    if (is_hash(t) && (eq(t->next, "if") || eq(t->next, "ifdef") || eq(t->next, "ifndef"))) {
      t = skip_cond_incl2(t->next->next);
      continue;
    }
    if (is_hash(t) && (eq(t->next, "elif") || eq(t->next, "else") || eq(t->next, "endif")))
      break;
    t = t->next;
  }
  return t;
}

static char *quote_string(const char *s) {
  Buf b = {0};
  buf_putc(&b, '"');
  for (; *s; s++) {
    if (*s == '\\' || *s == '"')
      buf_putc(&b, '\\');
    if (*s == '\n') {
      buf_puts(&b, "\\n");
      continue;
    }
    buf_putc(&b, *s);
  }
  buf_putc(&b, '"');
  return b.buf;
}

static struct token *new_str_token(const char *s, const struct token *tmpl) {
  return tokenize_text(quote_string(s), tmpl);
}

static struct token *new_num_token(int64_t v, const struct token *tmpl) {
  return tokenize_text(format("%" PRId64, v), tmpl);
}

/* the rest of a line, ending with EOF */
static struct token *copy_line(struct token **rest, struct token *t) {
  struct token head = {0}, *cur = &head;
  for (; !t->bol; t = t->next)
    cur = cur->next = copy_token(t);
  cur->next = new_eof(t);
  *rest = t;
  return head.next;
}

/* the spelling of tokens, with one space where there was white space */
static char *join_tokens(struct token *t, struct token *end) {
  Buf b = {0};
  for (struct token *u = t; u != end && u->kind != CPP_EOF; u = u->next) {
    if (u != t && (u->space || u->bol))
      buf_putc(&b, ' ');
    buf_write(&b, u->loc, u->len);
  }
  return b.buf ? b.buf : xstrdup("");
}

/* 6.10.3.2: # (in string literals and character constants, " and \ are
   escaped) */
static struct token *stringize(struct token *hash, struct token *arg) {
  Buf b = {0};
  buf_putc(&b, '"');
  for (struct token *t = arg; t->kind != CPP_EOF; t = t->next) {
    if (t != arg && (t->space || t->bol))
      buf_putc(&b, ' ');
    if (t->kind == CPP_STR || t->kind == CPP_CHAR) {
      for (int i = 0; i < t->len; i++) {
        if (t->loc[i] == '"' || t->loc[i] == '\\')
          buf_putc(&b, '\\');
        buf_putc(&b, t->loc[i]);
      }
    } else {
      buf_write(&b, t->loc, t->len);
    }
  }
  buf_putc(&b, '"');
  struct token *t = tokenize_text(b.buf, hash);
  if (t->kind != CPP_STR || t->next->kind != CPP_EOF)
    error_tok(hash, "'#' produced an invalid string literal");
  return t;
}

/* the value of a string literal or character constant's characters:
   simple, octal and hexadecimal escapes (for #line and #pragma names) */
static char *unescape(const struct token *t) {
  const char *s = t->loc;
  while (*s != '"' && *s != '\'')
    s++;
  Buf b = {0};
  for (s++; s < t->loc + t->len - 1; s++) {
    if (*s != '\\') {
      buf_putc(&b, *s);
      continue;
    }
    s++;
    int c = *s;
    switch (c) {
    case 'n': c = '\n'; break;
    case 't': c = '\t'; break;
    case 'r': c = '\r'; break;
    case 'a': c = '\a'; break;
    case 'b': c = '\b'; break;
    case 'f': c = '\f'; break;
    case 'v': c = '\v'; break;
    case 'x':
      c = 0;
      while (isxdigit((unsigned char)s[1]))
        c = c * 16 + from_hex(*++s);
      break;
    default:
      if (c >= '0' && c <= '7') {
        c -= '0';
        for (int k = 0; k < 2 && s[1] >= '0' && s[1] <= '7'; k++)
          c = c * 8 + (*++s - '0');
      }
    }
    buf_putc(&b, c);
  }
  return b.buf ? b.buf : xstrdup("");
}

static void use_hook(struct cpp *p, struct token *t) {
  if (p->hooks.use) {
    set_where(t);
    p->hooks.use(p->hooks.arg, t);
  }
}

/* ================================================================ #if */

typedef struct {
  int64_t v;
  bool u;  /* unsigned: uintmax_t */
} PPVal;

typedef struct {
  struct cpp *p;
  struct token *t;
  bool eval;  /* false in an operand that is not evaluated */
} Expr;

static PPVal pp_cond(Expr *e);

static void pp_expect(Expr *e, const char *op) {
  if (!eq(e->t, op))
    error_tok(e->t, "expected '%s' in preprocessor expression", op);
  e->t = e->t->next;
}

/* an integer constant (6.4.4.1): its value, and whether it is unsigned */
static bool pp_number(const struct token *t, PPVal *v) {
  const char *s = t->text;
  int base = 10;
  if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X') && isxdigit((unsigned char)s[2])) {
    base = 16;
    s += 2;
  } else if (s[0] == '0' && (s[1] == 'b' || s[1] == 'B') && (s[2] == '0' || s[2] == '1')) {
    base = 2;  /* an extension */
    s += 2;
  } else if (s[0] == '0') {
    base = 8;
  }
  uint64_t x = 0;
  bool overflow = false;
  for (; *s; s++) {
    int d = isdigit((unsigned char)*s) ? *s - '0'
            : isxdigit((unsigned char)*s) && base == 16 ? from_hex(*s) : 99;
    if (d >= base)
      break;
    if (x > (UINT64_MAX - d) / base)
      overflow = true;
    x = x * base + d;
  }
  /* the suffix: u, l, ll, in either order and case */
  bool u = false;
  int l = 0;
  const char *suf = s;
  while (*s) {
    if ((*s == 'u' || *s == 'U') && !u) {
      u = true;
      s++;
    } else if (!l && (startswith(s, "ll") || startswith(s, "LL"))) {
      l = 2;
      s += 2;
    } else if (!l && (*s == 'l' || *s == 'L')) {
      l = 1;
      s++;
    } else {
      return false;
    }
  }
  (void)suf;
  if (overflow)
    warn_tok(t, "integer constant is too large");
  v->v = (int64_t)x;
  /* a constant that does not fit intmax_t is unsigned (6.4.4.1p5 in #if) */
  v->u = u || (x > INT64_MAX && base != 10) || x > INT64_MAX;
  return true;
}

/* the value of a character constant (int for plain ones: char is signed;
   the code of the character for L, u and U) */
static int64_t pp_char(const struct token *t) {
  const char *s = t->loc;
  bool wide = *s != '\'';
  if (wide)
    s += s[1] == '\'' ? 1 : 2;
  s++;
  int64_t v = 0;
  int n = 0;
  while (*s != '\'') {
    uint32_t c;
    if (*s == '\\') {
      s++;
      switch (*s) {
      case 'n': c = '\n'; s++; break;
      case 't': c = '\t'; s++; break;
      case 'r': c = '\r'; s++; break;
      case 'a': c = '\a'; s++; break;
      case 'b': c = '\b'; s++; break;
      case 'f': c = '\f'; s++; break;
      case 'v': c = '\v'; s++; break;
      case 'x':
        c = 0;
        for (s++; isxdigit((unsigned char)*s); s++)
          c = c * 16 + from_hex(*s);
        break;
      case 'u':
      case 'U': {
        char *q;
        bool ok;
        c = read_ucn(&q, (char *)s - 1, &ok);
        s = q;
        break;
      }
      default:
        if (*s >= '0' && *s <= '7') {
          c = 0;
          for (int k = 0; k < 3 && *s >= '0' && *s <= '7'; k++)
            c = c * 8 + (*s++ - '0');
        } else {
          c = (unsigned char)*s++;
        }
      }
      if (!wide) {
        v = (v << 8) | (c & 0xff);
        n++;
        continue;
      }
    } else {
      char *q;
      c = decode_utf8(&q, (char *)s);
      if (!wide && q - s > 1) {  /* UTF-8 bytes of a plain constant */
        for (; s < q; s++, n++)
          v = (v << 8) | (unsigned char)*s;
        continue;
      }
      s = q;
    }
    v = wide ? (int64_t)c : (v << 8) | (c & 0xff);
    n++;
  }
  if (!wide && n == 1)
    v = (signed char)v;
  else if (!wide)
    v = (int32_t)v;
  return v;
}

static PPVal pp_primary(Expr *e) {
  struct token *t = e->t;
  if (eq(t, "(")) {
    e->t = t->next;
    PPVal v = pp_cond(e);
    if (eq(e->t, ","))
      error_tok(e->t, "comma operator in preprocessor expression");
    pp_expect(e, ")");
    return v;
  }
  if (t->kind == CPP_NUM) {
    PPVal v;
    if (!pp_number(t, &v))
      error_tok(t, "invalid integer constant in preprocessor expression");
    e->t = t->next;
    return v;
  }
  if (t->kind == CPP_CHAR) {
    e->t = t->next;
    return (PPVal){pp_char(t), false};
  }
  if (t->kind == CPP_EOF)
    error_tok(t, "#if with no expression");
  if (t->kind == CPP_STR)
    error_tok(t, "string literal in preprocessor expression");
  error_tok(t, "token '%s' is not valid in preprocessor expressions", t->text);
  return (PPVal){0, false};
}

static PPVal pp_unary(Expr *e) {
  struct token *t = e->t;
  if (eq(t, "+")) {
    e->t = t->next;
    return pp_unary(e);
  }
  if (eq(t, "-")) {
    e->t = t->next;
    PPVal v = pp_unary(e);
    v.v = (int64_t)(0 - (uint64_t)v.v);
    return v;
  }
  if (eq(t, "~")) {
    e->t = t->next;
    PPVal v = pp_unary(e);
    v.v = ~v.v;
    return v;
  }
  if (eq(t, "!")) {
    e->t = t->next;
    PPVal v = pp_unary(e);
    v.v = !v.v;
    v.u = false;
    return v;
  }
  return pp_primary(e);
}

static int pp_prec(const struct token *t) {
  static const char *ops[][4] = {
      {"||"}, {"&&"}, {"|"}, {"^"}, {"&"}, {"==", "!="}, {"<", ">", "<=", ">="},
      {"<<", ">>"}, {"+", "-"}, {"*", "/", "%"},
  };
  if (t->kind != CPP_PUNCT)
    return -1;
  for (int i = 0; i < 10; i++)
    for (int k = 0; k < 4 && ops[i][k]; k++)
      if (!strcmp(t->text, ops[i][k]))
        return i + 1;
  return -1;
}

static PPVal pp_binary(Expr *e, int minprec) {
  PPVal lhs = pp_unary(e);
  for (;;) {
    struct token *op = e->t;
    int prec = pp_prec(op);
    if (prec < minprec)
      return lhs;
    e->t = op->next;
    const char *s = op->text;
    if (!strcmp(s, "&&") || !strcmp(s, "||")) {
      bool is_and = s[0] == '&';
      bool saved = e->eval;
      if (is_and ? !lhs.v : !!lhs.v)
        e->eval = false;
      PPVal rhs = pp_binary(e, prec + 1);
      e->eval = saved;
      lhs.v = is_and ? (lhs.v && rhs.v) : (lhs.v || rhs.v);
      lhs.u = false;
      continue;
    }
    PPVal rhs = pp_binary(e, prec + 1);
    bool u = lhs.u || rhs.u;
    uint64_t a = lhs.v, b = rhs.v;
    int64_t sa = lhs.v, sb = rhs.v;
    PPVal r = {0, u};
    if (!strcmp(s, "*")) {
      r.v = (int64_t)(a * b);
    } else if (!strcmp(s, "/") || !strcmp(s, "%")) {
      if (b == 0) {
        if (e->eval)
          error_tok(op, "division by zero in #if");
      } else if (u) {
        r.v = s[0] == '/' ? (int64_t)(a / b) : (int64_t)(a % b);
      } else if (sa == INT64_MIN && sb == -1) {
        r.v = s[0] == '/' ? sa : 0;
      } else {
        r.v = s[0] == '/' ? sa / sb : sa % sb;
      }
    } else if (!strcmp(s, "+")) {
      r.v = (int64_t)(a + b);
    } else if (!strcmp(s, "-")) {
      r.v = (int64_t)(a - b);
    } else if (!strcmp(s, "<<")) {
      r.u = lhs.u;
      r.v = b >= 64 ? 0 : (int64_t)(a << b);
    } else if (!strcmp(s, ">>")) {
      r.u = lhs.u;
      if (lhs.u)
        r.v = b >= 64 ? 0 : (int64_t)(a >> b);
      else
        r.v = b >= 64 ? (sa < 0 ? -1 : 0) : sa >> b;
    } else if (!strcmp(s, "<")) {
      r = (PPVal){u ? a < b : sa < sb, false};
    } else if (!strcmp(s, ">")) {
      r = (PPVal){u ? a > b : sa > sb, false};
    } else if (!strcmp(s, "<=")) {
      r = (PPVal){u ? a <= b : sa <= sb, false};
    } else if (!strcmp(s, ">=")) {
      r = (PPVal){u ? a >= b : sa >= sb, false};
    } else if (!strcmp(s, "==")) {
      r = (PPVal){a == b, false};
    } else if (!strcmp(s, "!=")) {
      r = (PPVal){a != b, false};
    } else if (!strcmp(s, "&")) {
      r.v = a & b;
    } else if (!strcmp(s, "^")) {
      r.v = a ^ b;
    } else if (!strcmp(s, "|")) {
      r.v = a | b;
    }
    lhs = r;
  }
}

static PPVal pp_cond(Expr *e) {
  PPVal c = pp_binary(e, 1);
  if (!eq(e->t, "?"))
    return c;
  e->t = e->t->next;
  bool saved = e->eval;
  if (!c.v)
    e->eval = false;
  PPVal a = pp_cond(e);
  e->eval = saved;
  pp_expect(e, ":");
  if (c.v)
    e->eval = false;
  PPVal b = pp_cond(e);
  e->eval = saved;
  PPVal r = c.v ? a : b;
  r.u = a.u || b.u;
  return r;
}

static char *read_include_filename(struct cpp *p, struct token **rest, struct token *t,
                                   bool *quoted);
static char *find_include(struct cpp *p, const char *name, bool quoted, const struct token *at,
                          bool next, int *idx, const char **builtin);
static Macro *find_macro(struct cpp *p, const struct token *t);

/* the line of #if: defined and __has_include replaced, macros expanded */
static struct token *read_const_expr(struct cpp *p, struct token **rest, struct token *t) {
  t = copy_line(rest, t);
  struct token head = {0}, *cur = &head;
  while (t->kind != CPP_EOF) {
    if (eq(t, "defined")) {
      struct token *start = t;
      bool paren = eq(t->next, "(");
      t = paren ? t->next->next : t->next;
      if (t->kind != CPP_IDENT)
        error_tok(start, "macro name must be an identifier");
      use_hook(p, t);
      Macro *m = find_macro(p, t);
      t = t->next;
      if (paren) {
        if (!eq(t, ")"))
          error_tok(t, "expected ')' after \"defined\"");
        t = t->next;
      }
      cur = cur->next = new_num_token(m ? 1 : 0, start);
      continue;
    }
    if (eq(t, "__has_include") || eq(t, "__has_include_next")) {
      struct token *start = t;
      bool next = eq(t, "__has_include_next");
      if (!eq(t->next, "("))
        error_tok(t, "expected '(' after %s", t->text);
      t = t->next->next;
      struct token h = {0}, *c = &h;
      int depth = 0;
      while (t->kind != CPP_EOF && !(depth == 0 && eq(t, ")"))) {
        if (eq(t, "("))
          depth++;
        if (eq(t, ")"))
          depth--;
        c = c->next = copy_token(t);
        t = t->next;
      }
      if (!eq(t, ")"))
        error_tok(start, "expected ')'");
      t = t->next;
      c->next = new_eof(start);
      c->next->bol = true;
      struct token *dummy;
      bool quoted;
      char *name = read_include_filename(p, &dummy, h.next, &quoted);
      int idx;
      const char *builtin;
      bool found = find_include(p, name, quoted, start, next, &idx, &builtin) != NULL;
      cur = cur->next = new_num_token(found, start);
      continue;
    }
    cur = cur->next = t;
    t = t->next;
  }
  cur->next = t;
  return head.next;
}

static bool eval_const_expr(struct cpp *p, struct token **rest, struct token *t) {
  struct token *start = t;
  struct token *expr = preprocess2(p, read_const_expr(p, rest, t->next));
  if (expr->kind == CPP_EOF)
    error_tok(start, "no expression");
  /* identifiers left after expansion are 0 (6.10.1p4) */
  for (struct token *u = expr; u->kind != CPP_EOF; u = u->next)
    if (u->kind == CPP_IDENT) {
      struct token *next = u->next;
      *u = *new_num_token(0, u);
      u->next = next;
    }
  Expr e = {p, expr, true};
  PPVal v = pp_cond(&e);
  if (e.t->kind != CPP_EOF) {
    if (eq(e.t, ","))
      error_tok(e.t, "comma operator in preprocessor expression");
    error_tok(e.t, "missing binary operator before token '%s'", e.t->text);
  }
  return v.v != 0;
}

static void push_cond_incl(struct cpp *p, struct token *t, bool included) {
  CondIncl *ci = xcalloc(1, sizeof *ci);
  ci->next = p->cond;
  ci->ctx = IN_THEN;
  ci->tok = t;
  ci->included = included;
  p->cond = ci;
}

/* ================================================================ #define */

static Macro *find_macro(struct cpp *p, const struct token *t) {
  if (t->kind != CPP_IDENT)
    return NULL;
  return hashmap_get(&p->macros, t->text);
}

static Macro *add_macro(struct cpp *p, char *name, bool is_objlike, struct token *body) {
  Macro *m = xcalloc(1, sizeof *m);
  m->name = name;
  m->is_objlike = is_objlike;
  m->body = body;
  hashmap_put(&p->macros, name, m);
  return m;
}

static MacroParam *read_macro_params(struct token **rest, struct token *t, char **va_args_name) {
  MacroParam head = {0}, *cur = &head;
  while (!eq(t, ")")) {
    if (t->bol)
      error_tok(t, "missing ')' in macro parameter list");
    if (cur != &head) {
      if (!eq(t, ","))
        error_tok(t, "expected ',' in macro parameter list");
      t = t->next;
    }
    if (eq(t, "...")) {
      *va_args_name = "__VA_ARGS__";
      if (!eq(t->next, ")"))
        error_tok(t->next, "expected ')' after '...'");
      *rest = t->next->next;
      return head.next;
    }
    if (t->kind != CPP_IDENT)
      error_tok(t, "expected parameter name");
    if (eq(t->next, "...")) {  /* GNU: a named variable argument "name..." */
      *va_args_name = t->text;
      if (!eq(t->next->next, ")"))
        error_tok(t->next->next, "expected ')' after '...'");
      *rest = t->next->next->next;
      return head.next;
    }
    if (eq(t, "__VA_ARGS__"))
      error_tok(t, "__VA_ARGS__ can not be used as a parameter name");
    for (MacroParam *q = head.next; q; q = q->next)
      if (eq(t, q->name))
        error_tok(t, "duplicate macro parameter '%s'", q->name);
    MacroParam *m = xcalloc(1, sizeof *m);
    m->name = t->text;
    cur = cur->next = m;
    t = t->next;
  }
  *rest = t->next;
  return head.next;
}

/* are two replacement lists identical (6.10.3p2)? */
static bool same_body(struct token *a, struct token *b) {
  for (bool first = true;; a = a->next, b = b->next, first = false) {
    if (!a || !b)
      return a == b;
    if (a->kind == CPP_EOF || b->kind == CPP_EOF)
      return a->kind == b->kind;
    if (a->len != b->len || memcmp(a->loc, b->loc, a->len))
      return false;
    if (!first && (a->space || a->bol) != (b->space || b->bol))
      return false;
  }
}

static bool same_params(Macro *m, MacroParam *q, char *va) {
  MacroParam *r = m->params;
  for (; q && r; q = q->next, r = r->next)
    if (strcmp(q->name, r->name))
      return false;
  if (q || r)
    return false;
  return (m->va_args_name == NULL) == (va == NULL);
}

static bool is_param(struct token *t, MacroParam *params, char *va) {
  for (MacroParam *q = params; q; q = q->next)
    if (eq(t, q->name))
      return true;
  return va && eq(t, va);
}

static void read_macro_definition(struct cpp *p, struct token **rest, struct token *t) {
  if (t->kind != CPP_IDENT)
    error_tok(t, t->bol ? "no macro name given in #define directive"
                        : "macro names must be identifiers");
  char *name = t->text;
  struct token *name_tok = t;
  if (!strcmp(name, "defined"))
    error_tok(t, "\"defined\" cannot be used as a macro name");
  t = t->next;
  Macro *old = hashmap_get(&p->macros, name);
  Macro *m;
  if (!t->space && eq(t, "(") && !t->bol) {
    char *va = NULL;
    MacroParam *params = read_macro_params(&t, t->next, &va);
    struct token *body = copy_line(rest, t);
    for (struct token *u = body; u->kind != CPP_EOF; u = u->next) {
      if (eq(u, "__VA_ARGS__") && !va)
        error_tok(u, "__VA_ARGS__ can only appear in the expansion of a variadic macro");
      if (eq(u, "#") && !is_param(u->next, params, va))
        error_tok(u, "'#' is not followed by a macro parameter");
    }
    if (eq(body, "##"))
      error_tok(body, "'##' cannot appear at either end of a macro expansion");
    for (struct token *u = body; u->kind != CPP_EOF; u = u->next)
      if (eq(u, "##") && u->next->kind == CPP_EOF)
        error_tok(u, "'##' cannot appear at either end of a macro expansion");
    if (old && (old->handler || old->is_objlike || !same_params(old, params, va) ||
                !same_body(old->body, body)))
      warn_tok(name_tok, "'%s' redefined", name);
    m = add_macro(p, name, false, body);
    m->params = params;
    m->va_args_name = va;
  } else {
    /* 6.10.3p3: white space between the name and the replacement list */
    if (!t->bol && !t->space)
      warn_tok(t, "missing whitespace after the macro name");
    struct token *body = copy_line(rest, t);
    if (eq(body, "##"))
      error_tok(body, "'##' cannot appear at either end of a macro expansion");
    for (struct token *u = body; u->kind != CPP_EOF; u = u->next) {
      if (eq(u, "##") && u->next->kind == CPP_EOF)
        error_tok(u, "'##' cannot appear at either end of a macro expansion");
      if (eq(u, "__VA_ARGS__"))
        error_tok(u, "__VA_ARGS__ can only appear in the expansion of a variadic macro");
    }
    if (old && (old->handler || !old->is_objlike || !same_body(old->body, body)))
      warn_tok(name_tok, "'%s' redefined", name);
    m = add_macro(p, name, true, body);
  }
  if (p->hooks.define) {
    set_where(name_tok);
    p->hooks.define(p->hooks.arg, name_tok);
  }
}

/* ================================================================ expansion */

static MacroArg *read_macro_arg_one(struct token **rest, struct token *t, bool read_rest) {
  struct token head = {0}, *cur = &head;
  int level = 0;
  for (;;) {
    if (level == 0 && eq(t, ")"))
      break;
    if (level == 0 && !read_rest && eq(t, ","))
      break;
    if (t->kind == CPP_EOF)
      error_tok(t, "unterminated argument list invoking macro");
    if (is_hash(t))
      error_tok(t, "preprocessing directive within macro arguments is not supported");
    if (eq(t, "("))
      level++;
    else if (eq(t, ")"))
      level--;
    cur = cur->next = copy_token(t);
    t = t->next;
  }
  cur->next = new_eof(t);
  MacroArg *arg = xcalloc(1, sizeof *arg);
  arg->tok = head.next;
  *rest = t;
  return arg;
}

static MacroArg *read_macro_args(struct token **rest, struct token *t, Macro *m) {
  struct token *start = t;
  t = t->next->next;  /* the name and ( */
  MacroArg head = {0}, *cur = &head;
  int nparams = 0, nargs = 0;
  for (MacroParam *q = m->params; q; q = q->next)
    nparams++;
  for (MacroParam *q = m->params; q; q = q->next) {
    if (cur != &head) {
      if (!eq(t, ","))
        error_tok(start, "macro \"%s\" requires %d arguments, but only %d given", m->name,
                  nparams, nargs);
      t = t->next;
    }
    cur = cur->next = read_macro_arg_one(&t, t, false);
    cur->name = q->name;
    nargs++;
  }
  if (m->va_args_name) {
    MacroArg *arg;
    if (eq(t, ")")) {
      arg = xcalloc(1, sizeof *arg);
      arg->tok = new_eof(t);
      arg->omitted = nparams > 0;
    } else {
      if (nparams > 0) {
        if (!eq(t, ","))
          error_tok(t, "expected ','");
        t = t->next;
      }
      arg = read_macro_arg_one(&t, t, true);
    }
    arg->name = m->va_args_name;
    arg->is_va_args = true;
    cur = cur->next = arg;
  } else if (!eq(t, ")")) {
    error_tok(start, "macro \"%s\" passed too many arguments (takes %d)", m->name, nparams);
  }
  *rest = t;
  return head.next;
}

static MacroArg *find_arg(MacroArg *args, struct token *t) {
  for (MacroArg *a = args; a; a = a->next)
    if (t->kind == CPP_IDENT && !strcmp(t->text, a->name))
      return a;
  return NULL;
}

/* 6.10.3.3: ## */
static struct token *paste(struct token *lhs, struct token *rhs) {
  char *buf = format("%.*s%.*s", lhs->len, lhs->loc, rhs->len, rhs->loc);
  struct token *t = tokenize_text(buf, lhs);
  if (t->next->kind != CPP_EOF)
    error_tok(lhs, "pasting \"%.*s\" and \"%.*s\" does not give a valid preprocessing token",
              lhs->len, lhs->loc, rhs->len, rhs->loc);
  return t;
}

static struct token *new_placemarker(struct token *tmpl) {
  struct token *t = copy_token(tmpl);
  t->kind = CPP_OTHER;
  t->text = t->loc = "";
  t->len = 0;
  return t;
}

static bool is_placemarker(struct token *t) { return t->kind == CPP_OTHER && t->len == 0; }

/* *cur becomes cur pasted with rhs, keeping its flags and link */
static void paste_into(struct token *cur, struct token *rhs) {
  if (is_placemarker(rhs))
    return;
  struct token *next = cur->next;
  bool bol = cur->bol, space = cur->space;
  struct hideset *hs = cur->hs;
  *cur = is_placemarker(cur) ? *rhs : *paste(cur, rhs);
  cur->next = next;
  if (!is_placemarker(rhs))
    cur->bol = bol;
  cur->space = space;
  cur->hs = hs;
}

static MacroArg *va_arg_of(MacroArg *args) {
  for (MacroArg *a = args; a; a = a->next)
    if (a->is_va_args)
      return a;
  return NULL;
}

static struct token *expanded_arg(struct cpp *p, MacroArg *arg) {
  if (!arg->expanded)
    arg->expanded = preprocess2(p, arg->tok);
  return arg->expanded;
}

/* the parameters replaced by the arguments (6.10.3.1-3) */
static struct token *subst(struct cpp *p, struct token *t, MacroArg *args) {
  struct token head = {0}, *cur = &head;
  while (t->kind != CPP_EOF) {
    if (eq(t, "#")) {
      MacroArg *arg = find_arg(args, t->next);
      if (!arg)
        error_tok(t->next, "'#' is not followed by a macro parameter");
      cur = cur->next = stringize(t, arg->tok);
      cur->space = t->space;
      t = t->next->next;
      continue;
    }
    /* GNU: ", ## __VA_ARGS__" drops the comma when the variable arguments
       are entirely absent (a constraint violation in C17 otherwise) */
    if (eq(t, ",") && eq(t->next, "##")) {
      MacroArg *arg = find_arg(args, t->next->next);
      if (arg && arg->is_va_args) {
        MacroArg *va = va_arg_of(args);
        if (va->omitted) {
          t = t->next->next->next;
          continue;
        }
        cur = cur->next = copy_token(t);
        if (va->tok->kind == CPP_EOF) {
          t = t->next->next->next;
          continue;
        }
        t = t->next->next;
        for (struct token *u = va->tok; u->kind != CPP_EOF; u = u->next)
          cur = cur->next = copy_token(u);
        t = t->next;
        continue;
      }
    }
    if (eq(t, "##")) {
      if (cur == &head || t->next->kind == CPP_EOF)
        error_tok(t, "'##' cannot appear at either end of a macro expansion");
      MacroArg *arg = find_arg(args, t->next);
      struct token *r = arg ? arg->tok : t->next;
      if (arg && r->kind == CPP_EOF)
        r = new_placemarker(t->next);
      paste_into(cur, r);
      if (arg)
        for (struct token *u = r->next; u && u->kind != CPP_EOF; u = u->next)
          cur = cur->next = copy_token(u);
      t = t->next->next;
      continue;
    }
    MacroArg *arg = find_arg(args, t);
    if (arg && eq(t->next, "##")) {  /* the left operand of ##: unexpanded */
      if (arg->tok->kind == CPP_EOF) {
        cur = cur->next = new_placemarker(t);
        cur->space = t->space;
      } else {
        for (struct token *u = arg->tok; u->kind != CPP_EOF; u = u->next) {
          cur = cur->next = copy_token(u);
          if (u == arg->tok)
            cur->space = t->space;
        }
      }
      t = t->next;
      continue;
    }
    if (arg) {
      bool first = true;
      for (struct token *u = expanded_arg(p, arg); u->kind != CPP_EOF; u = u->next) {
        cur = cur->next = copy_token(u);
        if (first)
          cur->space = t->space;
        first = false;
      }
      t = t->next;
      continue;
    }
    cur = cur->next = copy_token(t);
    t = t->next;
  }
  cur->next = t;
  /* the placemarkers go */
  struct token h2 = {0}, *c2 = &h2;
  for (struct token *u = head.next; u; u = u->next) {
    if (u->kind == CPP_EOF) {
      c2->next = u;
      break;
    }
    if (!is_placemarker(u))
      c2 = c2->next = u;
  }
  return h2.next;
}

/* a macro that expands to nothing gives its white space to the token
   after it (for -E) */
static struct token *space_next(struct token *macro_tok, struct token *next) {
  if (next->kind == CPP_EOF || next->space || next->bol)
    return next;
  if (!macro_tok->space && !macro_tok->bol)
    return next;
  struct token *t = copy_token(next);
  t->next = next->next;
  t->space = true;
  return t;
}

/* ## in an object-like macro */
static struct token *paste_objlike(struct token *body) {
  bool has = false;
  for (struct token *t = body; t && t->kind != CPP_EOF; t = t->next)
    if (eq(t, "##"))
      has = true;
  if (!has)
    return body;
  struct token head = {0}, *cur = &head, *t = body;
  while (t->kind != CPP_EOF) {
    if (eq(t, "##") && cur != &head && t->next->kind != CPP_EOF) {
      paste_into(cur, t->next);
      t = t->next->next;
      continue;
    }
    cur = cur->next = copy_token(t);
    t = t->next;
  }
  cur->next = t;
  return head.next;
}

static void set_origin(struct token *t, struct token *origin) {
  for (; t && t->kind != CPP_EOF; t = t->next)
    t->origin = origin;
}

/* expands t if it names a macro */
static bool expand_macro(struct cpp *p, struct token **rest, struct token *t) {
  if (t->kind != CPP_IDENT || hideset_contains(t->hs, t->text))
    return false;
  Macro *m = find_macro(p, t);
  if (!m)
    return false;
  if (m->handler) {  /* __LINE__ and the like */
    use_hook(p, t);
    struct token *r = m->handler(p, t);
    r->space = t->space;
    r->bol = t->bol;
    r->next = t->next;
    *rest = r;
    return true;
  }
  struct token *origin = t->origin ? t->origin : t;
  if (m->is_objlike) {
    use_hook(p, t);
    struct hideset *hs = hideset_union(t->hs, new_hideset(m->name));
    struct token *body = add_hideset(paste_objlike(m->body), hs);
    set_origin(body, origin);
    if (body && body->kind != CPP_EOF) {
      body->bol = t->bol;
      body->space = t->space;
      *rest = append(body, t->next);
    } else {
      *rest = space_next(t, t->next);
    }
    return true;
  }
  /* a function-like macro not followed by ( is an identifier */
  if (!eq(t->next, "("))
    return false;
  use_hook(p, t);
  struct token *macro_tok = t;
  MacroArg *args = read_macro_args(&t, t, m);
  struct token *rparen = t;
  /* the hide set: (HS(name) & HS(rparen)) | {name} */
  struct hideset *hs = hideset_intersection(macro_tok->hs, rparen->hs);
  hs = hideset_union(hs, new_hideset(m->name));
  struct token *body = add_hideset(subst(p, m->body, args), hs);
  set_origin(body, origin);
  if (body && body->kind != CPP_EOF) {
    body->bol = macro_tok->bol;
    body->space = macro_tok->space;
    *rest = append(body, t->next);
  } else {
    *rest = space_next(macro_tok, t->next);
  }
  return true;
}

/* ================================================================ #include */

static char *join_path(const char *dir, const char *file) {
  if (!strcmp(dir, "."))
    return xstrdup(file);
  return format("%s/%s", dir, file);
}

static char *dir_of(const char *path) {
  const char *slash = strrchr(path, '/');
  if (!slash)
    return xstrdup(".");
  if (slash == path)
    return xstrdup("/");
  return xstrndup(path, slash - path);
}

/* the search list: the -I directories, the built-in headers (NULL), the
   system directories; *idx: the position (-1: the including file's
   directory, -2: an absolute name) */
static int nsearch(struct cpp *p) { return p->idirs.len + 1 + p->sdirs.len; }

static char *find_include(struct cpp *p, const char *name, bool quoted, const struct token *at,
                          bool next, int *idx, const char **builtin) {
  *builtin = NULL;
  if (name[0] == '/') {
    *idx = -2;
    return is_regular_file(name) ? xstrdup(name) : NULL;
  }
  int from = 0;
  const struct token *pos = at;
  while (pos->origin)
    pos = pos->origin;
  if (next) {
    from = pos->sf && pos->sf->dirindex >= 0 ? pos->sf->dirindex + 1 : 0;
  } else if (quoted && pos->sf) {
    char *dir = pos->sf->quotedir ? xstrdup(pos->sf->quotedir) : dir_of(pos->sf->path);
    char *path = join_path(dir, name);
    if (is_regular_file(path)) {
      *idx = -1;
      return path;
    }
  }
  for (int i = from; i < nsearch(p); i++) {
    if (i == p->idirs.len) {
      for (size_t k = 0; k < sizeof builtin_headers / sizeof *builtin_headers; k++)
        if (!strcmp(name, builtin_headers[k].name)) {
          *idx = i;
          *builtin = builtin_headers[k].text;
          return format("<built-in>/%s", name);
        }
      continue;
    }
    const char *dir = i < p->idirs.len ? p->idirs.data[i] : p->sdirs.data[i - p->idirs.len - 1];
    char *path = join_path(dir, name);
    if (is_regular_file(path)) {
      *idx = i;
      return path;
    }
  }
  return NULL;
}

static char *read_include_filename(struct cpp *p, struct token **rest, struct token *t,
                                   bool *quoted) {
  if (t->kind == CPP_STR) {  /* "q-char-sequence": no escapes (6.4.7) */
    *quoted = true;
    if (t->loc[0] != '"')
      error_tok(t, "expected \"FILENAME\" or <FILENAME>");
    *rest = skip_line(t->next);
    return xstrndup(t->loc + 1, t->len - 2);
  }
  if (eq(t, "<")) {
    struct token *start = t;
    for (; !eq(t, ">"); t = t->next)
      if (t->bol || t->kind == CPP_EOF)
        error_tok(start, "missing terminating > character");
    *quoted = false;
    *rest = skip_line(t->next);
    return join_tokens(start->next, t);
  }
  if (t->kind == CPP_IDENT) {  /* #include MACRO (6.10.2p4) */
    struct token *line = preprocess2(p, copy_line(rest, t));
    if (line->kind == CPP_EOF)
      error_tok(t, "empty filename in #include");
    struct token *last = line;
    while (last->next && last->next->kind != CPP_EOF)
      last = last->next;
    last->next = new_eof(t);
    last->next->bol = true;
    struct token *dummy;
    return read_include_filename(p, &dummy, line, quoted);
  }
  error_tok(t, "#include expects \"FILENAME\" or <FILENAME>");
  return NULL;
}

/* the macro X of a "#ifndef X / #define X ... #endif" file */
static char *detect_include_guard(struct token *t) {
  if (!is_hash(t) || !eq(t->next, "ifndef"))
    return NULL;
  t = t->next->next;
  if (t->kind != CPP_IDENT)
    return NULL;
  char *macro = t->text;
  t = t->next;
  if (!is_hash(t) || !eq(t->next, "define") || !eq(t->next->next, macro))
    return NULL;
  while (t->kind != CPP_EOF) {
    if (!is_hash(t)) {
      t = t->next;
      continue;
    }
    if (eq(t->next, "endif") && t->next->next->kind == CPP_EOF)
      return macro;
    if (eq(t->next, "if") || eq(t->next, "ifdef") || eq(t->next, "ifndef"))
      t = skip_cond_incl2(t->next->next);
    else
      t = t->next;
  }
  return NULL;
}

/* a file's name for #pragma once */
static char *file_key(const char *path) {
  char *r = realpath(path, NULL);
  return r ? r : xstrdup(path);
}

static struct token *include_file(struct cpp *p, struct token *rest, char *path,
                                  struct token *at, int idx, const char *builtin) {
  if (hashmap_get(&p->pragma_once, file_key(path)))
    return rest;
  char *guard = hashmap_get(&p->guards, path);
  if (guard && hashmap_get(&p->macros, guard))
    return rest;
  if (p->depth > 200)
    error_tok(at, "#include nested too deeply");
  struct token *t = tokenize_file(path, path, idx, builtin);
  if (!t)
    error_tok(at, "%s: cannot open file: %s", path, strerror(errno));
  if (idx >= p->idirs.len)
    t->sf->system = true;
  guard = detect_include_guard(t);
  if (guard)
    hashmap_put(&p->guards, path, guard);
  return append(t, rest);
}

/* #line (6.10.4), and line markers "# 123 "file"" */
static void read_line_marker(struct cpp *p, struct token **rest, struct token *t, bool gnu) {
  struct token *start = t;
  t = preprocess2(p, copy_line(rest, t));
  bool ok = t->kind == CPP_NUM;
  for (int i = 0; ok && i < t->len; i++)
    ok = isdigit((unsigned char)t->loc[i]);
  if (!ok)
    error_tok(t, "\"%s\" after #line is not a positive integer", t->text);
  long long line = strtoll(t->text, NULL, 10);
  if (!gnu && (line == 0 || line > 2147483647))
    warn_tok(t, "line number out of range");
  const char *name = NULL;
  struct token *next = t->next;
  if (next->kind == CPP_STR) {
    name = unescape(next);
    next = next->next;
  } else if (next->kind != CPP_EOF) {
    error_tok(next, "invalid filename in #line directive");
  }
  if (!gnu && next->kind != CPP_EOF)
    error_tok(next, "extra tokens at end of #line directive");
  /* from the next physical line on */
  struct srcfile *sf = start->sf;
  if (!sf)
    return;
  const char *cur;
  int oline;
  where(sf, start->line, &cur, &oline);
  add_linechange(sf, start->line + 1, (int)line - (start->line + 1), name ? name : cur);
}

/* ================================================================ #pragma */

static struct token *make_pragma_token(struct token *start, char *text) {
  struct token *t = copy_token(start);
  t->kind = CPP_OTHER;
  t->text = t->loc = text;
  t->len = strlen(text);
  t->bol = true;
  t->space = false;
  return t;
}

bool cpp_is_pragma(const struct token *t) {
  return t->kind == CPP_OTHER && startswith(t->text, "#pragma");
}

/* a #pragma whose tokens are the rest of the line: the token to keep for
   -E, or NULL */
static struct token *do_pragma(struct cpp *p, struct token *pragma, struct token *t,
                               struct token **rest) {
  if (eq(t, "once") && t->next->bol) {
    if (t->sf)
      hashmap_put(&p->pragma_once, file_key(t->sf->path), (void *)1);
    *rest = skip_line(t->next);
    return NULL;
  }
  struct token *line = copy_line(rest, t);
  if ((eq(line, "push_macro") || eq(line, "pop_macro")) && eq(line->next, "(") &&
      line->next->next->kind == CPP_STR && eq(line->next->next->next, ")")) {
    char *name = unescape(line->next->next);
    typedef struct MStack {
      struct MStack *next;
      Macro *m;
    } MStack;
    MStack *top = hashmap_get(&p->pushed, name);
    if (eq(line, "push_macro")) {
      MStack *e = xcalloc(1, sizeof *e);
      Macro *cur = hashmap_get(&p->macros, name);
      if (cur) {
        e->m = xmalloc(sizeof *e->m);
        *e->m = *cur;
      }
      e->next = top;
      hashmap_put(&p->pushed, name, e);
    } else if (top) {
      if (top->m)
        hashmap_put(&p->macros, name, top->m);
      else
        hashmap_delete(&p->macros, name);
      hashmap_put(&p->pushed, name, top->next);
    }
    return NULL;
  }
  if (eq(line, "GCC") && eq(line->next, "system_header")) {
    if (t->sf)
      t->sf->system = true;
    return NULL;
  }
  if (!p->keep_pragmas)
    return NULL;
  return make_pragma_token(pragma, format("#pragma %s", join_tokens(line, NULL)));
}

/* _Pragma ( string-literal ) (6.10.9) */
static struct token *pragma_operator(struct cpp *p, struct token *t, struct token **rest) {
  struct token *start = t;
  t = t->next;
  if (!eq(t, "("))
    error_tok(t, "_Pragma takes a parenthesized string literal");
  t = t->next;
  if (t->kind != CPP_STR || t->loc[0] == 'u' || t->loc[0] == 'U')
    error_tok(t, "_Pragma takes a parenthesized string literal");
  struct token *str = t;
  t = t->next;
  if (!eq(t, ")"))
    error_tok(t, "_Pragma takes a parenthesized string literal");
  *rest = t->next;
  /* destringized: the L deleted, \" and \\ unescaped */
  const char *s = str->loc;
  if (*s == 'L')
    s++;
  Buf b = {0};
  for (s++; s < str->loc + str->len - 1; s++) {
    if (*s == '\\' && (s[1] == '"' || s[1] == '\\'))
      s++;
    buf_putc(&b, *s);
  }
  struct token *line = tokenize_text(b.buf ? b.buf : "", start);
  if (line->kind == CPP_EOF)
    return NULL;
  struct token *dummy;
  return do_pragma(p, start, line, &dummy);
}

/* ================================================================ the directives */

/* the place to carry on after an error: setjmp in a frame that stays */
#define RECOVERABLE(p) (!(p)->hooks.diag)

static struct token *preprocess2(struct cpp *p, struct token *tok) {
  struct token head = {0};
  struct token *volatile cur = &head;
  struct token *volatile t = tok;
  jmp_buf jb;
  jmp_buf *saved = recover;

  while (t->kind != CPP_EOF) {
    struct token *volatile start = t;
    if (p->preprocessed) {  /* a .i file */
      if (is_hash(t) && t->next->kind == CPP_NUM && !t->next->bol) {
        struct token *r;
        read_line_marker(p, &r, t->next, true);
        t = r;
        continue;
      }
      if (is_hash(t) && eq(t->next, "pragma")) {
        struct token *r;
        struct token *res = do_pragma(p, t, t->next->next, &r);
        t = r;
        if (res)
          cur = cur->next = res;
        continue;
      }
      cur = cur->next = t;
      t = t->next;
      continue;
    }

    bool risky = is_hash(t) || (t->kind == CPP_IDENT && find_macro(p, t)) ||
                 (eq(t, "_Pragma") && t->kind == CPP_IDENT);
    if (risky && RECOVERABLE(p)) {
      if (setjmp(jb)) {
        /* after an error: a directive is skipped; a macro name, or
           _Pragma, is taken as it is */
        recover = &jb;
        if (is_hash(start)) {
          t = start->next;
          while (!t->bol)
            t = t->next;
        } else {
          cur = cur->next = start;
          t = start->next;
        }
        continue;
      }
      recover = &jb;
    }

    struct token *r;
    if (expand_macro(p, &r, t)) {
      t = r;
      continue;
    }
    if (eq(t, "_Pragma") && t->kind == CPP_IDENT && !hideset_contains(t->hs, "_Pragma")) {
      struct token *res = pragma_operator(p, t, &r);
      t = r;
      if (res)
        cur = cur->next = res;
      continue;
    }
    if (!is_hash(t) || t->origin) {
      cur = cur->next = t;
      t = t->next;
      continue;
    }

    struct token *hash = t;
    struct token *d = t->next;
    if (d->bol) {  /* the null directive */
      t = d;
      continue;
    }

    if (eq(d, "include") || eq(d, "include_next")) {
      bool next = eq(d, "include_next");
      bool quoted;
      char *name = read_include_filename(p, &r, d->next, &quoted);
      if (!*name)
        error_tok(hash, "empty filename in #include");
      int idx;
      const char *builtin;
      char *path = find_include(p, name, quoted, hash, next, &idx, &builtin);
      if (!path)
        error_tok(d->next, "%s: No such file or directory", name);
      t = include_file(p, r, path, d->next, idx, builtin);
      continue;
    }
    if (eq(d, "define")) {
      read_macro_definition(p, &r, d->next);
      t = r;
      continue;
    }
    if (eq(d, "undef")) {
      d = d->next;
      if (d->kind != CPP_IDENT)
        error_tok(d, "macro name must be an identifier");
      if (eq(d, "defined"))
        error_tok(d, "\"defined\" cannot be used as a macro name");
      use_hook(p, d);
      Macro *m = find_macro(p, d);
      if (m && m->is_predefined)
        warn_tok(d, "undefining builtin macro '%s'", d->text);
      hashmap_delete(&p->macros, d->text);
      t = skip_line(d->next);
      continue;
    }
    if (eq(d, "if")) {
      bool val = eval_const_expr(p, &r, d);
      push_cond_incl(p, hash, val);
      t = val ? r : skip_cond_incl(r);
      continue;
    }
    if (eq(d, "ifdef") || eq(d, "ifndef")) {
      bool is_ifdef = eq(d, "ifdef");
      struct token *name = d->next;
      if (name->bol)
        error_tok(d, "no macro name given in #%s directive", is_ifdef ? "ifdef" : "ifndef");
      if (name->kind != CPP_IDENT)
        error_tok(name, "macro names must be identifiers");
      use_hook(p, name);
      bool defined = find_macro(p, name);
      bool val = is_ifdef ? defined : !defined;
      push_cond_incl(p, d, val);
      t = skip_line(name->next);
      if (!val)
        t = skip_cond_incl(t);
      continue;
    }
    if (eq(d, "elif")) {
      if (!p->cond || p->cond->ctx == IN_ELSE)
        error_tok(hash, "#elif without #if");
      p->cond->ctx = IN_ELIF;
      if (p->cond->included) {
        t = skip_cond_incl(d->next);
      } else if (eval_const_expr(p, &r, d)) {
        p->cond->included = true;
        t = r;
      } else {
        t = skip_cond_incl(r);
      }
      continue;
    }
    if (eq(d, "else")) {
      if (!p->cond || p->cond->ctx == IN_ELSE)
        error_tok(hash, "#else without #if");
      p->cond->ctx = IN_ELSE;
      t = skip_line(d->next);
      if (p->cond->included)
        t = skip_cond_incl(t);
      continue;
    }
    if (eq(d, "endif")) {
      if (!p->cond)
        error_tok(hash, "#endif without #if");
      p->cond = p->cond->next;
      t = skip_line(d->next);
      continue;
    }
    if (eq(d, "line")) {
      read_line_marker(p, &r, d->next, false);
      t = r;
      continue;
    }
    if (d->kind == CPP_NUM) {
      read_line_marker(p, &r, d, true);
      t = r;
      continue;
    }
    if (eq(d, "pragma")) {
      struct token *res = do_pragma(p, hash, d->next, &r);
      t = r;
      if (res)
        cur = cur->next = res;
      continue;
    }
    if (eq(d, "error") || eq(d, "warning")) {
      Buf b = {0};
      struct token *u = d->next;
      for (; !u->bol; u = u->next) {
        if (b.len && u->space)
          buf_putc(&b, ' ');
        buf_write(&b, u->loc, u->len);
      }
      if (eq(d, "error"))
        error_tok(hash, "#error %s", b.buf ? b.buf : "");
      warn_tok(hash, "#warning %s", b.buf ? b.buf : "");
      t = u;
      continue;
    }
    error_tok(d, "invalid preprocessing directive #%s", d->text);
  }
  cur->next = t;
  recover = saved;
  return head.next;
}

/* ================================================================ predefined macros */

static struct token *file_macro(struct cpp *p, struct token *t) {
  (void)p;
  const char *f;
  int l;
  cpp_where(t, &f, &l);
  return new_str_token(f, t);
}

static struct token *line_macro(struct cpp *p, struct token *t) {
  (void)p;
  const char *f;
  int l;
  cpp_where(t, &f, &l);
  return new_num_token(l, t);
}

static struct token *counter_macro(struct cpp *p, struct token *t) {
  return new_num_token(p->counter++, t);
}

static struct token *base_file_macro(struct cpp *p, struct token *t) {
  return new_str_token(p->base_file ? p->base_file : "", t);
}

/* the modification time of the current file */
static struct token *timestamp_macro(struct cpp *p, struct token *t) {
  (void)p;
  const struct token *pos = t;
  while (pos->origin)
    pos = pos->origin;
  struct stat st;
  if (!pos->sf || stat(pos->sf->path, &st) != 0)
    return new_str_token("??? ??? ?? ??:??:?? ????", t);
  char buf[64];
  strftime(buf, sizeof buf, "%a %b %e %H:%M:%S %Y", localtime(&st.st_mtime));
  return new_str_token(buf, t);
}

/* "name body", as on a #define line (also "name(x) body") */
static void define_text(struct cpp *p, const char *text, bool predefined) {
  struct srcfile *sf = new_srcfile("<built-in>", "<built-in>", xstrdup(text), -1);
  sf->contents = format("%s\n", text);
  struct token *t = tokenize_buf(sf->contents, sf);
  struct cpp_hooks saved = p->hooks;
  p->hooks.define = NULL;
  struct token *rest;
  read_macro_definition(p, &rest, t);
  p->hooks = saved;
  if (predefined)
    ((Macro *)hashmap_get(&p->macros, t->text))->is_predefined = true;
}

static void add_builtin(struct cpp *p, char *name, macro_handler_fn *fn) {
  Macro *m = add_macro(p, name, true, NULL);
  m->handler = fn;
  m->is_predefined = true;
}

static void predefine(struct cpp *p) {
  static const char *defs[] = {
      /* 6.10.8 */
      "__STDC__ 1", "__STDC_HOSTED__ 1", "__STDC_VERSION__ 201710L",
      "__STDC_ISO_10646__ 201706L", "__STDC_UTF_16__ 1", "__STDC_UTF_32__ 1",
      "__STDC_IEC_559__ 1",
      /* the system */
#if defined __linux__
      "__linux__ 1", "__linux 1",
#endif
#if defined __unix__
      "__unix__ 1", "__unix 1",
#endif
#if defined __ELF__
      "__ELF__ 1",
#endif
#if defined __x86_64__
      "__x86_64__ 1", "__x86_64 1", "__amd64__ 1", "__amd64 1",
#elif defined __i386__
      "__i386__ 1", "__i386 1",
#elif defined __aarch64__
      "__aarch64__ 1",
#elif defined __arm__
      "__arm__ 1",
#elif defined __riscv
      "__riscv 1",
#endif
#if defined __APPLE__
      "__APPLE__ 1",
#endif
#if defined __FreeBSD__
      "__FreeBSD__ 1",
#endif
      /* the types (LP64) */
      "__LP64__ 1", "_LP64 1", "__CHAR_BIT__ 8", "__SCHAR_MAX__ 0x7f", "__SHRT_MAX__ 0x7fff",
      "__INT_MAX__ 0x7fffffff", "__LONG_MAX__ 0x7fffffffffffffffL",
      "__LONG_LONG_MAX__ 0x7fffffffffffffffLL", "__WCHAR_MAX__ 0x7fffffff",
      "__WCHAR_MIN__ (-0x7fffffff - 1)", "__SIZEOF_SHORT__ 2", "__SIZEOF_INT__ 4",
      "__SIZEOF_LONG__ 8", "__SIZEOF_LONG_LONG__ 8", "__SIZEOF_POINTER__ 8",
      "__SIZEOF_FLOAT__ 4", "__SIZEOF_DOUBLE__ 8", "__SIZEOF_LONG_DOUBLE__ 16",
      "__SIZEOF_SIZE_T__ 8", "__SIZEOF_WCHAR_T__ 4", "__SIZEOF_WINT_T__ 4",
      "__SIZEOF_PTRDIFF_T__ 8", "__SIZE_TYPE__ unsigned long", "__PTRDIFF_TYPE__ long",
      "__WCHAR_TYPE__ int", "__WINT_TYPE__ unsigned int", "__INTMAX_TYPE__ long",
      "__UINTMAX_TYPE__ unsigned long", "__CHAR16_TYPE__ unsigned short",
      "__CHAR32_TYPE__ unsigned int", "__ORDER_LITTLE_ENDIAN__ 1234",
      "__ORDER_BIG_ENDIAN__ 4321", "__ORDER_PDP_ENDIAN__ 3412",
      "__BYTE_ORDER__ __ORDER_LITTLE_ENDIAN__", "__FLOAT_WORD_ORDER__ __ORDER_LITTLE_ENDIAN__",
      "__USER_LABEL_PREFIX__ ", "__REGISTER_PREFIX__ ",
  };
  for (size_t i = 0; i < sizeof defs / sizeof *defs; i++)
    define_text(p, defs[i], startswith(defs[i], "__STDC"));
  add_builtin(p, "__FILE__", file_macro);
  add_builtin(p, "__LINE__", line_macro);
  add_builtin(p, "__COUNTER__", counter_macro);
  add_builtin(p, "__TIMESTAMP__", timestamp_macro);
  add_builtin(p, "__BASE_FILE__", base_file_macro);
  time_t now = time(NULL);
  char *sde = getenv("SOURCE_DATE_EPOCH");
  if (sde && *sde)
    now = (time_t)strtoll(sde, NULL, 10);
  struct tm *tm = localtime(&now);
  static const char mon[][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  define_text(p, format("__DATE__ \"%s %2d %d\"", mon[tm->tm_mon], tm->tm_mday, tm->tm_year + 1900),
              true);
  define_text(p, format("__TIME__ \"%02d:%02d:%02d\"", tm->tm_hour, tm->tm_min, tm->tm_sec),
              true);
}

/* ================================================================ the interface */

struct cpp *cpp_new(void) { return xcalloc(1, sizeof(struct cpp)); }

void cpp_define(struct cpp *p, const char *arg) {
  p->ops = xrealloc(p->ops, (p->nops + 1) * sizeof *p->ops);
  p->ops[p->nops++] = (struct defop){false, xstrdup(arg)};
}

void cpp_undef(struct cpp *p, const char *name) {
  p->ops = xrealloc(p->ops, (p->nops + 1) * sizeof *p->ops);
  p->ops[p->nops++] = (struct defop){true, xstrdup(name)};
}

void cpp_include_dir(struct cpp *p, const char *dir) { strarray_push(&p->idirs, xstrdup(dir)); }

void cpp_system_dir(struct cpp *p, const char *dir) { strarray_push(&p->sdirs, xstrdup(dir)); }

void cpp_set_hooks(struct cpp *p, const struct cpp_hooks *h) { p->hooks = *h; }

void cpp_set_quote_dir(struct cpp *p, const char *dir) {
  free(p->quotedir);
  p->quotedir = dir ? xstrdup(dir) : NULL;
}

void cpp_set_preprocessed(struct cpp *p, bool on) { p->preprocessed = on; }

void cpp_keep_pragmas(struct cpp *p, bool on) { p->keep_pragmas = on; }

const char *cpp_base_file(struct cpp *p) { return p->base_file; }

/* the usual system directories, those that are there */
static void default_system_dirs(struct cpp *p) {
  static const char *dirs[] = {
      "/usr/local/include",
#if defined __x86_64__ && defined __linux__
      "/usr/include/x86_64-linux-gnu",
#elif defined __i386__ && defined __linux__
      "/usr/include/i386-linux-gnu",
#elif defined __aarch64__ && defined __linux__
      "/usr/include/aarch64-linux-gnu",
#elif defined __arm__ && defined __linux__
      "/usr/include/arm-linux-gnueabihf",
#elif defined __riscv && defined __linux__
      "/usr/include/riscv64-linux-gnu",
#endif
      "/usr/include",
  };
  for (size_t i = 0; i < sizeof dirs / sizeof *dirs; i++) {
    struct stat st;
    if (stat(dirs[i], &st) == 0 && S_ISDIR(st.st_mode))
      cpp_system_dir(p, dirs[i]);
  }
}

struct token *cpp_run(struct cpp *p, const char *path, const char *display) {
  diag_hooks = &p->hooks;
  recover = NULL;
  jmp_buf jb;
  if (RECOVERABLE(p)) {
    if (setjmp(jb)) {  /* an error outside any directive or macro */
      recover = NULL;
      return NULL;
    }
    recover = &jb;
  }
  p->macros = (HashMap){0};
  p->pragma_once = (HashMap){0};
  p->guards = (HashMap){0};
  p->pushed = (HashMap){0};
  p->cond = NULL;
  p->depth = 0;
  p->counter = 0;
  p->base_file = xstrdup(display);
  if (!p->sdirs.len)
    default_system_dirs(p);
  predefine(p);
  for (int i = 0; i < p->nops; i++) {
    if (p->ops[i].undef) {
      hashmap_delete(&p->macros, p->ops[i].arg);
      continue;
    }
    char *a = xstrdup(p->ops[i].arg);
    char *e = strchr(a, '=');
    if (e)
      *e = ' ';
    define_text(p, e ? a : format("%s 1", a), false);
  }
  struct token *t = tokenize_file(path, display, -1, NULL);
  if (!t) {
    recover = NULL;
    return NULL;
  }
  if (p->quotedir)
    t->sf->quotedir = xstrdup(p->quotedir);
  t = preprocess2(p, t);
  if (p->cond)
    error_soft(p->cond->tok, "unterminated conditional directive");
  for (struct token *u = t; u; u = u->next)
    set_where(u);
  recover = NULL;
  return t;
}

/* ================================================================ -E */

/* would printing a and then b give other tokens? */
static bool would_paste(struct token *a, struct token *b) {
  if (cpp_is_pragma(a))
    return false;
  struct token *t = tokenize_buf(format("%.*s%.*s\n", a->len, a->loc, b->len, b->loc), NULL);
  return t->len != a->len;
}

/* the macros defined at the end of the last cpp_run, one "#define" line
   each (c17 -E -dM): object-like with their replacement lists,
   function-like with their parameters; the built-in ones (__FILE__,
   __LINE__, ...) with their names only */
void cpp_print_macros(struct cpp *p, FILE *out) {
  for (int i = 0; i < p->macros.capacity; i++) {
    HashEntry *ent = &p->macros.buckets[i];
    if (!ent->key || ent->key == TOMBSTONE)
      continue;
    Macro *m = ent->val;
    fprintf(out, "#define %s", m->name);
    if (!m->is_objlike) {
      fputc('(', out);
      for (MacroParam *a = m->params; a; a = a->next)
        fprintf(out, "%s%s", a->name, a->next || m->va_args_name ? "," : "");
      if (m->va_args_name)
        fprintf(out, "%s...", strcmp(m->va_args_name, "__VA_ARGS__") ? m->va_args_name : "");
      fputc(')', out);
    }
    bool first = true;
    for (struct token *t = m->body; t && t->kind != CPP_EOF; t = t->next) {
      if (first || t->space)
        fputc(' ', out);
      first = false;
      fwrite(t->loc, 1, t->len, out);
    }
    fputc('\n', out);
  }
}

void cpp_print(struct token *tok, FILE *out) {
  const char *cur_file = NULL;
  int cur_line = 0;
  bool line_start = true;
  struct token *prev = NULL;
  for (; tok->kind != CPP_EOF; tok = tok->next) {
    struct token *pos = tok;
    while (pos->origin)
      pos = pos->origin;
    const char *fname;
    int line;
    where(pos->sf, pos->line, &fname, &line);
    bool pragma = cpp_is_pragma(tok);
    if (!cur_file || strcmp(cur_file, fname) || line > cur_line + 8 || (pragma && line != cur_line)) {
      if (!line_start)
        fputc('\n', out);
      fprintf(out, "# %d \"", line);
      for (const char *c = fname; *c; c++) {
        if (*c == '"' || *c == '\\')
          fputc('\\', out);
        fputc(*c, out);
      }
      fprintf(out, "\"\n");
      cur_file = fname;
      cur_line = line;
      line_start = true;
    } else if (line > cur_line) {
      for (; cur_line < line; cur_line++)
        fputc('\n', out);
      line_start = true;
    }
    if (pragma) {
      if (!line_start)
        fputc('\n', out);
      fprintf(out, "%s\n", tok->text);
      cur_line++;
      line_start = true;
      prev = NULL;
      continue;
    }
    if (line_start) {
      if (tok == pos && tok->col > 1)
        fprintf(out, "%*s", tok->col - 1, "");
    } else if (tok->space || tok->bol || (prev && would_paste(prev, tok))) {
      fputc(' ', out);
    }
    fprintf(out, "%.*s", tok->len, tok->loc);
    line_start = false;
    prev = tok;
  }
  if (!line_start)
    fputc('\n', out);
}
