/*
 * lex.c - the lexer of the locale definition and charmap files, for
 * localedef
 *
 * The tokens are those of glibc's localedef: <names> (<Uxxxx> a UCS4
 * value), identifiers and keywords, numbers, -1, ";", ",", "(", ")",
 * the ellipses "..", "...", "....", "..(2)..", "....(2)....", character
 * codes (\d, \x, \ octal, one or more bytes), strings, and UTF-8
 * characters; the comment and escape characters as set; a line that
 * ends with the escape character goes on to the next.  A string, when
 * strings are translated, is the bytes of its characters in the charmap
 * (startmb: NULL when a character is not in it), and, when asked, their
 * UCS4 values (startwc).
 */

#define _POSIX_C_SOURCE 202405L

#include "localedef.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

/* ---- keywords ---- */

static const struct keyword locfile_keywords[] = {
    { "escape_char", tok_escape_char, 0 }, { "comment_char", tok_comment_char, 0 },
    { "repertoiremap", tok_repertoiremap, 0 }, { "include", tok_include, 0 },
    { "LC_CTYPE", tok_lc_ctype, 0 }, { "END", tok_end, 0 }, { "copy", tok_copy, 0 },
    { "upper", tok_upper, 0 }, { "lower", tok_lower, 0 }, { "alpha", tok_alpha, 0 },
    { "digit", tok_digit, 0 }, { "outdigit", tok_outdigit, 0 }, { "alnum", tok_alnum, 0 },
    { "space", tok_space, 0 }, { "cntrl", tok_cntrl, 0 }, { "punct", tok_punct, 0 },
    { "graph", tok_graph, 0 }, { "print", tok_print, 0 }, { "xdigit", tok_xdigit, 0 },
    { "blank", tok_blank, 0 }, { "charclass", tok_charclass, 0 }, { "class", tok_class, 0 },
    { "charconv", tok_charconv, 0 }, { "toupper", tok_toupper, 0 }, { "tolower", tok_tolower, 0 },
    { "map", tok_map, 0 }, { "translit_start", tok_translit_start, 0 },
    { "translit_end", tok_translit_end, 0 }, { "translit_ignore", tok_translit_ignore, 0 },
    { "default_missing", tok_default_missing, 0 }, { "LC_COLLATE", tok_lc_collate, 0 },
    { "codepoint_collation", tok_codepoint_collation, 0 },
    { "coll_weight_max", tok_coll_weight_max, 0 }, { "section-symbol", tok_section_symbol, 0 },
    { "collating-element", tok_collating_element, 0 },
    { "collating-symbol", tok_collating_symbol, 0 },
    { "symbol-equivalence", tok_symbol_equivalence, 0 }, { "script", tok_script, 0 },
    { "order_start", tok_order_start, 0 }, { "order_end", tok_order_end, 0 },
    { "from", tok_from, 0 }, { "forward", tok_forward, 0 }, { "backward", tok_backward, 0 },
    { "position", tok_position, 0 }, { "UNDEFINED", tok_undefined, 0 },
    { "IGNORE", tok_ignore, 0 }, { "reorder-after", tok_reorder_after, 0 },
    { "reorder-end", tok_reorder_end, 0 },
    { "reorder-sections-after", tok_reorder_sections_after, 0 },
    { "reorder-sections-end", tok_reorder_sections_end, 0 }, { "define", tok_define, 0 },
    { "undef", tok_undef, 0 }, { "ifdef", tok_ifdef, 0 }, { "else", tok_else, 0 },
    { "elifdef", tok_elifdef, 0 }, { "elifndef", tok_elifndef, 0 }, { "endif", tok_endif, 0 },
    { "LC_MONETARY", tok_lc_monetary, 0 }, { "int_curr_symbol", tok_int_curr_symbol, 0 },
    { "currency_symbol", tok_currency_symbol, 0 },
    { "mon_decimal_point", tok_mon_decimal_point, 0 },
    { "mon_thousands_sep", tok_mon_thousands_sep, 0 }, { "mon_grouping", tok_mon_grouping, 0 },
    { "positive_sign", tok_positive_sign, 0 }, { "negative_sign", tok_negative_sign, 0 },
    { "int_frac_digits", tok_int_frac_digits, 0 }, { "frac_digits", tok_frac_digits, 0 },
    { "p_cs_precedes", tok_p_cs_precedes, 0 }, { "p_sep_by_space", tok_p_sep_by_space, 0 },
    { "n_cs_precedes", tok_n_cs_precedes, 0 }, { "n_sep_by_space", tok_n_sep_by_space, 0 },
    { "p_sign_posn", tok_p_sign_posn, 0 }, { "n_sign_posn", tok_n_sign_posn, 0 },
    { "int_p_cs_precedes", tok_int_p_cs_precedes, 0 },
    { "int_p_sep_by_space", tok_int_p_sep_by_space, 0 },
    { "int_n_cs_precedes", tok_int_n_cs_precedes, 0 },
    { "int_n_sep_by_space", tok_int_n_sep_by_space, 0 },
    { "int_p_sign_posn", tok_int_p_sign_posn, 0 }, { "int_n_sign_posn", tok_int_n_sign_posn, 0 },
    { "duo_int_curr_symbol", tok_duo_int_curr_symbol, 0 },
    { "duo_currency_symbol", tok_duo_currency_symbol, 0 },
    { "duo_int_frac_digits", tok_duo_int_frac_digits, 0 },
    { "duo_frac_digits", tok_duo_frac_digits, 0 },
    { "duo_p_cs_precedes", tok_duo_p_cs_precedes, 0 },
    { "duo_p_sep_by_space", tok_duo_p_sep_by_space, 0 },
    { "duo_n_cs_precedes", tok_duo_n_cs_precedes, 0 },
    { "duo_n_sep_by_space", tok_duo_n_sep_by_space, 0 },
    { "duo_int_p_cs_precedes", tok_duo_int_p_cs_precedes, 0 },
    { "duo_int_p_sep_by_space", tok_duo_int_p_sep_by_space, 0 },
    { "duo_int_n_cs_precedes", tok_duo_int_n_cs_precedes, 0 },
    { "duo_int_n_sep_by_space", tok_duo_int_n_sep_by_space, 0 },
    { "duo_p_sign_posn", tok_duo_p_sign_posn, 0 }, { "duo_n_sign_posn", tok_duo_n_sign_posn, 0 },
    { "duo_int_p_sign_posn", tok_duo_int_p_sign_posn, 0 },
    { "duo_int_n_sign_posn", tok_duo_int_n_sign_posn, 0 },
    { "uno_valid_from", tok_uno_valid_from, 0 }, { "uno_valid_to", tok_uno_valid_to, 0 },
    { "duo_valid_from", tok_duo_valid_from, 0 }, { "duo_valid_to", tok_duo_valid_to, 0 },
    { "conversion_rate", tok_conversion_rate, 0 }, { "LC_NUMERIC", tok_lc_numeric, 0 },
    { "decimal_point", tok_decimal_point, 0 }, { "thousands_sep", tok_thousands_sep, 0 },
    { "grouping", tok_grouping, 0 }, { "LC_TIME", tok_lc_time, 0 }, { "abday", tok_abday, 0 },
    { "day", tok_day, 0 }, { "week", tok_week, 0 }, { "abmon", tok_abmon, 0 },
    { "mon", tok_mon, 0 }, { "d_t_fmt", tok_d_t_fmt, 0 }, { "d_fmt", tok_d_fmt, 0 },
    { "t_fmt", tok_t_fmt, 0 }, { "am_pm", tok_am_pm, 0 }, { "t_fmt_ampm", tok_t_fmt_ampm, 0 },
    { "era", tok_era, 0 }, { "era_year", tok_era_year, 0 }, { "era_d_fmt", tok_era_d_fmt, 0 },
    { "era_d_t_fmt", tok_era_d_t_fmt, 0 }, { "era_t_fmt", tok_era_t_fmt, 0 },
    { "alt_digits", tok_alt_digits, 0 }, { "first_weekday", tok_first_weekday, 0 },
    { "first_workday", tok_first_workday, 0 }, { "cal_direction", tok_cal_direction, 0 },
    { "timezone", tok_timezone, 0 }, { "date_fmt", tok_date_fmt, 0 },
    { "alt_mon", tok_alt_mon, 0 }, { "ab_alt_mon", tok_ab_alt_mon, 0 },
    { "LC_MESSAGES", tok_lc_messages, 0 }, { "yesexpr", tok_yesexpr, 0 },
    { "noexpr", tok_noexpr, 0 }, { "yesstr", tok_yesstr, 0 }, { "nostr", tok_nostr, 0 },
    { "LC_PAPER", tok_lc_paper, 0 }, { "height", tok_height, 0 }, { "width", tok_width, 0 },
    { "LC_NAME", tok_lc_name, 0 }, { "name_fmt", tok_name_fmt, 0 },
    { "name_gen", tok_name_gen, 0 }, { "name_mr", tok_name_mr, 0 },
    { "name_mrs", tok_name_mrs, 0 }, { "name_miss", tok_name_miss, 0 },
    { "name_ms", tok_name_ms, 0 }, { "LC_ADDRESS", tok_lc_address, 0 },
    { "postal_fmt", tok_postal_fmt, 0 }, { "country_name", tok_country_name, 0 },
    { "country_post", tok_country_post, 0 }, { "country_ab2", tok_country_ab2, 0 },
    { "country_ab3", tok_country_ab3, 0 }, { "country_num", tok_country_num, 0 },
    { "country_car", tok_country_car, 0 }, { "country_isbn", tok_country_isbn, 0 },
    { "lang_name", tok_lang_name, 0 }, { "lang_ab", tok_lang_ab, 0 },
    { "lang_term", tok_lang_term, 0 }, { "lang_lib", tok_lang_lib, 0 },
    { "LC_TELEPHONE", tok_lc_telephone, 0 }, { "tel_int_fmt", tok_tel_int_fmt, 0 },
    { "tel_dom_fmt", tok_tel_dom_fmt, 0 }, { "int_select", tok_int_select, 0 },
    { "int_prefix", tok_int_prefix, 0 }, { "LC_MEASUREMENT", tok_lc_measurement, 0 },
    { "measurement", tok_measurement, 0 }, { "LC_IDENTIFICATION", tok_lc_identification, 0 },
    { "title", tok_title, 0 }, { "source", tok_source, 0 }, { "address", tok_address, 0 },
    { "contact", tok_contact, 0 }, { "email", tok_email, 0 }, { "tel", tok_tel, 0 },
    { "fax", tok_fax, 0 }, { "language", tok_language, 0 }, { "territory", tok_territory, 0 },
    { "audience", tok_audience, 0 }, { "application", tok_application, 0 },
    { "abbreviation", tok_abbreviation, 0 }, { "revision", tok_revision, 0 },
    { "date", tok_date, 0 }, { "category", tok_category, 0 },
};

