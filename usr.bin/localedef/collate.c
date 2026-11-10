/*
 * collate.c - LC_COLLATE of localedef: collating-element,
 * collating-symbol, symbol-equivalence, script, section-symbol,
 * order_start (with sections and directions), the weights (IGNORE,
 * strings of symbols, ellipses "..", "...", "...."), UNDEFINED,
 * reorder-after/reorder-end, reorder-sections-after/-end, copy (also
 * with definitions after it), define/undef/ifdef/else/elifdef/elifndef/
 * endif, codepoint_collation; the file: glibc's tables (the weights of
 * the multibyte and the wide characters, their sequences, the
 * collating elements' hash table, the collation sequences).
 *
 * As glibc's localedef: the weights are numbered from 2 on each level
 * in the order of the list (made of the definitions, the ellipses
 * expanded and reorder-after applied); the sequences that start with
 * the same byte or wide character are kept longest first, and runs of
 * consecutive sequences share an entry. The places in the pools are
 * those glibc gives them, so the file is the same byte for byte.
 */

#define _POSIX_C_SOURCE 202405L

#include "localedef.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

enum { sort_forward = 1, sort_backward = 2, sort_position = 4 };

struct element;

struct section {
    struct section *def_next, *next;
    const char *name;
    struct element *first, *last;
    int *rules;
    int ruleidx;
};

struct wlist {
    int cnt;
    struct element **w;
};

struct element {
    const char *name;
    const char *mbs;
    size_t nmbs;
    const uint32_t *wcs;
    size_t nwcs;
    int *mborder;
    int wcorder;
    unsigned used_in_level;
    struct wlist *weights;
    int is_character;
    int mbseqorder, wcseqorder;
    const char *file;
    size_t line;
    struct section *section;
    struct element *last, *next, *mbnext, *mblast, *wcnext, *wclast;
};

/* a weight that the elements of an ellipsis take from themselves */
static struct element ellipsis_marker;
#define ELLIPSIS (&ellipsis_marker)

struct symbol {
    const char *name;
    struct element *order;
};

/* a map from wide characters to the lists of the elements they start */
struct wcmap {
    uint32_t *keys;
    struct element **vals;
    size_t size, n;
};

struct collate {
    bool codepoint_collation;
    long col_weight_max;
    struct section *known_sections, *sections, *current_section;
    struct section unnamed_section;
    bool unnamed_section_defined;
    struct section error_section, symbol_section;
    struct element *start;
    struct element undefined;
    struct element *cursor;
    struct element ellipsis_weight;
    struct htab elem_table, sym_table, seq_table;
    struct element *mbheads[256];
    struct wcmap wcheads;
    unsigned char mbseqorder[256];
    struct table3 wcseqorder;
    enum { else_none, else_ignore, else_seen } else_action;
};

static int nrules;

struct name_list {
    struct name_list *next;
    char *str;
};
static struct name_list *defined;

/* ---- the map of the wide characters ---- */

static size_t wcmap_slot(const struct wcmap *m, uint32_t k)
{
    size_t i = (k * 2654435761u) & (m->size - 1);
    while (m->vals[i] && m->keys[i] != k)
        i = (i + 1) & (m->size - 1);
    return i;
}

static struct element *wcmap_get(const struct wcmap *m, uint32_t k)
{
    return m->size ? m->vals[wcmap_slot(m, k)] : NULL;
}

static void wcmap_put(struct wcmap *m, uint32_t k, struct element *v)
{
    if (2 * (m->n + 1) > m->size) {
        struct wcmap o = *m;
        m->size = o.size ? 2 * o.size : 1024;
        m->keys = xcalloc(m->size, sizeof *m->keys);
        m->vals = xcalloc(m->size, sizeof *m->vals);
        m->n = 0;
        for (size_t i = 0; i < o.size; i++)
            if (o.vals[i])
                wcmap_put(m, o.keys[i], o.vals[i]);
        free(o.keys);
        free(o.vals);
    }
    size_t i = wcmap_slot(m, k);
    if (!m->vals[i])
        m->n++;
    m->keys[i] = k;
    m->vals[i] = v;
}

static int cmp_u32(const void *a, const void *b)
{
    uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
    return x < y ? -1 : x > y;
}

/* ---- the pools of the output ---- */

struct pool {
    unsigned char *d;
    size_t n, cap;
};

static void pool_grow(struct pool *p, const void *data, size_t n)
{
    if (p->cap - p->n < n) {
        size_t c = p->cap ? 2 * p->cap : 4096;
        while (c - p->n < n)
            c *= 2;
        p->d = xrealloc(p->d, c);
        p->cap = c;
    }
    if (n)
        memcpy(p->d + p->n, data, n);
    p->n += n;
}

static void pool_1(struct pool *p, int c)
{
    unsigned char b = (unsigned char)c;
    pool_grow(p, &b, 1);
}

static void pool_i32(struct pool *p, int32_t v)
{
    pool_grow(p, &v, sizeof v);
}

/* the pool as the next element of the file; the pool starts anew */
static void pool_emit(struct locfile *f, struct pool *p)
{
    add_locale_raw_data(f, p->d, p->n);
    p->n = 0;
}

/* ---- the elements ---- */

static int utf8_encode(char *buf, int val)
{
    int retval;
    if (val < 0x80) {
        *buf = (char)val;
        retval = 1;
    } else {
        int step;
        for (step = 2; step < 6; ++step)
            if (((uint32_t)val & (~(uint32_t)0 << (5 * step + 1))) == 0)
                break;
        retval = step;
        *buf = (char)(unsigned char)(~0xff >> step);
        --step;
        do {
            buf[step] = (char)(0x80 | (val & 0x3f));
            val >>= 6;
        } while (--step > 0);
        *buf = (char)(*buf | val);
    }
    return retval;
}

static struct section *make_seclist_elem(const char *string, struct section *next)
{
    struct section *n = xcalloc(1, sizeof *n);
    n->next = next;
    n->name = string;
    return n;
}

static struct element *new_element(struct collate *c, const char *mbs, size_t mbslen, const uint32_t *wcs,
                                   const char *name, size_t namelen, int is_character)
{
    struct element *n = xcalloc(1, sizeof *n);
    n->name = name ? xstrndup(name, namelen) : NULL;
    if (mbs) {
        char *m = xmalloc(mbslen + 1);
        memcpy(m, mbs, mbslen);
        m[mbslen] = '\0';
        n->mbs = m;
        n->nmbs = mbslen;
    }
    if (wcs) {
        size_t nw = wcslen32(wcs);
        if (nw == 0)
            nw = 1;         /* <U0000> */
        uint32_t *w = xmalloc((nw + 1) * sizeof *w);
        memcpy(w, wcs, nw * sizeof *w);
        w[nw] = 0;
        n->wcs = w;
        n->nwcs = nw;
    }
    n->is_character = is_character;
    n->section = c->current_section;
    return n;
}

static struct symbol *new_symbol(const char *name, size_t len)
{
    struct symbol *s = xcalloc(1, sizeof *s);
    s->name = xstrndup(name, len);
    return s;
}

static int check_duplicate(struct lexer *lr, struct collate *c, const struct charmap *cm,
                           const struct repertoire *rep, const char *symbol, size_t len)
{
    void *ignore;
    if (ht_find(&cm->char_table, symbol, len, &ignore) == 0) {
        lr_error(lr, "`%.*s' already defined in charmap", (int)len, symbol);
        return 1;
    }
    if (rep && ht_find(&rep->char_table, symbol, len, &ignore) == 0) {
        lr_error(lr, "`%.*s' already defined in repertoire", (int)len, symbol);
        return 1;
    }
    if (ht_find(&c->sym_table, symbol, len, &ignore) == 0) {
        lr_error(lr, "`%.*s' already defined as collating symbol", (int)len, symbol);
        return 1;
    }
    if (ht_find(&c->elem_table, symbol, len, &ignore) == 0) {
        lr_error(lr, "`%.*s' already defined as collating element", (int)len, symbol);
        return 1;
    }
    return 0;
}

static void read_directions(struct lexer *lr, struct token *arg, const struct charmap *cm,
                            const struct repertoire *rep, struct localedef *result, struct collate *c)
{
    int cnt = 0, max = nrules ? nrules : 10;
    int *rules = xcalloc((size_t)max, sizeof *rules);
    int warned = 0;
    for (;;) {
        int valid = 0;
        if (arg->tok == tok_forward || arg->tok == tok_backward) {
            int me = arg->tok == tok_forward ? sort_forward : sort_backward;
            int other = me == sort_forward ? sort_backward : sort_forward;
            if (rules[cnt] & other) {
                if (!warned) {
                    lr_error(lr, "%s: `forward' and `backward' are mutually excluding each other", "LC_COLLATE");
                    warned = 1;
                }
            } else if (rules[cnt] & me) {
                if (!warned)
                    lr_error(lr, "%s: `%s' mentioned more than once in definition of weight %d", "LC_COLLATE",
                             me == sort_forward ? "forward" : "backward", cnt + 1);
            } else
                rules[cnt] |= me;
            valid = 1;
        } else if (arg->tok == tok_position) {
            if (rules[cnt] & sort_position) {
                if (!warned)
                    lr_error(lr, "%s: `%s' mentioned more than once in definition of weight %d", "LC_COLLATE",
                             "position", cnt + 1);
            } else
                rules[cnt] |= sort_position;
            valid = 1;
        }
        if (valid)
            arg = lr_token(lr, cm, result, rep, verbose);
        if (arg->tok == tok_eof || arg->tok == tok_eol || arg->tok == tok_comma || arg->tok == tok_semicolon) {
            if (!valid && !warned) {
                lr_error(lr, "%s: syntax error", "LC_COLLATE");
                warned = 1;
            }
            if (arg->tok != tok_comma && rules[cnt] != 0) {
                if (rules[cnt] == sort_position)
                    rules[cnt] = sort_position | sort_forward;
                ++cnt;
            }
            if (arg->tok == tok_eof || arg->tok == tok_eol)
                break;
            if (nrules == 0) {
                if (cnt == max) {
                    max += 10;
                    rules = xrealloc(rules, (size_t)max * sizeof *rules);
                    memset(&rules[cnt], 0, (size_t)(max - cnt) * sizeof *rules);
                }
            } else if (cnt == nrules) {
                if (!warned) {
                    lr_error(lr, "%s: too many rules; first entry only had %d", "LC_COLLATE", nrules);
                    warned = 1;
                }
                lr_ignore_rest(lr, 0);
                break;
            }
        } else if (!warned) {
            lr_error(lr, "%s: syntax error", "LC_COLLATE");
            warned = 1;
        }
        arg = lr_token(lr, cm, result, rep, verbose);
    }
    if (nrules == 0) {
        nrules = cnt;
    } else if (cnt < nrules) {
        if (!warned)
            lr_error(lr, "%s: not enough sorting rules", "LC_COLLATE");
        do
            rules[cnt] = sort_forward;
        while (++cnt < nrules);
    }
    c->current_section->rules = rules;
}

