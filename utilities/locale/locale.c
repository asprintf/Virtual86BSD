/*
 * locale.c - locale: get locale-specific information (POSIX.1-2024,
 * XCU locale)
 *
 * Synopsis: locale [-a|-m]
 *           locale [-ck] name...
 *
 * The values are those of the C library for the locale of the
 * environment (setlocale(LC_ALL, "")), through nl_langinfo() and
 * localeconv(): the keywords of LC_MONETARY, LC_NUMERIC, LC_TIME and
 * LC_MESSAGES of XBD 7.3, and charmap (the codeset).  For LC_CTYPE and
 * LC_COLLATE no keyword values are written (the standard leaves it to
 * the implementation): their keywords are known, and select their
 * category, but give no lines.
 *
 * The public locales (-a): C and POSIX, and the locales of the C
 * library: the names in its locale archive (the GNU C library's file
 * /usr/lib/locale/locale-archive) and the directories of /usr/lib/locale
 * and of LOCPATH (when locale is not set-user-ID or set-group-ID), each
 * one that newlocale() accepts, and C.utf8 when the C library has it
 * without a file (Shall's built-in C.UTF-8) and no name of it was found;
 * sorted, each once.  The charmaps (-m):
 * the files of /usr/share/i18n/charmaps, less .gz or .bz2, sorted (make
 * can set the places: -DLOCALE_DIR=... -DCHARMAP_DIR=...).
 *
 * Choices where the standard leaves room:
 *  - Without operands, the categories are written in the order LC_CTYPE,
 *    LC_COLLATE, LC_TIME, LC_NUMERIC, LC_MONETARY, LC_MESSAGES; the
 *    implied value is that of LC_ALL, else LANG, else POSIX (the names as
 *    the environment gives them).  A value with characters other than
 *    letters, digits and _ . - @ : , + = / is put in single quotes.
 *  - The GNU C library keeps the alternative month names, and the entries
 *    of era and alt_digits, in its own way: they are taken so there.
 *  - Compound values are written as one string, the entries separated by
 *    semicolons; with -k the semicolons, backslashes, double quotes and
 *    control characters in an entry are preceded by a backslash (the
 *    escape character of every locale here), without -k entries are
 *    written as they are.  Numeric values that the locale leaves
 *    unspecified (CHAR_MAX) are -1, so is an empty grouping.
 *  - With -c, charmap is under LC_CTYPE.
 *  - A name that is neither a category nor a keyword is reported and the
 *    status is 1; the others are still written.  A locale of the
 *    environment that cannot be used is reported as a warning (the
 *    values are those of the POSIX locale then), and the status stays 0.
 *  - -a and -m are usage errors with each other, with -c or -k, or with
 *    operands; so are -c and -k without operands (status 2).
 */

#define _POSIX_C_SOURCE 202405L
#define _XOPEN_SOURCE 800

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <langinfo.h>
#include <limits.h>
#include <locale.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef LOCALE_DIR
#define LOCALE_DIR "/usr/lib/locale"
#endif
#ifndef CHARMAP_DIR
#define CHARMAP_DIR "/usr/share/i18n/charmaps"
#endif

static int status;

/* ---- the keywords ---- */

enum kind { STR, LIST, SEMI, NUM, GROUPING, NONE };
enum cat { CTYPE, COLLATE, TIME, NUMERIC, MONETARY, MESSAGES, NCATS };

static const char *catnames[NCATS] = { "LC_CTYPE", "LC_COLLATE", "LC_TIME", "LC_NUMERIC", "LC_MONETARY",
                                       "LC_MESSAGES" };

struct kw {
    const char *name;
    enum cat cat;
    enum kind kind;
    int item;                   /* nl_langinfo() item, or the first of a list */
    int n;                      /* entries of a list */
    size_t off;                 /* in struct lconv: char * or char */
};

#define LC(f) offsetof(struct lconv, f)

