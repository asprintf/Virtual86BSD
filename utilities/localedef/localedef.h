/*
 * localedef.h - the declarations shared by the parts of localedef
 */

#ifndef LOCALEDEF_H
#define LOCALEDEF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define ILLEGAL_CHAR_VALUE ((uint32_t)0xffffffffu)
#define UNINITIALIZED_CHAR_VALUE ((uint32_t)0xfffffffeu)

/* the categories, by the numbers of the binary files (those of glibc) */
enum {
    C_CTYPE = 0, C_NUMERIC = 1, C_TIME = 2, C_COLLATE = 3, C_MONETARY = 4, C_MESSAGES = 5,
    C_ALL = 6, C_PAPER = 7, C_NAME = 8, C_ADDRESS = 9, C_TELEPHONE = 10, C_MEASUREMENT = 11,
    C_IDENTIFICATION = 12, C_LAST = 13
};
#define ALL_CATEGORIES (((1 << C_LAST) - 1) & ~(1 << C_ALL))

extern const char *const category_names[C_LAST];

/* ---- memory, diagnostics ---- */

void *xmalloc(size_t n);
void *xcalloc(size_t n, size_t m);
void *xrealloc(void *p, size_t n);
char *xstrdup(const char *s);
char *xstrndup(const char *s, size_t n);
char *xasprintf(const char *fmt, ...);
void *xmemdup(const void *p, size_t n);
/* strsep */
char *str_sep(char **sp, const char *delim);

extern int nerrors, nwarnings;
extern bool verbose, quiet;
/* an error: no output is made (unless -c); status != 0: exit now */
void record_error(int status, const char *fmt, ...);
/* a warning: the output is made, the exit status is 1 */
void record_warning(const char *fmt, ...);

/* ---- a hash table, as glibc's localedef has (its order of iteration
   is that of glibc's, which the output depends on) ---- */

struct hentry {
    unsigned long used;
    const void *key;
    size_t keylen;
    void *data;
    struct hentry *next;
};

struct htab {
    unsigned long size, filled;
    struct hentry *first;
    struct hentry *table;
};

void ht_init(struct htab *h, unsigned long size);
int ht_insert(struct htab *h, const void *key, size_t keylen, void *data);
int ht_find(const struct htab *h, const void *key, size_t keylen, void **data);
int ht_set(struct htab *h, const void *key, size_t keylen, void *data);
int ht_iterate(const struct htab *h, void **ptr, const void **key, size_t *keylen, void **data);
uint32_t compute_hashval(const void *key, size_t keylen);
unsigned long next_prime(unsigned long seed);

/* ---- the character map ---- */

struct charseq {
    const char *name;
    uint32_t ucs4;
    int nbytes;
    unsigned char bytes[];
};

struct width_rule {
    struct charseq *from, *to;
    unsigned width;
};

struct charmap {
    const char *code_set_name;
    const char *repertoiremap;
    int mb_cur_min, mb_cur_max;
    struct width_rule *width_rules;
    size_t nwidth_rules, nwidth_rules_max;
    unsigned width_default;
    struct htab char_table;     /* by name */
    struct htab byte_table;     /* by bytes */
    struct htab ucs4_table;
};

extern bool enc_not_ascii_compatible;

struct charmap *charmap_read(const char *name);
struct charseq *charmap_find_value(const struct charmap *cm, const char *name, size_t len);
struct charseq *charmap_find_symbol(const struct charmap *cm, const char *bytes, size_t n);
/* a file of the charmaps directory (or another), uncompressed */
FILE *open_maybe_compressed(const char *path);

/* ---- the lexer ---- */

