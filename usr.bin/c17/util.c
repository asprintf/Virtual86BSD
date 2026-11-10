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

/* util.c - strings, paths, diagnostics (memory, Buf and hash maps: cpp.c) */
#include "c17.h"

bool opt_w;
int nwarnings;
jmp_buf *error_jmp;

bool startswith(const char *p, const char *q) {
  return strncmp(p, q, strlen(q)) == 0;
}

bool endswith(const char *p, const char *q) {
  size_t len1 = strlen(p), len2 = strlen(q);
  return len1 >= len2 && !strcmp(p + len1 - len2, q);
}

char *path_dirname(const char *path) {
  const char *slash = strrchr(path, '/');
  if (!slash)
    return xstrdup(".");
  if (slash == path)
    return xstrdup("/");
  return xstrndup(path, slash - path);
}

char *path_basename(const char *path) {
  size_t n = strlen(path);
  while (n > 1 && path[n - 1] == '/')
    n--;
  const char *end = path + n;
  const char *p = end;
  while (p > path && p[-1] != '/')
    p--;
  return xstrndup(p, end - p);
}

/* Replace the extension of the basename of tmpl with extn. */
char *replace_extn(char *tmpl, char *extn) {
  char *filename = path_basename(tmpl);
  char *dot = strrchr(filename, '.');
  if (dot)
    *dot = '\0';
  return format("%s%s", filename, extn);
}

bool file_exists(const char *path) {
  struct stat st;
  return !stat(path, &st);
}

bool is_regular_file(const char *path) {
  struct stat st;
  return !stat(path, &st) && S_ISREG(st.st_mode);
}

/* ------------------------------------------------------ diagnostics */

void fatal(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fprintf(stderr, "%s: ", progname);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  va_end(ap);
  exit(1);
}

void internal_error(char *file, int line, char *msg) {
  fprintf(stderr, "%s: internal compiler error: %s (%s:%d)\n", progname, msg,
          file, line);
  exit(3);
}
/* Prints a diagnostic of the form
     foo.c:10:5: error: message
         x = y +;
                ^
   (the caret under loc, on the line of contents it is in) */
static void report_at(const char *filename, int line_no, int col_no, const char *contents,
                      const char *loc, const char *kind, const char *msg) {
  fprintf(stderr, "%s:%d:%d: %s: %s\n", filename, line_no, col_no, kind, msg);
  if (!contents || !loc)
    return;
  if (loc < contents || loc > contents + strlen(contents))
    return;
  const char *line = loc;
  while (contents < line && line[-1] != '\n')
    line--;
  const char *end = loc;
  while (*end && *end != '\n')
    end++;
  if (end - line > 400)
    return;
  fprintf(stderr, "  %.*s\n", (int)(end - line), line);
  int pos = display_width(line, loc - line) + 2;
  fprintf(stderr, "%*s^\n", pos, "");
}

/* For tokens produced by macro expansion, report the expansion site
   as the main location (what users usually want to see). */
static Token *report_tok(Token *tok) {
  while (tok && tok->origin)
    tok = tok->origin;
  return tok;
}

static void verror_tok(Token *tok, char *kind, char *fmt, va_list ap) {
  char *msg;
  va_list ap2;
  va_copy(ap2, ap);
  int n = vsnprintf(NULL, 0, fmt, ap);
  msg = xmalloc(n + 1);
  vsnprintf(msg, n + 1, fmt, ap2);
  va_end(ap2);
  Token *t = report_tok(tok);
  if (!t || !t->file)
    fprintf(stderr, "%s: %s: %s\n", progname, kind, msg);
  else
    report_at(tok_filename(t), tok_line(t), t->col_no, t->file->contents, t->loc, kind, msg);
  free(msg);
}

NORETURN static void bail(void) {
  if (error_jmp)
    longjmp(*error_jmp, 1);
  exit(1);
}

void error_tok(Token *tok, char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  verror_tok(tok, "error", fmt, ap);
  va_end(ap);
  bail();
}

/* Warnings are not issued for code in system headers (as GCC). */
static bool in_system_header(Token *tok) {
  Token *root = report_tok(tok);
  return (root && root->file && root->file->is_system) ||
         (tok && tok->file && tok->file->is_system);
}

void warn_tok(Token *tok, char *fmt, ...) {
  if (opt_w || in_system_header(tok))
    return;
  nwarnings++;
  va_list ap;
  va_start(ap, fmt);
  verror_tok(tok, "warning", fmt, ap);
  va_end(ap);
}

void note_tok(Token *tok, char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  verror_tok(tok, "note", fmt, ap);
  va_end(ap);
}

/* the preprocessor's diagnostics */
void cpp_diagnostic(void *arg, bool error, const struct token *t, const char *msg) {
  (void)arg;
  bool system = t && t->sf && t->sf->system;
  while (t && t->origin)
    t = t->origin;
  if (t && t->sf && t->sf->system)
    system = true;
  if (!error && (opt_w || system))
    return;
  if (!error)
    nwarnings++;
  const char *kind = error ? "error" : "warning";
  if (!t) {
    fprintf(stderr, "%s: %s: %s\n", progname, kind, msg);
  } else {
    const char *file;
    int line;
    cpp_where(t, &file, &line);
    report_at(file, line, t->col, t->sf ? t->sf->contents : NULL, t->loc, kind, msg);
  }
  if (error)
    bail();
}