static struct element *find_element(struct collate *c, const char *str, size_t len)
{
    void *r = NULL;
    if (ht_find(&c->seq_table, str, len, &r) != 0) {
        void *p;
        if (ht_find(&c->sym_table, str, len, &p) == 0) {
            struct symbol *sym = p;
            r = sym->order;
            if (!r)
                r = sym->order = new_element(c, NULL, 0, NULL, NULL, 0, 0);
        } else if (ht_find(&c->elem_table, str, len, &r) != 0) {
            /* a character, defined later */
            r = new_element(c, NULL, 0, NULL, str, len, 1);
            ht_insert(&c->seq_table, str, len, r);
        }
    }
    return r;
}

static void unlink_element(struct collate *c)
{
    if (c->cursor == c->start) {
        c->cursor = NULL;
    } else {
        if (c->cursor->next)
            c->cursor->next->last = c->cursor->last;
        if (c->cursor->last)
            c->cursor->last->next = c->cursor->next;
        c->cursor = c->cursor->last;
    }
}

static struct element **one_weight(struct element *e)
{
    struct element **w = xmalloc(sizeof *w);
    w[0] = e;
    return w;
}

static void insert_weights(struct lexer *lr, struct element *elem, const struct charmap *cm,
                           const struct repertoire *rep, struct localedef *result, struct collate *c,
                           enum tok ellipsis)
{
    int weight_cnt = 0;
    struct token *arg;
    elem->file = lr->fname;
    elem->line = lr->lineno;
    elem->last = c->cursor;
    elem->next = c->cursor ? c->cursor->next : NULL;
    if (c->cursor && c->cursor->next)
        c->cursor->next->last = elem;
    if (c->cursor)
        c->cursor->next = elem;
    if (!c->start)
        c->start = elem;
    elem->section = c->current_section;
    if (c->current_section) {
        if (!c->current_section->first)
            c->current_section->first = elem;
        if (c->current_section->last == c->cursor)
            c->current_section->last = elem;
    }
    c->cursor = elem;
    elem->weights = xcalloc((size_t)(nrules ? nrules : 1), sizeof *elem->weights);
    arg = lr_token(lr, cm, result, rep, verbose);
    do {
        if (arg->tok == tok_eof || arg->tok == tok_eol)
            break;
        if (arg->tok == tok_ignore) {
            elem->weights[weight_cnt].w = one_weight(NULL);
            elem->weights[weight_cnt].cnt = 1;
        } else if (arg->tok == tok_bsymbol || arg->tok == tok_ucs4) {
            char ucs4str[16];
            const char *symstr;
            size_t symlen;
            if (arg->tok == tok_bsymbol) {
                symstr = arg->str.startmb;
                symlen = arg->str.lenmb;
            } else {
                snprintf(ucs4str, sizeof ucs4str, "U%08X", arg->ucs4);
                symstr = ucs4str;
                symlen = 9;
            }
            struct element *val = find_element(c, symstr, symlen);
            if (!val)
                break;
            elem->weights[weight_cnt].w = one_weight(val);
            elem->weights[weight_cnt].cnt = 1;
        } else if (arg->tok == tok_string) {
            const char *cp = arg->str.startmb;
            int cnt = 0, max = 0;
            struct element **weights = NULL;
            if (!cp || *cp == '\0') {
                lr_error(lr, "%s: empty weight string not allowed", "LC_COLLATE");
                lr_ignore_rest(lr, 0);
                break;
            }
            bool bad = false;
            do {
                struct element *charelem;
                if (*cp == '<') {
                    const char *startp = ++cp;
                    size_t len;
                    char newstr[16];
                    while (*cp != '>') {
                        if (*cp == lr->escape_char)
                            ++cp;
                        if (*cp == '\0') {
                            bad = true;
                            break;
                        }
                        ++cp;
                    }
                    if (bad)
                        break;
                    if (cp - startp == 5 && startp[0] == 'U' && isxdigit((unsigned char)startp[1]) &&
                        isxdigit((unsigned char)startp[2]) && isxdigit((unsigned char)startp[3]) &&
                        isxdigit((unsigned char)startp[4])) {
                        snprintf(newstr, sizeof newstr, "U%08lX", strtoul(startp + 1, NULL, 16));
                        startp = newstr;
                        len = 9;
                    } else
                        len = (size_t)(cp - startp);
                    charelem = find_element(c, startp, len);
                    ++cp;
                } else {
                    charelem = find_element(c, cp++, 1);
                }
                if (!charelem) {
                    lr_ignore_rest(lr, 0);
                    break;
                }
                if (cnt >= max) {
                    max += 10;
                    weights = xrealloc(weights, (size_t)max * sizeof *weights);
                }
                weights[cnt++] = charelem;
            } while (*cp != '\0');
            if (bad) {
                lr_error(lr, "%s: syntax error", "LC_COLLATE");
                lr_ignore_rest(lr, 0);
                free(weights);
                break;
            }
            elem->weights[weight_cnt].w = weights;
            elem->weights[weight_cnt].cnt = cnt;
        } else if (ellipsis != tok_none &&
                   (arg->tok == tok_ellipsis2 || arg->tok == tok_ellipsis3 || arg->tok == tok_ellipsis4)) {
            if (arg->tok != ellipsis)
                lr_error(lr, "%s: weights must use the same ellipsis symbol as the name", "LC_COLLATE");
            elem->weights[weight_cnt].w = one_weight(ELLIPSIS);
            elem->weights[weight_cnt].cnt = 1;
        } else {
            lr_error(lr, "%s: syntax error", "LC_COLLATE");
            lr_ignore_rest(lr, 0);
            break;
        }
        arg = lr_token(lr, cm, result, rep, verbose);
        if (arg->tok == tok_semicolon)
            arg = lr_token(lr, cm, result, rep, verbose);
        else if (arg->tok != tok_eof && arg->tok != tok_eol) {
            lr_error(lr, "%s: syntax error", "LC_COLLATE");
            lr_ignore_rest(lr, 0);
            break;
        }
    } while (++weight_cnt < nrules);
    if (weight_cnt < nrules) {
        do {
            elem->weights[weight_cnt].w = one_weight(ellipsis == tok_none ? elem : ELLIPSIS);
            elem->weights[weight_cnt].cnt = 1;
        } while (++weight_cnt < nrules);
    } else if (arg->tok == tok_ignore || arg->tok == tok_bsymbol) {
        lr_error(lr, "%s: too many values", "LC_COLLATE");
        lr_ignore_rest(lr, 0);
    } else
        lr_ignore_rest(lr, arg->tok != tok_eol && arg->tok != tok_eof);
}

static int insert_value(struct lexer *lr, const char *symstr, size_t symlen, const struct charmap *cm,
                        const struct repertoire *rep, struct localedef *result, struct collate *c)
{
    struct charseq *seq = charmap_find_value(cm, symstr, symlen);
    uint32_t wc;
    struct element *elem = NULL;
    if (!seq || seq->ucs4 == UNINITIALIZED_CHAR_VALUE) {
        wc = repertoire_find_value(rep, symstr, symlen);
        if (seq)
            seq->ucs4 = wc;
    } else
        wc = seq->ucs4;
    if (wc == ILLEGAL_CHAR_VALUE && !seq) {
        void *p;
        if (ht_find(&c->elem_table, symstr, symlen, &p) != 0) {
            struct symbol *sym = NULL;
            if (ht_find(&c->sym_table, symstr, symlen, &p) == 0) {
                sym = p;
                elem = sym->order;
            }
            if (!elem) {
                elem = new_element(c, NULL, 0, NULL, symstr, symlen, 0);
                if (sym)
                    sym->order = elem;
                else
                    ht_insert(&c->seq_table, symstr, symlen, elem);
            }
        } else
            elem = p;
    } else {
        void *p;
        if (ht_find(&c->seq_table, symstr, symlen, &p) != 0) {
            uint32_t wcs[2] = { wc, 0 };
            elem = new_element(c, seq ? (const char *)seq->bytes : NULL, seq ? (size_t)seq->nbytes : 0,
                               wc == ILLEGAL_CHAR_VALUE ? NULL : wcs, symstr, symlen, 1);
            ht_insert(&c->seq_table, symstr, symlen, elem);
        } else {
            elem = p;
            if (!elem->mbs && seq) {
                elem->mbs = xstrndup((const char *)seq->bytes, (size_t)seq->nbytes);
                elem->nmbs = (size_t)seq->nbytes;
            }
            if (!elem->wcs && wc != ILLEGAL_CHAR_VALUE) {
                uint32_t *w = xmalloc(2 * sizeof *w);
                w[0] = wc;
                w[1] = 0;
                elem->wcs = w;
                elem->nwcs = 1;
            }
        }
    }
    if (elem->next || elem == c->cursor) {
        lr_error(lr, "order for `%.*s' already defined at %s:%zu", (int)symlen, symstr, elem->file, elem->line);
        lr_ignore_rest(lr, 0);
        return 1;
    }
    insert_weights(lr, elem, cm, rep, result, c, tok_none);
    return 0;
}