enum tok {
    tok_none = 0, tok_eof, tok_eol, tok_bsymbol, tok_ident, tok_ellipsis2, tok_ellipsis3,
    tok_ellipsis4, tok_ellipsis2_2, tok_ellipsis4_2, tok_semicolon, tok_comma, tok_open_brace,
    tok_close_brace, tok_charcode, tok_ucs4, tok_number, tok_minus1, tok_string, tok_include,
    tok_escape_char, tok_comment_char, tok_charmap, tok_end, tok_g0esc, tok_g1esc, tok_g2esc,
    tok_g3esc, tok_escseq, tok_addset, tok_charids, tok_code_set_name, tok_mb_cur_max,
    tok_mb_cur_min, tok_charconv, tok_width, tok_width_variable, tok_width_default,
    tok_repertoiremap, tok_lc_ctype, tok_copy,
    /* the order of these: the bits of the classes */
    tok_upper, tok_lower, tok_alpha, tok_digit, tok_xdigit, tok_space, tok_print, tok_graph,
    tok_blank, tok_cntrl, tok_punct, tok_alnum,
    tok_class, tok_charclass, tok_toupper, tok_tolower, tok_map, tok_translit_start,
    tok_translit_end, tok_translit_ignore, tok_default_missing, tok_outdigit, tok_lc_collate,
    tok_codepoint_collation, tok_coll_weight_max, tok_section_symbol, tok_collating_element,
    tok_collating_symbol, tok_symbol_equivalence, tok_script, tok_order_start, tok_order_end,
    tok_from, tok_forward, tok_backward, tok_position, tok_undefined, tok_ignore,
    tok_reorder_after, tok_reorder_end, tok_reorder_sections_after, tok_reorder_sections_end,
    tok_define, tok_undef, tok_ifdef, tok_else, tok_elifdef, tok_elifndef, tok_endif,
    tok_lc_monetary, tok_int_curr_symbol, tok_currency_symbol, tok_mon_decimal_point,
    tok_mon_thousands_sep, tok_mon_grouping, tok_positive_sign, tok_negative_sign,
    tok_int_frac_digits, tok_frac_digits, tok_p_cs_precedes, tok_p_sep_by_space,
    tok_n_cs_precedes, tok_n_sep_by_space, tok_p_sign_posn, tok_n_sign_posn,
    tok_int_p_cs_precedes, tok_int_p_sep_by_space, tok_int_n_cs_precedes,
    tok_int_n_sep_by_space, tok_int_p_sign_posn, tok_int_n_sign_posn, tok_duo_int_curr_symbol,
    tok_duo_currency_symbol, tok_duo_int_frac_digits, tok_duo_frac_digits,
    tok_duo_p_cs_precedes, tok_duo_p_sep_by_space, tok_duo_n_cs_precedes,
    tok_duo_n_sep_by_space, tok_duo_int_p_cs_precedes, tok_duo_int_p_sep_by_space,
    tok_duo_int_n_cs_precedes, tok_duo_int_n_sep_by_space, tok_duo_p_sign_posn,
    tok_duo_n_sign_posn, tok_duo_int_p_sign_posn, tok_duo_int_n_sign_posn, tok_uno_valid_from,
    tok_uno_valid_to, tok_duo_valid_from, tok_duo_valid_to, tok_conversion_rate, tok_lc_numeric,
    tok_decimal_point, tok_thousands_sep, tok_grouping, tok_lc_time, tok_abday, tok_day,
    tok_week, tok_abmon, tok_mon, tok_d_t_fmt, tok_d_fmt, tok_t_fmt, tok_am_pm, tok_t_fmt_ampm,
    tok_era, tok_era_year, tok_era_d_fmt, tok_era_d_t_fmt, tok_era_t_fmt, tok_alt_digits,
    tok_first_weekday, tok_first_workday, tok_cal_direction, tok_timezone, tok_date_fmt,
    tok_alt_mon, tok_ab_alt_mon, tok_lc_messages, tok_yesexpr, tok_noexpr, tok_yesstr,
    tok_nostr, tok_lc_paper, tok_height, tok_lc_name, tok_name_fmt, tok_name_gen, tok_name_mr,
    tok_name_mrs, tok_name_miss, tok_name_ms, tok_lc_address, tok_postal_fmt, tok_country_name,
    tok_country_post, tok_country_ab2, tok_country_ab3, tok_country_num, tok_country_car,
    tok_country_isbn, tok_lang_name, tok_lang_ab, tok_lang_term, tok_lang_lib, tok_lc_telephone,
    tok_tel_int_fmt, tok_tel_dom_fmt, tok_int_select, tok_int_prefix, tok_lc_measurement,
    tok_measurement, tok_lc_identification, tok_title, tok_source, tok_address, tok_contact,
    tok_email, tok_tel, tok_fax, tok_language, tok_territory, tok_audience, tok_application,
    tok_abbreviation, tok_revision, tok_date, tok_category,
    tok_error
};

struct token {
    enum tok tok;
    struct {
        char *startmb;          /* NULL: a string that cannot be encoded */
        size_t lenmb;
        uint32_t *startwc;
        size_t lenwc;
    } str;
    unsigned long num;
    struct {
        unsigned char bytes[16];
        int nbytes;
    } charcode;
    uint32_t ucs4;
};

struct keyword {
    const char *name;
    enum tok tok;
    int symname;                /* 1: written <name>, 0: as a word */
};

struct localedef;
struct repertoire;

struct lexer {
    FILE *fp;
    const char *fname;
    char *buf;
    size_t bufsize, bufact;
    size_t lineno;
    size_t idx;
    char comment_char, escape_char;
    struct token token;
    int translate_strings, return_widestr;
    const struct keyword *(*keyword)(const char *s, size_t len);
};

const struct keyword *locfile_keyword(const char *s, size_t len);
const struct keyword *charmap_keyword(const char *s, size_t len);

struct lexer *lr_open(const char *fname, const struct keyword *(*kw)(const char *, size_t));
struct lexer *lr_create(FILE *fp, const char *fname, const struct keyword *(*kw)(const char *, size_t));
void lr_close(struct lexer *lr);
int lr_next(struct lexer *lr);
struct token *lr_token(struct lexer *lr, const struct charmap *cm, struct localedef *locale,
                       const struct repertoire *rep, int verbose);
