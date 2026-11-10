/*
 * simple.c - the categories LC_NUMERIC, LC_MONETARY, LC_TIME,
 * LC_MESSAGES, LC_PAPER, LC_NAME, LC_ADDRESS, LC_TELEPHONE,
 * LC_MEASUREMENT and LC_IDENTIFICATION of localedef: their keywords,
 * their defaults (those of glibc), and their files
 */

#define _POSIX_C_SOURCE 202405L

#include "localedef.h"

#include <regex.h>
#include <stdlib.h>
#include <string.h>

static const uint32_t empty_wstr[1] = { 0 };

/* ---- the loop of a category's section ---- */

struct section {
    struct lexer *lr;
    struct localedef *result;
    const struct charmap *cm;
    const struct repertoire *rep;
    int ignore;
    const char *name;
};

/* the first token of the section; false when it is "copy" (done) */
static bool section_start(struct section *s, struct token **now, enum tok cattok, int category)
{
    lr_ignore_rest(s->lr, 1);
    do
        *now = lr_token(s->lr, s->cm, s->result, s->rep, verbose);
    while ((*now)->tok == tok_eol);
    if ((*now)->tok == tok_copy) {
        handle_copy(s->lr, s->cm, s->result->repertoire_name, s->result, cattok, category, s->name, s->ignore);
        return false;
    }
    return true;
}

/* "END name": true when the section is done */
static bool section_end(struct section *s, enum tok cattok)
{
    struct token *now = lr_token(s->lr, s->cm, s->result, s->rep, verbose);
    if (now->tok == tok_eof)
        return false;
    if (now->tok == tok_eol)
        lr_error(s->lr, "%s: incomplete `END' line", s->name);
    else if (now->tok != cattok)
        lr_error(s->lr, "%1$s: definition does not end with `END %1$s'", s->name);
    lr_ignore_rest(s->lr, now->tok == cattok);
    return true;
}

static struct token *next_token(struct section *s)
{
    return lr_token(s->lr, s->cm, s->result, s->rep, verbose);
}

/* a string field: its multibyte (and wide) value */
static bool str_field(struct section *s, const char **field, const uint32_t **wfield, const char *fname)
{
    struct token *now = next_token(s);
    if (now->tok != tok_string)
        return false;
    if (*field)
        lr_error(s->lr, "%s: field `%s' declared more than once", s->name, fname);
    else if (!now->str.startmb || (wfield && !now->str.startwc)) {
        lr_error(s->lr, "%s: unknown character in field `%s'", s->name, fname);
        *field = "";
        if (wfield)
            *wfield = empty_wstr;
    } else {
        *field = now->str.startmb;
        if (wfield)
            *wfield = now->str.startwc;
    }
    return true;
}

/* grouping, mon_grouping: numbers (or -1) separated by ";" */
static bool grouping_field(struct section *s, char **grouping, size_t *len, const char *fname)
{
    struct token *now = next_token(s);
    if (now->tok != tok_minus1 && now->tok != tok_number)
        return false;
    size_t act = 0, max = 10;
    char *g = xmalloc(max);
    do {
        if (act + 1 >= max) {
            max *= 2;
            g = xrealloc(g, max);
        }
        if (act > 0 && g[act - 1] == '\177') {
            lr_error(s->lr, "%s: `-1' must be last entry in `%s' field", s->name, fname);
            lr_ignore_rest(s->lr, 0);
            break;
        }
        if (now->tok == tok_minus1)
            g[act++] = '\177';
        else if (now->num == 0)
            g[act++] = '\377';
        else if (now->num > 126)
            lr_error(s->lr, "%s: values for field `%s' must be smaller than 127", s->name, fname);
        else
            g[act++] = (char)now->num;
        now = next_token(s);
        if (now->tok != tok_semicolon)
            break;
        now = next_token(s);
    } while (now->tok == tok_minus1 || now->tok == tok_number);
    if (now->tok != tok_eol) {
        free(g);
        return false;
    }
    if (act == 1 && g[0] == '\177')
        act--;
    g[act++] = '\0';
    *grouping = g;
    *len = act;
    return true;
}

/* a category not defined, nor copied: a warning, the defaults */
static void *missing(struct localedef *locale, int category, size_t size, bool *nothing)
{
    record_warning("No definition for %s category found", category_names[category]);
    *nothing = true;
    void *p = xcalloc(1, size);
    locale->categories[category] = p;
    return p;
}

/* ---- LC_NUMERIC ---- */

struct numeric {
    const char *decimal_point, *thousands_sep;
    char *grouping;
    size_t grouping_len;
    uint32_t decimal_point_wc, thousands_sep_wc;
};

/* decimal_point, mon_decimal_point ...: a string of one character */
static bool char_field(struct section *s, const char **field, uint32_t *wc, const char *fname)
{
    s->lr->return_widestr = 1;
    struct token *now = next_token(s);
    s->lr->return_widestr = 0;
    if (now->tok != tok_string)
        return false;
    if (*field)
        lr_error(s->lr, "%s: field `%s' declared more than once", s->name, fname);
    else if (!now->str.startmb) {
        lr_error(s->lr, "%s: unknown character in field `%s'", s->name, fname);
        *field = "";
        *wc = 0;
    } else if (now->str.startwc && now->str.lenwc > 2) {
        lr_error(s->lr, "%s: value for field `%s' must be a single character", s->name, fname);
    } else {
        *field = now->str.startmb;
        if (now->str.startwc)
            *wc = now->str.startwc[0];
    }
    return true;
}

void numeric_read(struct lexer *lr, struct localedef *result, const struct charmap *cm,
                  const char *repertoire_name, int ignore)
{
    struct section s = { lr, result, cm, NULL, ignore, "LC_NUMERIC" };
    struct token *now;
    if (repertoire_name)
        s.rep = repertoire_read(repertoire_name);
    if (!section_start(&s, &now, tok_lc_numeric, C_NUMERIC))
        return;
    struct numeric *n = NULL;
    if (!ignore)
        n = result->categories[C_NUMERIC] = xcalloc(1, sizeof *n);
    lr->translate_strings = 1;
    lr->return_widestr = 0;
    for (; now->tok != tok_eof; now = next_token(&s)) {
        if (now->tok == tok_eol)
            continue;
        if (ignore && now->tok != tok_end) {
            lr_ignore_rest(lr, 0);
            continue;
        }
        bool ok = true;
        switch (now->tok) {
        case tok_decimal_point: ok = char_field(&s, &n->decimal_point, &n->decimal_point_wc, "decimal_point"); break;
        case tok_thousands_sep: ok = char_field(&s, &n->thousands_sep, &n->thousands_sep_wc, "thousands_sep"); break;
        case tok_grouping: ok = grouping_field(&s, &n->grouping, &n->grouping_len, "grouping"); break;
        case tok_end:
            if (section_end(&s, tok_lc_numeric))
                return;
            continue;
        default: ok = false;
        }
        if (!ok)
            SYNTAX_ERROR(lr, "%s: syntax error", s.name);
    }
    lr_error(lr, "%s: premature end of file", s.name);
}

void numeric_finish(struct localedef *locale, const struct charmap *cm)
{
    struct numeric *n = resolve_copy(locale, C_NUMERIC, cm);
    bool nothing = false;
    if (!n)
        n = missing(locale, C_NUMERIC, sizeof *n, &nothing);
    if (!n->decimal_point) {
        if (!nothing)
            record_error(0, "%s: field `%s' not defined", "LC_NUMERIC", "decimal_point");
        n->decimal_point = ".";
    } else if (n->decimal_point[0] == '\0' && !nothing) {
        record_error(0, "%s: value for field `%s' must not be an empty string", "LC_NUMERIC", "decimal_point");
    }
    if (n->decimal_point_wc == 0)
        n->decimal_point_wc = '.';
    if (n->grouping_len == 0 && !nothing)
        record_error(0, "%s: field `%s' not defined", "LC_NUMERIC", "grouping");
}

void numeric_output(struct localedef *locale, const struct charmap *cm, const char *out)
{
    struct numeric *n = locale->categories[C_NUMERIC];
    struct locfile f;
    init_locale_data(&f, 6);
    add_locale_string(&f, n->decimal_point ? n->decimal_point : "");
    add_locale_string(&f, n->thousands_sep ? n->thousands_sep : "");
    add_locale_raw_data(&f, n->grouping, n->grouping_len);
    add_locale_uint32(&f, n->decimal_point_wc);
    add_locale_uint32(&f, n->thousands_sep_wc);
    add_locale_string(&f, cm->code_set_name);
    write_locale_data(out, C_NUMERIC, &f);
}