/* the weights of an element of an ellipsis: those of the ellipsis */
static void ellipsis_weights(struct collate *c, struct element *elem)
{
    elem->weights = xcalloc((size_t)(nrules ? nrules : 1), sizeof *elem->weights);
    for (int cnt = 0; cnt < nrules; ++cnt)
        if (c->ellipsis_weight.weights[cnt].cnt == 1 && c->ellipsis_weight.weights[cnt].w[0] == ELLIPSIS) {
            elem->weights[cnt].w = one_weight(elem);
            elem->weights[cnt].cnt = 1;
        } else {
            elem->weights[cnt] = c->ellipsis_weight.weights[cnt];
        }
}

static void handle_ellipsis(struct lexer *lr, const char *symstr, size_t symlen, enum tok ellipsis,
                            const struct charmap *cm, const struct repertoire *rep, struct localedef *result,
                            struct collate *c)
{
    unlink_element(c);
    struct element *startp = c->cursor, *endp;
    if (symstr && insert_value(lr, symstr, symlen, cm, rep, result, c))
        return;
    c->cursor = startp;
    endp = c->cursor ? c->cursor->next : c->start;
    if (ellipsis == tok_ellipsis3) {
        size_t len = startp && startp->mbs ? startp->nmbs : endp && endp->mbs ? endp->nmbs : 0;
        char *mbcnt = xmalloc(len + 1), *mbend = xmalloc(len + 1);
        if (startp && endp && startp->mbs && endp->mbs && startp->nmbs != endp->nmbs) {
            lr_error(lr, "%s: byte sequences of first and last character must have the same length", "LC_COLLATE");
            return;
        }
        if ((!startp || startp->mbs) && (!endp || endp->mbs)) {
            int cnt;
            if (!startp || !startp->mbs)
                memset(mbcnt, 0, len);
            else {
                memcpy(mbcnt, startp->mbs, len);
                for (cnt = (int)len - 1; cnt >= 0; --cnt)
                    if (++mbcnt[cnt] != '\0')
                        break;
            }
            mbcnt[len] = '\0';
            if (!endp || !endp->mbs)
                memset(mbend, 0, len);
            else
                memcpy(mbend, endp->mbs, len);
            mbend[len] = '\0';
            int ret = memcmp(mbcnt, mbend, len);
            if (ret >= 0) {
                if (ret > 0)
                    lr_error(lr, "%s: byte sequence of first character of range is not lower than that of the last character",
                             "LC_COLLATE");
                return;
            }
            for (;;) {
                struct charseq *seq = charmap_find_symbol(cm, mbcnt, len);
                if (seq) {
                    struct element *elem;
                    size_t namelen = strlen(seq->name);
                    void *p;
                    if (seq->ucs4 == UNINITIALIZED_CHAR_VALUE)
                        seq->ucs4 = repertoire_find_value(rep, seq->name, namelen);
                    if (ht_find(&c->seq_table, seq->name, namelen, &p) != 0) {
                        uint32_t wcs[2] = { seq->ucs4, 0 };
                        elem = new_element(c, mbcnt, len, seq->ucs4 == ILLEGAL_CHAR_VALUE ? NULL : wcs, seq->name,
                                           namelen, 1);
                        ht_insert(&c->seq_table, seq->name, namelen, elem);
                    } else
                        elem = p;
                    if (elem->next || (c->cursor && elem->next == c->cursor)) {
                        lr_error(lr, "order for `%.*s' already defined at %s:%zu", (int)namelen, seq->name, elem->file,
                                 elem->line);
                        goto increment;
                    }
                    elem->last = c->cursor;
                    if (!c->cursor)
                        elem->next = NULL;
                    else {
                        elem->next = c->cursor->next;
                        elem->last->next = elem;
                        if (elem->next)
                            elem->next->last = elem;
                    }
                    if (!c->start)
                        c->start = elem;
                    c->cursor = elem;
                    ellipsis_weights(c, elem);
                }
            increment:
                for (cnt = (int)len - 1; cnt >= 0; --cnt)
                    if (++mbcnt[cnt] != '\0')
                        break;
                if (cnt < 0 || memcmp(mbcnt, mbend, len) >= 0)
                    break;
            }
        }
    } else {
        if (!startp)
            lr_error(lr, "%s: symbolic range ellipsis must not directly follow `order_start'", "LC_COLLATE");
        else if (!endp)
            lr_error(lr, "%s: symbolic range ellipsis must not be directly followed by `order_end'", "LC_COLLATE");
        else {
            const char *sname = startp->name ? startp->name : "", *ename = endp->name ? endp->name : "";
            size_t lenfrom = strlen(sname), lento = strlen(ename);
            char *buf = xmalloc(lento + 32);
            size_t preflen = 0;
            long from, to;
            char *cp;
            int base = ellipsis == tok_ellipsis2 ? 16 : 10;
            if (lenfrom != lento) {
            invalid_range:
                lr_error(lr, "`%s' and `%.*s' are not valid names for symbolic range", sname, (int)lento, ename);
                return;
            }
            while (sname[preflen] == ename[preflen])
                if (sname[preflen] == '\0')
                    return;
                else
                    ++preflen;
            errno = 0;
            from = strtol(sname + preflen, &cp, base);
            if ((from == (long)UINT_MAX && errno == ERANGE) || *cp != '\0')
                goto invalid_range;
            errno = 0;
            to = strtol(ename + preflen, &cp, base);
            if ((to == (long)UINT_MAX && errno == ERANGE) || *cp != '\0')
                goto invalid_range;
            memcpy(buf, sname, preflen);
            for (++from; from < to; ++from) {
                struct element *elem = NULL;
                struct charseq *seq;
                uint32_t wc;
                void *p;
                sprintf(buf + preflen, base == 10 ? "%0*ld" : "%0*lX", (int)(lenfrom - preflen), from);
                if (ht_find(&c->seq_table, buf, symlen, &p) == 0) {
                    elem = p;
                    if (elem->next || (c->cursor && elem->next == c->cursor)) {
                        lr_error(lr, "%s: order for `%.*s' already defined at %s:%zu", "LC_COLLATE", (int)lenfrom, buf,
                                 elem->file, elem->line);
                        continue;
                    }
                    if (!elem->name) {
                        lr_error(lr, "%s: `%s' must be a character", "LC_COLLATE", buf);
                        continue;
                    }
                }
                if (!elem || (!elem->mbs && !elem->wcs)) {
                    seq = charmap_find_value(cm, buf, lenfrom);
                    if (!seq || seq->ucs4 == UNINITIALIZED_CHAR_VALUE) {
                        wc = repertoire_find_value(rep, buf, lenfrom);
                        if (seq)
                            seq->ucs4 = wc;
                    } else
                        wc = seq->ucs4;
                    if (wc == ILLEGAL_CHAR_VALUE && !seq)
                        continue;
                    if (!elem) {
                        uint32_t wcs[2] = { wc, 0 };
                        elem = new_element(c, seq ? (const char *)seq->bytes : NULL, seq ? (size_t)seq->nbytes : 0,
                                           wc == ILLEGAL_CHAR_VALUE ? NULL : wcs, buf, lenfrom, 1);
                    } else {
                        if (seq) {
                            elem->mbs = xstrndup((const char *)seq->bytes, (size_t)seq->nbytes);
                            elem->nmbs = (size_t)seq->nbytes;
                        }
                        if (wc != ILLEGAL_CHAR_VALUE) {
                            uint32_t *w = xmalloc(2 * sizeof *w);
                            w[0] = wc;
                            w[1] = 0;
                            elem->wcs = w;
                            elem->nwcs = 1;
                        }
                    }
                    elem->file = lr->fname;
                    elem->line = lr->lineno;
                    elem->section = c->current_section;
                }
                elem->last = c->cursor;
                elem->next = c->cursor->next;
                elem->last->next = elem;
                if (elem->next)
                    elem->next->last = elem;
                c->cursor = elem;
                ellipsis_weights(c, elem);
            }
        }
    }
    c->cursor = endp;
}

static void startup(struct lexer *lr, struct localedef *locale, struct localedef *copy_locale, int ignore)
{
    if (!ignore && !locale->categories[C_COLLATE]) {
        if (!copy_locale) {
            struct collate *c = xcalloc(1, sizeof *c);
            locale->categories[C_COLLATE] = c;
            ht_init(&c->elem_table, 100);
            ht_init(&c->sym_table, 100);
            ht_init(&c->seq_table, 500);
            c->col_weight_max = -1;
        } else
            locale->categories[C_COLLATE] = copy_locale->categories[C_COLLATE];
    }
    lr->translate_strings = 0;
    lr->return_widestr = 0;
}