static const struct kw kws[] = {
    { "upper", CTYPE, NONE, 0, 0, 0 }, { "lower", CTYPE, NONE, 0, 0, 0 },
    { "alpha", CTYPE, NONE, 0, 0, 0 }, { "digit", CTYPE, NONE, 0, 0, 0 },
    { "space", CTYPE, NONE, 0, 0, 0 }, { "cntrl", CTYPE, NONE, 0, 0, 0 },
    { "punct", CTYPE, NONE, 0, 0, 0 }, { "graph", CTYPE, NONE, 0, 0, 0 },
    { "print", CTYPE, NONE, 0, 0, 0 }, { "xdigit", CTYPE, NONE, 0, 0, 0 },
    { "blank", CTYPE, NONE, 0, 0, 0 }, { "charclass", CTYPE, NONE, 0, 0, 0 },
    { "toupper", CTYPE, NONE, 0, 0, 0 }, { "tolower", CTYPE, NONE, 0, 0, 0 },
    { "charconv", CTYPE, NONE, 0, 0, 0 },
    { "collating-element", COLLATE, NONE, 0, 0, 0 }, { "collating-symbol", COLLATE, NONE, 0, 0, 0 },
    { "order_start", COLLATE, NONE, 0, 0, 0 },
    { "abday", TIME, LIST, ABDAY_1, 7, 0 },
    { "day", TIME, LIST, DAY_1, 7, 0 },
    { "abmon", TIME, LIST, ABMON_1, 12, 0 },
    { "ab_alt_mon", TIME, LIST, -1, 12, 0 },
    { "mon", TIME, LIST, MON_1, 12, 0 },
    { "alt_mon", TIME, LIST, -2, 12, 0 },
    { "d_t_fmt", TIME, STR, D_T_FMT, 0, 0 },
    { "d_fmt", TIME, STR, D_FMT, 0, 0 },
    { "t_fmt", TIME, STR, T_FMT, 0, 0 },
    { "am_pm", TIME, LIST, -3, 2, 0 },
    { "t_fmt_ampm", TIME, STR, T_FMT_AMPM, 0, 0 },
    { "era", TIME, SEMI, ERA, 0, 0 },
    { "era_d_fmt", TIME, STR, ERA_D_FMT, 0, 0 },
    { "era_t_fmt", TIME, STR, ERA_T_FMT, 0, 0 },
    { "era_d_t_fmt", TIME, STR, ERA_D_T_FMT, 0, 0 },
    { "alt_digits", TIME, SEMI, ALT_DIGITS, 0, 0 },
    { "decimal_point", NUMERIC, STR, -1, 0, LC(decimal_point) },
    { "thousands_sep", NUMERIC, STR, -1, 0, LC(thousands_sep) },
    { "grouping", NUMERIC, GROUPING, -1, 0, LC(grouping) },
    { "int_curr_symbol", MONETARY, STR, -1, 0, LC(int_curr_symbol) },
    { "currency_symbol", MONETARY, STR, -1, 0, LC(currency_symbol) },
    { "mon_decimal_point", MONETARY, STR, -1, 0, LC(mon_decimal_point) },
    { "mon_thousands_sep", MONETARY, STR, -1, 0, LC(mon_thousands_sep) },
    { "mon_grouping", MONETARY, GROUPING, -1, 0, LC(mon_grouping) },
    { "positive_sign", MONETARY, STR, -1, 0, LC(positive_sign) },
    { "negative_sign", MONETARY, STR, -1, 0, LC(negative_sign) },
    { "int_frac_digits", MONETARY, NUM, -1, 0, LC(int_frac_digits) },
    { "frac_digits", MONETARY, NUM, -1, 0, LC(frac_digits) },
    { "p_cs_precedes", MONETARY, NUM, -1, 0, LC(p_cs_precedes) },
    { "p_sep_by_space", MONETARY, NUM, -1, 0, LC(p_sep_by_space) },
    { "n_cs_precedes", MONETARY, NUM, -1, 0, LC(n_cs_precedes) },
    { "n_sep_by_space", MONETARY, NUM, -1, 0, LC(n_sep_by_space) },
    { "p_sign_posn", MONETARY, NUM, -1, 0, LC(p_sign_posn) },
    { "n_sign_posn", MONETARY, NUM, -1, 0, LC(n_sign_posn) },
    { "int_p_cs_precedes", MONETARY, NUM, -1, 0, LC(int_p_cs_precedes) },
    { "int_p_sep_by_space", MONETARY, NUM, -1, 0, LC(int_p_sep_by_space) },
    { "int_n_cs_precedes", MONETARY, NUM, -1, 0, LC(int_n_cs_precedes) },
    { "int_n_sep_by_space", MONETARY, NUM, -1, 0, LC(int_n_sep_by_space) },
    { "int_p_sign_posn", MONETARY, NUM, -1, 0, LC(int_p_sign_posn) },
    { "int_n_sign_posn", MONETARY, NUM, -1, 0, LC(int_n_sign_posn) },
    { "yesexpr", MESSAGES, STR, YESEXPR, 0, 0 },
    { "noexpr", MESSAGES, STR, NOEXPR, 0, 0 },
};
enum { NKWS = sizeof kws / sizeof kws[0] };