void lr_ignore_rest(struct lexer *lr, int verbose);
void lr_error(struct lexer *lr, const char *fmt, ...);
int lr_getc(struct lexer *lr);
int lr_ungetn(struct lexer *lr, size_t n);
#define SYNTAX_ERROR(lr, ...) do { lr_error(lr, __VA_ARGS__); lr_ignore_rest(lr, 0); } while (0)

/* ---- the repertoire (none: the names of the charmap give the values) ---- */

struct repertoire {
    struct htab char_table, reverse_table, seq_table;
};

uint32_t repertoire_find_value(const struct repertoire *rep, const char *name, size_t len);
const char *repertoire_find_symbol(const struct repertoire *rep, uint32_t ucs);
struct charseq *repertoire_find_seq(const struct repertoire *rep, uint32_t ucs);
struct repertoire *repertoire_read(const char *name);

/* ---- the locales read ---- */

struct localedef {
    struct localedef *next;
    const char *name;
    int needed, avail;
    void *categories[C_LAST];
    const char *copy_name[C_LAST];
    const char *repertoire_name;
};

extern struct localedef *locales;
extern const char *repertoire_global;

int locfile_read(struct localedef *result, const struct charmap *cm);
struct localedef *add_to_readlist(int category, const char *name, const char *repertoire_name,
                                  int generate, struct localedef *copy_locale);
struct localedef *find_locale(int category, const char *name, const char *repertoire_name,
                              const struct charmap *cm);
struct localedef *load_locale(int category, const char *name, const char *repertoire_name,
                              const struct charmap *cm, struct localedef *copy_locale);
void handle_copy(struct lexer *lr, const struct charmap *cm, const char *repertoire_name,
                 struct localedef *result, enum tok token, int category, const char *catname,
                 int ignore_content);
/* the category's data, after copy: a locale's own or that it copies */
void *resolve_copy(struct localedef *locale, int category, const struct charmap *cm);

/* ---- the output: a category's file ---- */

struct locfile {
    size_t n_elements, next_element;
    uint32_t *offsets;
    unsigned char *data;
    size_t len, cap;
    int stage;
};

void init_locale_data(struct locfile *f, size_t n);
void align_locale_data(struct locfile *f, size_t boundary);
void add_locale_empty(struct locfile *f);
void add_locale_raw_data(struct locfile *f, const void *data, size_t size);
void add_locale_string(struct locfile *f, const char *s);
void add_locale_wstring(struct locfile *f, const uint32_t *s);
void add_locale_uint32(struct locfile *f, uint32_t v);
void add_locale_uint32_array(struct locfile *f, const uint32_t *v, size_t n);
void add_locale_char(struct locfile *f, char c);
void start_locale_structure(struct locfile *f);
void end_locale_structure(struct locfile *f);
void start_locale_prelude(struct locfile *f);
void end_locale_prelude(struct locfile *f);
void write_locale_data(const char *output_path, int category, struct locfile *f);
size_t wcslen32(const uint32_t *s);

/* glibc's 3-level tables (of uint8 or uint32 values, or bit sets) */
struct table3 {
    unsigned p, q;
    int esize;                  /* 1, 4 */
    uint32_t def;
    size_t level1_alloc, level1_size;
    uint32_t *level1;
    size_t level2_alloc, level2_size;
    uint32_t *level2;
    size_t level3_alloc, level3_size;
    void *level3;
};
void t3_init(struct table3 *t, unsigned p, unsigned q, int esize, uint32_t def);
uint32_t t3_get(const struct table3 *t, uint32_t wc);
void t3_add(struct table3 *t, uint32_t wc, uint32_t value);
void add_locale_table3(struct locfile *f, struct table3 *t, bool bits);

/* ---- the categories ---- */

#define CATEGORY_FUNCS(name) \
    void name##_read(struct lexer *lr, struct localedef *result, const struct charmap *cm, \
                     const char *repertoire_name, int ignore_content); \
    void name##_finish(struct localedef *locale, const struct charmap *cm); \
    void name##_output(struct localedef *locale, const struct charmap *cm, const char *output_path);

CATEGORY_FUNCS(ctype)
CATEGORY_FUNCS(collate)
CATEGORY_FUNCS(monetary)
CATEGORY_FUNCS(numeric)
CATEGORY_FUNCS(time)
CATEGORY_FUNCS(messages)
CATEGORY_FUNCS(paper)
CATEGORY_FUNCS(name)
CATEGORY_FUNCS(address)
CATEGORY_FUNCS(telephone)
CATEGORY_FUNCS(measurement)
CATEGORY_FUNCS(identification)

/* LC_CTYPE's transliteration of a character, for strings of a charmap
   that does not have it; NULL if none */
uint32_t *find_translit(struct localedef *locale, const struct charmap *cm, uint32_t wch);

/* where the locale sources and charmaps are */
#define LOCSRCDIR "/usr/share/i18n/locales"
#define CHARMAP_PATH "/usr/share/i18n/charmaps"
#define REPERTOIREMAP_PATH "/usr/share/i18n/repertoiremaps"
#define DEFAULT_CHARMAP "ANSI_X3.4-1968"

#endif
