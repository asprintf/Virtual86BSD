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
 * gettext.c - gettext, ngettext: retrieve text strings from messages
 * objects (POSIX.1-2024, XCU gettext and ngettext)
 *
 * Synopsis: gettext [-e|-E] [-d textdomain] [textdomain] msgid
 *           gettext [-e|-E] [-n] -s [-d textdomain] msgid...
 *           ngettext [-e|-E] [-d textdomain] [textdomain] msgid msgid_plural n
 *
 * One program, acting by its name (ngettext is a link).  The messages
 * objects are read here (mo.c), not through the C library, and are
 * looked for as the standard's gettext() does:
 *  1. NLSPATH, when set and not empty: each template in turn (%N the text
 *     domain, %L the locale name, %l %t %c its language, territory and
 *     codeset, %% a %; an empty template is %N), the first that names a
 *     messages object;
 *  2. LANGUAGE, when set and not empty: DIR/NAME/LC_MESSAGES/DOMAIN.mo
 *     for each locale name in it, and the shorter forms of the name (as
 *     GNU gettext: without the modifier last, the codeset first, also
 *     with the codeset normalized, "utf8" for "UTF-8"), the first form
 *     that is a messages object; if that has not the message, the next
 *     name (names with a slash, ".", ".." and empty ones are left out);
 *  3. when LANGUAGE led to no messages object: the same for the locale
 *     of LC_MESSAGES, the first messages object found.
 * DIR is TEXTDOMAINDIR when set and not empty, else /usr/share/locale
 * (make can set another: -DLOCALEDIR=...).  In the C and POSIX locales,
 * nothing is looked for.  A message found is converted (iconv) from the
 * charset of its messages object to the codeset of the locale; when that
 * cannot be done, it is as if it were not found.
 *
 * Choices where the standard leaves room:
 *  - Without -e and -E, escapes are not processed (-E), also without -s.
 *  - \c (with -e) ends the output: what follows it, and the other msgid
 *    operands, are not written, and no newline is (as with -n).
 *  - With no text domain, the msgid (or msgid_plural) is written, with
 *    status 0.
 *  - A message without plural forms is the translation for ngettext
 *    whatever n is (as in GNU gettext).
 *  - n must be a decimal integer (the digits of strtoul(), and nothing
 *    after them); else it is an error.  A plural form that the
 *    expression does not select (out of range, or a division by zero)
 *    is as if the message were not found.
 *  - -n without -s has no effect.  Usage errors exit with status 2,
 *    other errors with status 1.
 */

#define _POSIX_C_SOURCE 202405L
#define _XOPEN_SOURCE 800

#include "cesc.h"
#include "mo.h"

#include <ctype.h>
#include <errno.h>
#include <iconv.h>
#include <langinfo.h>
#include <limits.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#ifndef LOCALEDIR
#define LOCALEDIR "/usr/share/locale"
#endif

static const char *prog = "gettext";

static void *xmalloc(size_t n)
{
    void *p = malloc(n ? n : 1);
    if (!p) {
        fprintf(stderr, "%s: out of memory\n", prog);
        exit(1);
    }
    return p;
}

/* ---- locale names ---- */

struct lname {
    char lang[64], terr[64], codeset[64], norm[64], mod[64];
    bool has_terr, has_codeset, has_mod;
};