static bool o_c, o_k;

/* entry i of a list keyword */
static const char *list_entry(const struct kw *k, int i)
{
    switch (k->item) {
    case -1:                    /* ab_alt_mon */
#if defined(ABALTMON_1)
        return nl_langinfo(ABALTMON_1 + i);
#elif defined(__GLIBC__)
        return nl_langinfo(_NL_ABALTMON_1 + i);
#else
        return nl_langinfo(ABMON_1 + i);
#endif
    case -2:                    /* alt_mon */
#if defined(ALTMON_1)
        return nl_langinfo(ALTMON_1 + i);
#elif defined(__GLIBC__)
        return nl_langinfo(__ALTMON_1 + i);
#else
        return nl_langinfo(MON_1 + i);
#endif
    case -3:
        return nl_langinfo(i == 0 ? AM_STR : PM_STR);
    default:
        return nl_langinfo(k->item + i);
    }
}

/* s escaped for a -k value */
static void put_escaped(const char *s)
{
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == ';' || c == '\\' || c == '"' || iscntrl(c))
            putchar('\\');
        putchar(c);
    }
}

static void put_keyword(const struct kw *k)
{
    struct lconv *lc = localeconv();
    switch (k->kind) {
    case NONE:
        return;
    case STR: {
        const char *v = k->item >= 0 ? nl_langinfo(k->item) : *(char **)((char *)lc + k->off);
        if (!v)
            v = "";
        if (o_k) {
            printf("%s=\"", k->name);
            put_escaped(v);
            printf("\"\n");
        } else {
            printf("%s\n", v);
        }
        return;
    }
    case LIST:
        if (o_k)
            printf("%s=\"", k->name);
        for (int i = 0; i < k->n; i++) {
            const char *v = list_entry(k, i);
            if (i)
                putchar(';');
            if (o_k)
                put_escaped(v ? v : "");
            else
                fputs(v ? v : "", stdout);
        }
        printf(o_k ? "\"\n" : "\n");
        return;
    case SEMI: {
        /* a list separated by semicolons; the GNU C library keeps the
           entries as strings one after the other, and gives the first */
        const char *v = nl_langinfo(k->item);
        if (!v)
            v = "";
        if (o_k)
            printf("%s=\"", k->name);
#ifdef __GLIBC__
        int n = k->item == ERA ? (int)(intptr_t)nl_langinfo(_NL_TIME_ERA_NUM_ENTRIES) : 100;
        for (int i = 0; i < n && *v; i++) {
            if (i)
                putchar(';');
            if (o_k)
                put_escaped(v);
            else
                fputs(v, stdout);
            v += strlen(v) + 1;
        }
#else
        fputs(v, stdout);
#endif
        printf(o_k ? "\"\n" : "\n");
        return;
    }
    case NUM: {
        char v = *((char *)lc + k->off);
        int n = v == CHAR_MAX ? -1 : v;
        if (o_k)
            printf("%s=%d\n", k->name, n);
        else
            printf("%d\n", n);
        return;
    }
    case GROUPING: {
        const char *g = *(char **)((char *)lc + k->off);
        if (o_k)
            printf("%s=", k->name);
        if (!g || !*g) {
            printf("-1");
        } else {
            for (const char *p = g; *p; p++) {
                if (p != g)
                    putchar(';');
                printf("%d", *p == CHAR_MAX ? -1 : *p);
                if (*p == CHAR_MAX)
                    break;
            }
        }
        putchar('\n');
        return;
    }
    }
}

static void put_charmap(void)
{
    const char *cs = nl_langinfo(CODESET);
    if (o_k)
        printf("charmap=\"%s\"\n", cs);
    else
        printf("%s\n", cs);
}

/* ---- without operands: the environment ---- */

static bool plain(const char *s)
{
    if (!*s)
        return true;
    for (; *s; s++)
        if (!isalnum((unsigned char)*s) && !strchr("_.-@:,+=/", *s))
            return false;
    return true;
}

static void put_value(const char *v)
{
    if (plain(v)) {
        fputs(v, stdout);
        return;
    }
    putchar('\'');
    for (; *v; v++) {
        if (*v == '\'')
            fputs("'\\''", stdout);
        else
            putchar(*v);
    }
    putchar('\'');
}

