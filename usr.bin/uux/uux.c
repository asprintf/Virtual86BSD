/*
 * uux.c - uux: remote command execution (POSIX.1-2024, XCU uux)
 *
 * Synopsis: uux [-jnp] command-string...
 *
 * The UUCP system is that of uucomm.c.  The command string (the
 * operands, joined by spaces) is a pipeline: words (with ' " and \
 * quoting), "|", "<" and ">"; ">>", "<<", ">|", ">&", ";", "&", "&&",
 * "||" are not accepted.  The first command may be system!command: it
 * runs there (!command, or no system: here); the others run there too.
 *
 * Files: an unquoted word system!path (!path: here) whose system is
 * known (with another name before "!", it is a word as it is); the word after "<" or ">" (a path without a system: here, made
 * absolute with the current directory); a word in parentheses,
 * (system!path), an output file.  A file on the system where the command
 * runs is given to it by its path (~user and ~/ expanded there); a file
 * of another system is copied to the directory where the command runs
 * and given by its last name (two of the same name are an error); an
 * output file elsewhere (> or ( )) is made there by that name and sent
 * where it goes after the command.  Other words are as they are.
 *
 * The command runs with sh in a directory of the spool of its system;
 * another system runs only the commands of its configuration's
 * "commands" (rmail and rnews by default), with files in its public
 * directory (and "read", "write").  Its standard output goes where ">"
 * says, or is discarded; another system's standard error comes here.
 *
 * -j: the job's id written.  -n: no mail when the command fails (the
 * user gets mail when it cannot be run, or exits with a status other
 * than 0, otherwise).  -p (or "-" as the first operand): the standard
 * input is the command's.
 *
 * Exit status: 0 when the command was run (whatever its status); 1 when
 * it could not be (a diagnostic: a system that cannot be reached leaves
 * the job queued); 2 for usage errors.
 */

#define _POSIX_C_SOURCE 202405L

#include "uucomm.h"

#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct tok {
    char *text;
    bool quoted, paren;
    int op;                     /* 0, '|', '<', '>' */
};

static struct tok *toks;
static size_t ntoks;

static void fail(const char *fmt, const char *arg)
{
    fprintf(stderr, "uux: ");
    fprintf(stderr, fmt, arg);
    fputc('\n', stderr);
    exit(1);
}

static void add_tok(char *text, bool quoted, bool paren, int op)
{
    toks = xrealloc(toks, (ntoks + 1) * sizeof *toks);
    toks[ntoks++] = (struct tok){ text, quoted, paren, op };
}

static void lex(const char *s)
{
    while (*s) {
        if (*s == ' ' || *s == '\t' || *s == '\n') {
            s++;
            continue;
        }
        if (*s == '|') {
            if (s[1] == '|')
                fail("%s: only a pipeline can be run", "||");
            add_tok(xstrdup("|"), false, false, '|');
            s++;
            continue;
        }
        if (*s == '<') {
            if (s[1] == '<')
                fail("%s: not accepted", "<<");
            add_tok(xstrdup("<"), false, false, '<');
            s++;
            continue;
        }
        if (*s == '>') {
            if (s[1] == '>' || s[1] == '|' || s[1] == '&') {
                char op[3] = { '>', s[1], 0 };
                fail("%s: not accepted", op);
            }
            add_tok(xstrdup(">"), false, false, '>');
            s++;
            continue;
        }
        if (*s == ';' || *s == '&') {
            char op[2] = { *s, 0 };
            fail("%s: only a pipeline can be run", op);
        }
        if (*s == '(') {
            const char *e = strchr(s, ')');
            if (!e)
                fail("%s: no \")\"", s);
            size_t n = (size_t)(e - s - 1);
            char *t = xmalloc(n + 1);
            memcpy(t, s + 1, n);
            t[n] = '\0';
            add_tok(t, false, true, 0);
            s = e + 1;
            continue;
        }
        /* a word */
        size_t cap = 16, n = 0;
        char *t = xmalloc(cap);
        bool quoted = false;
        while (*s && !strchr(" \t\n|<>;&()", *s)) {
            if (n + 4 > cap) {
                cap *= 2;
                t = xrealloc(t, cap);
            }
            if (*s == '\\') {
                quoted = true;
                if (s[1])
                    t[n++] = s[1];
                s += s[1] ? 2 : 1;
            } else if (*s == '\'') {
                quoted = true;
                const char *e = strchr(s + 1, '\'');
                if (!e)
                    fail("%s: no closing quote", s);
                size_t l = (size_t)(e - s - 1);
                if (n + l + 4 > cap) {
                    cap = (n + l + 4) * 2;
                    t = xrealloc(t, cap);
                }
                memcpy(t + n, s + 1, l);
                n += l;
                s = e + 1;
            } else if (*s == '"') {
                quoted = true;
                s++;
                while (*s && *s != '"') {
                    if (n + 4 > cap) {
                        cap *= 2;
                        t = xrealloc(t, cap);
                    }
                    if (*s == '\\' && s[1] && strchr("\"\\$`\n", s[1]))
                        s++;
                    t[n++] = *s++;
                }
                if (*s != '"')
                    fail("%s", "no closing quote");
                s++;
            } else {
                t[n++] = *s++;
            }
        }
        t[n] = '\0';
        add_tok(t, quoted, false, 0);
    }
}