/* ---- reading ---- */

/* the pseudo token of "elifndef" taken as an "ifndef" */
#define TOK_IFNDEF tok_error

static enum tok skip_to(struct lexer *lr, const struct charmap *cm, int to_endif)
{
    for (;;) {
        struct token *now = lr_token(lr, cm, NULL, NULL, 0);
        enum tok t = now->tok;
        if (t == tok_eof || t == tok_end)
            return t;
        if (t == tok_ifdef) {
            lr_error(lr, "%s: nested conditionals not supported", "LC_COLLATE");
            t = skip_to(lr, cm, 1);
            if (t == tok_eof || t == tok_end)
                return t;
        } else if (t == tok_endif || (!to_endif && t == tok_else)) {
            lr_ignore_rest(lr, 1);
            return t;
        } else if (!to_endif && (t == tok_elifdef || t == tok_elifndef)) {
            return t;
        } else if (t == tok_else) {
            lr_error(lr, "%s: more than one 'else'", "LC_COLLATE");
        }
        lr_ignore_rest(lr, 0);
    }
}

static void add_define(struct token *arg)
{
    struct name_list *n = xmalloc(sizeof *n);
    n->str = xstrndup(arg->str.startmb, arg->str.lenmb);
    n->next = defined;
    defined = n;
}

static bool is_defined(struct token *arg)
{
    for (struct name_list *d = defined; d; d = d->next)
        if (strncmp(arg->str.startmb, d->str, arg->str.lenmb) == 0 && d->str[arg->str.lenmb] == '\0')
            return true;
    return false;
}