static const struct keyword charmap_keywords[] = {
    { "code_set_name", tok_code_set_name, 1 }, { "mb_cur_max", tok_mb_cur_max, 1 },
    { "mb_cur_min", tok_mb_cur_min, 1 }, { "escape_char", tok_escape_char, 1 },
    { "comment_char", tok_comment_char, 1 }, { "g0esc", tok_g0esc, 1 },
    { "g1esc", tok_g1esc, 1 }, { "g2esc", tok_g2esc, 1 }, { "g3esc", tok_g3esc, 1 },
    { "escseq", tok_escseq, 1 }, { "addset", tok_addset, 1 }, { "include", tok_include, 1 },
    { "CHARMAP", tok_charmap, 0 }, { "END", tok_end, 0 }, { "WIDTH", tok_width, 0 },
    { "WIDTH_VARIABLE", tok_width_variable, 0 }, { "WIDTH_DEFAULT", tok_width_default, 0 },
};

static const struct keyword *find_kw(const struct keyword *v, size_t n, const char *s, size_t len)
{
    for (size_t i = 0; i < n; i++)
        if (strlen(v[i].name) == len && memcmp(v[i].name, s, len) == 0)
            return &v[i];
    return NULL;
}

const struct keyword *locfile_keyword(const char *s, size_t len)
{
    return find_kw(locfile_keywords, sizeof locfile_keywords / sizeof locfile_keywords[0], s, len);
}