static void put_implied(const char *v)
{
    putchar('"');
    for (; *v; v++) {
        if (strchr("\"\\$`", *v))
            putchar('\\');
        putchar(*v);
    }
    putchar('"');
}

static const char *env(const char *name)
{
    const char *v = getenv(name);
    return v && *v ? v : NULL;
}

static void summary(void)
{
    const char *lang = getenv("LANG"), *all = env("LC_ALL");
    printf("LANG=");
    put_value(lang ? lang : "");
    putchar('\n');
    for (int c = 0; c < NCATS; c++) {
        const char *v = env(catnames[c]);
        printf("%s=", catnames[c]);
        if (v && !all) {
            put_value(v);
        } else {
            const char *implied = all ? all : env("LANG") ? env("LANG") : "POSIX";
            put_implied(implied);
        }
        putchar('\n');
    }
    printf("LC_ALL=");
    put_value(all ? all : "");
    putchar('\n');
}

/* ---- -a and -m ---- */

static char **names;
static size_t nnames, capnames;

static void add_name(const char *s)
{
    for (size_t i = 0; i < nnames; i++)
        if (strcmp(names[i], s) == 0)
            return;
    if (nnames == capnames) {
        capnames = capnames ? capnames * 2 : 64;
        names = realloc(names, capnames * sizeof *names);
        if (!names) {
            fputs("locale: out of memory\n", stderr);
            exit(1);
        }
    }
    names[nnames] = strdup(s);
    if (!names[nnames]) {
        fputs("locale: out of memory\n", stderr);
        exit(1);
    }
    nnames++;
}

static int by_name(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

static void add_locale(const char *name)
{
    if (!*name || strchr(name, '/') || name[0] == '.')
        return;
    locale_t l = newlocale(LC_ALL_MASK, name, (locale_t)0);
    if (l == (locale_t)0)
        return;
    freelocale(l);
    add_name(name);
}

static uint32_t word(const unsigned char *p)
{
    uint32_t v;
    memcpy(&v, p, 4);           /* the archive is in the machine's byte order */
    return v;
}

/* the names in the GNU C library's locale archive */
static void archive_names(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return;
    unsigned char *d = NULL;
    size_t n = 0, cap = 0, k;
    unsigned char buf[65536];
    while ((k = fread(buf, 1, sizeof buf, f)) > 0) {
        if (n + k > cap) {
            cap = (n + k) * 2;
            unsigned char *nd = realloc(d, cap);
            if (!nd) {
                free(d);
                fclose(f);
                return;
            }
            d = nd;
        }
        memcpy(d + n, buf, k);
        n += k;
    }
    fclose(f);
    if (n < 36 || word(d) != 0xde020109u) {
        free(d);
        return;
    }
    uint32_t hoff = word(d + 8), hsize = word(d + 16);
    if (hoff > n || hsize > (n - hoff) / 12) {
        free(d);
        return;
    }
    for (uint32_t i = 0; i < hsize; i++) {
        const unsigned char *e = d + hoff + 12 * i;
        uint32_t noff = word(e + 4), rec = word(e + 8);
        if (rec == 0 || noff >= n || !memchr(d + noff, '\0', n - noff))
            continue;
        add_locale((const char *)d + noff);
    }
    free(d);
}

static void dir_names(const char *dir)
{
    DIR *d = opendir(dir);
    if (!d)
        return;
    struct dirent *e;
    while ((e = readdir(d))) {
        char p[PATH_MAX];
        struct stat st;
        if (e->d_name[0] == '.')
            continue;
        if (snprintf(p, sizeof p, "%s/%s/LC_CTYPE", dir, e->d_name) < (int)sizeof p && stat(p, &st) == 0)
            add_locale(e->d_name);
    }
    closedir(d);
}

/* C.UTF-8, C.utf8, c.utf-8 ...: the UTF-8 C locale */
static int is_c_utf8(const char *n)
{
    if ((n[0] != 'C' && n[0] != 'c') || n[1] != '.')
        return 0;
    n += 2;
    const char *u = "utf8";
    for (; *n; n++) {
        if (*n == '-')
            continue;
        if (!*u || tolower((unsigned char)*n) != *u)
            return 0;
        u++;
    }
    return !*u;
}

static void list_locales(void)
{
    add_name("C");
    add_name("POSIX");
    archive_names(LOCALE_DIR "/locale-archive");
    dir_names(LOCALE_DIR);
    const char *lp = getuid() == geteuid() && getgid() == getegid() ? getenv("LOCPATH") : NULL;
    if (lp && *lp) {
        char *copy = strdup(lp), *save = NULL;
        for (char *t = copy ? strtok_r(copy, ":", &save) : NULL; t; t = strtok_r(NULL, ":", &save))
            dir_names(t);
        free(copy);
    }
    /* a C library with a built-in UTF-8 C locale (Shall's has C.UTF-8, which
       is no file): listed as glibc lists its C.utf8, unless a file has it */
    int have_cutf8 = 0;
    for (size_t i = 0; i < nnames; i++)
        if (is_c_utf8(names[i]))
            have_cutf8 = 1;
    if (!have_cutf8)
        add_locale("C.utf8");
    if (nnames > 1)
        qsort(names, nnames, sizeof *names, by_name);
    for (size_t i = 0; i < nnames; i++)
        printf("%s\n", names[i]);
}

static void list_charmaps(void)
{
    DIR *d = opendir(CHARMAP_DIR);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d))) {
            if (e->d_name[0] == '.')
                continue;
            char n[NAME_MAX + 1];
            snprintf(n, sizeof n, "%s", e->d_name);
            size_t l = strlen(n);
            if (l > 3 && strcmp(n + l - 3, ".gz") == 0)
                n[l - 3] = '\0';
            else if (l > 4 && strcmp(n + l - 4, ".bz2") == 0)
                n[l - 4] = '\0';
            add_name(n);
        }
        closedir(d);
    }
    /* a C library with a built-in UTF-8 C locale (Shall's has C.UTF-8, which
       is no file): listed as glibc lists its C.utf8, unless a file has it */
    int have_cutf8 = 0;
    for (size_t i = 0; i < nnames; i++)
        if (is_c_utf8(names[i]))
            have_cutf8 = 1;
    if (!have_cutf8)
        add_locale("C.utf8");
    if (nnames > 1)
        qsort(names, nnames, sizeof *names, by_name);
    for (size_t i = 0; i < nnames; i++)
        printf("%s\n", names[i]);
}