void collate_read(struct lexer *lr, struct localedef *result, const struct charmap *cm, const char *repertoire_name,
                  int ignore)
{
    const struct repertoire *rep = NULL;
    struct collate *c;
    struct token *now, *arg = NULL;
    enum tok nowtok, was_ellipsis = tok_none;
    struct localedef *copy_locale = NULL;
    int state = 0;
    if (repertoire_name)
        rep = repertoire_read(repertoire_name);
    lr_ignore_rest(lr, 1);
    for (;;) {
        do {
            now = lr_token(lr, cm, result, NULL, verbose);
            nowtok = now->tok;
        } while (nowtok == tok_eol);
        if (nowtok != tok_define)
            break;
        if (ignore)
            lr_ignore_rest(lr, 0);
        else {
            arg = lr_token(lr, cm, result, NULL, verbose);
            if (arg->tok != tok_ident)
                SYNTAX_ERROR(lr, "%s: syntax error", "LC_COLLATE");
            else {
                add_define(arg);
                lr_ignore_rest(lr, 1);
            }
        }
    }
    if (nowtok == tok_copy) {
        now = lr_token(lr, cm, result, NULL, verbose);
        if (now->tok != tok_string) {
            SYNTAX_ERROR(lr, "%s: syntax error", "LC_COLLATE");
        skip_category:
            do
                now = lr_token(lr, cm, result, NULL, verbose);
            while (now->tok != tok_eof && now->tok != tok_end);
            if (now->tok != tok_eof || (now = lr_token(lr, cm, result, NULL, verbose), now->tok == tok_eof))
                lr_error(lr, "%s: premature end of file", "LC_COLLATE");
            else if (now->tok != tok_lc_collate) {
                lr_error(lr, "%1$s: definition does not end with `END %1$s'", "LC_COLLATE");
                lr_ignore_rest(lr, 0);
            } else
                lr_ignore_rest(lr, 1);
            return;
        }
        if (!ignore) {
            copy_locale = load_locale(C_COLLATE, now->str.startmb, repertoire_name, cm, NULL);
            if (!(copy_locale->avail & (1 << C_COLLATE)) && locfile_read(copy_locale, cm) != 0)
                goto skip_category;
            if (!copy_locale->categories[C_COLLATE])
                return;
        }
        lr_ignore_rest(lr, 1);
        now = lr_token(lr, cm, result, NULL, verbose);
        nowtok = now->tok;
    }
    startup(lr, result, copy_locale, ignore);
    c = result->categories[C_COLLATE];
    for (;;) {
        char ucs4buf[16];
        const char *symstr;
        size_t symlen;
        struct element *seqp;
        void *p;
        if (nowtok == tok_eof)
            break;
        if (nowtok == tok_eol) {
            now = lr_token(lr, cm, result, NULL, verbose);
            nowtok = now->tok;
            continue;
        }
        switch (nowtok) {
        case tok_codepoint_collation:
            c->codepoint_collation = true;
            break;
        case tok_copy:
            now = lr_token(lr, cm, result, NULL, verbose);
            if (now->tok != tok_string)
                goto err_label;
            if (!ignore)
                load_locale(C_COLLATE, now->str.startmb, repertoire_name, cm, result);
            lr_ignore_rest(lr, 1);
            break;
        case tok_coll_weight_max:
            if (ignore) {
                lr_ignore_rest(lr, 0);
                break;
            }
            if (state != 0)
                goto err_label;
            arg = lr_token(lr, cm, result, NULL, verbose);
            if (arg->tok != tok_number)
                goto err_label;
            if (c->col_weight_max != -1)
                lr_error(lr, "%s: duplicate definition of `%s'", "LC_COLLATE", "col_weight_max");
            else
                c->col_weight_max = (long)arg->num;
            lr_ignore_rest(lr, 1);
            break;
        case tok_section_symbol:
            if (ignore) {
                lr_ignore_rest(lr, 0);
                break;
            }
            if (state != 0)
                goto err_label;
            arg = lr_token(lr, cm, result, rep, verbose);
            if (arg->tok != tok_bsymbol)
                goto err_label;
            else {
                struct section *known = c->sections;
                while (known && strcmp(known->name ? known->name : "", arg->str.startmb) != 0)
                    known = known->next;
                if (known)
                    lr_error(lr, "%s: duplicate declaration of section `%s'", "LC_COLLATE", arg->str.startmb);
                else
                    c->sections = make_seclist_elem(arg->str.startmb, c->sections);
                lr_ignore_rest(lr, known == NULL);
            }
            break;
        case tok_collating_element:
            if (ignore) {
                lr_ignore_rest(lr, 0);
                break;
            }
            if (state != 0 && state != 2)
                goto err_label;
            arg = lr_token(lr, cm, result, rep, verbose);
            if (arg->tok != tok_bsymbol)
                goto err_label;
            else {
                const char *symbol = arg->str.startmb;
                size_t symbol_len = arg->str.lenmb;
                arg = lr_token(lr, cm, result, rep, verbose);
                if (arg->tok != tok_from)
                    goto err_label;
                lr->return_widestr = 1;
                lr->translate_strings = 1;
                arg = lr_token(lr, cm, result, rep, verbose);
                lr->return_widestr = 0;
                lr->translate_strings = 0;
                if (arg->tok != tok_string)
                    goto err_label;
                if (symbol && !check_duplicate(lr, c, cm, rep, symbol, symbol_len) && arg->str.startmb)
                    ht_insert(&c->elem_table, symbol, symbol_len,
                              new_element(c, arg->str.startmb, arg->str.lenmb, arg->str.startwc, symbol, symbol_len, 0));
                lr_ignore_rest(lr, 1);
            }
            break;
        case tok_collating_symbol:
            if (ignore) {
                lr_ignore_rest(lr, 0);
                break;
            }
            if (state != 0 && state != 2)
                goto err_label;
            arg = lr_token(lr, cm, result, rep, verbose);
            if (arg->tok != tok_bsymbol)
                goto err_label;
            else {
                char *symbol = arg->str.startmb, *endsymbol = NULL;
                size_t symbol_len = arg->str.lenmb;
                enum tok ellipsis = tok_none;
                arg = lr_token(lr, cm, result, rep, verbose);
                if (arg->tok == tok_ellipsis2 || arg->tok == tok_ellipsis4) {
                    ellipsis = arg->tok;
                    arg = lr_token(lr, cm, result, rep, verbose);
                    if (arg->tok != tok_bsymbol)
                        goto err_label;
                    endsymbol = arg->str.startmb;
                    lr_ignore_rest(lr, 1);
                } else if (arg->tok != tok_eol)
                    goto err_label;
                if (!symbol || (ellipsis != tok_none && !endsymbol)) {
                    lr_error(lr, "%s: unknown character in collating symbol name", "LC_COLLATE");
                } else if (ellipsis == tok_none) {
                    if (!check_duplicate(lr, c, cm, rep, symbol, symbol_len))
                        ht_insert(&c->sym_table, symbol, symbol_len, new_symbol(symbol, symbol_len));
                } else if (symbol_len != strlen(endsymbol)) {
                col_sym_inv_range:
                    lr_error(lr, "invalid names for character range");
                } else {
                    size_t prefixlen;
                    char *endp;
                    for (prefixlen = 0; prefixlen < symbol_len; ++prefixlen)
                        if (symbol[prefixlen] != endsymbol[prefixlen])
                            break;
                    unsigned long from = strtoul(&symbol[prefixlen], &endp, ellipsis == tok_ellipsis2 ? 16 : 10);
                    if (*endp != '\0')
                        goto col_sym_inv_range;
                    unsigned long to = strtoul(&endsymbol[prefixlen], &endp, ellipsis == tok_ellipsis2 ? 16 : 10);
                    if (*endp != '\0' || from > to)
                        goto col_sym_inv_range;
                    while (from <= to) {
                        char *symbuf = xmalloc(symbol_len + 32);
                        sprintf(symbuf, ellipsis == tok_ellipsis2 ? "%.*s%.*lX" : "%.*s%.*lu", (int)prefixlen, symbol,
                                (int)(symbol_len - prefixlen), from);
                        if (check_duplicate(lr, c, cm, rep, symbuf, symbol_len))
                            break;
                        ht_insert(&c->sym_table, symbuf, symbol_len, new_symbol(symbuf, symbol_len));
                        ++from;
                    }
                }
            }
            break;
        case tok_symbol_equivalence:
            if (ignore) {
                lr_ignore_rest(lr, 0);
                break;
            }
            if (state != 0)
                goto err_label;
            arg = lr_token(lr, cm, result, rep, verbose);
            if (arg->tok != tok_bsymbol)
                goto err_label;
            else {
                const char *newname = arg->str.startmb;
                size_t newname_len = arg->str.lenmb;
                void *symval;
                arg = lr_token(lr, cm, result, rep, verbose);
                if (arg->tok != tok_bsymbol)
                    goto err_label;
                const char *symname = arg->str.startmb;
                size_t symname_len = arg->str.lenmb;
                if (!newname) {
                    lr_error(lr, "%s: unknown character in equivalent definition name", "LC_COLLATE");
                    break;
                }
                if (!symname) {
                    lr_error(lr, "%s: unknown character in equivalent definition value", "LC_COLLATE");
                    break;
                }
                if (ht_find(&c->sym_table, symname, symname_len, &symval) != 0) {
                    lr_error(lr, "%s: unknown symbol `%s' in equivalent definition", "LC_COLLATE", symname);
                    break;
                }
                if (ht_insert(&c->sym_table, newname, newname_len, symval) < 0) {
                    lr_error(lr, "error while adding equivalent collating symbol");
                    break;
                }
            }
            lr_ignore_rest(lr, 1);
            break;
        case tok_script:
            if (ignore) {
                lr_ignore_rest(lr, 0);
                break;
            }
            arg = lr_token(lr, cm, result, rep, verbose);
            if (arg->tok != tok_bsymbol)
                goto err_label;
            else {
                struct section *runp = c->known_sections;
                while (runp)
                    if (strncmp(runp->name, arg->str.startmb, arg->str.lenmb) == 0 &&
                        runp->name[arg->str.lenmb] == '\0')
                        break;
                    else
                        runp = runp->def_next;
                if (runp) {
                    lr_error(lr, "duplicate definition of script `%s'", runp->name);
                    lr_ignore_rest(lr, 0);
                    break;
                }
                runp = xcalloc(1, sizeof *runp);
                runp->name = xstrndup(arg->str.startmb, arg->str.lenmb);
                runp->def_next = c->known_sections;
                c->known_sections = runp;
            }
            lr_ignore_rest(lr, 1);
            break;
        case tok_order_start:
            if (ignore) {
                lr_ignore_rest(lr, 0);
                break;
            }
            if (state != 0 && state != 1 && state != 2)
                goto err_label;
            state = 1;
            arg = lr_token(lr, cm, result, rep, verbose);
            if (arg->tok == tok_bsymbol) {
                struct section *sp = c->known_sections;
                while (sp && (!sp->name || strncmp(sp->name, arg->str.startmb, arg->str.lenmb) != 0 ||
                              sp->name[arg->str.lenmb] != '\0'))
                    sp = sp->def_next;
                if (!sp) {
                    lr_error(lr, "%s: unknown section name `%.*s'", "LC_COLLATE", (int)arg->str.lenmb,
                             arg->str.startmb);
                    c->current_section = &c->error_section;
                    if (!c->error_section.first) {
                        if (!c->sections)
                            c->sections = &c->error_section;
                        else {
                            sp = c->sections;
                            while (sp->next)
                                sp = sp->next;
                            sp->next = &c->error_section;
                        }
                        c->error_section.next = NULL;
                    }
                } else {
                    if (sp->first)
                        lr_error(lr, "%s: multiple order definitions for section `%s'", "LC_COLLATE", sp->name);
                    else {
                        if (c->current_section) {
                            sp->next = c->current_section->next;
                            c->current_section->next = sp;
                        } else if (!c->sections)
                            c->sections = sp;
                        c->current_section = sp;
                    }
                    arg = lr_token(lr, cm, result, rep, verbose);
                    if (arg->tok == tok_eol) {
                        if (nrules > 1)
                            lr_error(lr, "%s: invalid number of sorting rules", "LC_COLLATE");
                        else
                            nrules = 1;
                        sp->rules = xmalloc((size_t)nrules * sizeof *sp->rules);
                        for (int i = 0; i < nrules; ++i)
                            sp->rules[i] = sort_forward;
                        break;
                    }
                    arg = lr_token(lr, cm, result, rep, verbose);
                }
            } else {
                c->current_section = &c->unnamed_section;
                if (c->unnamed_section_defined)
                    lr_error(lr, "%s: multiple order definitions for unnamed section", "LC_COLLATE");
                else {
                    c->unnamed_section.next = c->sections;
                    c->sections = &c->unnamed_section;
                    c->unnamed_section_defined = true;
                }
            }
            read_directions(lr, arg, cm, rep, result, c);
            lr->translate_strings = 0;
            break;
        case tok_order_end:
            if (ignore) {
                lr_ignore_rest(lr, 0);
                break;
            }
            if (state != 1)
                goto err_label;
            if (was_ellipsis != tok_none) {
                handle_ellipsis(lr, NULL, 0, was_ellipsis, cm, rep, result, c);
                was_ellipsis = tok_none;
            }
            state = 2;
            lr_ignore_rest(lr, 1);
            break;
        case tok_reorder_after:
            if (ignore) {
                lr_ignore_rest(lr, 0);
                break;
            }
            if (state == 1) {
                lr_error(lr, "%s: missing `order_end' keyword", "LC_COLLATE");
                state = 2;
                if (was_ellipsis != tok_none) {
                    handle_ellipsis(lr, NULL, 0, was_ellipsis, cm, rep, result, c);
                    was_ellipsis = tok_none;
                }
            } else if (state == 0 && !copy_locale)
                goto err_label;
            else if (state != 0 && state != 2 && state != 3)
                goto err_label;
            state = 3;
            arg = lr_token(lr, cm, result, rep, verbose);
            if (arg->tok == tok_bsymbol || arg->tok == tok_ucs4) {
                char ucsbuf[16];
                const char *startmb;
                size_t lenmb;
                int no_error = 1;
                if (arg->tok == tok_bsymbol) {
                    startmb = arg->str.startmb;
                    lenmb = arg->str.lenmb;
                } else {
                    snprintf(ucsbuf, sizeof ucsbuf, "U%08X", arg->ucs4);
                    startmb = ucsbuf;
                    lenmb = 9;
                }
                if (ht_find(&c->seq_table, startmb, lenmb, &p) == 0)
                    c->cursor = p;
                else if (ht_find(&c->sym_table, startmb, lenmb, &p) == 0) {
                    struct symbol *symbp = p;
                    if (symbp->order && (symbp->order->last || symbp->order->next))
                        c->cursor = symbp->order;
                    else {
                        lr_error(lr, "%s: order for collating symbol %.*s not yet defined", "LC_COLLATE", (int)lenmb,
                                 startmb);
                        c->cursor = NULL;
                        no_error = 0;
                    }
                } else if (ht_find(&c->elem_table, startmb, lenmb, &p) == 0) {
                    struct element *insp = p;
                    if (insp->last || insp->next)
                        c->cursor = insp;
                    else {
                        lr_error(lr, "%s: order for collating element %.*s not yet defined", "LC_COLLATE", (int)lenmb,
                                 startmb);
                        c->cursor = NULL;
                        no_error = 0;
                    }
                } else {
                    lr_error(lr, "%s: cannot reorder after %.*s: symbol not known", "LC_COLLATE", (int)lenmb, startmb);
                    c->cursor = NULL;
                    no_error = 0;
                }
                lr_ignore_rest(lr, no_error);
            } else
                goto err_label;
            break;
        case tok_reorder_end:
            if (ignore)
                break;
            if (state != 3)
                goto err_label;
            state = 4;
            lr_ignore_rest(lr, 1);
            break;
        case tok_reorder_sections_after:
            if (ignore) {
                lr_ignore_rest(lr, 0);
                break;
            }
            if (state == 1) {
                lr_error(lr, "%s: missing `order_end' keyword", "LC_COLLATE");
                state = 2;
                if (was_ellipsis != tok_none) {
                    handle_ellipsis(lr, NULL, 0, was_ellipsis, cm, rep, result, c);
                    was_ellipsis = tok_none;
                }
            } else if (state == 3) {
                record_error(0, "%s: missing `reorder-end' keyword", "LC_COLLATE");
                state = 4;
            } else if (state != 2 && state != 4)
                goto err_label;
            state = 5;
            arg = lr_token(lr, cm, result, rep, verbose);
            if (arg->tok == tok_bsymbol) {
                struct section *runp = c->sections;
                while (runp && !(runp->name && strlen(runp->name) == arg->str.lenmb &&
                                 memcmp(runp->name, arg->str.startmb, arg->str.lenmb) == 0))
                    runp = runp->next;
                if (runp)
                    c->current_section = runp;
                else {
                    lr_error(lr, "%s: section `%.*s' not known", "LC_COLLATE", (int)arg->str.lenmb, arg->str.startmb);
                    do {
                        lr_ignore_rest(lr, 0);
                        now = lr_token(lr, cm, result, NULL, verbose);
                    } while (now->tok == tok_reorder_sections_after || now->tok == tok_reorder_sections_end ||
                             now->tok == tok_end);
                    nowtok = now->tok;
                    continue;
                }
            } else
                goto err_label;
            break;
        case tok_reorder_sections_end:
            if (ignore)
                break;
            if (state != 5)
                goto err_label;
            state = 6;
            lr_ignore_rest(lr, 1);
            break;
        case tok_bsymbol:
        case tok_ucs4:
            if (ignore) {
                lr_ignore_rest(lr, 0);
                break;
            }
            if (state != 0 && state != 1 && state != 3 && state != 5)
                goto err_label;
            if ((state == 0 || state == 5) && nowtok == tok_ucs4)
                goto err_label;
            if (nowtok == tok_ucs4) {
                snprintf(ucs4buf, sizeof ucs4buf, "U%08X", now->ucs4);
                symstr = ucs4buf;
                symlen = 9;
            } else if (arg) {
                symstr = now->str.startmb;
                symlen = now->str.lenmb;
            } else {
                lr_error(lr, "%s: bad symbol <%.*s>", "LC_COLLATE", (int)now->str.lenmb, now->str.startmb);
                break;
            }
            if (state == 0) {
                if (ht_find(&c->seq_table, symstr, symlen, &p) == 0) {
                    seqp = p;
                    if (seqp->is_character)
                        goto err_label;
                    goto move_entry;
                } else {
                    if (ht_find(&c->sym_table, symstr, symlen, &p) != 0)
                        goto err_label;
                    if (!c->sections)
                        c->sections = c->current_section = &c->symbol_section;
                }
                if (was_ellipsis != tok_none) {
                    handle_ellipsis(lr, symstr, symlen, was_ellipsis, cm, rep, result, c);
                    was_ellipsis = tok_none;
                    break;
                }
            } else if (state == 3) {
                if (!c->cursor) {
                    lr_ignore_rest(lr, 0);
                    break;
                }
                if (ht_find(&c->seq_table, symstr, symlen, &p) == 0) {
                    seqp = p;
                    goto move_entry;
                }
                if (ht_find(&c->sym_table, symstr, symlen, &p) == 0 && (seqp = ((struct symbol *)p)->order) != NULL)
                    goto move_entry;
                if (ht_find(&c->elem_table, symstr, symlen, &p) == 0 &&
                    (seqp = p, seqp->last || seqp->next || (c->start && seqp == c->start))) {
                move_entry:
                    if (!seqp->last)
                        c->start = seqp->next;
                    else
                        seqp->last->next = seqp->next;
                    if (seqp->next)
                        seqp->next->last = seqp->last;
                    if (seqp->section) {
                        if (seqp->section->first == seqp) {
                            if (seqp->section->first == seqp->section->last)
                                seqp->section->first = seqp->section->last = NULL;
                            else
                                seqp->section->first = seqp->next;
                        } else if (seqp->section->last == seqp)
                            seqp->section->last = seqp->last;
                    }
                    insert_weights(lr, seqp, cm, rep, result, c, tok_none);
                    break;
                }
            } else if (state == 5) {
                struct section *runp = c->sections, *prevp = NULL;
                while (runp && !(runp->name && strlen(runp->name) == symlen && memcmp(runp->name, symstr, symlen) == 0)) {
                    prevp = runp;
                    runp = runp->next;
                }
                if (!runp) {
                    lr_error(lr, "%s: section `%.*s' not known", "LC_COLLATE", (int)symlen, symstr);
                    lr_ignore_rest(lr, 0);
                } else {
                    if (runp != c->current_section) {
                        if (prevp)
                            prevp->next = runp->next;
                        else
                            c->sections = runp->next;
                        runp->next = c->current_section->next;
                        c->current_section->next = runp;
                        c->current_section = runp;
                    }
                    arg = lr_token(lr, cm, result, rep, verbose);
                    if (arg->tok != tok_eof && arg->tok != tok_eol)
                        read_directions(lr, arg, cm, rep, result, c);
                }
                break;
            } else if (was_ellipsis != tok_none) {
                handle_ellipsis(lr, symstr, symlen, was_ellipsis, cm, rep, result, c);
                was_ellipsis = tok_none;
                break;
            }
            insert_value(lr, symstr, symlen, cm, rep, result, c);
            break;
        case tok_undefined:
            if (ignore) {
                lr_ignore_rest(lr, 0);
                break;
            }
            if (state != 1)
                goto err_label;
            if (was_ellipsis != tok_none) {
                lr_error(lr, "%s: cannot have `%s' as end of ellipsis range", "LC_COLLATE", "UNDEFINED");
                unlink_element(c);
                was_ellipsis = tok_none;
            }
            if (c->undefined.next || &c->undefined == c->cursor) {
                lr_error(lr, "%s: order for `%.*s' already defined at %s:%zu", "LC_COLLATE", 9, "UNDEFINED",
                         c->undefined.file, c->undefined.line);
                lr_ignore_rest(lr, 0);
            } else
                insert_weights(lr, &c->undefined, cm, rep, result, c, tok_none);
            break;
        case tok_ellipsis2:
        case tok_ellipsis3:
        case tok_ellipsis4:
            if (was_ellipsis != tok_none)
                goto err_label;
            if (state != 0 && state != 1 && state != 3)
                goto err_label;
            was_ellipsis = nowtok;
            insert_weights(lr, &c->ellipsis_weight, cm, rep, result, c, nowtok);
            break;
        case tok_end:
        seen_end:
            if (!ignore) {
                if (state == 0 && !copy_locale && !c->codepoint_collation)
                    lr_error(lr, "%s: empty category description not allowed", "LC_COLLATE");
                else if (state == 1) {
                    lr_error(lr, "%s: missing `order_end' keyword", "LC_COLLATE");
                    if (was_ellipsis != tok_none) {
                        handle_ellipsis(lr, NULL, 0, was_ellipsis, cm, rep, result, c);
                        was_ellipsis = tok_none;
                    }
                } else if (state == 3)
                    record_error(0, "%s: missing `reorder-end' keyword", "LC_COLLATE");
                else if (state == 5)
                    record_error(0, "%s: missing `reorder-sections-end' keyword", "LC_COLLATE");
            }
            arg = lr_token(lr, cm, result, NULL, verbose);
            if (arg->tok == tok_eof)
                break;
            if (arg->tok == tok_eol)
                lr_error(lr, "%s: incomplete `END' line", "LC_COLLATE");
            else if (arg->tok != tok_lc_collate)
                lr_error(lr, "%1$s: definition does not end with `END %1$s'", "LC_COLLATE");
            lr_ignore_rest(lr, arg->tok == tok_lc_collate);
            return;
        case tok_define:
            if (ignore) {
                lr_ignore_rest(lr, 0);
                break;
            }
            arg = lr_token(lr, cm, result, NULL, verbose);
            if (arg->tok != tok_ident)
                goto err_label;
            add_define(arg);
            lr_ignore_rest(lr, 1);
            break;
        case tok_undef: {
            if (ignore) {
                lr_ignore_rest(lr, 0);
                break;
            }
            arg = lr_token(lr, cm, result, NULL, verbose);
            if (arg->tok != tok_ident)
                goto err_label;
            struct name_list **dp = &defined;
            while (*dp)
                if (strncmp(arg->str.startmb, (*dp)->str, arg->str.lenmb) == 0 && (*dp)->str[arg->str.lenmb] == '\0') {
                    struct name_list *old = *dp;
                    *dp = old->next;
                    free(old->str);
                    free(old);
                } else
                    dp = &(*dp)->next;
            lr_ignore_rest(lr, 1);
            break;
        }
        case tok_ifdef:
            if (ignore) {
                lr_ignore_rest(lr, 0);
                break;
            }
        found_ifdef:
            arg = lr_token(lr, cm, result, NULL, verbose);
            if (arg->tok != tok_ident)
                goto err_label;
            lr_ignore_rest(lr, 1);
            if (c->else_action == else_none) {
                bool d = is_defined(arg);
                if ((nowtok == tok_ifdef && d) || (nowtok == TOK_IFNDEF && !d))
                    c->else_action = else_ignore;
                else {
                    nowtok = skip_to(lr, cm, 0);
                    if (nowtok == tok_else)
                        c->else_action = else_seen;
                    else if (nowtok == tok_elifdef) {
                        nowtok = tok_ifdef;
                        goto found_ifdef;
                    } else if (nowtok == tok_elifndef) {
                        nowtok = TOK_IFNDEF;
                        goto found_ifdef;
                    } else if (nowtok == tok_eof)
                        goto seen_eof;
                    else if (nowtok == tok_end)
                        goto seen_end;
                }
            } else {
                lr_error(lr, "%s: nested conditionals not supported", "LC_COLLATE");
                nowtok = skip_to(lr, cm, 1);
                if (nowtok == tok_eof)
                    goto seen_eof;
                else if (nowtok == tok_end)
                    goto seen_end;
            }
            break;
        case tok_elifdef:
        case tok_elifndef:
        case tok_else:
            if (ignore) {
                lr_ignore_rest(lr, 0);
                break;
            }
            lr_ignore_rest(lr, 1);
            if (c->else_action == else_ignore) {
                nowtok = skip_to(lr, cm, 1);
                if (nowtok == tok_eof)
                    goto seen_eof;
                else if (nowtok == tok_end)
                    goto seen_end;
            } else
                lr_error(lr, "%s: '%s' without matching 'ifdef' or 'ifndef'", "LC_COLLATE",
                         nowtok == tok_else ? "else" : nowtok == tok_elifdef ? "elifdef" : "elifndef");
            break;
        case tok_endif:
            if (ignore) {
                lr_ignore_rest(lr, 0);
                break;
            }
            lr_ignore_rest(lr, 1);
            if (c->else_action != else_ignore && c->else_action != else_seen)
                lr_error(lr, "%s: 'endif' without matching 'ifdef' or 'ifndef'", "LC_COLLATE");
            c->else_action = else_none;
            break;
        default:
        err_label:
            SYNTAX_ERROR(lr, "%s: syntax error", "LC_COLLATE");
        }
        now = lr_token(lr, cm, result, NULL, verbose);
        nowtok = now->tok;
    }
seen_eof:
    lr_error(lr, "%s: premature end of file", "LC_COLLATE");
}