const struct keyword *charmap_keyword(const char *s, size_t len)
{
    return find_kw(charmap_keywords, sizeof charmap_keywords / sizeof charmap_keywords[0], s, len);
}

/* ---- lines ---- */

void lr_error(struct lexer *lr, const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "localedef: %s:%zu: ", lr->fname, lr->lineno);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    /* the output is made all the same (as glibc does), but the exit
       status says there was a problem */
    nwarnings++;
}

struct lexer *lr_create(FILE *fp, const char *fname, const struct keyword *(*kw)(const char *, size_t))
{
    struct lexer *lr = xcalloc(1, sizeof *lr);
    lr->fp = fp;
    lr->fname = xstrdup(fname);
    lr->lineno = 1;
    lr->comment_char = '#';
    lr->escape_char = '\\';
    lr->translate_strings = 1;
    ssize_t n = getdelim(&lr->buf, &lr->bufsize, '\n', fp);
    if (n < 0) {
        int e = errno;
        fclose(fp);
        free(lr->buf);
        free((char *)lr->fname);
        free(lr);
        errno = e ? e : ENOENT;
        return NULL;
    }
    if (n > 1 && lr->buf[n - 2] == '\\' && lr->buf[n - 1] == '\n')
        n -= 2;
    lr->buf[n] = '\0';
    lr->bufact = (size_t)n;
    lr->keyword = kw;
    return lr;
}