static struct uucmd cmd;
static const char *xsys;        /* where it runs: NULL here */

static void add_word(int kind, const char *text, const char *sys, const char *path)
{
    cmd.w = xrealloc(cmd.w, (cmd.n + 1) * sizeof *cmd.w);
    cmd.w[cmd.n++] = (struct uuword){ kind, xstrdup(text), sys ? xstrdup(sys) : NULL, path ? xstrdup(path) : NULL };
}

static bool same_sys(const char *a, const char *b)
{
    if (uu_is_local(a) || uu_is_local(b))
        return uu_is_local(a) && uu_is_local(b);
    return strcmp(a, b) == 0;
}

static const char *last_name(const char *path)
{
    const char *s = strrchr(path, '/');
    return s && s[1] ? s + 1 : s ? path : path;
}

/* a file: on the system that runs (a path), or to copy there or back */
static void file_word(const char *sys, const char *path, bool output)
{
    if (!*path)
        fail("%s: no file name", sys ? sys : "");
    char *abs = uu_absolute(path);
    if (same_sys(sys, xsys)) {
        add_word(W_PATH, abs, NULL, NULL);
    } else {
        const char *name = last_name(abs);
        for (size_t i = 0; i < cmd.n; i++)
            if ((cmd.w[i].kind == W_GATHER || cmd.w[i].kind == W_OUT) && strcmp(cmd.w[i].text, name) == 0)
                fail("%s: two files of the same name", name);
        add_word(output ? W_OUT : W_GATHER, name, uu_is_local(sys) ? NULL : sys, abs);
    }
    free(abs);
}

/* is this word sys!path with a known system (or none)? */
static bool is_file(const char *w, char **sys, char **path)
{
    if (!strchr(w, '!'))
        return false;
    uu_split(w, sys, path);
    /* another system that is not known: not a file (a mail address) */
    return !**sys || uu_known(*sys);
}

