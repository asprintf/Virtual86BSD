/* tokenize.c - c17's tokens: from the preprocessor's (cpp.c), with the
   literal conversion of translation phases 5 to 7 */
#include "c17.h"

char *tok_text(Token *tok) { return xstrndup(tok->loc, tok->len); }

/* Presumed line number (6.10.4) */
int tok_line(Token *tok) { return tok->line_no; }

/* Presumed source file name */
char *tok_filename(Token *tok) { return tok->filename ? tok->filename : "<built-in>"; }

bool equal(Token *tok, char *op) {
  if (tok->kind == TK_PUNCT)
    return !strcmp(tok->punct, op);
  if (tok->kind == TK_STR || tok->kind == TK_EOF)
    return false;
  return (int)strlen(op) == tok->len && memcmp(tok->loc, op, tok->len) == 0;
}

Token *skip(Token *tok, char *op) {
  if (!equal(tok, op)) {
    if (tok->kind == TK_EOF)
      error_tok(tok, "expected '%s' at end of input", op);
    error_tok(tok, "expected '%s' before '%s'", op, tok_text(tok));
  }
  return tok->next;
}

bool consume(Token **rest, Token *tok, char *str) {
  if (equal(tok, str)) {
    *rest = tok->next;
    return true;
  }
  *rest = tok;
  return false;
}

static int from_hex(char c) {
  if ('0' <= c && c <= '9')
    return c - '0';
  if ('a' <= c && c <= 'f')
    return c - 'a' + 10;
  return c - 'A' + 10;
}

/* Reads \uXXXX or \UXXXXXXXX at p (pointing at the backslash). */
static uint32_t read_ucn(char **rest, char *p, Token *errtok, bool *ok) {
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
  (void)errtok;
  return c;
}

/* ------------------------------------------ literal conversion (phase 5-7) */

/* A decoded literal element: either a character (code point) or a raw
   value from a numeric escape. */
typedef struct {
  uint32_t v;
  bool raw;
} Elem;

typedef struct {
  Elem *e;
  int len, cap;
} ElemVec;

static void ev_push(ElemVec *ev, uint32_t v, bool raw) {
  if (ev->len == ev->cap) {
    ev->cap = ev->cap ? ev->cap * 2 : 32;
    ev->e = xrealloc(ev->e, sizeof(Elem) * ev->cap);
  }
  ev->e[ev->len].v = v;
  ev->e[ev->len].raw = raw;
  ev->len++;
}

/* Decodes the body of a literal between quotes. */
static void decode_literal(Token *tok, char *p, char *end, ElemVec *ev, uint64_t maxval) {
  while (p < end) {
    if (*p != '\\') {
      uint32_t c = decode_utf8(&p, p);
      ev_push(ev, c, false);
      continue;
    }
    char *start = p;
    p++;
    switch (*p) {
    case '\'': case '"': case '?': case '\\':
      ev_push(ev, *p++, false);
      continue;
    case 'a': p++; ev_push(ev, 7, false); continue;
    case 'b': p++; ev_push(ev, 8, false); continue;
    case 'f': p++; ev_push(ev, 12, false); continue;
    case 'n': p++; ev_push(ev, 10, false); continue;
    case 'r': p++; ev_push(ev, 13, false); continue;
    case 't': p++; ev_push(ev, 9, false); continue;
    case 'v': p++; ev_push(ev, 11, false); continue;
    case 'e': p++; ev_push(ev, 27, false); continue; /* common extension */
    case 'u': case 'U': {
      bool ok;
      uint32_t c = read_ucn(&p, start, tok, &ok);
      if (!ok)
        error_tok(tok, "incomplete universal character name");
      if ((c < 0xA0 && c != '$' && c != '@' && c != '`') || (c >= 0xD800 && c <= 0xDFFF) ||
          c > 0x10FFFF)
        error_tok(tok, "\\%c%0*X is not a valid universal character",
                  start[1], start[1] == 'u' ? 4 : 8, c);
      ev_push(ev, c, false);
      continue;
    }
    case 'x': {
      p++;
      if (!isxdigit((unsigned char)*p))
        error_tok(tok, "\\x used with no following hex digits");
      uint64_t c = 0;
      bool overflow = false;
      for (; isxdigit((unsigned char)*p); p++) {
        if (c >> 60)
          overflow = true;
        c = (c << 4) | from_hex(*p);
      }
      if (overflow || c > maxval) {
        warn_tok(tok, "hex escape sequence out of range");
        c &= maxval;
      }
      ev_push(ev, (uint32_t)c, true);
      continue;
    }
    default:
      if ('0' <= *p && *p <= '7') {
        uint32_t c = *p++ - '0';
        if ('0' <= *p && *p <= '7') {
          c = (c << 3) + (*p++ - '0');
          if ('0' <= *p && *p <= '7')
            c = (c << 3) + (*p++ - '0');
        }
        if (c > maxval) {
          warn_tok(tok, "octal escape sequence out of range");
          c &= maxval;
        }
        ev_push(ev, c, true);
        continue;
      }
      /* Unknown escape: undefined; diagnose and take the char. */
      warn_tok(tok, "unknown escape sequence '\\%c'", *p);
      ev_push(ev, (unsigned char)*p++, false);
      continue;
    }
  }
}