/* ---- LC_MONETARY ---- */

#define MON_BYTES                                                                                  \
    X(int_frac_digits) X(frac_digits) X(p_cs_precedes) X(p_sep_by_space) X(n_cs_precedes)          \
    X(n_sep_by_space) X(p_sign_posn) X(n_sign_posn) X(int_p_cs_precedes) X(int_p_sep_by_space)    \
    X(int_n_cs_precedes) X(int_n_sep_by_space) X(int_p_sign_posn) X(int_n_sign_posn)              \
    X(duo_int_frac_digits) X(duo_frac_digits) X(duo_p_cs_precedes) X(duo_p_sep_by_space)         \
    X(duo_n_cs_precedes) X(duo_n_sep_by_space) X(duo_p_sign_posn) X(duo_n_sign_posn)             \
    X(duo_int_p_cs_precedes) X(duo_int_p_sep_by_space) X(duo_int_n_cs_precedes)                  \
    X(duo_int_n_sep_by_space) X(duo_int_p_sign_posn) X(duo_int_n_sign_posn)

struct monetary {
    const char *int_curr_symbol, *currency_symbol, *mon_decimal_point, *mon_thousands_sep;
    uint32_t mon_decimal_point_wc, mon_thousands_sep_wc;
    char *mon_grouping;
    size_t mon_grouping_len;
    const char *positive_sign, *negative_sign;
#define X(f) signed char f;
    MON_BYTES
#undef X
    const char *duo_int_curr_symbol, *duo_currency_symbol;
    uint32_t uno_valid_from, uno_valid_to, duo_valid_from, duo_valid_to;
    uint32_t conversion_rate[2];
    char *crncystr;
};

static void monetary_init(struct monetary *m)
{
    memset(m, 0, sizeof *m);
#define X(f) m->f = -2;
    MON_BYTES
#undef X
}

static bool int_field_m(struct section *s, signed char *field, const char *fname)
{
    struct token *now = next_token(s);
    if (now->tok != tok_minus1 && now->tok != tok_number)
        return false;
    if (*field != -2)
        lr_error(s->lr, "%s: field `%s' declared more than once", s->name, fname);
    else
        *field = now->tok == tok_minus1 ? -1 : (signed char)now->num;
    return true;
}

static bool word_field_m(struct section *s, uint32_t *field, const char *fname)
{
    struct token *now = next_token(s);
    if (now->tok != tok_minus1 && now->tok != tok_number)
        return false;
    if (*field != 0)
        lr_error(s->lr, "%s: field `%s' declared more than once", s->name, fname);
    else
        *field = now->tok == tok_minus1 ? (uint32_t)-1 : (uint32_t)now->num;
    return true;
}