/* ---- the weights ---- */

void collate_finish(struct localedef *locale, const struct charmap *cm)
{
    struct collate *c = locale->categories[C_COLLATE];
    int wcact, mbseqact, wcseqact, i, ruleidx;
    int *mbact;
    struct element *runp;
    int need_undefined = 0;
    struct section *sect;
    (void)cm;
    if (!c) {
        record_warning("No definition for %s category found", "LC_COLLATE");
        return;
    }
    if (c->codepoint_collation)
        return;
    if (nrules > 32)
        record_error(4, "too many rules");
    mbact = xcalloc((size_t)(nrules ? nrules : 1), sizeof *mbact);
    for (i = 0; i < nrules; ++i)
        for (sect = c->sections; sect; sect = sect->next)
            if (c->current_section && sect != c->current_section && sect->rules && c->current_section->rules &&
                (sect->rules[i] & sort_position) != (c->current_section->rules[i] & sort_position)) {
                record_error(0, "%s: `position' must be used for a specific level in all sections or none",
                             "LC_COLLATE");
                break;
            }
    for (runp = c->start; runp; runp = runp->next)
        if (runp->mbs)
            for (i = 0; i < nrules; ++i)
                for (int j = 0; j < runp->weights[i].cnt; ++j)
                    if (runp->weights[i].w[j]) {
                        if (!runp->weights[i].w[j]->weights) {
                            record_error(0, "%s:%zu: symbol `%s' not defined", runp->file, runp->line,
                                         runp->weights[i].w[j]->name ? runp->weights[i].w[j]->name : "");
                            need_undefined = 1;
                            runp->weights[i].w[j] = &c->undefined;
                        } else
                            runp->weights[i].w[j]->used_in_level |= 1u << i;
                    }
    for (i = 0; i < nrules; ++i)
        mbact[i] = 2;
    wcact = 2;
    mbseqact = 0;
    wcseqact = 0;
    for (runp = c->start; runp; runp = runp->next) {
        if (runp->used_in_level) {
            runp->mborder = xmalloc((size_t)nrules * sizeof(int));
            for (i = 0; i < nrules; ++i)
                runp->mborder[i] = runp->used_in_level & (1u << i) ? mbact[i]++ : 0;
        }
        if (runp->mbs) {
            struct element **eptr = &c->mbheads[(unsigned char)runp->mbs[0]], *lastp = NULL;
            bool dup = false;
            while (*eptr) {
                if ((*eptr)->nmbs < runp->nmbs)
                    break;
                if ((*eptr)->nmbs == runp->nmbs) {
                    int r = memcmp((*eptr)->mbs, runp->mbs, runp->nmbs);
                    if (r == 0) {
                        record_error(0, "%s:%zu: symbol `%s' has the same encoding as", (*eptr)->file, (*eptr)->line,
                                     (*eptr)->name);
                        record_error(0, "%s:%zu: symbol `%s'", runp->file, runp->line, runp->name);
                        dup = true;
                        break;
                    } else if (r < 0)
                        break;
                }
                lastp = *eptr;
                eptr = &(*eptr)->mbnext;
            }
            if (!dup) {
                runp->mbnext = *eptr;
                runp->mblast = lastp;
                if (*eptr)
                    (*eptr)->mblast = runp;
                *eptr = runp;
            }
        }
        if (runp->used_in_level)
            runp->wcorder = wcact++;
        if (runp->is_character) {
            if (runp->nmbs == 1)
                c->mbseqorder[(unsigned char)runp->mbs[0]] = (unsigned char)mbseqact++;
            runp->wcseqorder = wcseqact++;
        } else if (runp->mbs && runp->weights)
            runp->wcseqorder = wcseqact++;
    }
    for (i = 1; i < 256; ++i)
        if (!c->mbheads[i]) {
            need_undefined = 1;
            c->mbheads[i] = &c->undefined;
        }
    t3_init(&c->wcseqorder, 6, 10, 4, ~(uint32_t)0);
    for (runp = c->start; runp; runp = runp->next) {
        if (!runp->wcs)
            continue;
        if (runp->is_character)
            t3_add(&c->wcseqorder, runp->wcs[0], (uint32_t)runp->wcseqorder);
        struct element *e = wcmap_get(&c->wcheads, runp->wcs[0]), **eptr = &e, *lastp = NULL;
        bool dup = false;
        while (*eptr) {
            if ((*eptr)->nwcs < runp->nwcs)
                break;
            if ((*eptr)->nwcs == runp->nwcs) {
                int r = 0;
                for (size_t k = 0; k < runp->nwcs && !r; k++)
                    r = (int32_t)(*eptr)->wcs[k] < (int32_t)runp->wcs[k] ? -1
                        : (int32_t)(*eptr)->wcs[k] > (int32_t)runp->wcs[k];
                if (r == 0) {
                    record_error(0, "%s:%zu: symbol `%s' has the same encoding as", (*eptr)->file, (*eptr)->line,
                                 (*eptr)->name);
                    record_error(0, "%s:%zu: symbol `%s'", runp->file, runp->line, runp->name);
                    dup = true;
                    break;
                } else if (r < 0)
                    break;
            }
            lastp = *eptr;
            eptr = &(*eptr)->wcnext;
        }
        if (dup)
            continue;
        runp->wcnext = *eptr;
        runp->wclast = lastp;
        if (*eptr)
            (*eptr)->wclast = runp;
        *eptr = runp;
        if (eptr == &e)
            wcmap_put(&c->wcheads, runp->wcs[0], e);
    }
    c->undefined.used_in_level = need_undefined ? ~0u : 0;
    if (!c->undefined.file) {
        if (need_undefined) {
            c->undefined.mborder = xmalloc((size_t)(nrules ? nrules : 1) * sizeof(int));
            for (i = 0; i < nrules; ++i)
                c->undefined.mborder[i] = mbact[i]++;
        }
        c->undefined.wcorder = wcact++;
    }
    sect = c->sections;
    while (sect && !sect->rules)
        sect = sect->next;
    if (!sect) {
        record_error(4, "too many errors; giving up");
        return;
    }
    ruleidx = 0;
    do {
        struct section *osect = c->sections;
        while (osect != sect)
            if (osect->rules && memcmp(osect->rules, sect->rules, (size_t)nrules * sizeof *osect->rules) == 0)
                break;
            else
                osect = osect->next;
        sect->ruleidx = osect == sect ? ruleidx++ : osect->ruleidx;
        do
            sect = sect->next;
        while (sect && !sect->rules);
    } while (sect);
    free(mbact);
}