typedef enum { ENC_NONE, ENC_U8, ENC_U16, ENC_U32, ENC_WIDE } Encoding;

static Encoding literal_prefix(Token *tok, int *plen) {
  char *p = tok->loc;
  if (startswith(p, "u8")) { *plen = 2; return ENC_U8; }
  if (*p == 'u') { *plen = 1; return ENC_U16; }
  if (*p == 'U') { *plen = 1; return ENC_U32; }
  if (*p == 'L') { *plen = 1; return ENC_WIDE; }
  *plen = 0;
  return ENC_NONE;
}

static Type *enc_type(Encoding enc) {
  switch (enc) {
  case ENC_U16: return ty_char16;
  case ENC_U32: return ty_char32;
  case ENC_WIDE: return ty_wchar;
  default: return ty_char;
  }
}

/* Encodes elements into code units of the given width. */
static void encode_elems(ElemVec *ev, Encoding enc, Buf *out, int *nunits) {
  int n = 0;
  for (int i = 0; i < ev->len; i++) {
    uint32_t v = ev->e[i].v;
    if (enc == ENC_NONE || enc == ENC_U8) {
      if (ev->e[i].raw) {
        buf_putc(out, (char)v);
        n++;
      } else {
        char tmp[4];
        int len = encode_utf8(tmp, v);
        buf_write(out, tmp, len);
        n += len;
      }
    } else if (enc == ENC_U16) {
      if (!ev->e[i].raw && v >= 0x10000) {
        v -= 0x10000;
        uint16_t hi = 0xd800 + ((v >> 10) & 0x3ff);
        uint16_t lo = 0xdc00 + (v & 0x3ff);
        buf_write(out, (char *)&hi, 2);
        buf_write(out, (char *)&lo, 2);
        n += 2;
      } else {
        uint16_t u = v;
        buf_write(out, (char *)&u, 2);
        n++;
      }
    } else {
      uint32_t u = v;
      buf_write(out, (char *)&u, 4);
      n++;
    }
  }
  *nunits = n;
}

/* Re-decodes the raw bytes of a narrow string as UTF-8 when it is
   concatenated with a wide one. */
static void decode_string_token(Token *tok, ElemVec *ev, Encoding enc) {
  int plen;
  literal_prefix(tok, &plen);
  uint64_t maxval;
  switch (enc) {
  case ENC_U16: maxval = 0xffff; break;
  case ENC_U32: case ENC_WIDE: maxval = 0xffffffff; break;
  default: maxval = 0xff; break;
  }
  decode_literal(tok, tok->loc + plen + 1, tok->loc + tok->len - 1, ev, maxval);
}

/* Converts a sequence of adjacent string literal tokens beginning at tok
   into one TK_STR token (phase 6).  Returns the last token consumed. */