struct lexer *lr_open(const char *fname, const struct keyword *(*kw)(const char *, size_t))
{
    if (!fname || strcmp(fname, "-") == 0 || strcmp(fname, "/dev/stdin") == 0)
        return lr_create(stdin, "<stdin>", kw);
    FILE *fp = fopen(fname, "r");
    if (!fp)
        return NULL;
    return lr_create(fp, fname, kw);
}

void lr_close(struct lexer *lr)
{
    if (lr->fp != stdin)
        fclose(lr->fp);
    free(lr->buf);
    free(lr);
}

int lr_next(struct lexer *lr)
{
    ssize_t n = getdelim(&lr->buf, &lr->bufsize, '\n', lr->fp);
    if (n < 0)
        return -1;
    lr->lineno++;
    if (n > 1 && lr->buf[n - 2] == lr->escape_char && lr->buf[n - 1] == '\n')
        n -= 2;
    lr->buf[n] = '\0';
    lr->bufact = (size_t)n;
    lr->idx = 0;
    return 0;
}

int lr_getc(struct lexer *lr)
{
    if (lr->idx == lr->bufact) {
        if (lr->bufact != 0)
            if (lr_next(lr) < 0)
                return EOF;
        if (lr->bufact == 0)
            return EOF;
    }
    return (unsigned char)lr->buf[lr->idx++];
}

int lr_ungetn(struct lexer *lr, size_t n)
{
    if (lr->idx < n)
        return -1;
    lr->idx -= n;
    return 0;
}

static void lr_ungetc(struct lexer *lr, int ch)
{
    if (lr->idx > 0 && ch != EOF)
        lr->idx--;
}

void lr_ignore_rest(struct lexer *lr, int vb)
{
    if (vb) {
        while (isspace((unsigned char)lr->buf[lr->idx]) && lr->buf[lr->idx] != '\n' &&
               lr->buf[lr->idx] != lr->comment_char)
            if (lr->buf[lr->idx] == '\0') {
                if (lr_next(lr) < 0)
                    return;
            } else {
                lr->idx++;
            }
        if (lr->buf[lr->idx] != '\n' && !feof(lr->fp) && lr->buf[lr->idx] != lr->comment_char)
            lr_error(lr, "trailing garbage at end of line");
    }
    /* a continued line */
    while (lr->bufact > 0 && lr->buf[lr->bufact - 1] != '\n')
        if (lr_next(lr) < 0)
            break;
    lr->idx = lr->bufact;
}

/* ---- tokens ---- */

struct lrbuf {
    size_t act, max;
    char *buf;
};

static void lrb_init(struct lrbuf *b)
{
    b->act = 0;
    b->max = 56;
    b->buf = xmalloc(b->max);
}

static void addc(struct lrbuf *b, char c)
{
    if (b->act == b->max) {
        b->max *= 2;
        b->buf = xrealloc(b->buf, b->max);
    }
    b->buf[b->act++] = c;
}

static void adds(struct lrbuf *b, const unsigned char *s, size_t l)
{
    if (b->max - b->act < l) {
        size_t m = 2 * b->max;
        if (m < b->act + l)
            m = b->act + l;
        b->buf = xrealloc(b->buf, m);
        b->max = m;
    }
    memcpy(b->buf + b->act, s, l);
    b->act += l;
}