static void usage(void)
{
    fputs("usage: locale [-a|-m]\n       locale [-ck] name...\n", stderr);
    exit(2);
}

int main(int argc, char *argv[])
{
    if (!setlocale(LC_ALL, "")) {
        fputs("locale: warning: the locale of the environment cannot be used; the values are the POSIX "
              "locale's\n", stderr);
        setlocale(LC_ALL, "C");
    }
    bool o_a = false, o_m = false;
    int c;
    while ((c = getopt(argc, argv, "amck")) != -1) {
        switch (c) {
        case 'a': o_a = true; break;
        case 'm': o_m = true; break;
        case 'c': o_c = true; break;
        case 'k': o_k = true; break;
        default: usage();
        }
    }
    int nops = argc - optind;
    if ((o_a || o_m) && (nops || o_c || o_k || (o_a && o_m)))
        usage();
    if ((o_c || o_k) && !nops)
        usage();
    if (o_a)
        list_locales();
    else if (o_m)
        list_charmaps();
    else if (!nops)
        summary();
    for (int i = optind; i < argc; i++) {
        const char *n = argv[i];
        bool done = false;
        for (int k = 0; k < NCATS && !done; k++)
            if (strcmp(n, catnames[k]) == 0) {
                if (o_c)
                    printf("%s\n", catnames[k]);
                for (int j = 0; j < NKWS; j++)
                    if (kws[j].cat == (enum cat)k)
                        put_keyword(&kws[j]);
                done = true;
            }
        for (int j = 0; j < NKWS && !done; j++)
            if (strcmp(n, kws[j].name) == 0) {
                if (o_c)
                    printf("%s\n", catnames[kws[j].cat]);
                put_keyword(&kws[j]);
                done = true;
            }
        if (!done && strcmp(n, "charmap") == 0) {
            if (o_c)
                printf("LC_CTYPE\n");
            put_charmap();
            done = true;
        }
        if (!done) {
            fprintf(stderr, "locale: unknown name \"%s\"\n", n);
            status = 1;
        }
    }
    if (fflush(stdout) != 0 || ferror(stdout)) {
        fprintf(stderr, "locale: standard output: %s\n", strerror(errno));
        return 1;
    }
    return status;
}