static Token *join_strings(Token *tok) {
  Encoding enc = ENC_NONE;
  Token *t = tok;
  for (; t->kind == TK_STR; t = t->next) {
    int plen;
    Encoding e = literal_prefix(t, &plen);
    if (e == ENC_NONE)
      continue;
    if (enc != ENC_NONE && enc != e)
      error_tok(t, "unsupported non-standard concatenation of string literals");
    enc = e;
  }
  Token *last = tok;
  ElemVec ev = {0};
  for (t = tok; t->kind == TK_STR; t = t->next) {
    decode_string_token(t, &ev, enc);
    last = t;
  }
  Buf b = {0};
  int n;
  encode_elems(&ev, enc, &b, &n);
  Type *base = enc_type(enc);
  int unit = base->size;
  for (int i = 0; i < unit; i++)
    buf_putc(&b, 0);
  tok->str = b.buf ? b.buf : xcalloc(1, 4);
  tok->str_len = (n + 1) * unit;
  tok->ty = array_of(base, n + 1);
  tok->next = last->next;
  return tok;
}

static void convert_char(Token *tok) {
  int plen;
  Encoding enc = literal_prefix(tok, &plen);
  char *start = tok->loc + plen + 1;
  char *end = tok->loc + tok->len - 1;
  if (start == end)
    error_tok(tok, "empty character constant");
  ElemVec ev = {0};
  uint64_t maxval = enc == ENC_NONE ? 0xff : enc == ENC_U16 ? 0xffff : 0xffffffff;
  decode_literal(tok, start, end, &ev, maxval);

  if (enc == ENC_NONE) {
    /* Each element contributes its UTF-8 bytes (or raw byte). */
    Buf b = {0};
    int n;
    encode_elems(&ev, ENC_NONE, &b, &n);
    if (n == 1) {
      tok->val = (signed char)b.buf[0];
    } else {
      if (n > 4)
        warn_tok(tok, "character constant too long for its type");
      else
        warn_tok(tok, "multi-character character constant");
      uint32_t v = 0;
      for (int i = 0; i < n; i++)
        v = (v << 8) | (unsigned char)b.buf[i];
      tok->val = (int32_t)v;
    }
    tok->ty = ty_int;
  } else {
    if (ev.len > 1)
      warn_tok(tok, "character constant too long for its type");
    uint32_t v = ev.e[ev.len - 1].v;
    if (enc == ENC_U16) {
      if (!ev.e[ev.len - 1].raw && v > 0xffff)
        warn_tok(tok, "character not encodable in a single code unit");
      tok->val = (uint16_t)v;
      tok->ty = ty_char16;
    } else if (enc == ENC_U32) {
      tok->val = v;
      tok->ty = ty_char32;
    } else {
      tok->val = (int32_t)v;
      tok->ty = ty_wchar;
    }
  }
  tok->kind = TK_NUM;
}