static void lrb_to_token(struct lrbuf *b, struct lexer *lr)
{
    lr->token.str.startmb = xrealloc(b->buf, b->act + 1);
    lr->token.str.startmb[b->act] = '\0';
    lr->token.str.lenmb = b->act;
}

static struct token *get_toplvl_escape(struct lexer *lr)
{
    size_t start = lr->idx - 1;
    unsigned char *bytes = lr->token.charcode.bytes;
    int nbytes = 0;
    int ch;
    do {
        unsigned byte, base = 8;
        ch = lr_getc(lr);
        if (ch == 'd') {
            base = 10;
            ch = lr_getc(lr);
        } else if (ch == 'x') {
            base = 16;
            ch = lr_getc(lr);
        }
        if ((base == 16 && !isxdigit(ch)) || (base != 16 && (ch < '0' || ch >= (int)('0' + base)))) {
        esc_error:
            lr->token.str.startmb = &lr->buf[start];
            while (ch != EOF && !isspace(ch))
                ch = lr_getc(lr);
            lr->token.str.lenmb = lr->idx - start;
            lr->token.tok = tok_error;
            return &lr->token;
        }
        byte = isdigit(ch) ? (unsigned)(ch - '0') : (unsigned)(tolower(ch) - 'a' + 10);
        ch = lr_getc(lr);
        if ((base == 16 && !isxdigit(ch)) || (base != 16 && (ch < '0' || ch >= (int)('0' + base))))
            goto esc_error;
        byte *= base;
        byte += isdigit(ch) ? (unsigned)(ch - '0') : (unsigned)(tolower(ch) - 'a' + 10);
        ch = lr_getc(lr);
        if (base != 16 && isdigit(ch)) {
            byte *= base;
            byte += (unsigned)(ch - '0');
            ch = lr_getc(lr);
        }
        bytes[nbytes++] = (unsigned char)byte;
    } while (ch == lr->escape_char && nbytes < (int)sizeof lr->token.charcode.bytes);
    if (!isspace(ch))
        lr_error(lr, "garbage at end of character code specification");
    lr_ungetn(lr, 1);
    lr->token.tok = tok_charcode;
    lr->token.charcode.nbytes = nbytes;
    return &lr->token;
}

static struct token *get_symname(struct lexer *lr)
{
    struct lrbuf b;
    int ch;
    lrb_init(&b);
    do {
        ch = lr_getc(lr);
        if (ch == lr->escape_char) {
            int c2 = lr_getc(lr);
            addc(&b, (char)c2);
            if (c2 == '\n')
                ch = '\n';
        } else {
            addc(&b, (char)ch);
        }
    } while (ch != '>' && ch != '\n' && ch != EOF);
    if (ch == '\n' || ch == EOF)
        lr_error(lr, "unterminated symbolic name");
    /* <Uxxxx>, <Uxxxxxxxx> */
    if (b.buf[0] == 'U' && (b.act == 6 || b.act == 10)) {
        char *cp = b.buf + 1;
        while (cp < &b.buf[b.act - 1] && isxdigit((unsigned char)*cp))
            cp++;
        if (cp == &b.buf[b.act - 1]) {
            addc(&b, '\0');
            lr->token.tok = tok_ucs4;
            lr->token.ucs4 = (uint32_t)strtoul(b.buf + 1, NULL, 16);
            free(b.buf);
            return &lr->token;
        }
    }
    const struct keyword *kw = lr->keyword(b.buf, b.act - 1);
    if (kw && kw->symname == 1) {
        lr->token.tok = kw->tok;
        free(b.buf);
    } else {
        lr->token.tok = tok_bsymbol;
        lrb_to_token(&b, lr);
        lr->token.str.lenmb--;  /* not the '>' */
        lr->token.str.startmb[lr->token.str.lenmb] = '\0';
    }
    return &lr->token;
}