/* split language[_territory][.codeset][@modifier] */
static bool split_name(const char *s, struct lname *n)
{
    memset(n, 0, sizeof *n);
    size_t l = strcspn(s, "_.@");
    if (l == 0 || l >= sizeof n->lang)
        return false;
    memcpy(n->lang, s, l);
    s += l;
    if (*s == '_') {
        s++;
        l = strcspn(s, ".@");
        if (l >= sizeof n->terr)
            return false;
        memcpy(n->terr, s, l);
        n->has_terr = true;
        s += l;
    }
    if (*s == '.') {
        s++;
        l = strcspn(s, "@");
        if (l >= sizeof n->codeset)
            return false;
        memcpy(n->codeset, s, l);
        n->has_codeset = true;
        s += l;
        /* normalized: the letters lowercased and the digits; "iso" first
           when it is all digits */
        size_t k = 0;
        bool alpha = false;
        for (const char *c = n->codeset; *c && k + 4 < sizeof n->norm; c++)
            if (isalnum((unsigned char)*c)) {
                alpha |= isalpha((unsigned char)*c) != 0;
                n->norm[k++] = (char)tolower((unsigned char)*c);
            }
        n->norm[k] = '\0';
        if (!alpha && k > 0) {
            memmove(n->norm + 3, n->norm, k + 1);
            memcpy(n->norm, "iso", 3);
        }
    }
    if (*s == '@') {
        s++;
        if (strlen(s) >= sizeof n->mod)
            return false;
        strcpy(n->mod, s);
        n->has_mod = true;
    } else if (*s) {
        return false;
    }
    return true;
}

enum { M_NORM = 1, M_CODESET = 2, M_TERR = 4, M_MOD = 8 };

/* the forms of a locale name, the most specific first: the names in out
   (each of size 256); their number */
static int name_forms(const char *s, char out[16][256])
{
    struct lname n;
    if (!split_name(s, &n)) {
        snprintf(out[0], 256, "%s", s);
        return 1;
    }
    int k = 0;
    for (int mask = 15; mask >= 0; mask--) {
        if ((mask & (M_NORM | M_CODESET)) == (M_NORM | M_CODESET))
            continue;
        if ((mask & M_TERR) && !n.has_terr)
            continue;
        if ((mask & M_MOD) && !n.has_mod)
            continue;
        if ((mask & (M_CODESET | M_NORM)) && !n.has_codeset)
            continue;
        if ((mask & M_NORM) && (!n.norm[0] || strcmp(n.norm, n.codeset) == 0))
            continue;
        snprintf(out[k++], 256, "%s%s%s%s%s%s%s", n.lang, mask & M_TERR ? "_" : "", mask & M_TERR ? n.terr : "",
                 mask & (M_CODESET | M_NORM) ? "." : "",
                 mask & M_CODESET ? n.codeset : mask & M_NORM ? n.norm : "", mask & M_MOD ? "@" : "",
                 mask & M_MOD ? n.mod : "");
    }
    return k;
}

static bool name_ok(const char *s)
{
    return *s && !strchr(s, '/') && strcmp(s, ".") != 0 && strcmp(s, "..") != 0;
}

/* ---- finding the message ---- */

static const char *domain, *dir, *locname;
static bool plural_wanted;
static unsigned long n_arg = 1;

/* the translation of msgid in the messages object at path (malloc'd, in
   the locale's codeset), or NULL; *object set when path is a messages
   object */
static char *lookup(const char *path, const char *msgid, bool *object)
{
    struct mo m;
    *object = false;
    if (!mo_load(path, &m))
        return NULL;
    *object = true;
    size_t len;
    const char *t = mo_find(&m, msgid, &len);
    char *result = NULL;
    if (!t)
        goto out;
    /* the form */
    if (memchr(t, '\0', len)) {
        unsigned long idx = 0;
        if (plural_wanted) {
            if (!m.plural) {
                idx = n_arg == 1 ? 0 : 1;
            } else if (!plural_eval(m.plural, n_arg, &idx) || idx >= m.nplurals) {
                goto out;
            }
        }
        const char *f = t, *end = t + len;
        for (unsigned long i = 0; i < idx; i++) {
            const char *z = memchr(f, '\0', (size_t)(end - f));
            if (!z)
                goto out;
            f = z + 1;
        }
        if (f > end)
            goto out;
        t = f;
        len = strnlen(f, (size_t)(end - f));
    }
    /* the codeset */
    const char *to = nl_langinfo(CODESET);
    if (m.charset[0] && to && *to && strcasecmp(m.charset, to) != 0) {
        struct lname a, b;
        char x[80], y[80];
        snprintf(x, sizeof x, "x.%s", m.charset);
        snprintf(y, sizeof y, "x.%s", to);
        bool same = split_name(x, &a) && split_name(y, &b) && strcmp(a.norm, b.norm) == 0;
        if (!same) {
            iconv_t cd = iconv_open(to, m.charset);
            if (cd == (iconv_t)-1)
                goto out;
            size_t cap = len * 4 + 16, left = cap;
            char *buf = xmalloc(cap + 1), *o = buf, *i = (char *)t;
            size_t inleft = len;
            size_t r = iconv(cd, &i, &inleft, &o, &left);
            if (r != (size_t)-1)
                r = iconv(cd, NULL, NULL, &o, &left);
            iconv_close(cd);
            if (r == (size_t)-1) {
                free(buf);
                goto out;
            }
            *o = '\0';
            result = buf;
            goto out;
        }
    }
    result = xmalloc(len + 1);
    memcpy(result, t, len);
    result[len] = '\0';
out:
    mo_free(&m);
    return result;
}