/* Integer constants (6.4.4.1). */
static bool convert_pp_int(Token *tok) {
  char *p = tok->loc;
  char *end = tok->loc + tok->len;
  int base = 10;
  if (!strncasecmp(p, "0x", 2) && isxdigit((unsigned char)p[2])) {
    p += 2;
    base = 16;
  } else if (!strncasecmp(p, "0b", 2) && (p[2] == '0' || p[2] == '1')) {
    p += 2;
    base = 2;
  } else if (*p == '0') {
    base = 8;
  }

  uint64_t val = 0;
  bool overflow = false;
  char *digits = p;
  for (; p < end; p++) {
    int d;
    if (isdigit((unsigned char)*p))
      d = *p - '0';
    else if (base == 16 && isxdigit((unsigned char)*p))
      d = from_hex(*p);
    else
      break;
    if (d >= base)
      return false;
    if (val > (UINT64_MAX - d) / base)
      overflow = true;
    val = val * base + d;
  }
  if (p == digits && base != 8)
    return false;

  /* Suffix */
  bool l = false, ll = false, u = false;
  int slen = end - p;
  char s[8] = {0};
  if (slen > 3)
    return false;
  memcpy(s, p, slen);
  if (slen == 0) {
  } else if (!strcmp(s, "u") || !strcmp(s, "U")) {
    u = true;
  } else if (!strcmp(s, "l") || !strcmp(s, "L")) {
    l = true;
  } else if (!strcmp(s, "ll") || !strcmp(s, "LL")) {
    ll = true;
  } else if (!strcasecmp(s, "ul") || !strcasecmp(s, "lu")) {
    u = l = true;
  } else if (!strcmp(s, "ull") || !strcmp(s, "uLL") || !strcmp(s, "Ull") ||
             !strcmp(s, "ULL") || !strcmp(s, "llu") || !strcmp(s, "llU") ||
             !strcmp(s, "LLu") || !strcmp(s, "LLU")) {
    u = ll = true;
  } else {
    return false;
  }

  if (base == 2)
    warn_tok(tok, "binary integer constants are an extension");
  if (overflow)
    error_tok(tok, "integer constant is too large for any type");

  Type *ty;
  if (base == 10) {
    if (u) {
      if (ll)
        ty = ty_ullong;
      else if (l)
        ty = val >> 63 ? ty_ulong : ty_ulong;
      else
        ty = val >> 32 ? ty_ulong : ty_uint;
    } else {
      if (ll)
        ty = ty_llong;
      else if (l)
        ty = ty_long;
      else
        ty = val > INT32_MAX ? ty_long : ty_int;
      if (val >> 63) {
        warn_tok(tok, "integer constant is so large that it is unsigned");
        ty = ll ? ty_ullong : ty_ulong;
      }
    }
  } else {
    if (ll)
      ty = u || (val >> 63) ? ty_ullong : ty_llong;
    else if (l)
      ty = u || (val >> 63) ? ty_ulong : ty_long;
    else if (u)
      ty = val >> 32 ? ty_ulong : ty_uint;
    else if (val >> 63)
      ty = ty_ulong;
    else if (val >> 32)
      ty = ty_long;
    else if (val >> 31)
      ty = ty_uint;
    else
      ty = ty_int;
  }
  tok->kind = TK_NUM;
  tok->val = (int64_t)val;
  tok->ty = ty;
  return true;
}

extern Type *imaginary_type_hook(Type *ty);