static struct token *get_ident(struct lexer *lr)
{
    struct lrbuf b;
    int ch;
    lrb_init(&b);
    addc(&b, lr->buf[lr->idx - 1]);
    while (!isspace(ch = lr_getc(lr)) && ch != '"' && ch != ';' && ch != '<' && ch != ',' &&
           ch != EOF) {
        if (ch == lr->escape_char) {
            ch = lr_getc(lr);
            if (ch == '\n' || ch == EOF) {
                lr_error(lr, "invalid escape sequence");
                break;
            }
        }
        addc(&b, (char)ch);
    }
    lr_ungetc(lr, ch);
    const struct keyword *kw = lr->keyword(b.buf, b.act);
    if (kw && kw->symname == 0) {
        lr->token.tok = kw->tok;
        free(b.buf);
    } else {
        lr->token.tok = tok_ident;
        lrb_to_token(&b, lr);
    }
    return &lr->token;
}

static bool utf8_error(struct lexer *lr, int c1, int c2, int c3, int c4)
{
    char buf[48];
    if (c2 < 0)
        snprintf(buf, sizeof buf, "0x%02x", c1);
    else if (c3 < 0)
        snprintf(buf, sizeof buf, "0x%02x 0x%02x", c1, c2);
    else if (c4 < 0)
        snprintf(buf, sizeof buf, "0x%02x 0x%02x 0x%02x", c1, c2, c3);
    else
        snprintf(buf, sizeof buf, "0x%02x 0x%02x 0x%02x 0x%02x", c1, c2, c3, c4);
    lr_error(lr, "invalid UTF-8 sequence %s", buf);
    return false;
}

static bool trail(int c)
{
    return c >= 0 && (c & 0xc0) == 0x80;
}

static bool utf8_decode(struct lexer *lr, int c1, uint32_t *wch)
{
    if (c1 < 0xc2)
        return utf8_error(lr, c1, -1, -1, -1);
    int c2 = lr_getc(lr);
    if (!trail(c2))
        return utf8_error(lr, c1, c2, -1, -1);
    if (c1 <= 0xdf) {
        *wch = ((uint32_t)(c1 & 0x1f) << 6) | (uint32_t)(c2 & 0x3f);
        return true;
    }
    int c3 = lr_getc(lr);
    if (!trail(c3) || c1 < 0xe0)
        return utf8_error(lr, c1, c2, c3, -1);
    if (c1 <= 0xef) {
        uint32_t r = ((uint32_t)(c1 & 0x0f) << 12) | ((uint32_t)(c2 & 0x3f) << 6) | (uint32_t)(c3 & 0x3f);
        if (r < 0x800)
            return utf8_error(lr, c1, c2, c3, -1);
        *wch = r;
        return true;
    }
    int c4 = lr_getc(lr);
    if (!trail(c4) || c1 < 0xf0 || c1 > 0xf4)
        return utf8_error(lr, c1, c2, c3, c4);
    uint32_t r = ((uint32_t)(c1 & 0x07) << 18) | ((uint32_t)(c2 & 0x3f) << 12) |
                 ((uint32_t)(c3 & 0x3f) << 6) | (uint32_t)(c4 & 0x3f);
    if (r < 0x10000)
        return utf8_error(lr, c1, c2, c3, c4);
    *wch = r;
    return true;
}

/* the bytes of a character in the charmap (or its transliteration) */
static bool translate_ucs(struct localedef *locale, const struct charmap *cm, const struct repertoire *rep,
                          uint32_t wch, struct lrbuf *b)
{
    char utmp[16];
    snprintf(utmp, sizeof utmp, "U%08X", wch);
    struct charseq *seq = charmap_find_value(cm, utmp, 9);
    if (!seq) {
        if (rep) {
            const char *sym = repertoire_find_symbol(rep, wch);
            if (sym)
                seq = charmap_find_value(cm, sym, strlen(sym));
        }
        if (!seq) {
            if (locale) {
                if (!(locale->avail & (1 << C_CTYPE))) {
                    int old = locale->needed;
                    locale->needed = 0;
                    locale = load_locale(C_CTYPE, locale->name, locale->repertoire_name, (struct charmap *)cm, locale);
                    locale->needed = old;
                }
                uint32_t *tr;
                if ((locale->avail & (1 << C_CTYPE)) && (tr = find_translit(locale, cm, wch)) != NULL) {
                    for (int i = 0; tr[i] != 0; i++) {
                        snprintf(utmp, sizeof utmp, "U%08X", tr[i]);
                        seq = charmap_find_value(cm, utmp, 9);
                        if (seq)
                            adds(b, seq->bytes, (size_t)seq->nbytes);
                    }
                    return true;
                }
            }
            return false;
        }
    }
    adds(b, seq->bytes, (size_t)seq->nbytes);
    return true;
}