/* the NLSPATH template t expanded for the domain and locale */
static char *expand(const char *t, size_t tl)
{
    struct lname n;
    bool split = split_name(locname, &n);
    size_t cap = 256 + tl * 2 + strlen(domain) * tl + strlen(locname) * tl, k = 0;
    char *out = xmalloc(cap);
    if (tl == 0) {
        snprintf(out, cap, "%s", domain);
        return out;
    }
    for (size_t i = 0; i < tl && k + 1 < cap; i++) {
        if (t[i] != '%' || i + 1 == tl) {
            out[k++] = t[i];
            continue;
        }
        const char *v = NULL;
        switch (t[++i]) {
        case 'N': v = domain; break;
        case 'L': v = locname; break;
        case 'l': v = split ? n.lang : ""; break;
        case 't': v = split ? n.terr : ""; break;
        case 'c': v = split ? n.codeset : ""; break;
        case '%': v = "%"; break;
        default:
            out[k++] = '%';
            out[k++] = t[i];
            continue;
        }
        k += (size_t)snprintf(out + k, cap - k, "%s", v);
    }
    out[k < cap ? k : cap - 1] = '\0';
    return out;
}

/* the translation of msgid (malloc'd), or NULL */
static char *find(const char *msgid)
{
    if (!locname || strcmp(locname, "C") == 0 || strcmp(locname, "POSIX") == 0)
        return NULL;
    bool object;
    const char *nls = getenv("NLSPATH");
    if (nls && *nls) {
        const char *s = nls;
        for (;;) {
            size_t l = strcspn(s, ":");
            char *path = expand(s, l);
            char *r = lookup(path, msgid, &object);
            free(path);
            if (object)
                return r;
            if (!s[l])
                break;
            s += l + 1;
        }
    }
    char forms[16][256];
    char path[PATH_MAX];
    const char *lang = getenv("LANGUAGE");
    bool found_object = false;
    if (lang && *lang) {
        char *copy = xmalloc(strlen(lang) + 1);
        strcpy(copy, lang);
        char *save = NULL;
        for (char *name = strtok_r(copy, ":", &save); name; name = strtok_r(NULL, ":", &save)) {
            if (!name_ok(name))
                continue;
            int k = name_forms(name, forms);
            for (int i = 0; i < k; i++) {
                if (snprintf(path, sizeof path, "%s/%s/LC_MESSAGES/%s.mo", dir, forms[i], domain) >= (int)sizeof path)
                    continue;
                char *r = lookup(path, msgid, &object);
                if (r) {
                    free(copy);
                    return r;
                }
                if (object) {
                    found_object = true;
                    break;          /* found, but not the message: the next name */
                }
            }
        }
        free(copy);
    }
    if (found_object || !name_ok(locname))
        return NULL;
    int k = name_forms(locname, forms);
    for (int i = 0; i < k; i++) {
        if (snprintf(path, sizeof path, "%s/%s/LC_MESSAGES/%s.mo", dir, forms[i], domain) >= (int)sizeof path)
            continue;
        char *r = lookup(path, msgid, &object);
        if (object)
            return r;
    }
    return NULL;
}

/* ---- the utility ---- */