/* ---- the output ---- */

static int32_t output_weight(struct pool *pool, struct collate *c, struct element *elem)
{
    if (elem == &c->undefined)
        return 0;
    int32_t retval = (int32_t)pool->n;
    for (int cnt = 0; cnt < nrules; ++cnt) {
        char *buf = xmalloc((size_t)elem->weights[cnt].cnt * 7 + 1);
        int len = 0;
        for (int i = 0; i < elem->weights[cnt].cnt; ++i)
            if (elem->weights[cnt].w[i])
                len += utf8_encode(&buf[len], elem->weights[cnt].w[i]->mborder ? elem->weights[cnt].w[i]->mborder[cnt] : 0);
        pool_1(pool, len);
        pool_grow(pool, buf, (size_t)len);
        free(buf);
    }
    return retval | ((elem->section ? elem->section->ruleidx & 0x7f : 0) << 24);
}

static int32_t output_weightwc(struct pool *pool, struct collate *c, struct element *elem)
{
    if (elem == &c->undefined)
        return 0;
    int32_t retval = (int32_t)(pool->n / sizeof(int32_t));
    for (int cnt = 0; cnt < nrules; ++cnt) {
        int32_t j = 0;
        for (int i = 0; i < elem->weights[cnt].cnt; ++i)
            if (elem->weights[cnt].w[i])
                j++;
        pool_i32(pool, j);
        for (int i = 0; i < elem->weights[cnt].cnt; ++i)
            if (elem->weights[cnt].w[i])
                pool_i32(pool, elem->weights[cnt].w[i]->wcorder);
    }
    return retval | ((elem->section ? elem->section->ruleidx & 0x7f : 0) << 24);
}

static void align_pool(struct pool *p)
{
    while (p->n % 4)
        pool_1(p, 0);
}

/* the sequences of the multibyte characters as glibc compares bytes: as char */
static bool mb_consecutive(const struct element *a, const struct element *b)
{
    return b && a->nmbs == b->nmbs && memcmp(a->mbs, b->mbs, a->nmbs - 1) == 0 &&
           (signed char)a->mbs[a->nmbs - 1] == (signed char)b->mbs[a->nmbs - 1] + 1;
}

static bool wc_consecutive(const struct element *a, const struct element *b)
{
    return b && a->nwcs == b->nwcs && memcmp(a->wcs, b->wcs, (a->nwcs - 1) * sizeof(uint32_t)) == 0 &&
           a->wcs[a->nwcs - 1] == b->wcs[a->nwcs - 1] + 1;
}