#define ADDWC(c)                                                                                   \
    do {                                                                                           \
        if (wact == wmax) {                                                                        \
            wmax *= 2;                                                                             \
            wbuf = xrealloc(wbuf, wmax * sizeof *wbuf);                                            \
        }                                                                                          \
        wbuf[wact++] = (c);                                                                        \
    } while (0)

static struct token *get_string(struct lexer *lr, const struct charmap *cm, struct localedef *locale,
                                const struct repertoire *rep, int vb)
{
    int return_widestr = lr->return_widestr;
    struct lrbuf b;
    uint32_t *wbuf = NULL;
    size_t wact = 0, wmax = 56;
    (void)vb;
    lrb_init(&b);
    lr->token.tok = tok_string;
    lr->token.str.startwc = NULL;
    lr->token.str.lenwc = 0;
    if (!lr->translate_strings) {
        int ch;
        while ((ch = lr_getc(lr)) != '"' && ch != '\n' && ch != EOF) {
            if (ch >= 0x80)
                lr_error(lr, "illegal 8-bit character in untranslated string");
            addc(&b, (char)ch);
        }
        if (b.act > 0 && b.buf[b.act - 1] == lr->escape_char &&
            (b.act == 1 || b.buf[b.act - 2] != lr->escape_char)) {
            lr_error(lr, "illegal escape sequence at end of string");
            b.act--;
        } else if (ch == '\n' || ch == EOF) {
            lr_error(lr, "unterminated string");
        }
        addc(&b, '\0');
        b.act--;
    } else {
        bool illegal = false;
        int ch;
        if (return_widestr)
            wbuf = xmalloc(wmax * sizeof *wbuf);
        while ((ch = lr_getc(lr)) != '"' && ch != '\n' && ch != EOF) {
            uint32_t wch;
            if (ch != '<') {
                if (ch == lr->escape_char) {
                    ch = lr_getc(lr);
                    if (ch >= 0x80) {
                        lr_error(lr, "illegal 8-bit escape sequence");
                        illegal = true;
                        break;
                    }
                    if (ch == '\n' || ch == EOF)
                        break;
                    addc(&b, (char)ch);
                    wch = (uint32_t)ch;
                } else if (ch < 0x80) {
                    wch = (uint32_t)ch;
                    addc(&b, (char)ch);
                } else {
                    if (!utf8_decode(lr, ch, &wch)) {
                        illegal = true;
                        break;
                    }
                    if (!translate_ucs(locale, cm, rep, wch, &b)) {
                        illegal = true;
                        continue;
                    }
                }
                if (return_widestr)
                    ADDWC(wch);
                continue;
            }
            /* <name> */
            size_t start = b.act;
            while ((ch = lr_getc(lr)) != '>' && ch != '\n' && ch != EOF) {
                if (ch == lr->escape_char) {
                    ch = lr_getc(lr);
                    if (ch == '\n' || ch == EOF)
                        break;
                }
                addc(&b, (char)ch);
            }
            if (ch == '\n' || ch == EOF)
                break;
            if (b.act == start) {
                illegal = true;
                continue;
            }
            if (b.buf[start] == 'U' && (b.act - start == 5 || b.act - start == 9)) {
                char *cp = b.buf + start + 1;
                while (cp < &b.buf[b.act] && isxdigit((unsigned char)*cp))
                    cp++;
                if (cp == &b.buf[b.act]) {
                    addc(&b, '\0');
                    wch = (uint32_t)strtoul(b.buf + start + 1, NULL, 16);
                    b.act = start;
                    if (return_widestr)
                        ADDWC(wch);
                    if (!translate_ucs(locale, cm, rep, wch, &b))
                        illegal = true;
                    continue;
                }
            }
            struct charseq *seq = charmap_find_value(cm, &b.buf[start], b.act - start);
            if (!seq) {
                lr_error(lr, "symbol `%.*s' not in charmap", (int)(b.act - start), &b.buf[start]);
                illegal = true;
            }
            if (return_widestr) {
                if (seq && seq->ucs4 != UNINITIALIZED_CHAR_VALUE) {
                    wch = seq->ucs4;
                } else {
                    wch = repertoire_find_value(rep, &b.buf[start], b.act - start);
                    if (seq)
                        seq->ucs4 = wch;
                }
                if (wch == ILLEGAL_CHAR_VALUE) {
                    lr_error(lr, "symbol `%.*s' not in repertoire map", (int)(b.act - start), &b.buf[start]);
                    illegal = true;
                } else {
                    ADDWC(wch);
                }
            }
            b.act = start;
            if (seq)
                adds(&b, seq->bytes, (size_t)seq->nbytes);
        }
        if (ch == '\n' || ch == EOF) {
            lr_error(lr, "unterminated string");
            illegal = true;
        }
        if (illegal) {
            free(b.buf);
            free(wbuf);
            lr->token.str.startmb = NULL;
            lr->token.str.lenmb = 0;
            lr->token.str.startwc = NULL;
            lr->token.str.lenwc = 0;
            return &lr->token;
        }
        if (return_widestr) {
            ADDWC(0);
            lr->token.str.startwc = xrealloc(wbuf, wact * sizeof *wbuf);
            lr->token.str.lenwc = wact;
        }
    }
    lrb_to_token(&b, lr);
    return &lr->token;
}

