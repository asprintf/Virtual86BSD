#include <wordexp.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

/* The expansions are done by the shell, /bin/sh: words are checked here
 * for the characters the shell would treat as operators, and for command
 * substitution if WRDE_NOCMD; then
 *     sh -c 'eval "printf %s\\\\0 x $1"' sh WORDS
 * prints the fields separated by null bytes. */

extern char **__environ;

static int check(const char *s, int flags)
{
	int sq = 0, dq = 0, depth = 0, brace = 0;
	for (; *s; s++) {
		if (sq) { if (*s == '\'') sq = 0; continue; }
		switch (*s) {
		case '\\':
			if (!s[1]) return WRDE_SYNTAX;
			s++;
			break;
		case '\'':
			if (!dq) sq = 1;
			break;
		case '"':
			dq = !dq;
			break;
		case '`':
			if (flags & WRDE_NOCMD) return WRDE_CMDSUB;
			break;
		case '$':
			if (s[1] == '(' && s[2] != '(' && (flags & WRDE_NOCMD)) return WRDE_CMDSUB;
			if (s[1] == '(') { depth++; s++; }
			else if (s[1] == '{') { brace++; s++; }
			break;
		case '(':
			if (depth) depth++;
			else if (!dq) return WRDE_BADCHAR;
			break;
		case ')':
			if (depth) depth--;
			else if (!dq) return WRDE_BADCHAR;
			break;
		case '}':
			if (brace) brace--;
			else if (!dq) return WRDE_BADCHAR;
			break;
		case '\n': case '|': case '&': case ';': case '<': case '>': case '{':
			if (!dq && !depth && !brace) return WRDE_BADCHAR;
			break;
		}
	}
	return sq || dq || depth || brace ? WRDE_SYNTAX : 0;
}

void wordfree(wordexp_t *we)
{
	size_t i;
	if (!we->we_wordv) return;
	for (i = 0; i < we->we_wordc; i++) free(we->we_wordv[we->we_offs + i]);
	free(we->we_wordv);
	we->we_wordv = 0;
	we->we_wordc = 0;
}

int wordexp(const char *__RESTRICT s, wordexp_t *__RESTRICT we, int flags)
{
	int p[2], st, r, nul, e;
	pid_t pid;
	char *out = 0, *no, **v, *w;
	size_t n = 0, cap = 0, nw, i, offs, old;
	ssize_t k;
	const char *script = flags & WRDE_UNDEF ? "set -u; eval \"printf %s\\\\\\\\0 x $1\"" : "eval \"printf %s\\\\\\\\0 x $1\"";

	if (flags & WRDE_REUSE) wordfree(we);
	if (!(flags & WRDE_APPEND)) {
		we->we_wordc = 0;
		we->we_wordv = 0;
		if (!(flags & WRDE_DOOFFS)) we->we_offs = 0;
	}
	if ((r = check(s, flags))) return r;
	if (pipe2(p, O_CLOEXEC) < 0) return WRDE_NOSPACE;
	pid = fork();
	if (pid < 0) { close(p[0]); close(p[1]); return WRDE_NOSPACE; }
	if (!pid) {
		if (p[1] != 1) { dup2(p[1], 1); } else fcntl(1, F_SETFD, 0);
		if (!(flags & WRDE_SHOWERR)) {
			int fd = open("/dev/null", O_WRONLY);
			if (fd >= 0 && fd != 2) { dup2(fd, 2); close(fd); }
		}
		execl("/bin/sh", "sh", "-c", script, "sh", s, (char *)0);
		_exit(127);
	}
	close(p[1]);
	for (;;) {
		if (cap - n < 4096) {
			cap = cap ? 2 * cap : 8192;
			if (!(no = realloc(out, cap))) { free(out); out = 0; break; }
			out = no;
		}
		k = read(p[0], out + n, cap - n);
		if (k < 0 && errno == EINTR) continue;
		if (k <= 0) break;
		n += (size_t)k;
	}
	close(p[0]);
	while (waitpid(pid, &st, 0) < 0 && errno == EINTR);
	if (!out) return WRDE_NOSPACE;
	if (!WIFEXITED(st) || WEXITSTATUS(st) || n < 2 || memcmp(out, "x", 2)) {
		free(out);
		return flags & WRDE_UNDEF ? WRDE_BADVAL : WRDE_SYNTAX;
	}
	for (nw = 0, i = 2; i < n; i++) nw += !out[i];
	offs = flags & WRDE_DOOFFS ? we->we_offs : 0;
	old = we->we_wordv ? we->we_wordc : 0;
	if (!(v = realloc(we->we_wordv, (offs + old + nw + 1) * sizeof *v))) { free(out); return WRDE_NOSPACE; }
	if (!we->we_wordv) for (i = 0; i < offs; i++) v[i] = 0;
	we->we_wordv = v;
	e = 0;
	for (i = 2, nul = 0; i < n; i = (size_t)nul + 1) {
		size_t l = strlen(out + i);
		nul = (int)(i + l);
		if (!(w = malloc(l + 1))) { e = WRDE_NOSPACE; break; }
		memcpy(w, out + i, l + 1);
		v[offs + we->we_wordc++] = w;
	}
	v[offs + we->we_wordc] = 0;
	free(out);
	return e;
}