static void convert_pp_number(Token *tok) {
  if (convert_pp_int(tok))
    return;

  /* Floating constant (6.4.4.2) */
  char *s = tok_text(tok);
  int n = strlen(s);
  bool imag = false;
  /* GNU imaginary suffix i/j in any position among the suffix letters. */
  for (int k = 0; k < 2 && n > 0; k++) {
    char c = s[n - 1];
    if (c == 'i' || c == 'j' || c == 'I' || c == 'J') {
      if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X') && k == 0 &&
          !strpbrk(s, "pP")) /* hex digit? */
        break;
      imag = true;
      s[--n] = '\0';
      continue;
    }
    if (strchr("fFlL", c) && n >= 2 && strchr("ijIJ", s[n - 2]) &&
        !(s[0] == '0' && (s[1] == 'x' || s[1] == 'X') && !strpbrk(s, "pP"))) {
      imag = true;
      memmove(s + n - 2, s + n - 1, 2);
      n--;
    }
    break;
  }
  if (imag) {
    /* Try as an integer first (e.g. 1i). */
    Token t = *tok;
    t.loc = s;
    t.len = n;
    if (convert_pp_int(&t)) {
      tok->kind = TK_NUM;
      tok->fval = t.ty->is_unsigned ? (long double)(uint64_t)t.val : (long double)t.val;
      tok->ty = imaginary_type_hook(ty_double);
      return;
    }
  }

  char suffix = n > 0 ? s[n - 1] : 0;
  bool is_hex = s[0] == '0' && (s[1] == 'x' || s[1] == 'X');
  Type *ty = ty_double;
  if (suffix == 'f' || suffix == 'F') {
    if (is_hex && !strpbrk(s, "pP"))
      goto invalid;
    ty = ty_float;
    s[--n] = '\0';
  } else if (suffix == 'l' || suffix == 'L') {
    ty = ty_ldouble;
    s[--n] = '\0';
  }
  /* Validate the form. */
  {
    char *p = s;
    if (is_hex) {
      p += 2;
      bool digits = false;
      while (isxdigit((unsigned char)*p)) p++, digits = true;
      if (*p == '.') {
        p++;
        while (isxdigit((unsigned char)*p)) p++, digits = true;
      }
      if (!digits || (*p != 'p' && *p != 'P'))
        goto invalid;
      p++;
      if (*p == '+' || *p == '-')
        p++;
      if (!isdigit((unsigned char)*p))
        goto invalid;
      while (isdigit((unsigned char)*p)) p++;
    } else {
      bool digits = false;
      while (isdigit((unsigned char)*p)) p++, digits = true;
      bool frac = false;
      if (*p == '.') {
        p++;
        frac = true;
        while (isdigit((unsigned char)*p)) p++, digits = true;
      }
      if (!digits)
        goto invalid;
      bool expo = false;
      if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-')
          p++;
        if (!isdigit((unsigned char)*p))
          goto invalid;
        while (isdigit((unsigned char)*p)) p++;
        expo = true;
      }
      if (!frac && !expo)
        goto invalid;
    }
    if (*p)
      goto invalid;
  }

  long double val;
  errno = 0;
  if (ty == ty_float)
    val = strtof(s, NULL);
  else if (ty == ty_double)
    val = strtod(s, NULL);
  else
    val = strtold(s, NULL);
  if (errno == ERANGE && (val == HUGE_VALL || val == -HUGE_VALL || val == HUGE_VAL ||
                          val == HUGE_VALF))
    warn_tok(tok, "floating constant exceeds range of '%s'", type_name(ty));

  tok->kind = TK_NUM;
  tok->fval = val;
  tok->ty = imag ? imaginary_type_hook(ty) : ty;
  return;

invalid:
  error_tok(tok, "invalid numeric constant '%.*s'", tok->len, tok->loc);
}

static char *keywords[] = {
    "auto", "break", "case", "char", "const", "continue", "default", "do",
    "double", "else", "enum", "extern", "float", "for", "goto", "if", "inline",
    "int", "long", "register", "restrict", "return", "short", "signed",
    "sizeof", "static", "struct", "switch", "typedef", "union", "unsigned",
    "void", "volatile", "while", "_Alignas", "_Alignof", "_Atomic", "_Bool",
    "_Complex", "_Generic", "_Imaginary", "_Noreturn", "_Static_assert",
    "_Thread_local",
    /* Extensions (reserved identifiers only) */
    "__attribute__", "__attribute", "__typeof__", "__typeof", "__asm__",
    "__asm", "__inline", "__inline__", "__restrict", "__restrict__",
    "__signed", "__signed__", "__const", "__const__", "__volatile",
    "__volatile__", "__extension__", "__alignof__", "__alignof",
    "__builtin_va_start", "__builtin_va_arg", "__builtin_va_end",
    "__builtin_va_copy", "__builtin_offsetof", "__builtin_va_list",
    "__real__", "__real", "__imag__", "__imag", "__thread",
    "__builtin_types_compatible_p", "__builtin_complex",
};

static HashMap keyword_map;

static bool is_keyword(Token *tok) {
  if (!keyword_map.capacity)
    for (int i = 0; i < (int)(sizeof(keywords) / sizeof(*keywords)); i++)
      hashmap_put(&keyword_map, keywords[i], (void *)1);
  return hashmap_get2(&keyword_map, tok->loc, tok->len);
}