struct token *lr_token(struct lexer *lr, const struct charmap *cm, struct localedef *locale,
                       const struct repertoire *rep, int vb)
{
    int ch;
    for (;;) {
        do {
            ch = lr_getc(lr);
            if (ch == EOF) {
                lr->token.tok = tok_eof;
                return &lr->token;
            }
            if (ch == '\n') {
                lr->token.tok = tok_eol;
                return &lr->token;
            }
        } while (isspace(ch));
        if (ch != lr->comment_char)
            break;
        /* no newline at the end of the buffer: a continued line; only
           this physical line is a comment */
        if (lr->buf[lr->bufact - 1] != '\n') {
            lr->idx = lr->bufact;
            continue;
        }
        lr_ignore_rest(lr, 0);
        lr->token.tok = tok_eol;
        return &lr->token;
    }
    if (ch == lr->escape_char)
        return get_toplvl_escape(lr);
    if (ch == '.') {
        const char *p = &lr->buf[lr->idx];
        if (strncmp(p, "...(2)....", 10) == 0) {
            lr->idx += 10;
            lr->token.tok = tok_ellipsis4_2;
            return &lr->token;
        }
        if (strncmp(p, "...", 3) == 0) {
            lr->idx += 3;
            lr->token.tok = tok_ellipsis4;
            return &lr->token;
        }
        if (strncmp(p, "..", 2) == 0) {
            lr->idx += 2;
            lr->token.tok = tok_ellipsis3;
            return &lr->token;
        }
        if (strncmp(p, ".(2)..", 6) == 0) {
            lr->idx += 6;
            lr->token.tok = tok_ellipsis2_2;
            return &lr->token;
        }
        if (*p == '.') {
            lr->idx++;
            lr->token.tok = tok_ellipsis2;
            return &lr->token;
        }
    }
    switch (ch) {
    case '<':
        return get_symname(lr);
    case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
        lr->token.tok = tok_number;
        lr->token.num = (unsigned long)(ch - '0');
        while (isdigit(ch = lr_getc(lr))) {
            lr->token.num *= 10;
            lr->token.num += (unsigned long)(ch - '0');
        }
        if (isalpha(ch))
            lr_error(lr, "garbage at end of number");
        lr_ungetn(lr, 1);
        return &lr->token;
    case ';':
        lr->token.tok = tok_semicolon;
        return &lr->token;
    case ',':
        lr->token.tok = tok_comma;
        return &lr->token;
    case '(':
        lr->token.tok = tok_open_brace;
        return &lr->token;
    case ')':
        lr->token.tok = tok_close_brace;
        return &lr->token;
    case '"':
        return get_string(lr, cm, locale, rep, vb);
    case '-':
        ch = lr_getc(lr);
        if (ch == '1') {
            lr->token.tok = tok_minus1;
            return &lr->token;
        }
        lr_ungetn(lr, 2);
        break;
    default:
        if (ch >= 0x80) {
            uint32_t wch;
            if (!utf8_decode(lr, ch, &wch)) {
                lr->token.tok = tok_error;
                return &lr->token;
            }
            lr->token.tok = tok_ucs4;
            lr->token.ucs4 = wch;
            return &lr->token;
        }
    }
    return get_ident(lr);
}