void monetary_read(struct lexer *lr, struct localedef *result, const struct charmap *cm,
                   const char *repertoire_name, int ignore)
{
    struct section s = { lr, result, cm, NULL, ignore, "LC_MONETARY" };
    struct token *now;
    if (repertoire_name)
        s.rep = repertoire_read(repertoire_name);
    if (!section_start(&s, &now, tok_lc_monetary, C_MONETARY))
        return;
    struct monetary *m = NULL;
    if (!ignore) {
        m = result->categories[C_MONETARY] = xmalloc(sizeof *m);
        monetary_init(m);
    }
    lr->translate_strings = 1;
    lr->return_widestr = 0;
    for (; now->tok != tok_eof; now = next_token(&s)) {
        if (now->tok == tok_eol)
            continue;
        if (ignore && now->tok != tok_end) {
            lr_ignore_rest(lr, 0);
            continue;
        }
        bool ok = true;
        const struct repertoire *rep = s.rep;
        s.rep = NULL;           /* glibc reads these strings without it */
        switch (now->tok) {
#define S(f)                                                                                       \
    case tok_##f:                                                                                  \
        ok = str_field(&s, &m->f, NULL, #f);                                                       \
        if (ok)                                                                                    \
            lr_ignore_rest(lr, 1);                                                                 \
        break;
        S(int_curr_symbol) S(currency_symbol) S(positive_sign) S(negative_sign)
        S(duo_int_curr_symbol) S(duo_currency_symbol)
#undef S
        case tok_mon_decimal_point:
            s.rep = rep;
            ok = char_field(&s, &m->mon_decimal_point, &m->mon_decimal_point_wc, "mon_decimal_point");
            break;
        case tok_mon_thousands_sep:
            s.rep = rep;
            ok = char_field(&s, &m->mon_thousands_sep, &m->mon_thousands_sep_wc, "mon_thousands_sep");
            break;
#define X(f)                                                                                       \
    case tok_##f:                                                                                  \
        ok = int_field_m(&s, &m->f, #f);                                                           \
        break;
        MON_BYTES
#undef X
        case tok_uno_valid_from: ok = word_field_m(&s, &m->uno_valid_from, "uno_valid_from"); break;
        case tok_uno_valid_to: ok = word_field_m(&s, &m->uno_valid_to, "uno_valid_to"); break;
        case tok_duo_valid_from: ok = word_field_m(&s, &m->duo_valid_from, "duo_valid_from"); break;
        case tok_duo_valid_to: ok = word_field_m(&s, &m->duo_valid_to, "duo_valid_to"); break;
        case tok_mon_grouping: ok = grouping_field(&s, &m->mon_grouping, &m->mon_grouping_len, "mon_grouping"); break;
        case tok_conversion_rate: {
            now = next_token(&s);
            if (now->tok != tok_number) {
                ok = false;
                break;
            }
            if (now->num == 0) {
            zero:
                lr_error(lr, "conversion rate value cannot be zero");
                m->conversion_rate[0] = m->conversion_rate[1] = 1;
                break;
            }
            m->conversion_rate[0] = (uint32_t)now->num;
            now = next_token(&s);
            if (now->tok != tok_semicolon) {
                ok = false;
                break;
            }
            now = next_token(&s);
            if (now->tok != tok_number) {
                ok = false;
                break;
            }
            if (now->num == 0)
                goto zero;
            m->conversion_rate[1] = (uint32_t)now->num;
            lr_ignore_rest(lr, 1);
            break;
        }
        case tok_end:
            s.rep = rep;
            if (section_end(&s, tok_lc_monetary))
                return;
            continue;
        default: ok = false;
        }
        s.rep = rep;
        if (!ok)
            SYNTAX_ERROR(lr, "%s: syntax error", s.name);
    }
    lr_error(lr, "%s: premature end of file", s.name);
}

void monetary_finish(struct localedef *locale, const struct charmap *cm)
{
    struct monetary *m = resolve_copy(locale, C_MONETARY, cm);
    bool nothing = false;
    if (!m) {
        m = missing(locale, C_MONETARY, sizeof *m, &nothing);
        monetary_init(m);
    }
#define TEST(f, init)                                                                              \
    if (!m->f) {                                                                                   \
        if (!nothing)                                                                              \
            record_warning("%s: field `%s' not defined", "LC_MONETARY", #f);                       \
        m->f = init;                                                                               \
    }
    TEST(int_curr_symbol, "");
    size_t l = strlen(m->int_curr_symbol);
    if (l != 4 && l != 0 && !nothing)
        record_error(0, "%s: value of field `int_curr_symbol' has wrong length", "LC_MONETARY");
    TEST(currency_symbol, "");
    if (!m->mon_decimal_point) {
        if (!nothing)
            record_warning("%s: field `%s' not defined, using defaults", "LC_MONETARY", "mon_decimal_point");
        m->mon_decimal_point = ".";
        m->mon_decimal_point_wc = '.';
    }
    if (!m->mon_thousands_sep) {
        if (!nothing)
            record_warning("%s: field `%s' not defined, using defaults", "LC_MONETARY", "mon_thousands_sep");
        m->mon_thousands_sep = "";
        m->mon_thousands_sep_wc = 0;
    }
    if (m->mon_grouping_len == 0) {
        if (!nothing)
            record_warning("%s: field `%s' not defined", "LC_MONETARY", "mon_grouping");
        m->mon_grouping = "\177";
        m->mon_grouping_len = 1;
    }
    TEST(positive_sign, "");
    TEST(negative_sign, "");
#undef TEST
#define TEST(f, min, max, init)                                                                    \
    if (m->f == -2) {                                                                              \
        if (!nothing)                                                                              \
            record_warning("%s: field `%s' not defined", "LC_MONETARY", #f);                       \
        m->f = init;                                                                               \
    } else if ((m->f < min || m->f > max) && min < max && !quiet && !nothing)                     \
        record_error(0, "%s: value for field `%s' must be in range %d...%d", "LC_MONETARY", #f, min, max);
    TEST(int_frac_digits, 1, 0, -1);
    TEST(frac_digits, 1, 0, -1);
    TEST(p_cs_precedes, -1, 1, -1);
    TEST(p_sep_by_space, -1, 2, -1);
    TEST(n_cs_precedes, -1, 1, -1);
    TEST(n_sep_by_space, -1, 2, -1);
    TEST(p_sign_posn, -1, 4, -1);
    TEST(n_sign_posn, -1, 4, -1);
#undef TEST
    m->crncystr = xmalloc(strlen(m->currency_symbol) + 2);
    m->crncystr[0] = m->p_cs_precedes ? '-' : '+';
    strcpy(&m->crncystr[1], m->currency_symbol);
#define TEST(f, alt, min, max)                                                                     \
    if (m->f == -2)                                                                                \
        m->f = m->alt;                                                                             \
    else if ((m->f < min || m->f > max) && !nothing)                                               \
        record_error(0, "%s: value for field `%s' must be in range %d...%d", "LC_MONETARY", #f, min, max);
    TEST(int_p_cs_precedes, p_cs_precedes, -1, 1);
    TEST(int_p_sep_by_space, p_sep_by_space, -1, 2);
    TEST(int_n_cs_precedes, n_cs_precedes, -1, 1);
    TEST(int_n_sep_by_space, n_sep_by_space, -1, 2);
    TEST(int_p_sign_posn, p_sign_posn, -1, 4);
    TEST(int_n_sign_posn, n_sign_posn, -1, 4);
    if (!m->duo_int_curr_symbol)
        m->duo_int_curr_symbol = m->int_curr_symbol;
    if (!m->duo_currency_symbol)
        m->duo_currency_symbol = m->currency_symbol;
    if (m->duo_int_frac_digits == -2)
        m->duo_int_frac_digits = m->int_frac_digits;
    if (m->duo_frac_digits == -2)
        m->duo_frac_digits = m->frac_digits;
    TEST(duo_p_cs_precedes, p_cs_precedes, -1, 1);
    TEST(duo_p_sep_by_space, p_sep_by_space, -1, 2);
    TEST(duo_n_cs_precedes, n_cs_precedes, -1, 1);
    TEST(duo_n_sep_by_space, n_sep_by_space, -1, 2);
    TEST(duo_int_p_cs_precedes, int_p_cs_precedes, -1, 1);
    TEST(duo_int_p_sep_by_space, int_p_sep_by_space, -1, 2);
    TEST(duo_int_n_cs_precedes, int_n_cs_precedes, -1, 1);
    TEST(duo_int_n_sep_by_space, int_n_sep_by_space, -1, 2);
    TEST(duo_p_sign_posn, p_sign_posn, -1, 4);
    TEST(duo_n_sign_posn, n_sign_posn, -1, 4);
    TEST(duo_int_p_sign_posn, int_p_sign_posn, -1, 4);
    TEST(duo_int_n_sign_posn, int_n_sign_posn, -1, 4);
#undef TEST
    if (m->uno_valid_from == 0)
        m->uno_valid_from = 10101;
    if (m->uno_valid_to == 0)
        m->uno_valid_to = 99991231;
    if (m->duo_valid_from == 0)
        m->duo_valid_from = 10101;
    if (m->duo_valid_to == 0)
        m->duo_valid_to = 99991231;
    if (m->conversion_rate[0] == 0)
        m->conversion_rate[0] = m->conversion_rate[1] = 1;
}

void monetary_output(struct localedef *locale, const struct charmap *cm, const char *out)
{
    struct monetary *m = locale->categories[C_MONETARY];
    struct locfile f;
    init_locale_data(&f, 46);
    add_locale_string(&f, m->int_curr_symbol);
    add_locale_string(&f, m->currency_symbol);
    add_locale_string(&f, m->mon_decimal_point);
    add_locale_string(&f, m->mon_thousands_sep);
    add_locale_raw_data(&f, m->mon_grouping, m->mon_grouping_len);
    add_locale_string(&f, m->positive_sign);
    add_locale_string(&f, m->negative_sign);
    add_locale_char(&f, m->int_frac_digits);
    add_locale_char(&f, m->frac_digits);
    add_locale_char(&f, m->p_cs_precedes);
    add_locale_char(&f, m->p_sep_by_space);
    add_locale_char(&f, m->n_cs_precedes);
    add_locale_char(&f, m->n_sep_by_space);
    add_locale_char(&f, m->p_sign_posn);
    add_locale_char(&f, m->n_sign_posn);
    add_locale_string(&f, m->crncystr);
    add_locale_char(&f, m->int_p_cs_precedes);
    add_locale_char(&f, m->int_p_sep_by_space);
    add_locale_char(&f, m->int_n_cs_precedes);
    add_locale_char(&f, m->int_n_sep_by_space);
    add_locale_char(&f, m->int_p_sign_posn);
    add_locale_char(&f, m->int_n_sign_posn);
    add_locale_string(&f, m->duo_int_curr_symbol);
    add_locale_string(&f, m->duo_currency_symbol);
    add_locale_char(&f, m->duo_int_frac_digits);
    add_locale_char(&f, m->duo_frac_digits);
    add_locale_char(&f, m->duo_p_cs_precedes);
    add_locale_char(&f, m->duo_p_sep_by_space);
    add_locale_char(&f, m->duo_n_cs_precedes);
    add_locale_char(&f, m->duo_n_sep_by_space);
    add_locale_char(&f, m->duo_int_p_cs_precedes);
    add_locale_char(&f, m->duo_int_p_sep_by_space);
    add_locale_char(&f, m->duo_int_n_cs_precedes);
    add_locale_char(&f, m->duo_int_n_sep_by_space);
    add_locale_char(&f, m->duo_p_sign_posn);
    add_locale_char(&f, m->duo_n_sign_posn);
    add_locale_char(&f, m->duo_int_p_sign_posn);
    add_locale_char(&f, m->duo_int_n_sign_posn);
    add_locale_uint32(&f, m->uno_valid_from);
    add_locale_uint32(&f, m->uno_valid_to);
    add_locale_uint32(&f, m->duo_valid_from);
    add_locale_uint32(&f, m->duo_valid_to);
    add_locale_uint32_array(&f, m->conversion_rate, 2);
    add_locale_uint32(&f, m->mon_decimal_point_wc);
    add_locale_uint32(&f, m->mon_thousands_sep_wc);
    add_locale_string(&f, cm->code_set_name);
    write_locale_data(out, C_MONETARY, &f);
}

/* ---- LC_TIME ---- */

struct era_data {
    int32_t direction, offset, start_date[3], stop_date[3];
    const char *name, *format;
    uint32_t *wname, *wformat;
};

struct timecat {
    const char *abday[7], *day[7], *abmon[12], *mon[12], *am_pm[2];
    const uint32_t *wabday[7], *wday[7], *wabmon[12], *wmon[12], *wam_pm[2];
    int abday_defined, day_defined, abmon_defined, mon_defined, am_pm_defined;
    const char *d_t_fmt, *d_fmt, *t_fmt, *t_fmt_ampm;
    const uint32_t *wd_t_fmt, *wd_fmt, *wt_fmt, *wt_fmt_ampm;
    const char **era;
    const uint32_t **wera;
    uint32_t num_era;
    const char *era_year, *era_d_t_fmt, *era_t_fmt, *era_d_fmt;
    const uint32_t *wera_year, *wera_d_t_fmt, *wera_t_fmt, *wera_d_fmt;
    const char *alt_digits[100];
    const uint32_t *walt_digits[100];
    int alt_digits_defined;
    const char *date_fmt;
    const uint32_t *wdate_fmt;
    const char *alt_mon[12], *ab_alt_mon[12];
    const uint32_t *walt_mon[12], *wab_alt_mon[12];
    int alt_mon_defined, ab_alt_mon_defined;
    unsigned char week_ndays;
    uint32_t week_1stday;
    unsigned char week_1stweek, first_weekday, first_workday, cal_direction;
    const char *timezone;
    const uint32_t *wtimezone;
    struct era_data *era_entries;
};

/* abday, day ...: strings separated by ";" */
static bool strarr_field(struct section *s, const char **arr, const uint32_t **warr, int *defined,
                         size_t min, size_t max, const char *fname)
{
    struct token *now = NULL;
    size_t cnt;
    for (cnt = 0; cnt < max; ++cnt) {
        now = next_token(s);
        if (now->tok == tok_eol) {
            if (cnt < min)
                lr_error(s->lr, "%s: too few values for field `%s'", s->name, fname);
            do {
                arr[cnt] = "";
                warr[cnt] = empty_wstr;
            } while (++cnt < max);
            break;
        } else if (now->tok != tok_string) {
            return false;
        } else if (!now->str.startmb || !now->str.startwc) {
            lr_error(s->lr, "%s: unknown character in field `%s'", s->name, fname);
            arr[cnt] = "";
            warr[cnt] = empty_wstr;
        } else {
            arr[cnt] = now->str.startmb;
            warr[cnt] = now->str.startwc;
        }
        now = next_token(s);
        if (now->tok != tok_semicolon && now->tok != tok_eol)
            break;
    }
    if (now->tok != tok_eol) {
        while (cnt < min) {
            arr[cnt] = "";
            warr[cnt++] = empty_wstr;
        }
        if (now->tok == tok_semicolon) {
            now = next_token(s);
            if (now->tok == tok_eol)
                lr_error(s->lr, "extra trailing semicolon");
            else if (now->tok == tok_string) {
                lr_error(s->lr, "%s: too many values for field `%s'", s->name, fname);
                lr_ignore_rest(s->lr, 0);
            } else
                return false;
        } else
            return false;
    }
    *defined = 1;
    return true;
}

static bool int_field_t(struct section *s, unsigned char *field, const char *fname)
{
    struct token *now = next_token(s);
    if (now->tok != tok_number)
        return false;
    if (*field != 0)
        lr_error(s->lr, "%s: field `%s' declared more than once", s->name, fname);
    else
        *field = (unsigned char)now->num;
    return true;
}

void time_read(struct lexer *lr, struct localedef *result, const struct charmap *cm,
               const char *repertoire_name, int ignore)
{
    struct section s = { lr, result, cm, NULL, ignore, "LC_TIME" };
    struct token *now;
    if (repertoire_name)
        s.rep = repertoire_read(repertoire_name);
    if (!section_start(&s, &now, tok_lc_time, C_TIME))
        return;
    struct timecat *t = NULL;
    if (!ignore)
        t = result->categories[C_TIME] = xcalloc(1, sizeof *t);
    lr->translate_strings = 1;
    lr->return_widestr = 1;
    for (; now->tok != tok_eof; now = next_token(&s)) {
        if (now->tok == tok_eol)
            continue;
        if (ignore && now->tok != tok_end) {
            lr_ignore_rest(lr, 0);
            continue;
        }
        bool ok = true;
        switch (now->tok) {
#define A(f, min, max)                                                                             \
    case tok_##f:                                                                                  \
        ok = strarr_field(&s, t->f, t->w##f, &t->f##_defined, min, max, #f);                       \
        break;
        A(abday, 7, 7) A(day, 7, 7) A(abmon, 12, 12) A(mon, 12, 12) A(am_pm, 2, 2)
        A(alt_digits, 0, 100) A(alt_mon, 12, 12) A(ab_alt_mon, 12, 12)
#undef A
        case tok_era:
            do {
                now = next_token(&s);
                if (now->tok != tok_string) {
                    ok = false;
                    break;
                }
                if (!now->str.startmb || !now->str.startwc) {
                    lr_error(lr, "%s: unknown character in field `%s'", s.name, "era");
                    lr_ignore_rest(lr, 0);
                    break;
                }
                t->era = xrealloc(t->era, (t->num_era + 1) * sizeof *t->era);
                t->wera = xrealloc(t->wera, (t->num_era + 1) * sizeof *t->wera);
                t->era[t->num_era] = now->str.startmb;
                t->wera[t->num_era++] = now->str.startwc;
                now = next_token(&s);
                if (now->tok != tok_eol && now->tok != tok_semicolon) {
                    ok = false;
                    break;
                }
            } while (now->tok == tok_semicolon);
            break;
#define S(f)                                                                                       \
    case tok_##f:                                                                                  \
        ok = str_field(&s, &t->f, &t->w##f, #f);                                                   \
        break;
        S(d_t_fmt) S(d_fmt) S(t_fmt) S(t_fmt_ampm) S(era_year) S(era_d_t_fmt) S(era_d_fmt)
        S(era_t_fmt) S(timezone) S(date_fmt)
#undef S
        case tok_first_weekday: ok = int_field_t(&s, &t->first_weekday, "first_weekday"); break;
        case tok_first_workday: ok = int_field_t(&s, &t->first_workday, "first_workday"); break;
        case tok_cal_direction: ok = int_field_t(&s, &t->cal_direction, "cal_direction"); break;
        case tok_week:
            now = next_token(&s);
            if (now->tok != tok_number) {
                ok = false;
                break;
            }
            t->week_ndays = (unsigned char)now->num;
            now = next_token(&s);
            if (now->tok != tok_semicolon) {
                ok = false;
                break;
            }
            now = next_token(&s);
            if (now->tok != tok_number) {
                ok = false;
                break;
            }
            t->week_1stday = (uint32_t)now->num;
            now = next_token(&s);
            if (now->tok != tok_semicolon) {
                ok = false;
                break;
            }
            now = next_token(&s);
            if (now->tok != tok_number) {
                ok = false;
                break;
            }
            t->week_1stweek = (unsigned char)now->num;
            lr_ignore_rest(lr, 1);
            break;
        case tok_end:
            if (section_end(&s, tok_lc_time)) {
                if (!ignore && !t->alt_mon_defined) {
                    memcpy(t->alt_mon, t->mon, sizeof t->mon);
                    memcpy(t->walt_mon, t->wmon, sizeof t->wmon);
                    t->alt_mon_defined = 1;
                }
                if (!ignore && !t->ab_alt_mon_defined) {
                    memcpy(t->ab_alt_mon, t->abmon, sizeof t->abmon);
                    memcpy(t->wab_alt_mon, t->wabmon, sizeof t->wabmon);
                    t->ab_alt_mon_defined = 1;
                }
                return;
            }
            continue;
        default: ok = false;
        }
        if (!ok)
            SYNTAX_ERROR(lr, "%s: syntax error", s.name);
    }
    lr_error(lr, "%s: premature end of file", s.name);
}

static const uint32_t *u32(const char *s)
{
    size_t n = strlen(s);
    uint32_t *w = xmalloc((n + 1) * sizeof *w);
    for (size_t i = 0; i <= n; i++)
        w[i] = (unsigned char)s[i];
    return w;
}

static uint32_t *wcschr32(uint32_t *s, uint32_t c)
{
    for (; *s; s++)
        if (*s == c)
            return s;
    return NULL;
}

static bool isleap(int32_t y)
{
    return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0);
}

/* the start or stop date of an era: "-*", "+*" or y/m/d */
static char *era_date(char *str, int32_t *date, size_t idx, const char *which)
{
    static const int days_per_month[12] = { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    char *endp;
    if (strncmp(str, "-*", 2) == 0 || strncmp(str, "+*", 2) == 0) {
        date[0] = date[1] = date[2] = str[0] == '-' ? (int32_t)0x80000000 : 0x7fffffff;
        if (str[2] != ':') {
            record_error(0, "%s: garbage at end of %s date in string %zu in `era' field", "LC_TIME", which, idx + 1);
            (void)str_sep(&str, ":");
            return str;
        }
        return str + 3;
    }
    date[0] = (int32_t)strtol(str, &endp, 10);
    if (endp == str || *endp != '/')
        goto invalid;
    str = endp + 1;
    date[0] -= 1900;
    if (date[0] < -1900)
        ++date[0];
    date[1] = (int32_t)strtol(str, &endp, 10);
    if (endp == str || *endp != '/')
        goto invalid;
    str = endp + 1;
    date[1] -= 1;
    date[2] = (int32_t)strtol(str, &endp, 10);
    if (endp == str) {
    invalid:
        record_error(0, "%s: invalid %s date in string %zu in `era' field", "LC_TIME", which, idx + 1);
        (void)str_sep(&str, ":");
        return str;
    }
    if (*endp != ':') {
        record_error(0, "%s: garbage at end of %s date in string %zu in `era' field", "LC_TIME", which, idx + 1);
        (void)str_sep(&str, ":");
        return str;
    }
    str = endp + 1;
    if (date[1] < 0 || date[1] >= 12 || date[2] < 0 || date[2] > days_per_month[date[1]] ||
        (date[1] == 2 && date[2] == 29 && !isleap(date[0])))
        record_error(0, "%s: %s date is invalid in string %zu in `era' field", "LC_TIME", which, idx + 1);
    return str;
}

void time_finish(struct localedef *locale, const struct charmap *cm)
{
    struct timecat *t = resolve_copy(locale, C_TIME, cm);
    bool nothing = false;
    if (!t)
        t = missing(locale, C_TIME, sizeof *t, &nothing);
#define TESTARR(f, ...)                                                                            \
    if (!t->f##_defined) {                                                                         \
        static const char *const init[] = { __VA_ARGS__ };                                         \
        if (!nothing)                                                                              \
            record_error(0, "%s: field `%s' not defined", "LC_TIME", #f);                          \
        for (size_t i = 0; i < sizeof init / sizeof init[0]; ++i)                                  \
            t->f[i] = init[i];                                                                     \
    }
    TESTARR(abday, "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat");
    TESTARR(day, "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday");
    TESTARR(abmon, "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec");
    TESTARR(mon, "January", "February", "March", "April", "May", "June", "July", "August", "September",
            "October", "November", "December");
    TESTARR(am_pm, "AM", "PM");
#undef TESTARR
#define TEST(f, init)                                                                              \
    if (!t->f) {                                                                                   \
        if (!nothing)                                                                              \
            record_error(0, "%s: field `%s' not defined", "LC_TIME", #f);                          \
        t->f = init;                                                                               \
    }
    TEST(d_t_fmt, "%a %b %e %H:%M:%S %Y");
    TEST(d_fmt, "%m/%d/%y");
    TEST(t_fmt, "%H:%M:%S");
#undef TEST
    if (!t->t_fmt_ampm) {
        if (t->am_pm[0][0] == '\0' && t->am_pm[1][0] == '\0') {
            t->t_fmt_ampm = t->t_fmt;
            t->wt_fmt_ampm = t->wt_fmt;
        } else {
            t->t_fmt_ampm = "%I:%M:%S %p";
            t->wt_fmt_ampm = u32("%I:%M:%S %p");
        }
    }
    if (t->num_era) {
        t->era_entries = xcalloc(t->num_era, sizeof *t->era_entries);
        for (size_t idx = 0; idx < t->num_era; ++idx) {
            struct era_data *e = &t->era_entries[idx];
            char *str = xstrdup(t->era[idx]), *endp;
            if (*str != '+' && *str != '-') {
                record_error(0, "%s: direction flag in string %zu in `era' field is not '+' nor '-'", "LC_TIME", idx + 1);
                e->direction = '+';
            } else
                e->direction = *str;
            if (*++str != ':') {
                record_error(0, "%s: direction flag in string %zu in `era' field is not a single character", "LC_TIME", idx + 1);
                (void)str_sep(&str, ":");
            } else
                ++str;
            e->offset = (int32_t)strtol(str, &endp, 10);
            if (endp == str) {
                record_error(0, "%s: invalid number for offset in string %zu in `era' field", "LC_TIME", idx + 1);
                (void)str_sep(&str, ":");
            } else if (*endp != ':') {
                record_error(0, "%s: garbage at end of offset value in string %zu in `era' field", "LC_TIME", idx + 1);
                (void)str_sep(&str, ":");
            } else
                str = endp + 1;
            if (str)
                str = era_date(str, e->start_date, idx, "starting");
            if (str)
                str = era_date(str, e->stop_date, idx, "stopping");
            if (!str || *str == '\0') {
                record_error(0, "%s: missing era name in string %zu in `era' field", "LC_TIME", idx + 1);
                e->name = e->format = "";
            } else {
                e->name = str_sep(&str, ":");
                if (!str || *str == '\0') {
                    record_error(0, "%s: missing era format in string %zu in `era' field", "LC_TIME", idx + 1);
                    e->name = e->format = "";
                } else
                    e->format = str;
            }
            /* the wide name and format */
            uint32_t *w = xmalloc((wcslen32(t->wera[idx]) + 1) * sizeof *w);
            memcpy(w, t->wera[idx], (wcslen32(t->wera[idx]) + 1) * sizeof *w);
            uint32_t *ws = wcschr32(w, ':');
            ws = ws ? wcschr32(ws + 1, ':') : NULL;
            ws = ws ? wcschr32(ws + 1, ':') : NULL;
            ws = ws ? wcschr32(ws + 1, ':') : NULL;
            static uint32_t empty[1] = { 0 };
            if (ws) {
                e->wname = ws + 1;
                ws = wcschr32(ws + 1, ':');
                if (ws) {
                    *ws = 0;
                    e->wformat = ws + 1;
                } else
                    e->wname = e->wformat = empty;
            } else
                e->wname = e->wformat = empty;
        }
    }
    if (t->week_ndays == 0)
        t->week_ndays = 7;
    if (t->week_1stday == 0)
        t->week_1stday = 19971130;
    if (t->week_1stweek == 0)
        t->week_1stweek = 7;
    if (t->week_1stweek > t->week_ndays)
        record_error(0, "%s: third operand for value of field `%s' must not be larger than %d", "LC_TIME", "week", 7);
    if (t->first_weekday == 0)
        t->first_weekday = 1;
    else if (t->first_weekday > t->week_ndays)
        record_error(0, "%s: values for field `%s' must not be larger than %d", "LC_TIME", "first_weekday", 7);
    if (t->first_workday == 0)
        t->first_workday = 2;
    else if (t->first_workday > t->week_ndays)
        record_error(0, "%s: values for field `%s' must not be larger than %d", "LC_TIME", "first_workday", 7);
    if (t->cal_direction == 0)
        t->cal_direction = 1;
    else if (t->cal_direction > 3)
        record_error(0, "%s: values for field `%s' must not be larger than %d", "LC_TIME", "cal_direction", 3);
    if (!t->timezone)
        t->timezone = "";
    if (!t->date_fmt)
        t->date_fmt = "%a %b %e %H:%M:%S %Z %Y";
    if (!t->wdate_fmt)
        t->wdate_fmt = u32("%a %b %e %H:%M:%S %Z %Y");
}

static const char *nz(const char *s)
{
    return s ? s : "";
}

static const uint32_t *wnz(const uint32_t *s)
{
    return s ? s : empty_wstr;
}

void time_output(struct localedef *locale, const struct charmap *cm, const char *out)
{
    struct timecat *t = locale->categories[C_TIME];
    struct locfile f;
    size_t n;
    init_locale_data(&f, 159);
    for (n = 0; n < 7; ++n)
        add_locale_string(&f, nz(t->abday[n]));
    for (n = 0; n < 7; ++n)
        add_locale_string(&f, nz(t->day[n]));
    for (n = 0; n < 12; ++n)
        add_locale_string(&f, nz(t->abmon[n]));
    for (n = 0; n < 12; ++n)
        add_locale_string(&f, nz(t->mon[n]));
    for (n = 0; n < 2; ++n)
        add_locale_string(&f, t->am_pm[n]);
    add_locale_string(&f, nz(t->d_t_fmt));
    add_locale_string(&f, nz(t->d_fmt));
    add_locale_string(&f, nz(t->t_fmt));
    add_locale_string(&f, nz(t->t_fmt_ampm));
    start_locale_structure(&f);
    for (n = 0; n < t->num_era; ++n)
        add_locale_string(&f, t->era[n]);
    end_locale_structure(&f);
    add_locale_string(&f, nz(t->era_year));
    add_locale_string(&f, nz(t->era_d_fmt));
    start_locale_structure(&f);
    for (n = 0; n < 100; ++n)
        add_locale_string(&f, nz(t->alt_digits[n]));
    end_locale_structure(&f);
    add_locale_string(&f, nz(t->era_d_t_fmt));
    add_locale_string(&f, nz(t->era_t_fmt));
    add_locale_uint32(&f, t->num_era);
    start_locale_structure(&f);
    for (n = 0; n < t->num_era; ++n) {
        struct era_data *e = &t->era_entries[n];
        add_locale_uint32(&f, (uint32_t)e->direction);
        add_locale_uint32(&f, (uint32_t)e->offset);
        add_locale_uint32(&f, (uint32_t)e->start_date[0]);
        add_locale_uint32(&f, (uint32_t)e->start_date[1]);
        add_locale_uint32(&f, (uint32_t)e->start_date[2]);
        add_locale_uint32(&f, (uint32_t)e->stop_date[0]);
        add_locale_uint32(&f, (uint32_t)e->stop_date[1]);
        add_locale_uint32(&f, (uint32_t)e->stop_date[2]);
        add_locale_string(&f, e->name);
        add_locale_string(&f, e->format);
        add_locale_wstring(&f, e->wname);
        add_locale_wstring(&f, e->wformat);
    }
    end_locale_structure(&f);
    for (n = 0; n < 7; ++n)
        add_locale_wstring(&f, wnz(t->wabday[n]));
    for (n = 0; n < 7; ++n)
        add_locale_wstring(&f, wnz(t->wday[n]));
    for (n = 0; n < 12; ++n)
        add_locale_wstring(&f, wnz(t->wabmon[n]));
    for (n = 0; n < 12; ++n)
        add_locale_wstring(&f, wnz(t->wmon[n]));
    for (n = 0; n < 2; ++n)
        add_locale_wstring(&f, wnz(t->wam_pm[n]));
    add_locale_wstring(&f, wnz(t->wd_t_fmt));
    add_locale_wstring(&f, wnz(t->wd_fmt));
    add_locale_wstring(&f, wnz(t->wt_fmt));
    add_locale_wstring(&f, wnz(t->wt_fmt_ampm));
    add_locale_wstring(&f, wnz(t->wera_year));
    add_locale_wstring(&f, wnz(t->wera_d_fmt));
    start_locale_structure(&f);
    for (n = 0; n < 100; ++n)
        add_locale_wstring(&f, wnz(t->walt_digits[n]));
    end_locale_structure(&f);
    add_locale_wstring(&f, wnz(t->wera_d_t_fmt));
    add_locale_wstring(&f, wnz(t->wera_t_fmt));
    add_locale_char(&f, (char)t->week_ndays);
    add_locale_uint32(&f, t->week_1stday);
    add_locale_char(&f, (char)t->week_1stweek);
    add_locale_char(&f, (char)t->first_weekday);
    add_locale_char(&f, (char)t->first_workday);
    add_locale_char(&f, (char)t->cal_direction);
    add_locale_string(&f, t->timezone);
    add_locale_string(&f, t->date_fmt);
    add_locale_wstring(&f, t->wdate_fmt);
    add_locale_string(&f, cm->code_set_name);
    for (n = 0; n < 12; ++n)
        add_locale_string(&f, nz(t->alt_mon[n]));
    for (n = 0; n < 12; ++n)
        add_locale_wstring(&f, wnz(t->walt_mon[n]));
    for (n = 0; n < 12; ++n)
        add_locale_string(&f, nz(t->ab_alt_mon[n]));
    for (n = 0; n < 12; ++n)
        add_locale_wstring(&f, wnz(t->wab_alt_mon[n]));
    write_locale_data(out, C_TIME, &f);
}

/* ---- categories of strings: LC_MESSAGES, LC_NAME, LC_TELEPHONE,
   LC_ADDRESS, LC_IDENTIFICATION (and the numbers of LC_PAPER,
   LC_MEASUREMENT, LC_ADDRESS) ---- */

struct field {
    enum tok tok;
    const char *name;
    int kind;                   /* 0 string, 1 number, 2 number or string */
};

/* a section of fields: the values (strings, or numbers) by index */
static void fields_read(struct section *s, enum tok cattok, int category, const struct field *fields,
                        size_t nfields, size_t size, const char ***strs, unsigned long **nums,
                        const char **cats)
{
    struct token *now;
    if (!section_start(s, &now, cattok, category))
        return;
    struct lexer *lr = s->lr;
    if (!s->ignore) {
        void *p = xcalloc(1, size);
        s->result->categories[category] = p;
        *strs = (const char **)p;
        *nums = (unsigned long *)((char *)p + nfields * sizeof(const char *));
    }
    lr->translate_strings = 1;
    lr->return_widestr = 0;
    for (; now->tok != tok_eof; now = next_token(s)) {
        if (now->tok == tok_eol)
            continue;
        if (s->ignore && now->tok != tok_end) {
            lr_ignore_rest(lr, 0);
            continue;
        }
        if (now->tok == tok_end) {
            if (section_end(s, cattok))
                return;
            continue;
        }
        bool ok = false;
        if (now->tok == tok_category && cats) {
            /* category "standard";LC_xxx */
            struct token *arg = next_token(s);
            if (arg->tok == tok_string || arg->tok == tok_ident) {
                const char *val = arg->str.startmb;
                struct token *c = next_token(s);
                if (c->tok == tok_semicolon) {
                    c = next_token(s);
                    int cat = -1;
                    switch (c->tok) {
                    case tok_lc_identification: cat = C_IDENTIFICATION; break;
                    case tok_lc_ctype: cat = C_CTYPE; break;
                    case tok_lc_collate: cat = C_COLLATE; break;
                    case tok_lc_time: cat = C_TIME; break;
                    case tok_lc_numeric: cat = C_NUMERIC; break;
                    case tok_lc_monetary: cat = C_MONETARY; break;
                    case tok_lc_messages: cat = C_MESSAGES; break;
                    case tok_lc_paper: cat = C_PAPER; break;
                    case tok_lc_name: cat = C_NAME; break;
                    case tok_lc_address: cat = C_ADDRESS; break;
                    case tok_lc_telephone: cat = C_TELEPHONE; break;
                    case tok_lc_measurement: cat = C_MEASUREMENT; break;
                    default: break;
                    }
                    if (cat >= 0) {
                        if (cats[cat])
                            lr_error(lr, "%s: duplicate category version definition", s->name);
                        else
                            cats[cat] = val;
                        ok = true;
                    }
                }
            }
        } else {
            for (size_t i = 0; i < nfields; i++) {
                if (fields[i].tok != now->tok)
                    continue;
                struct token *arg = next_token(s);
                if (fields[i].kind == 1) {
                    if (arg->tok != tok_number)
                        break;
                    if ((*nums)[i] != 0)
                        lr_error(lr, "%s: field `%s' declared more than once", s->name, fields[i].name);
                    else
                        (*nums)[i] = arg->num;
                    ok = true;
                    break;
                }
                if (arg->tok != tok_string && !(fields[i].kind == 2 && arg->tok == tok_number))
                    break;
                if ((*strs)[i])
                    lr_error(lr, "%s: field `%s' declared more than once", s->name, fields[i].name);
                else if (arg->tok == tok_number)
                    (*strs)[i] = xasprintf("%lu", arg->num);
                else if (!arg->str.startmb) {
                    lr_error(lr, "%s: unknown character in field `%s'", s->name, fields[i].name);
                    (*strs)[i] = "";
                } else
                    (*strs)[i] = arg->str.startmb;
                ok = true;
                break;
            }
        }
        if (!ok)
            SYNTAX_ERROR(lr, "%s: syntax error", s->name);
    }
    lr_error(lr, "%s: premature end of file", s->name);
}

/* a category of fields: strings, then numbers, then (identification)
   the category versions */
#define FIELDS_STRUCT(name, n)                                                                     \
    struct name {                                                                                  \
        const char *s[n];                                                                          \
        unsigned long v[n];                                                                        \
        const char *cat[C_LAST];                                                                   \
    }

static void *fields_category(struct localedef *locale, int category, size_t size, const struct charmap *cm,
                             bool *nothing)
{
    void *p = resolve_copy(locale, category, cm);
    *nothing = false;
    if (!p)
        p = missing(locale, category, size, nothing);
    return p;
}

/* LC_MESSAGES */

enum { M_YESEXPR, M_NOEXPR, M_YESSTR, M_NOSTR, M_N };
static const struct field messages_fields[] = {
    { tok_yesexpr, "yesexpr", 0 }, { tok_noexpr, "noexpr", 0 }, { tok_yesstr, "yesstr", 0 },
    { tok_nostr, "nostr", 0 },
};
FIELDS_STRUCT(messages, M_N);

void messages_read(struct lexer *lr, struct localedef *result, const struct charmap *cm,
                   const char *repertoire_name, int ignore)
{
    struct section s = { lr, result, cm, NULL, ignore, "LC_MESSAGES" };
    const char **strs = NULL;
    unsigned long *nums = NULL;
    if (repertoire_name)
        s.rep = repertoire_read(repertoire_name);
    fields_read(&s, tok_lc_messages, C_MESSAGES, messages_fields, M_N, sizeof(struct messages), &strs,
                &nums, NULL);
}

static void check_regex(const char *re, const char *fname)
{
    regex_t r;
    int res = regcomp(&r, re, REG_EXTENDED);
    if (res != 0 && !quiet) {
        char buf[256];
        regerror(res, &r, buf, sizeof buf);
        record_error(0, "%s: no correct regular expression for field `%s': %s", "LC_MESSAGES", fname, buf);
    } else if (res == 0)
        regfree(&r);
}

void messages_finish(struct localedef *locale, const struct charmap *cm)
{
    bool nothing;
    struct messages *m = fields_category(locale, C_MESSAGES, sizeof *m, cm, &nothing);
    if (!m->s[M_YESSTR])
        m->s[M_YESSTR] = "";
    if (!m->s[M_NOSTR])
        m->s[M_NOSTR] = "";
    for (int i = M_YESEXPR; i <= M_NOEXPR; i++) {
        const char *n = messages_fields[i].name;
        if (!m->s[i]) {
            if (!nothing)
                record_error(0, "%s: field `%s' undefined", "LC_MESSAGES", n);
            m->s[i] = i == M_YESEXPR ? "^[yY]" : "^[nN]";
        } else if (m->s[i][0] == '\0') {
            record_error(0, "%s: value for field `%s' must not be an empty string", "LC_MESSAGES", n);
        } else {
            check_regex(m->s[i], n);
        }
    }
}

void messages_output(struct localedef *locale, const struct charmap *cm, const char *out)
{
    struct messages *m = locale->categories[C_MESSAGES];
    struct locfile f;
    init_locale_data(&f, 5);
    for (int i = 0; i < M_N; i++)
        add_locale_string(&f, m->s[i]);
    add_locale_string(&f, cm->code_set_name);
    write_locale_data(out, C_MESSAGES, &f);
}

/* LC_PAPER */

enum { P_HEIGHT, P_WIDTH, P_N };
static const struct field paper_fields[] = { { tok_height, "height", 1 }, { tok_width, "width", 1 } };
FIELDS_STRUCT(paper, P_N);

void paper_read(struct lexer *lr, struct localedef *result, const struct charmap *cm,
                const char *repertoire_name, int ignore)
{
    struct section s = { lr, result, cm, NULL, ignore, "LC_PAPER" };
    const char **strs = NULL;
    unsigned long *nums = NULL;
    (void)repertoire_name;
    fields_read(&s, tok_lc_paper, C_PAPER, paper_fields, P_N, sizeof(struct paper), &strs, &nums, NULL);
}

void paper_finish(struct localedef *locale, const struct charmap *cm)
{
    bool nothing;
    struct paper *p = fields_category(locale, C_PAPER, sizeof *p, cm, &nothing);
    if (p->v[P_HEIGHT] == 0) {
        if (!nothing)
            record_error(0, "%s: field `%s' not defined", "LC_PAPER", "height");
        p->v[P_HEIGHT] = 297;
    }
    if (p->v[P_WIDTH] == 0) {
        if (!nothing)
            record_error(0, "%s: field `%s' not defined", "LC_PAPER", "width");
        p->v[P_WIDTH] = 210;
    }
}

void paper_output(struct localedef *locale, const struct charmap *cm, const char *out)
{
    struct paper *p = locale->categories[C_PAPER];
    struct locfile f;
    init_locale_data(&f, 3);
    add_locale_uint32(&f, (uint32_t)p->v[P_HEIGHT]);
    add_locale_uint32(&f, (uint32_t)p->v[P_WIDTH]);
    add_locale_string(&f, cm->code_set_name);
    write_locale_data(out, C_PAPER, &f);
}

/* LC_MEASUREMENT */

static const struct field measurement_fields[] = { { tok_measurement, "measurement", 1 } };
FIELDS_STRUCT(measurement, 1);

void measurement_read(struct lexer *lr, struct localedef *result, const struct charmap *cm,
                      const char *repertoire_name, int ignore)
{
    struct section s = { lr, result, cm, NULL, ignore, "LC_MEASUREMENT" };
    const char **strs = NULL;
    unsigned long *nums = NULL;
    (void)repertoire_name;
    fields_read(&s, tok_lc_measurement, C_MEASUREMENT, measurement_fields, 1, sizeof(struct measurement),
                &strs, &nums, NULL);
}

void measurement_finish(struct localedef *locale, const struct charmap *cm)
{
    bool nothing;
    struct measurement *m = fields_category(locale, C_MEASUREMENT, sizeof *m, cm, &nothing);
    if (m->v[0] == 0) {
        if (!nothing)
            record_error(0, "%s: field `%s' not defined", "LC_MEASUREMENT", "measurement");
        m->v[0] = 1;
    } else if (m->v[0] > 3)
        record_error(0, "%s: invalid value for field `%s'", "LC_MEASUREMENT", "measurement");
}

void measurement_output(struct localedef *locale, const struct charmap *cm, const char *out)
{
    struct measurement *m = locale->categories[C_MEASUREMENT];
    struct locfile f;
    init_locale_data(&f, 2);
    add_locale_char(&f, (char)m->v[0]);
    add_locale_string(&f, cm->code_set_name);
    write_locale_data(out, C_MEASUREMENT, &f);
}

/* LC_NAME */

enum { N_FMT, N_GEN, N_MR, N_MRS, N_MISS, N_MS, N_N };
static const struct field name_fields[] = {
    { tok_name_fmt, "name_fmt", 0 }, { tok_name_gen, "name_gen", 0 }, { tok_name_mr, "name_mr", 0 },
    { tok_name_mrs, "name_mrs", 0 }, { tok_name_miss, "name_miss", 0 }, { tok_name_ms, "name_ms", 0 },
};
FIELDS_STRUCT(namecat, N_N);

void name_read(struct lexer *lr, struct localedef *result, const struct charmap *cm,
               const char *repertoire_name, int ignore)
{
    struct section s = { lr, result, cm, NULL, ignore, "LC_NAME" };
    const char **strs = NULL;
    unsigned long *nums = NULL;
    (void)repertoire_name;
    fields_read(&s, tok_lc_name, C_NAME, name_fields, N_N, sizeof(struct namecat), &strs, &nums, NULL);
}

/* a format of %-escapes from a set (and %R before one, when romanize) */
static void check_fmt(const char *fmt, const char *set, bool romanize, const char *cat, const char *fname)
{
    for (const char *cp = fmt; *cp; cp++) {
        if (*cp != '%')
            continue;
        if (romanize && cp[1] == 'R')
            ++cp;
        if (!*++cp || !strchr(set, *cp)) {
            record_error(0, "%s: invalid escape sequence in field `%s'", cat, fname);
            return;
        }
    }
}

void name_finish(struct localedef *locale, const struct charmap *cm)
{
    bool nothing;
    struct namecat *n = fields_category(locale, C_NAME, sizeof *n, cm, &nothing);
    if (!n->s[N_FMT]) {
        if (!nothing)
            record_error(0, "%s: field `%s' not defined", "LC_NAME", "name_fmt");
        n->s[N_FMT] = "%p%t%g%t%m%t%f";
    } else if (n->s[N_FMT][0] == '\0')
        record_error(0, "%s: field `%s' must not be empty", "LC_NAME", "name_fmt");
    else
        check_fmt(n->s[N_FMT], "dfFgGlomMpsSt", true, "LC_NAME", "name_fmt");
    for (int i = N_GEN; i < N_N; i++)
        if (!n->s[i])
            n->s[i] = "";
}

void name_output(struct localedef *locale, const struct charmap *cm, const char *out)
{
    struct namecat *n = locale->categories[C_NAME];
    struct locfile f;
    init_locale_data(&f, 7);
    for (int i = 0; i < N_N; i++)
        add_locale_string(&f, n->s[i]);
    add_locale_string(&f, cm->code_set_name);
    write_locale_data(out, C_NAME, &f);
}

/* LC_TELEPHONE */

enum { T_INT_FMT, T_DOM_FMT, T_INT_SELECT, T_INT_PREFIX, T_N };
static const struct field telephone_fields[] = {
    { tok_tel_int_fmt, "tel_int_fmt", 0 }, { tok_tel_dom_fmt, "tel_dom_fmt", 0 },
    { tok_int_select, "int_select", 0 }, { tok_int_prefix, "int_prefix", 0 },
};
FIELDS_STRUCT(telephone, T_N);

void telephone_read(struct lexer *lr, struct localedef *result, const struct charmap *cm,
                    const char *repertoire_name, int ignore)
{
    struct section s = { lr, result, cm, NULL, ignore, "LC_TELEPHONE" };
    const char **strs = NULL;
    unsigned long *nums = NULL;
    (void)repertoire_name;
    fields_read(&s, tok_lc_telephone, C_TELEPHONE, telephone_fields, T_N, sizeof(struct telephone), &strs,
                &nums, NULL);
}

void telephone_finish(struct localedef *locale, const struct charmap *cm)
{
    bool nothing;
    struct telephone *t = fields_category(locale, C_TELEPHONE, sizeof *t, cm, &nothing);
    if (!t->s[T_INT_FMT]) {
        if (!nothing)
            record_error(0, "%s: field `%s' not defined", "LC_TELEPHONE", "tel_int_fmt");
        t->s[T_INT_FMT] = "+%c %a%t%l";
    } else if (t->s[T_INT_FMT][0] == '\0')
        record_error(0, "%s: field `%s' must not be empty", "LC_TELEPHONE", "tel_int_fmt");
    else
        check_fmt(t->s[T_INT_FMT], "aAcCelt", false, "LC_TELEPHONE", "tel_int_fmt");
    if (!t->s[T_DOM_FMT])
        t->s[T_DOM_FMT] = "";
    else if (t->s[T_DOM_FMT][0])
        check_fmt(t->s[T_DOM_FMT], "aAcCelt", false, "LC_TELEPHONE", "tel_dom_fmt");
    if (!t->s[T_INT_SELECT])
        t->s[T_INT_SELECT] = "";
    if (!t->s[T_INT_PREFIX])
        t->s[T_INT_PREFIX] = "";
}

void telephone_output(struct localedef *locale, const struct charmap *cm, const char *out)
{
    struct telephone *t = locale->categories[C_TELEPHONE];
    struct locfile f;
    init_locale_data(&f, 5);
    for (int i = 0; i < T_N; i++)
        add_locale_string(&f, t->s[i]);
    add_locale_string(&f, cm->code_set_name);
    write_locale_data(out, C_TELEPHONE, &f);
}

/* LC_ADDRESS */

enum {
    A_POSTAL_FMT, A_COUNTRY_NAME, A_COUNTRY_POST, A_COUNTRY_AB2, A_COUNTRY_AB3, A_COUNTRY_CAR,
    A_COUNTRY_NUM, A_COUNTRY_ISBN, A_LANG_NAME, A_LANG_AB, A_LANG_TERM, A_LANG_LIB, A_N
};
static const struct field address_fields[] = {
    { tok_postal_fmt, "postal_fmt", 0 }, { tok_country_name, "country_name", 0 },
    { tok_country_post, "country_post", 0 }, { tok_country_ab2, "country_ab2", 0 },
    { tok_country_ab3, "country_ab3", 0 }, { tok_country_car, "country_car", 0 },
    { tok_country_num, "country_num", 1 }, { tok_country_isbn, "country_isbn", 2 },
    { tok_lang_name, "lang_name", 0 }, { tok_lang_ab, "lang_ab", 0 },
    { tok_lang_term, "lang_term", 0 }, { tok_lang_lib, "lang_lib", 0 },
};
FIELDS_STRUCT(address, A_N);

void address_read(struct lexer *lr, struct localedef *result, const struct charmap *cm,
                  const char *repertoire_name, int ignore)
{
    struct section s = { lr, result, cm, NULL, ignore, "LC_ADDRESS" };
    const char **strs = NULL;
    unsigned long *nums = NULL;
    (void)repertoire_name;
    fields_read(&s, tok_lc_address, C_ADDRESS, address_fields, A_N, sizeof(struct address), &strs, &nums,
                NULL);
}

void address_finish(struct localedef *locale, const struct charmap *cm)
{
    bool nothing;
    struct address *a = fields_category(locale, C_ADDRESS, sizeof *a, cm, &nothing);
    if (!a->s[A_POSTAL_FMT]) {
        if (!nothing)
            record_error(0, "%s: field `%s' not defined", "LC_ADDRESS", "postal_fmt");
        a->s[A_POSTAL_FMT] = "%a%N%f%N%d%N%b%N%s %h %e %r%N%C-%z %T%N%c%N";
    } else if (a->s[A_POSTAL_FMT][0] == '\0')
        record_error(0, "%s: field `%s' must not be empty", "LC_ADDRESS", "postal_fmt");
    else
        check_fmt(a->s[A_POSTAL_FMT], "nafdbshNtreClzTSc%", true, "LC_ADDRESS", "postal_fmt");
    for (int i = A_COUNTRY_NAME; i <= A_LANG_TERM; i++)
        if (i != A_COUNTRY_NUM && i != A_COUNTRY_AB2 && i != A_COUNTRY_AB3 && !a->s[i])
            a->s[i] = "";
    if (!a->s[A_LANG_LIB])
        a->s[A_LANG_LIB] = a->s[A_LANG_TERM];
    if (!a->s[A_COUNTRY_AB2])
        a->s[A_COUNTRY_AB2] = "  ";
    if (!a->s[A_COUNTRY_AB3])
        a->s[A_COUNTRY_AB3] = "   ";
}

void address_output(struct localedef *locale, const struct charmap *cm, const char *out)
{
    struct address *a = locale->categories[C_ADDRESS];
    struct locfile f;
    init_locale_data(&f, 13);
    for (int i = 0; i < A_N; i++) {
        if (i == A_COUNTRY_NUM)
            add_locale_uint32(&f, (uint32_t)a->v[i]);
        else
            add_locale_string(&f, a->s[i]);
    }
    add_locale_string(&f, cm->code_set_name);
    write_locale_data(out, C_ADDRESS, &f);
}

/* LC_IDENTIFICATION */

enum {
    I_TITLE, I_SOURCE, I_ADDRESS, I_CONTACT, I_EMAIL, I_TEL, I_FAX, I_LANGUAGE, I_TERRITORY,
    I_AUDIENCE, I_APPLICATION, I_ABBREVIATION, I_REVISION, I_DATE, I_N
};
static const struct field identification_fields[] = {
    { tok_title, "title", 0 }, { tok_source, "source", 0 }, { tok_address, "address", 0 },
    { tok_contact, "contact", 0 }, { tok_email, "email", 0 }, { tok_tel, "tel", 0 }, { tok_fax, "fax", 0 },
    { tok_language, "language", 0 }, { tok_territory, "territory", 0 }, { tok_audience, "audience", 0 },
    { tok_application, "application", 0 }, { tok_abbreviation, "abbreviation", 0 },
    { tok_revision, "revision", 0 }, { tok_date, "date", 0 },
};
FIELDS_STRUCT(identification, I_N);

void identification_read(struct lexer *lr, struct localedef *result, const struct charmap *cm,
                         const char *repertoire_name, int ignore)
{
    struct section s = { lr, result, cm, NULL, ignore, "LC_IDENTIFICATION" };
    const char **strs = NULL;
    unsigned long *nums = NULL;
    (void)repertoire_name;
    /* the versions of the categories are kept after the numbers */
    const char *cats[C_LAST] = { 0 };
    fields_read(&s, tok_lc_identification, C_IDENTIFICATION, identification_fields, I_N,
                sizeof(struct identification), &strs, &nums, cats);
    struct identification *id = result->categories[C_IDENTIFICATION];
    if (id && !ignore && strs)
        memcpy(id->cat, cats, sizeof cats);
}

void identification_finish(struct localedef *locale, const struct charmap *cm)
{
    bool nothing;
    struct identification *id = fields_category(locale, C_IDENTIFICATION, sizeof *id, cm, &nothing);
    for (int i = 0; i < I_N; i++)
        if (!id->s[i])
            id->s[i] = "";
    static const char *const standards[] = { "posix:1993", "i18n:2004", "i18n:2012" };
    for (int c = 0; c < C_LAST; c++) {
        if (c == C_ALL)
            continue;
        if (!id->cat[c])
            id->cat[c] = "";
        else {
            bool matched = false;
            for (size_t i = 0; i < sizeof standards / sizeof standards[0]; i++)
                if (strcmp(id->cat[c], standards[i]) == 0)
                    matched = true;
            if (!matched)
                record_error(0, "%s: unknown standard `%s' for category `%s'", "LC_IDENTIFICATION",
                             id->cat[c], category_names[c]);
        }
    }
}

void identification_output(struct localedef *locale, const struct charmap *cm, const char *out)
{
    struct identification *id = locale->categories[C_IDENTIFICATION];
    struct locfile f;
    init_locale_data(&f, 16);
    for (int i = 0; i < I_N; i++)
        add_locale_string(&f, id->s[i]);
    start_locale_structure(&f);
    for (int c = 0; c < C_LAST; c++)
        if (c != C_ALL)
            add_locale_string(&f, id->cat[c]);
    end_locale_structure(&f);
    add_locale_string(&f, cm->code_set_name);
    write_locale_data(out, C_IDENTIFICATION, &f);
}