/* Converts an identifier spelled with UCNs to its UTF-8 form. */
static void normalize_ident(Token *tok) {
  if (!memchr(tok->loc, '\\', tok->len))
    return;
  Buf b = {0};
  char *p = tok->loc, *end = tok->loc + tok->len;
  while (p < end) {
    if (*p == '\\') {
      bool ok;
      uint32_t c = read_ucn(&p, p, tok, &ok);
      char tmp[4];
      buf_write(&b, tmp, encode_utf8(tmp, c));
    } else {
      buf_putc(&b, *p++);
    }
  }
  tok->loc = b.buf;
  tok->len = b.len;
}

/* Phase 7 preparation: converts preprocessing tokens into tokens. */
static void convert_pp_tokens(Token *tok) {
  for (Token *t = tok; t->kind != TK_EOF; t = t->next) {
    switch (t->kind) {
    case TK_IDENT:
      normalize_ident(t);
      if (is_keyword(t))
        t->kind = TK_KEYWORD;
      break;
    case TK_PP_NUM:
      convert_pp_number(t);
      break;
    case TK_CHAR:
      convert_char(t);
      break;
    case TK_STR:
      join_strings(t);
      break;
    case TK_OTHER:
      if (*t->loc == '\'' || *t->loc == '"')
        error_tok(t, "missing terminating %c character", *t->loc);
      error_tok(t, "stray '%.*s' in program", t->len, t->loc);
    default:
      break;
    }
  }
}

/* For the preprocessor's #include "..." and _Pragma: decodes the
   characters of a plain string literal token. */
Token *tokenize_string_literal(Token *tok, Type *basety) {
  (void)basety;
  Token *t = xcalloc(1, sizeof(Token));
  *t = *tok;
  t->next = NULL;
  ElemVec ev = {0};
  decode_string_token(tok, &ev, ENC_NONE);
  Buf b = {0};
  int n;
  encode_elems(&ev, ENC_NONE, &b, &n);
  buf_putc(&b, 0);
  t->str = b.buf;
  t->str_len = n + 1;
  return t;
}

/* ------------------------------------------ from the preprocessor's tokens */

static HashMap files;      /* struct srcfile -> File */
static HashMap converted;  /* struct token -> Token, for origins */

static File *file_of(struct srcfile *sf) {
  if (!sf)
    return NULL;
  File *f = hashmap_get2(&files, (char *)&sf, sizeof sf);
  if (!f) {
    f = xcalloc(1, sizeof *f);
    f->name = sf->display;
    f->contents = sf->contents;
    f->is_system = sf->system;
    hashmap_put2(&files, xstrndup((char *)&sf, sizeof sf), sizeof sf, f);
  }
  return f;
}

static Token *convert1(struct token *t) {
  static const TokenKind kinds[] = {
      [CPP_IDENT] = TK_IDENT, [CPP_PUNCT] = TK_PUNCT, [CPP_NUM] = TK_PP_NUM,
      [CPP_CHAR] = TK_CHAR,   [CPP_STR] = TK_STR,     [CPP_OTHER] = TK_OTHER,
      [CPP_EOF] = TK_EOF,
  };
  Token *tok = xcalloc(1, sizeof *tok);
  tok->kind = kinds[t->kind];
  tok->loc = t->loc;
  tok->len = t->len;
  if (t->kind == CPP_PUNCT)
    tok->punct = t->text;
  tok->file = file_of(t->sf);
  const char *name;
  cpp_where(t, &name, &tok->line_no);
  tok->filename = (char *)name;
  tok->col_no = t->col;
  tok->at_bol = t->bol;
  tok->has_space = t->space;
  if (t->origin) {
    struct token *o = t->origin;
    Token *ot = hashmap_get2(&converted, (char *)&o, sizeof o);
    if (!ot) {
      ot = convert1(o);
      hashmap_put2(&converted, xstrndup((char *)&o, sizeof o), sizeof o, ot);
    }
    tok->origin = ot;
  }
  return tok;
}

Token *convert_tokens(struct token *t) {
  Token head = {0}, *cur = &head;
  for (; t; t = t->next)
    cur = cur->next = convert1(t);
  convert_pp_tokens(head.next);
  return head.next;
}