static void add_to_tablewc(struct collate *c, struct table3 *tablewc, struct pool *weightpool, struct pool *extrapool,
                           struct pool *indpool, uint32_t ch, struct element *runp)
{
    if (!runp->wcnext && runp->nwcs == 1) {
        t3_add(tablewc, ch, (uint32_t)output_weightwc(weightpool, c, runp));
        return;
    }
    t3_add(tablewc, ch, (uint32_t) - (int32_t)(extrapool->n / sizeof(uint32_t)));
    do {
        if (wc_consecutive(runp, runp->wcnext)) {
            struct element *series_startp = runp, *curp;
            pool_i32(extrapool, -(int32_t)(indpool->n / sizeof(int32_t)));
            pool_i32(extrapool, (int32_t)runp->nwcs - 1);
            do
                runp = runp->wcnext;
            while (wc_consecutive(runp, runp->wcnext));
            curp = runp;
            for (size_t i = 1; i < runp->nwcs; ++i)
                pool_i32(extrapool, (int32_t)curp->wcs[i]);
            do {
                pool_i32(indpool, output_weightwc(weightpool, c, curp));
                curp = curp->wclast;
            } while (curp != series_startp);
            pool_i32(indpool, output_weightwc(weightpool, c, curp));
            for (size_t i = 1; i < curp->nwcs; ++i)
                pool_i32(extrapool, (int32_t)curp->wcs[i]);
        } else {
            int32_t weightidx = output_weightwc(weightpool, c, runp);
            pool_i32(extrapool, weightidx);
            pool_i32(extrapool, (int32_t)runp->nwcs - 1);
            for (size_t i = 1; i < runp->nwcs; ++i)
                pool_i32(extrapool, (int32_t)runp->wcs[i]);
        }
        runp = runp->wcnext;
    } while (runp);
}

static uint32_t elem_hash(const char *str, int32_t n)
{
    uint32_t result = (uint32_t)n;
    while (n-- > 0) {
        result <<= 3;
        result += (uint32_t)(int32_t)(signed char)*str++;
    }
    return result;
}

#define NELEMS 19

void collate_output(struct localedef *locale, const struct charmap *cm, const char *out)
{
    struct collate *c = locale->categories[C_COLLATE];
    struct locfile f;
    int32_t tablemb[256];
    struct pool weightpool = { 0 }, extrapool = { 0 }, indirectpool = { 0 };
    struct section *sect;
    struct table3 tablewc;
    struct element *runp;
    int i;
    init_locale_data(&f, NELEMS);
    add_locale_uint32(&f, (uint32_t)nrules);
    if (!c || c->codepoint_collation) {
        for (size_t idx = 1; idx < NELEMS; idx++) {
            if (idx == 13)
                add_locale_uint32(&f, 0);
            else if (idx == 18 && c)
                add_locale_string(&f, cm->code_set_name);
            else if (idx == 16 && c) {
                unsigned char seq[256];
                for (int k = 0; k < 256; k++)
                    seq[k] = (unsigned char)k;
                add_locale_raw_data(&f, seq, 256);
            } else if (idx == 17 && c) {
                uint32_t seq[263] = { 8, 1, 8, 0, 0xff, 6 * 4, 7 * 4 };
                for (uint32_t k = 0; k < 256; k++)
                    seq[7 + k] = k;
                add_locale_uint32_array(&f, seq, 263);
            } else
                add_locale_empty(&f);
        }
        write_locale_data(out, C_COLLATE, &f);
        return;
    }
    pool_i32(&extrapool, 0);
    pool_i32(&indirectpool, 0);
    for (sect = c->sections, i = 0; sect; sect = sect->next)
        if (sect->rules && sect->ruleidx == i) {
            for (int j = 0; j < nrules; ++j)
                pool_1(&weightpool, sect->rules[j]);
            ++i;
        }
    i = (nrules * i) % 4;
    if (i > 0)
        do
            pool_1(&weightpool, 0);
        while (++i < 4);
    pool_emit(&f, &weightpool);
    tablemb[0] = 0;
    if (c->undefined.used_in_level)
        output_weight(&weightpool, c, &c->undefined);
    for (int ch = 1; ch < 256; ++ch) {
        if (!c->mbheads[ch]->mbnext && c->mbheads[ch]->nmbs <= 1) {
            tablemb[ch] = output_weight(&weightpool, c, c->mbheads[ch]);
            continue;
        }
        struct element *lastp = NULL;
        runp = c->mbheads[ch];
        tablemb[ch] = -(int32_t)extrapool.n;
        do {
            if (mb_consecutive(runp, runp->mbnext)) {
                struct element *series_startp = runp, *curp;
                pool_i32(&extrapool, -(int32_t)(indirectpool.n / sizeof(int32_t)));
                do
                    runp = runp->mbnext;
                while (mb_consecutive(runp, runp->mbnext));
                curp = runp;
                pool_1(&extrapool, (int)curp->nmbs - 1);
                for (size_t k = 1; k < curp->nmbs; ++k)
                    pool_1(&extrapool, curp->mbs[k]);
                do {
                    pool_i32(&indirectpool, output_weight(&weightpool, c, curp));
                    curp = curp->mblast;
                } while (curp != series_startp);
                pool_i32(&indirectpool, output_weight(&weightpool, c, curp));
                for (size_t k = 1; k < curp->nmbs; ++k)
                    pool_1(&extrapool, curp->mbs[k]);
            } else {
                int32_t weightidx = output_weight(&weightpool, c, runp);
                pool_i32(&extrapool, weightidx);
                pool_1(&extrapool, (int)runp->nmbs - 1);
                for (size_t k = 1; k < runp->nmbs; ++k)
                    pool_1(&extrapool, runp->mbs[k]);
            }
            align_pool(&extrapool);
            lastp = runp;
            runp = runp->mbnext;
        } while (runp);
        if (lastp->nmbs != 1) {
            pool_i32(&extrapool, 0);
            pool_1(&extrapool, 0);
            pool_1(&extrapool, 0);
            align_pool(&extrapool);
        }
    }
    align_pool(&weightpool);
    add_locale_uint32_array(&f, (const uint32_t *)tablemb, 256);
    pool_emit(&f, &weightpool);
    pool_emit(&f, &extrapool);
    pool_emit(&f, &indirectpool);
    add_locale_empty(&f);
    add_locale_empty(&f);
    add_locale_empty(&f);
    pool_i32(&extrapool, 0);
    pool_i32(&indirectpool, 0);
    output_weightwc(&weightpool, c, &c->undefined);
    t3_init(&tablewc, 6, 10, 4, 0);
    {
        struct wcmap *m = &c->wcheads;
        uint32_t *keys = xmalloc((m->n + 1) * sizeof *keys);
        size_t nk = 0;
        for (size_t k = 0; k < m->size; k++)
            if (m->vals[k])
                keys[nk++] = m->keys[k];
        qsort(keys, nk, sizeof *keys, cmp_u32);
        for (size_t k = 0; k < nk; k++)
            add_to_tablewc(c, &tablewc, &weightpool, &extrapool, &indirectpool, keys[k], wcmap_get(m, keys[k]));
        free(keys);
    }
    add_locale_table3(&f, &tablewc, false);
    pool_emit(&f, &weightpool);
    pool_emit(&f, &extrapool);
    pool_emit(&f, &indirectpool);
    /* the collating elements' hash table */
    uint32_t elem_size = 0;
    for (runp = c->start; runp; runp = runp->next)
        if (runp->mbs && runp->weights && !runp->is_character)
            ++elem_size;
    elem_size = (uint32_t)next_prime(elem_size + (elem_size >> 1));
    uint32_t *elem_table = xcalloc((size_t)elem_size * 2, sizeof *elem_table);
    for (runp = c->start; runp; runp = runp->next) {
        if (!runp->mbs || !runp->weights || runp->is_character)
            continue;
        uint32_t namelen = (uint32_t)strlen(runp->name);
        uint32_t hash = elem_hash(runp->name, (int32_t)namelen);
        size_t idx = hash % elem_size;
        if (elem_table[idx * 2] != 0) {
            size_t iter = hash % (elem_size - 2) + 1;
            do {
                idx += iter;
                if (idx >= elem_size)
                    idx -= elem_size;
            } while (elem_table[idx * 2] != 0);
        }
        elem_table[idx * 2] = hash;
        elem_table[idx * 2 + 1] = (uint32_t)extrapool.n;
        pool_1(&extrapool, (int)namelen);
        pool_grow(&extrapool, runp->name, namelen);
        pool_1(&extrapool, (int)runp->nmbs);
        pool_grow(&extrapool, runp->mbs, runp->nmbs);
        if ((1 + namelen + 1 + runp->nmbs) % 4 != 0)
            pool_grow(&extrapool, "\0\0", 4 - (1 + namelen + 1 + runp->nmbs) % 4);
        pool_i32(&extrapool, runp->mbseqorder);
        pool_i32(&extrapool, (int32_t)runp->nwcs);
        pool_grow(&extrapool, runp->wcs, runp->nwcs * sizeof(uint32_t));
        pool_i32(&extrapool, runp->wcseqorder);
    }
    add_locale_uint32(&f, elem_size);
    add_locale_uint32_array(&f, elem_table, 2 * (size_t)elem_size);
    pool_emit(&f, &extrapool);
    add_locale_raw_data(&f, c->mbseqorder, 256);
    add_locale_table3(&f, &c->wcseqorder, false);
    add_locale_string(&f, cm->code_set_name);
    write_locale_data(out, C_COLLATE, &f);
}