static void usage(bool ng)
{
    if (ng)
        fputs("usage: ngettext [-e|-E] [-d textdomain] [textdomain] msgid msgid_plural n\n", stderr);
    else
        fputs("usage: gettext [-e|-E] [-d textdomain] [textdomain] msgid\n"
              "       gettext [-e|-E] [-n] -s [-d textdomain] msgid...\n", stderr);
    exit(2);
}

/* s with its escapes processed when esc (malloc'd); *stop set at \c */
static char *operand(const char *s, bool esc, bool *stop)
{
    size_t n = strlen(s), len;
    char *out = xmalloc(n + 1);
    if (!esc) {
        memcpy(out, s, n + 1);
        return out;
    }
    if (cesc(s, n, out, &len, CESC_C) == 1)
        *stop = true;
    out[len] = '\0';
    return out;
}

static bool put(const char *s)
{
    return fputs(s, stdout) != EOF;
}

int main(int argc, char *argv[])
{
    const char *base = strrchr(argv[0], '/');
    base = base ? base + 1 : argv[0];
    bool ng = strcmp(base, "ngettext") == 0;
    prog = ng ? "ngettext" : "gettext";
    int esc = -1;               /* -1 unset, 0 -E, 1 -e */
    bool o_n = false, o_s = false;
    const char *o_d = NULL;
    int c;
    while ((c = getopt(argc, argv, ng ? "eEd:" : "eEd:ns")) != -1) {
        switch (c) {
        case 'e': esc = 1; break;
        case 'E': esc = 0; break;
        case 'd': o_d = optarg; break;
        case 'n': o_n = true; break;
        case 's': o_s = true; break;
        default: usage(ng);
        }
    }
    int nops = argc - optind;
    char **ops = argv + optind;
    const char *td_op = NULL;
    if (ng) {
        if (nops == 4)
            td_op = *ops++, nops--;
        else if (nops != 3)
            usage(ng);
    } else if (o_s) {
        if (nops < 1)
            usage(ng);
    } else {
        if (nops == 2)
            td_op = *ops++, nops--;
        else if (nops != 1)
            usage(ng);
    }
    if (ng) {
        const char *s = ops[2];
        char *end;
        errno = 0;
        while (isspace((unsigned char)*s))
            s++;
        n_arg = strtoul(s, &end, 10);
        if (!isdigit((unsigned char)*s) || *end || errno) {
            fprintf(stderr, "ngettext: invalid number: %s\n", ops[2]);
            return 1;
        }
    }
    domain = td_op;
    if (!domain || !*domain)
        domain = o_d;
    if (!domain || !*domain)
        domain = getenv("TEXTDOMAIN");
    if (domain && !*domain)
        domain = NULL;
    if (domain) {
        setlocale(LC_ALL, "");
        locname = setlocale(LC_MESSAGES, NULL);
        dir = getenv("TEXTDOMAINDIR");
        if (!dir || !*dir)
            dir = LOCALEDIR;
    }
    bool use_esc = esc == 1;
    bool stop = false, ok = true;

    if (ng) {
        char *id = operand(ops[0], use_esc, &stop);
        bool stop2 = false;
        char *pl = operand(ops[1], use_esc, &stop2);
        plural_wanted = true;
        char *r = domain ? find(id) : NULL;
        ok = put(r ? r : n_arg == 1 ? id : pl);
        free(r);
        free(id);
        free(pl);
    } else if (o_s) {
        for (int i = 0; i < nops && !stop; i++) {
            char *id = operand(ops[i], use_esc, &stop);
            char *r = domain ? find(id) : NULL;
            if (i > 0)
                ok &= put(" ");
            ok &= put(r ? r : id);
            free(r);
            free(id);
        }
        if (!o_n && !stop)
            ok &= put("\n");
    } else {
        char *id = operand(ops[0], use_esc, &stop);
        char *r = domain ? find(id) : NULL;
        ok = put(r ? r : id);
        free(r);
        free(id);
    }
    if (!ok || fflush(stdout) != 0 || ferror(stdout)) {
        fprintf(stderr, "%s: standard output: %s\n", prog, strerror(errno));
        return 1;
    }
    return 0;
}