static void usage(void)
{
    fputs("usage: uux [-jnp] command-string...\n", stderr);
    exit(2);
}

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");
    progname = "uux";
    bool o_j = false, o_n = false, o_p = false;
    int c;
    while ((c = getopt(argc, argv, "jnp")) != -1) {
        switch (c) {
        case 'j': o_j = true; break;
        case 'n': o_n = true; break;
        case 'p': o_p = true; break;
        default: usage();
        }
    }
    if (optind < argc && strcmp(argv[optind], "-") == 0) {
        o_p = true;
        optind++;
    }
    if (optind >= argc)
        usage();
    if (!uu_config())
        return 1;
    size_t len = 1;
    for (int i = optind; i < argc; i++)
        len += strlen(argv[i]) + 1;
    char *line = xmalloc(len);
    line[0] = '\0';
    for (int i = optind; i < argc; i++) {
        if (i > optind)
            strcat(line, " ");
        strcat(line, argv[i]);
    }
    lex(line);
    if (ntoks == 0 || toks[0].op)
        fail("%s: no command", line);
    /* the system it runs on */
    char *first = toks[0].text;
    if (!toks[0].quoted && strchr(first, '!')) {
        char *s, *p;
        uu_split(first, &s, &p);
        if (!*p)
            fail("%s: no command", first);
        if (*s && !uu_known(s))
            fail("%s: unknown system", s);
        xsys = uu_is_local(s) ? NULL : s;
        toks[0].text = p;
    }
    bool start = true;
    const char *remote = xsys;
    for (size_t i = 0; i < ntoks; i++) {
        struct tok *t = &toks[i];
        if (t->op) {
            if (start)
                fail("%s: no command", t->text);
            add_word(W_OP, t->text, NULL, NULL);
            if (t->op == '|') {
                start = true;
                continue;
            }
            if (i + 1 >= ntoks || toks[i + 1].op || toks[i + 1].paren)
                fail("%s: no file name", t->text);
            struct tok *f = &toks[++i];
            char *s = NULL, *p;
            if (!f->quoted && strchr(f->text, '!')) {
                uu_split(f->text, &s, &p);
                if (*s && !uu_known(s))
                    fail("%s: unknown system", s);
            } else {
                p = xstrdup(f->text);
            }
            if (!remote && !uu_is_local(s))
                remote = s;
            file_word(s, p, t->op == '>');
            continue;
        }
        char *s, *p;
        if (start) {
            if (i > 0 && !t->quoted && strchr(t->text, '!'))
                fail("%s: only the first command can have a system name", t->text);
            if (!*t->text)
                fail("%s", "no command");
            add_word(W_LIT, t->text, NULL, NULL);
            start = false;
        } else if (t->paren) {
            if (strchr(t->text, '!')) {
                uu_split(t->text, &s, &p);
                if (*s && !uu_known(s))
                    fail("%s: unknown system", s);
                if (!remote && !uu_is_local(s))
                    remote = s;
                file_word(s, p, true);
            } else {
                add_word(W_LIT, t->text, NULL, NULL);
            }
        } else if (!t->quoted && is_file(t->text, &s, &p)) {
            if (!remote && !uu_is_local(s))
                remote = s;
            file_word(s, p, false);
        } else {
            add_word(W_LIT, t->text, NULL, NULL);
        }
    }
    if (start)
        fail("%s: no command after \"|\"", line);
    struct uujob j = { 0 };
    j.id = uu_new_id(remote ? remote : conf.nodename);
    if (!j.id) {
        fprintf(stderr, "uux: %s: cannot use the spool: %s\n", conf.spool, strerror(errno));
        return 1;
    }
    j.user = (char *)uu_user();
    j.system = (char *)(remote ? remote : conf.nodename);
    j.time = time(NULL);
    j.flags = o_n ? "n" : "";
    j.xsys = (char *)(xsys ? xsys : conf.nodename);
    j.cmd = cmd;
    j.line = line;
    if (o_p) {
        size_t cap = 8192, n = 0, r;
        char *d = xmalloc(cap);
        while ((r = fread(d + n, 1, cap - n, stdin)) > 0) {
            n += r;
            if (n == cap) {
                cap *= 2;
                d = xrealloc(d, cap);
            }
        }
        if (ferror(stdin)) {
            fprintf(stderr, "uux: standard input: %s\n", strerror(errno));
            return 1;
        }
        j.indata = uu_spool_data(j.id, 0, d, n);
        free(d);
        if (!j.indata) {
            fprintf(stderr, "uux: %s: cannot use the spool: %s\n", conf.spool, strerror(errno));
            return 1;
        }
    }
    if (!uu_save(&j)) {
        fprintf(stderr, "uux: %s: cannot queue the job: %s\n", conf.spool, strerror(errno));
        return 1;
    }
    if (o_j)
        printf("%s\n", j.id);
    if (fflush(stdout) != 0) {
        fprintf(stderr, "uux: standard output: %s\n", strerror(errno));
        return 1;
    }
    struct uujob *k = uu_load(j.id);
    if (!k) {
        fprintf(stderr, "uux: %s: the job is gone\n", j.id);
        return 1;
    }
    bool temporary;
    char *err;
    if (!uu_run_job(k, &temporary, &err)) {
        fprintf(stderr, "uux: %s%s\n", err ? err : "failed", temporary ? "; the job is queued" : "");
        return 1;
    }
    return 0;
}
