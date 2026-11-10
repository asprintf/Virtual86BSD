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
 * ctype.c - LC_CTYPE of localedef: the classes (upper ... alnum, and
 * charclass), the maps (toupper, tolower, and charconv), digits and
 * outdigits, transliteration (translit_start: include, translit_ignore,
 * default_missing, from to;to2...), copy (with definitions after it),
 * the POSIX defaults; the file: glibc's tables (the 256-entry arrays,
 * the 3-level tables of the classes, maps and widths, the
 * transliteration tables).
 *
 * The order in which characters are first named decides the order of
 * the blocks of the 3-level tables, so it follows glibc's: the order of
 * the definitions, then the WIDTH rules and the characters of the
 * charmap in the order of glibc's hash table.
 */

#define _POSIX_C_SOURCE 202405L

#include "localedef.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define EMPTY ((uint32_t)~0)
#define MAX_NR_CHARCLASS 32
#define MAX_NR_CHARMAP 16

/* the class bits as glibc's ctype.h and wctype.h have them on this host */
static bool little_endian(void)
{
    uint16_t x = 1;
    return *(unsigned char *)&x == 1;
}

static uint32_t ISbit(int bit)
{
    if (!little_endian())
        return (uint32_t)1 << bit;
    return bit < 8 ? (uint32_t)(1 << bit) << 8 : (uint32_t)(1 << bit) >> 8;
}

static uint32_t ISwbit(int bit)
{
    if (!little_endian())
        return (uint32_t)1 << bit;
    unsigned long b = 1UL << bit;
    return (uint32_t)(bit < 8 ? b << 24 : bit < 16 ? b << 8 : bit < 24 ? b >> 8 : b >> 24);
}

#define BITPOS(t) ((int)((t) - tok_upper))
#define BIT(t) ISbit(BITPOS(t))
#define BITw(t) ISwbit(BITPOS(t))

static uint32_t no_str[] = { 0 };

/* ---- a map from characters to their indexes ---- */

struct u32map {
    uint32_t *keys, *vals;
    size_t size, n;
};

static size_t u32map_slot(const struct u32map *m, uint32_t k)
{
    size_t i = (k * 2654435761u) & (m->size - 1);
    while (m->keys[i] != EMPTY && m->keys[i] != k)
        i = (i + 1) & (m->size - 1);
    return i;
}

static uint32_t u32map_get(const struct u32map *m, uint32_t k)
{
    if (!m->size)
        return EMPTY;
    size_t i = u32map_slot(m, k);
    return m->keys[i] == k ? m->vals[i] : EMPTY;
}

static void u32map_put(struct u32map *m, uint32_t k, uint32_t v)
{
    if (2 * (m->n + 1) > m->size) {
        struct u32map o = *m;
        m->size = o.size ? 2 * o.size : 1024;
        m->keys = xmalloc(m->size * sizeof *m->keys);
        m->vals = xmalloc(m->size * sizeof *m->vals);
        memset(m->keys, 0xff, m->size * sizeof *m->keys);
        m->n = 0;
        for (size_t i = 0; i < o.size; i++)
            if (o.keys[i] != EMPTY)
                u32map_put(m, o.keys[i], o.vals[i]);
        free(o.keys);
        free(o.vals);
    }
    size_t i = u32map_slot(m, k);
    if (m->keys[i] == EMPTY)
        m->n++;
    m->keys[i] = k;
    m->vals[i] = v;
}

/* ---- 3-level tables ---- */


void t3_init(struct table3 *t, unsigned p, unsigned q, int esize, uint32_t def)
{
    memset(t, 0, sizeof *t);
    t->p = p;
    t->q = q;
    t->esize = esize;
    t->def = def;
}

static uint32_t t3_l3(const struct table3 *t, size_t i)
{
    return t->esize == 1 ? ((uint8_t *)t->level3)[i] : ((uint32_t *)t->level3)[i];
}

static void t3_set3(struct table3 *t, size_t i, uint32_t v)
{
    if (t->esize == 1)
        ((uint8_t *)t->level3)[i] = (uint8_t)v;
    else
        ((uint32_t *)t->level3)[i] = v;
}

uint32_t t3_get(const struct table3 *t, uint32_t wc)
{
    uint32_t i1 = wc >> (t->q + t->p);
    if (i1 < t->level1_size) {
        uint32_t l1 = t->level1[i1];
        if (l1 != EMPTY) {
            uint32_t i2 = ((wc >> t->p) & ((1u << t->q) - 1)) + (l1 << t->q);
            uint32_t l2 = t->level2[i2];
            if (l2 != EMPTY)
                return t3_l3(t, (wc & ((1u << t->p) - 1)) + (l2 << t->p));
        }
    }
    return t->def;
}

/* the cell of wc, its blocks made as needed */
static size_t t3_cell(struct table3 *t, uint32_t wc, unsigned shift5)
{
    uint32_t i1 = wc >> (t->q + t->p + shift5);
    uint32_t i2 = (wc >> (t->p + shift5)) & ((1u << t->q) - 1);
    uint32_t i3 = (wc >> shift5) & ((1u << t->p) - 1);
    if (i1 >= t->level1_size) {
        if (i1 >= t->level1_alloc) {
            size_t alloc = 2 * t->level1_alloc;
            if (alloc <= i1)
                alloc = i1 + 1;
            t->level1 = xrealloc(t->level1, alloc * sizeof *t->level1);
            t->level1_alloc = alloc;
        }
        while (i1 >= t->level1_size)
            t->level1[t->level1_size++] = EMPTY;
    }
    if (t->level1[i1] == EMPTY) {
        if (t->level2_size == t->level2_alloc) {
            size_t alloc = 2 * t->level2_alloc + 1;
            t->level2 = xrealloc(t->level2, (alloc << t->q) * sizeof *t->level2);
            t->level2_alloc = alloc;
        }
        for (size_t i = t->level2_size << t->q; i < (t->level2_size + 1) << t->q; i++)
            t->level2[i] = EMPTY;
        t->level1[i1] = (uint32_t)t->level2_size++;
    }
    i2 += t->level1[i1] << t->q;
    if (t->level2[i2] == EMPTY) {
        if (t->level3_size == t->level3_alloc) {
            size_t alloc = 2 * t->level3_alloc + 1;
            t->level3 = xrealloc(t->level3, (alloc << t->p) * (size_t)t->esize);
            t->level3_alloc = alloc;
        }
        for (size_t i = t->level3_size << t->p; i < (t->level3_size + 1) << t->p; i++)
            t3_set3(t, i, t->def);
        t->level2[i2] = (uint32_t)t->level3_size++;
    }
    return i3 + (t->level2[i2] << t->p);
}

void t3_add(struct table3 *t, uint32_t wc, uint32_t value)
{
    if (value == t3_get(t, wc))
        return;
    t3_set3(t, t3_cell(t, wc, 0), value);
}

/* a class: a bit set (32 characters to a level-3 entry) */
static void t3_add_bit(struct table3 *t, uint32_t wc)
{
    size_t i = t3_cell(t, wc, 5);
    t3_set3(t, i, t3_l3(t, i) | (uint32_t)1 << (wc & 0x1f));
}

/* the table, its blocks made unique, in the file */
void add_locale_table3(struct locfile *f, struct table3 *t, bool bits)
{
    size_t bs3 = ((size_t)1 << t->p) * (size_t)t->esize, bs2 = ((size_t)1 << t->q) * sizeof(uint32_t);
    uint32_t *reorder3 = xmalloc((t->level3_size + 1) * sizeof *reorder3);
    uint32_t *reorder2 = xmalloc((t->level2_size + 1) * sizeof *reorder2);
    unsigned char *l3 = t->level3;
    size_t k = 0, i, j;
    for (j = 0; j < t->level3_size; j++) {
        for (i = 0; i < k; i++)
            if (memcmp(l3 + i * bs3, l3 + j * bs3, bs3) == 0)
                break;
        reorder3[j] = (uint32_t)i;
        if (i == k) {
            if (i != j)
                memcpy(l3 + i * bs3, l3 + j * bs3, bs3);
            k++;
        }
    }
    t->level3_size = k;
    for (i = 0; i < (t->level2_size << t->q); i++)
        if (t->level2[i] != EMPTY)
            t->level2[i] = reorder3[t->level2[i]];
    k = 0;
    for (j = 0; j < t->level2_size; j++) {
        for (i = 0; i < k; i++)
            if (memcmp(&t->level2[i << t->q], &t->level2[j << t->q], bs2) == 0)
                break;
        reorder2[j] = (uint32_t)i;
        if (i == k) {
            if (i != j)
                memcpy(&t->level2[i << t->q], &t->level2[j << t->q], bs2);
            k++;
        }
    }
    t->level2_size = k;
    for (i = 0; i < t->level1_size; i++)
        if (t->level1[i] != EMPTY)
            t->level1[i] = reorder2[t->level1[i]];
    uint32_t level2_offset = (uint32_t)(5 * 4 + t->level1_size * 4);
    uint32_t level3_offset = (uint32_t)(level2_offset + (t->level2_size << t->q) * 4);
    unsigned s5 = bits ? 5 : 0;
    start_locale_structure(f);
    add_locale_uint32(f, t->q + t->p + s5);
    add_locale_uint32(f, (uint32_t)t->level1_size);
    add_locale_uint32(f, t->p + s5);
    add_locale_uint32(f, (1u << t->q) - 1);
    add_locale_uint32(f, (1u << t->p) - 1);
    for (i = 0; i < t->level1_size; i++)
        add_locale_uint32(f, t->level1[i] == EMPTY ? 0 : (t->level1[i] << t->q) * 4 + level2_offset);
    for (i = 0; i < (t->level2_size << t->q); i++)
        add_locale_uint32(f, t->level2[i] == EMPTY ? 0 : (uint32_t)((t->level2[i] << t->p) * (size_t)t->esize) + level3_offset);
    if (t->esize == 1)
        add_locale_raw_data(f, t->level3, t->level3_size << t->p);
    else
        add_locale_uint32_array(f, t->level3, t->level3_size << t->p);
    if (!bits)
        align_locale_data(f, 4);
    end_locale_structure(f);
    free(reorder3);
    free(reorder2);
}

/* ---- the category ---- */

struct translit_to {
    uint32_t *str;
    struct translit_to *next;
};

struct translit {
    uint32_t *from;
    struct translit_to *to;
    struct translit *next;
};

struct translit_ignore {
    uint32_t from, to, step;
    struct translit_ignore *next;
};

struct translit_include {
    const char *copy_locale, *copy_repertoire;
    struct translit_include *next;
};

struct ctype {
    uint32_t *charnames;
    size_t charnames_max, charnames_act;
    struct u32map charnames_idx;
    const struct repertoire *repertoire;
    size_t nr_charclass;
    const char *classnames[MAX_NR_CHARCLASS];
    uint32_t class256_collection[256];
    uint32_t *class_collection;
    size_t class_collection_max, class_collection_act;
    uint32_t class_done;
    struct charseq **mbdigits;
    size_t mbdigits_act, mbdigits_max;
    uint32_t *wcdigits;
    size_t wcdigits_act, wcdigits_max;
    struct charseq *mboutdigits[10];
    uint32_t wcoutdigits[10];
    size_t outdigits_act;
    const char *mapnames[MAX_NR_CHARMAP];
    uint32_t *map_collection[MAX_NR_CHARMAP];
    uint32_t map256_collection[2][256];
    size_t map_collection_max[MAX_NR_CHARMAP], map_collection_act[MAX_NR_CHARMAP];
    size_t map_collection_nr;
    int tomap_done[MAX_NR_CHARMAP];
    struct translit_include *translit_include;
    struct translit *translit;
    struct translit_ignore *translit_ignore;
    uint32_t ntranslit_ignore;
    uint32_t *default_missing;
    uint32_t to_nonascii, nonascii_case;
    /* the output */
    uint16_t *ctype_b;
    uint32_t *ctype32_b, **map_b, **map32_b, **class_b;
    struct table3 *class_3level, *map_3level, width;
    uint32_t class_offset, map_offset, mb_cur_max;
    const char *codeset_name;
    uint32_t *translit_from_idx, *translit_from_tbl, *translit_to_idx, *translit_to_tbl;
    uint32_t translit_idx_size;
    size_t translit_from_tbl_size, translit_to_tbl_size;
};

static const char *const longnames[] = { "zero", "one", "two", "three", "four",
                                         "five", "six", "seven", "eight", "nine" };
static const char *const uninames[] = { "U00000030", "U00000031", "U00000032", "U00000033", "U00000034",
                                        "U00000035", "U00000036", "U00000037", "U00000038", "U00000039" };
static const char digits[] = "0123456789";

static void class_new(struct lexer *lr, struct ctype *c, const char *name)
{
    for (size_t i = 0; i < c->nr_charclass; i++)
        if (strcmp(c->classnames[i], name) == 0) {
            if (lr)
                lr_error(lr, "character class `%s' already defined", name);
            return;
        }
    if (c->nr_charclass == MAX_NR_CHARCLASS)
        record_error(2, "implementation limit: no more than %d character classes allowed", MAX_NR_CHARCLASS);
    c->classnames[c->nr_charclass++] = name;
}

static void map_new(struct lexer *lr, struct ctype *c, const char *name, const struct charmap *cm)
{
    size_t max_chars = 0, i;
    for (i = 0; i < c->map_collection_nr; i++) {
        if (strcmp(c->mapnames[i], name) == 0)
            break;
        if (max_chars < c->map_collection_max[i])
            max_chars = c->map_collection_max[i];
    }
    if (i < c->map_collection_nr) {
        if (lr)
            lr_error(lr, "character map `%s' already defined", name);
        return;
    }
    if (c->map_collection_nr == MAX_NR_CHARMAP)
        record_error(2, "implementation limit: no more than %d character maps allowed", MAX_NR_CHARMAP);
    c->mapnames[i] = name;
    c->map_collection_max[i] = max_chars == 0 ? (cm->mb_cur_max == 1 ? 256 : 512) : max_chars;
    c->map_collection[i] = xcalloc(c->map_collection_max[i], sizeof(uint32_t));
    c->map_collection_act[i] = 256;
    c->map_collection_nr++;
}

static uint32_t *find_idx(struct ctype *c, uint32_t **table, size_t *max, size_t *act, uint32_t idx)
{
    if (idx < 256)
        return table ? &(*table)[idx] : NULL;
    uint32_t cnt = u32map_get(&c->charnames_idx, idx);
    if (cnt == EMPTY)
        cnt = (uint32_t)c->charnames_act;
    if (cnt == c->charnames_act) {
        if (c->charnames_act == c->charnames_max) {
            c->charnames_max *= 2;
            c->charnames = xrealloc(c->charnames, c->charnames_max * sizeof *c->charnames);
        }
        c->charnames[c->charnames_act++] = idx;
        u32map_put(&c->charnames_idx, idx, cnt);
    }
    if (!table)
        return NULL;
    if (!max)
        return cnt >= *act ? NULL : &(*table)[cnt];
    if (cnt >= *act) {
        if (cnt >= *max) {
            size_t old = *max;
            do
                *max *= 2;
            while (*max <= cnt);
            *table = xrealloc(*table, *max * sizeof **table);
            memset(&(*table)[old], 0, (*max - old) * sizeof **table);
        }
        *act = cnt + 1;
    }
    return &(*table)[cnt];
}

#define ELEM_CLASS(c, value) (*find_idx(c, &(c)->class_collection, &(c)->class_collection_max, &(c)->class_collection_act, value))
#define ELEM_MAP(c, n, value) (*find_idx(c, &(c)->map_collection[n], &(c)->map_collection_max[n], &(c)->map_collection_act[n], value))

static void startup(struct lexer *lr, struct localedef *locale, const struct charmap *cm,
                    struct localedef *copy_locale, int ignore)
{
    if (ignore || locale->categories[C_CTYPE])
        return;
    if (copy_locale) {
        locale->categories[C_CTYPE] = copy_locale->categories[C_CTYPE];
        return;
    }
    struct ctype *c = xcalloc(1, sizeof *c);
    locale->categories[C_CTYPE] = c;
    c->charnames_max = cm->mb_cur_max == 1 ? 256 : 512;
    c->charnames = xmalloc(c->charnames_max * sizeof *c->charnames);
    for (uint32_t i = 0; i < 256; i++)
        c->charnames[i] = i;
    c->charnames_act = 256;
    static const char *const classes[] = { "upper", "lower", "alpha", "digit", "xdigit", "space",
                                           "print", "graph", "blank", "cntrl", "punct", "alnum" };
    for (size_t i = 0; i < 12; i++)
        class_new(lr, c, classes[i]);
    c->class_collection_max = cm->mb_cur_max == 1 ? 256 : 512;
    c->class_collection = xcalloc(c->class_collection_max, sizeof *c->class_collection);
    c->class_collection_act = 256;
    map_new(lr, c, "toupper", cm);
    map_new(lr, c, "tolower", cm);
    for (uint32_t i = 0; i < 256; i++) {
        c->map_collection[0][i] = c->map_collection[1][i] = i;
        c->map256_collection[0][i] = c->map256_collection[1][i] = i;
    }
    if (enc_not_ascii_compatible)
        c->to_nonascii = 1;
}

/* the character of a token: its bytes in the charmap and its UCS4 value */
static int get_character(struct token *now, const struct charmap *cm, const struct repertoire *rep,
                         struct charseq **seqp, uint32_t *wchp)
{
    if (now->tok == tok_bsymbol) {
        *wchp = repertoire_find_value(rep, now->str.startmb, now->str.lenmb);
        *seqp = charmap_find_value(cm, now->str.startmb, now->str.lenmb);
    } else if (now->tok == tok_ucs4) {
        char utmp[16];
        snprintf(utmp, sizeof utmp, "U%08X", now->ucs4);
        *seqp = charmap_find_value(cm, utmp, 9);
        if (!*seqp)
            *seqp = repertoire_find_seq(rep, now->ucs4);
        if (!*seqp) {
            const char *sym = repertoire_find_symbol(rep, now->ucs4);
            *seqp = sym ? charmap_find_value(cm, sym, strlen(sym)) : NULL;
            if (*seqp)
                (*seqp)->ucs4 = now->ucs4;
        } else if ((*seqp)->ucs4 != now->ucs4) {
            *seqp = NULL;
        }
        *wchp = now->ucs4;
    } else if (now->tok == tok_charcode) {
        *seqp = charmap_find_symbol(cm, (const char *)now->charcode.bytes, (size_t)now->charcode.nbytes);
        if (!*seqp)
            *wchp = ILLEGAL_CHAR_VALUE;
        else {
            if ((*seqp)->ucs4 == UNINITIALIZED_CHAR_VALUE)
                (*seqp)->ucs4 = repertoire_find_value(rep, (*seqp)->name, strlen((*seqp)->name));
            *wchp = (*seqp)->ucs4;
        }
    } else
        return 1;
    return 0;
}

static void add_digit(struct ctype *c, struct charseq *seq, uint32_t wch)
{
    if (c->mbdigits_act == c->mbdigits_max) {
        c->mbdigits_max += 10;
        c->mbdigits = xrealloc(c->mbdigits, c->mbdigits_max * sizeof *c->mbdigits);
        c->wcdigits_max += 10;
        c->wcdigits = xrealloc(c->wcdigits, c->wcdigits_max * sizeof *c->wcdigits);
    }
    c->mbdigits[c->mbdigits_act++] = seq;
    c->wcdigits[c->wcdigits_act++] = wch;
}

static bool add_outdigit(struct lexer *lr, struct ctype *c, struct charseq *seq, uint32_t wch)
{
    if (c->outdigits_act >= 10) {
        lr_error(lr, "%s: field `%s' does not contain exactly ten entries", "LC_CTYPE", "outdigit");
        return false;
    }
    c->mboutdigits[c->outdigits_act] = seq;
    c->wcoutdigits[c->outdigits_act] = wch;
    c->outdigits_act++;
    return true;
}

/* one character of a range: in the classes, digits, outdigits */
static bool range_char(struct lexer *lr, struct ctype *c, struct charseq *seq, uint32_t wch,
                       uint32_t class256_bit, uint32_t class_bit, int handle_digits, bool seq_ok)
{
    if (seq && seq_ok && seq->nbytes == 1)
        c->class256_collection[seq->bytes[0]] |= class256_bit;
    if (wch != ILLEGAL_CHAR_VALUE && class_bit)
        ELEM_CLASS(c, wch) |= class_bit;
    if (handle_digits == 1)
        add_digit(c, seq, wch);
    else if (handle_digits == 2)
        return add_outdigit(lr, c, seq, wch);
    return true;
}

/* <name1>..<name2>: the names between them */
static void symbolic_ellipsis(struct lexer *lr, struct ctype *c, const struct charmap *cm,
                              const struct repertoire *rep, struct token *now, const char *last_str,
                              uint32_t class256_bit, uint32_t class_bit, int base, int ignore,
                              int handle_digits, int step)
{
    const char *nowstr = now->str.startmb;
    size_t len = now->str.lenmb;
    char *endp;
    if (strlen(last_str) != len) {
    invalid:
        lr_error(lr, "`%s' and `%.*s' are not valid names for symbolic range", last_str, (int)len, nowstr);
        return;
    }
    if (memcmp(last_str, nowstr, len) == 0)
        return;
    const char *cp = last_str;
    while (*cp == nowstr[cp - last_str])
        cp++;
    unsigned long from = strtoul(cp, &endp, base);
    if (*endp)
        goto invalid;
    unsigned long to = strtoul(nowstr + (cp - last_str), &endp, base);
    if ((size_t)(endp - nowstr) != len || from >= to)
        goto invalid;
    if (ignore)
        return;
    struct token tmp = *now;
    char *buf = xmalloc(len + 1);
    tmp.str.startmb = buf;
    while ((from += (unsigned long)step) <= to) {
        snprintf(buf, len + 1, base == 10 ? "%.*s%0*lu" : "%.*s%0*lX", (int)(cp - last_str), last_str,
                 (int)(len - (size_t)(cp - last_str)), from);
        struct charseq *seq;
        uint32_t wch;
        if (get_character(&tmp, cm, rep, &seq, &wch))
            goto invalid;
        if (!range_char(lr, c, seq, wch, class256_bit, class_bit, handle_digits, true))
            return;
    }
}

/* <Uxxxx>..<Uyyyy> */
static void ucs4_ellipsis(struct lexer *lr, struct ctype *c, const struct charmap *cm,
                          const struct repertoire *rep, struct token *now, uint32_t last_wch,
                          uint32_t class256_bit, uint32_t class_bit, int ignore, int handle_digits, int step)
{
    static struct charseq negative = { .ucs4 = ILLEGAL_CHAR_VALUE };
    if (last_wch > now->ucs4) {
        lr_error(lr, "to-value <U%0*X> of range is smaller than from-value <U%0*X>",
                 (now->ucs4 | last_wch) < 65536 ? 4 : 8, now->ucs4, (now->ucs4 | last_wch) < 65536 ? 4 : 8,
                 last_wch);
        return;
    }
    if (ignore)
        return;
    while ((last_wch += (uint32_t)step) <= now->ucs4) {
        char utmp[16];
        snprintf(utmp, sizeof utmp, "U%08X", last_wch);
        struct charseq *seq = charmap_find_value(cm, utmp, 9);
        if (!seq) {
            snprintf(utmp, sizeof utmp, "U%04X", last_wch);
            seq = charmap_find_value(cm, utmp, 5);
        }
        if (!seq)
            seq = repertoire_find_seq(rep, last_wch);
        if (!seq) {
            if (rep) {
                const char *sym = repertoire_find_symbol(rep, last_wch);
                if (sym)
                    seq = charmap_find_value(cm, sym, strlen(sym));
                if (!seq)
                    seq = &negative;
                else
                    seq->ucs4 = last_wch;
                uint32_t *key = xmalloc(sizeof *key);
                *key = last_wch;
                ht_insert(&((struct repertoire *)rep)->seq_table, key, sizeof *key, seq);
            } else
                seq = &negative;
        }
        bool same = seq->ucs4 == last_wch;
        if (same && seq->nbytes == 1)
            c->class256_collection[seq->bytes[0]] |= class256_bit;
        if (class_bit)
            ELEM_CLASS(c, last_wch) |= class_bit;
        if (handle_digits == 1)
            add_digit(c, same ? seq : NULL, last_wch);
        else if (handle_digits == 2 && !add_outdigit(lr, c, same ? seq : NULL, last_wch))
            return;
    }
}

/* \x41...\x5a */
static void charcode_ellipsis(struct lexer *lr, struct ctype *c, const struct charmap *cm,
                              const struct repertoire *rep, struct token *now, unsigned char *last,
                              int last_len, uint32_t class256_bit, uint32_t class_bit, int ignore,
                              int handle_digits)
{
    if (now->charcode.nbytes != last_len) {
        lr_error(lr, "start and end character sequence of range must have the same length");
        return;
    }
    if (memcmp(last, now->charcode.bytes, (size_t)last_len) > 0) {
        lr_error(lr, "to-value character sequence is smaller than from-value sequence");
        return;
    }
    if (ignore)
        return;
    do {
        for (int i = last_len - 1; i >= 0; --i)
            if (++last[i] != 0)
                break;
        if (last_len == 1)
            c->class256_collection[last[0]] |= class256_bit;
        struct charseq *seq = charmap_find_symbol(cm, (const char *)last, (size_t)last_len);
        uint32_t wch;
        if (seq) {
            if (seq->ucs4 == UNINITIALIZED_CHAR_VALUE)
                seq->ucs4 = repertoire_find_value(rep, seq->name, strlen(seq->name));
            wch = seq->ucs4;
            if (wch != ILLEGAL_CHAR_VALUE && class_bit)
                ELEM_CLASS(c, wch) |= class_bit;
        } else
            wch = ILLEGAL_CHAR_VALUE;
        if (handle_digits) {
            struct charseq *s = xmalloc(sizeof *s + (size_t)last_len);
            memcpy(s->bytes, last, (size_t)last_len);
            s->nbytes = last_len;
            s->name = "";
            s->ucs4 = wch;
            if (handle_digits == 1)
                add_digit(c, s, wch);
            else if (!add_outdigit(lr, c, s, wch))
                return;
        }
    } while (memcmp(last, now->charcode.bytes, (size_t)last_len) != 0);
}

/* ---- transliteration ---- */

static uint32_t *find_translit2(struct ctype *c, const struct charmap *cm, uint32_t wch)
{
    for (struct translit *t = c->translit; t; t = t->next) {
        if (t->from[0] == wch && t->from[1] == 0) {
            for (struct translit_to *to = t->to; to; to = to->next) {
                int i;
                for (i = 0; to->str[i] != 0; ++i) {
                    char utmp[16];
                    snprintf(utmp, sizeof utmp, "U%08X", to->str[i]);
                    if (!charmap_find_value(cm, utmp, 9))
                        break;
                }
                if (to->str[i] == 0)
                    return to->str;
            }
            break;
        }
    }
    for (struct translit_ignore *ti = c->translit_ignore; ti; ti = ti->next)
        if (ti->from <= wch && ti->to >= wch)
            for (uint32_t w = ti->from; w <= wch; w += ti->step)
                if (w == wch)
                    return no_str;
    return NULL;
}

uint32_t *find_translit(struct localedef *locale, const struct charmap *cm, uint32_t wch)
{
    struct ctype *c = locale->categories[C_CTYPE];
    uint32_t *r = NULL;
    if (!c)
        return NULL;
    if (c->translit)
        r = find_translit2(c, cm, wch);
    for (struct translit_include *i = c->translit_include; !r && i; i = i->next)
        r = find_translit(find_locale(C_CTYPE, i->copy_locale, i->copy_repertoire, cm), cm, wch);
    return r;
}

static uint32_t *read_widestring(struct lexer *lr, struct token *now, const struct charmap *cm,
                                 const struct repertoire *rep)
{
    uint32_t *w;
    if (now->tok == tok_default_missing)
        return no_str;
    if (now->tok == tok_bsymbol) {
        w = xmalloc(2 * sizeof *w);
        w[0] = repertoire_find_value(rep, now->str.startmb, now->str.lenmb);
        if (w[0] == ILLEGAL_CHAR_VALUE) {
            free(w);
            return NULL;
        }
        w[1] = 0;
    } else if (now->tok == tok_ucs4) {
        w = xmalloc(2 * sizeof *w);
        w[0] = now->ucs4;
        w[1] = 0;
    } else if (now->tok == tok_charcode) {
        struct charseq *seq = charmap_find_symbol(cm, (const char *)now->charcode.bytes, (size_t)now->charcode.nbytes);
        if (!seq)
            return NULL;
        if (seq->ucs4 == UNINITIALIZED_CHAR_VALUE)
            seq->ucs4 = repertoire_find_value(rep, seq->name, strlen(seq->name));
        if (seq->ucs4 == ILLEGAL_CHAR_VALUE)
            return NULL;
        w = xmalloc(2 * sizeof *w);
        w[0] = seq->ucs4;
        w[1] = 0;
    } else if (now->tok == tok_string) {
        w = now->str.startwc;
        if (!w || w[0] == 0)
            return NULL;
    } else {
        if (now->tok != tok_eol && now->tok != tok_eof)
            lr_ignore_rest(lr, 0);
        SYNTAX_ERROR(lr, "%s: syntax error", "LC_CTYPE");
        return (uint32_t *)-1;
    }
    return w;
}

static void read_translit_entry(struct lexer *lr, struct ctype *c, struct token *now, const struct charmap *cm,
                                const struct repertoire *rep)
{
    uint32_t *from = read_widestring(lr, now, cm, rep);
    if (!from || from == (uint32_t *)-1)
        return;
    struct translit *r = xcalloc(1, sizeof *r);
    r->from = from;
    struct translit_to **top = &r->to;
    bool first = true, ignore = false;
    uint32_t *buf = NULL;
    size_t blen = 0;
    for (;;) {
        now = lr_token(lr, cm, NULL, rep, verbose);
        if (!first && (now->tok == tok_semicolon || now->tok == tok_eol)) {
            if (!ignore) {
                buf = xrealloc(buf, (blen + 1) * sizeof *buf);
                buf[blen] = 0;
                *top = xcalloc(1, sizeof **top);
                (*top)->str = buf;
            }
            buf = NULL;
            blen = 0;
            if (now->tok == tok_eol) {
                r->next = c->translit;
                c->translit = r;
                return;
            }
            if (!ignore)
                top = &(*top)->next;
            ignore = false;
        } else {
            uint32_t *to = read_widestring(lr, now, cm, rep);
            if (to == (uint32_t *)-1) {
                free(buf);
                return;
            }
            if (!to)
                ignore = true;
            else {
                size_t l = wcslen32(to);
                buf = xrealloc(buf, (blen + l + 1) * sizeof *buf);
                memcpy(buf + blen, to, l * sizeof *buf);
                blen += l;
            }
            first = false;
        }
    }
}

static void read_translit_ignore_entry(struct lexer *lr, struct ctype *c, const struct charmap *cm,
                                       const struct repertoire *rep)
{
    for (;;) {
        struct token *now = lr_token(lr, cm, NULL, rep, verbose);
        struct translit_ignore *n;
        uint32_t from;
        if (now->tok == tok_eol || now->tok == tok_eof) {
            lr_error(lr, "premature end of `translit_ignore' definition");
            return;
        }
        if (now->tok != tok_bsymbol && now->tok != tok_ucs4) {
            lr_error(lr, "syntax error");
            lr_ignore_rest(lr, 0);
            return;
        }
        from = now->tok == tok_ucs4 ? now->ucs4 : repertoire_find_value(rep, now->str.startmb, now->str.lenmb);
        if (from == ILLEGAL_CHAR_VALUE) {
            lr_error(lr, "invalid character name");
            n = NULL;
        } else {
            n = xcalloc(1, sizeof *n);
            n->from = n->to = from;
            n->step = 1;
            n->next = c->translit_ignore;
            c->translit_ignore = n;
        }
        now = lr_token(lr, cm, NULL, rep, verbose);
        if (now->tok == tok_ellipsis2 || now->tok == tok_ellipsis2_2) {
            int step = now->tok == tok_ellipsis2_2 ? 2 : 1;
            now = lr_token(lr, cm, NULL, rep, verbose);
            if (now->tok == tok_eol || now->tok == tok_eof) {
                lr_error(lr, "premature end of `translit_ignore' definition");
                return;
            }
            if (now->tok != tok_bsymbol && now->tok != tok_ucs4) {
                lr_error(lr, "syntax error");
                lr_ignore_rest(lr, 0);
                return;
            }
            uint32_t to = now->tok == tok_ucs4 ? now->ucs4 : repertoire_find_value(rep, now->str.startmb, now->str.lenmb);
            if (to == ILLEGAL_CHAR_VALUE)
                lr_error(lr, "invalid character name");
            else if (to >= from) {
                if (n) {
                    n->to = to;
                    n->step = (uint32_t)step;
                }
            } else
                lr_error(lr, "to-value <U%0*X> of range is smaller than from-value <U%0*X>",
                         (to | from) < 65536 ? 4 : 8, to, (to | from) < 65536 ? 4 : 8, from);
            now = lr_token(lr, cm, NULL, rep, verbose);
        }
        if (now->tok == tok_eol || now->tok == tok_eof)
            return;
        if (now->tok == tok_semicolon)
            continue;
        lr_error(lr, "syntax error");
        lr_ignore_rest(lr, 0);
        return;
    }
}

/* ---- reading ---- */

static void skip_category(struct lexer *lr, const struct charmap *cm)
{
    struct token *now;
    do
        now = lr_token(lr, cm, NULL, NULL, verbose);
    while (now->tok != tok_eof && now->tok != tok_end);
    if (now->tok == tok_eof || (now = lr_token(lr, cm, NULL, NULL, verbose), now->tok == tok_eof))
        lr_error(lr, "%s: premature end of file", "LC_CTYPE");
    else if (now->tok != tok_lc_ctype) {
        lr_error(lr, "%1$s: definition does not end with `END %1$s'", "LC_CTYPE");
        lr_ignore_rest(lr, 0);
    } else
        lr_ignore_rest(lr, 1);
}

void ctype_read(struct lexer *lr, struct localedef *result, const struct charmap *cm, const char *repertoire_name,
                int ignore)
{
    const struct repertoire *rep = NULL;
    struct token *now;
    enum tok nowtok;
    uint32_t last_wch = 0;
    enum tok last_token = tok_none, ellipsis_token = tok_none;
    int step = 1;
    unsigned char last_charcode[16] = { 0 };
    int last_charcode_len = 0;
    char *last_str = NULL;
    size_t mapidx = 0;
    struct localedef *copy_locale = NULL;
    if (repertoire_name)
        rep = repertoire_read(repertoire_name);
    lr_ignore_rest(lr, 1);
    do
        now = lr_token(lr, cm, NULL, NULL, verbose);
    while (now->tok == tok_eol);
    nowtok = now->tok;
    if (nowtok == tok_copy) {
        now = lr_token(lr, cm, NULL, NULL, verbose);
        if (now->tok != tok_string) {
            SYNTAX_ERROR(lr, "%s: syntax error", "LC_CTYPE");
            skip_category(lr, cm);
            return;
        }
        if (!ignore) {
            copy_locale = load_locale(C_CTYPE, now->str.startmb, repertoire_name, cm, NULL);
            if (!(copy_locale->avail & (1 << C_CTYPE)) && locfile_read(copy_locale, cm) != 0) {
                skip_category(lr, cm);
                return;
            }
            if (!copy_locale->categories[C_CTYPE])
                return;
        }
        lr_ignore_rest(lr, 1);
        now = lr_token(lr, cm, NULL, NULL, verbose);
        nowtok = now->tok;
    }
    startup(lr, result, cm, copy_locale, ignore);
    struct ctype *c = result->categories[C_CTYPE];
    if (!ignore)
        c->repertoire = rep;
    lr->translate_strings = 1;
    lr->return_widestr = 0;
    for (;;) {
        uint32_t class_bit = 0, class256_bit = 0;
        int handle_digits = 0;
        if (nowtok == tok_eof)
            break;
        if (nowtok == tok_eol) {
            now = lr_token(lr, cm, NULL, NULL, verbose);
            nowtok = now->tok;
            continue;
        }
        switch (nowtok) {
        case tok_charclass:
            now = lr_token(lr, cm, NULL, NULL, verbose);
            while (now->tok == tok_ident || now->tok == tok_string) {
                if (!ignore)
                    class_new(lr, c, xstrdup(now->str.startmb));
                now = lr_token(lr, cm, NULL, NULL, verbose);
                if (now->tok != tok_semicolon)
                    break;
                now = lr_token(lr, cm, NULL, NULL, verbose);
            }
            if (now->tok != tok_eol)
                SYNTAX_ERROR(lr, "%s: syntax error in definition of new character class", "LC_CTYPE");
            break;
        case tok_charconv:
            now = lr_token(lr, cm, NULL, NULL, verbose);
            while (now->tok == tok_ident || now->tok == tok_string) {
                if (!ignore)
                    map_new(lr, c, xstrdup(now->str.startmb), cm);
                now = lr_token(lr, cm, NULL, NULL, verbose);
                if (now->tok != tok_semicolon)
                    break;
                now = lr_token(lr, cm, NULL, NULL, verbose);
            }
            if (now->tok != tok_eol)
                SYNTAX_ERROR(lr, "%s: syntax error in definition of new character map", "LC_CTYPE");
            break;
        case tok_class: {
            if (ignore) {
                lr_ignore_rest(lr, 0);
                break;
            }
            now = lr_token(lr, cm, NULL, NULL, verbose);
            if (now->tok == tok_ident || now->tok == tok_string) {
                size_t cnt;
                for (cnt = 0; cnt < c->nr_charclass; ++cnt)
                    if (strcmp(c->classnames[cnt], now->str.startmb) == 0)
                        break;
                if (cnt >= c->nr_charclass) {
                    class_new(lr, c, xstrdup(now->str.startmb));
                    class_bit = ISwbit((int)c->nr_charclass - 1);
                } else
                    class_bit = ISwbit((int)cnt);
            } else if (now->tok == tok_digit)
                goto handle_tok_digit;
            else if (now->tok < tok_upper || now->tok > tok_blank)
                goto err;
            else {
                class_bit = BITw(now->tok);
                class256_bit = BIT(now->tok);
            }
            now = lr_token(lr, cm, NULL, NULL, verbose);
            if (now->tok != tok_semicolon)
                goto err;
            goto read_charclass;
        }
        case tok_upper: case tok_lower: case tok_alpha: case tok_alnum: case tok_space:
        case tok_cntrl: case tok_punct: case tok_graph: case tok_print: case tok_xdigit: case tok_blank:
            if (ignore) {
                lr_ignore_rest(lr, 0);
                break;
            }
            class_bit = BITw(nowtok);
            class256_bit = BIT(nowtok);
            handle_digits = 0;
        read_charclass:
            c->class_done |= class_bit;
            last_token = tok_none;
            ellipsis_token = tok_none;
            step = 1;
            now = lr_token(lr, cm, NULL, NULL, verbose);
            while (now->tok != tok_eol && now->tok != tok_eof) {
                uint32_t wch;
                struct charseq *seq;
                if (ellipsis_token == tok_none) {
                    if (get_character(now, cm, rep, &seq, &wch))
                        goto err;
                    if (!ignore && seq && seq->nbytes == 1)
                        c->class256_collection[seq->bytes[0]] |= class256_bit;
                    if (!ignore && wch != ILLEGAL_CHAR_VALUE && class_bit)
                        ELEM_CLASS(c, wch) |= class_bit;
                    last_token = now->tok;
                    free(last_str);
                    last_str = last_token == tok_bsymbol ? xstrndup(now->str.startmb, now->str.lenmb) : NULL;
                    last_wch = wch;
                    memcpy(last_charcode, now->charcode.bytes, 16);
                    last_charcode_len = now->charcode.nbytes;
                    if (!ignore && handle_digits == 1)
                        add_digit(c, seq, wch);
                    else if (!ignore && handle_digits == 2 && !add_outdigit(lr, c, seq, wch)) {
                        lr_ignore_rest(lr, 0);
                        break;
                    }
                } else {
                    if (last_token != now->tok) {
                        lr_error(lr, "ellipsis range must be marked by two operands of same type");
                        lr_ignore_rest(lr, 0);
                        break;
                    }
                    if (last_token == tok_bsymbol) {
                        if (ellipsis_token == tok_ellipsis3)
                            lr_error(lr, "with symbolic name range values the absolute ellipsis `...' must not be used");
                        symbolic_ellipsis(lr, c, cm, rep, now, last_str, class256_bit, class_bit,
                                          ellipsis_token == tok_ellipsis4 ? 10 : 16, ignore, handle_digits, step);
                    } else if (last_token == tok_ucs4) {
                        if (ellipsis_token != tok_ellipsis2)
                            lr_error(lr, "with UCS range values one must use the hexadecimal symbolic ellipsis `..'");
                        ucs4_ellipsis(lr, c, cm, rep, now, last_wch, class256_bit, class_bit, ignore, handle_digits, step);
                    } else {
                        if (ellipsis_token != tok_ellipsis3)
                            lr_error(lr, "with character code range values one must use the absolute ellipsis `...'");
                        charcode_ellipsis(lr, c, cm, rep, now, last_charcode, last_charcode_len, class256_bit,
                                          class_bit, ignore, handle_digits);
                    }
                    last_token = tok_none;
                }
                now = lr_token(lr, cm, NULL, NULL, verbose);
                if (now->tok == tok_eol || now->tok == tok_eof)
                    break;
                if (last_token != tok_none && now->tok >= tok_ellipsis2 && now->tok <= tok_ellipsis4_2) {
                    if (now->tok == tok_ellipsis2_2) {
                        now->tok = tok_ellipsis2;
                        step = 2;
                    } else if (now->tok == tok_ellipsis4_2) {
                        now->tok = tok_ellipsis4;
                        step = 2;
                    }
                    ellipsis_token = now->tok;
                    now = lr_token(lr, cm, NULL, NULL, verbose);
                    continue;
                }
                if (now->tok != tok_semicolon)
                    goto err;
                now = lr_token(lr, cm, NULL, NULL, verbose);
                ellipsis_token = tok_none;
                step = 1;
            }
            break;
        case tok_digit:
            if (ignore) {
                lr_ignore_rest(lr, 0);
                break;
            }
        handle_tok_digit:
            class_bit = ISwbit(3);
            class256_bit = ISbit(3);
            handle_digits = 1;
            goto read_charclass;
        case tok_outdigit:
            if (ignore) {
                lr_ignore_rest(lr, 0);
                break;
            }
            if (c->outdigits_act != 0)
                lr_error(lr, "%s: field `%s' declared more than once", "LC_CTYPE", "outdigit");
            class_bit = class256_bit = 0;
            handle_digits = 2;
            goto read_charclass;
        case tok_toupper:
            if (ignore) {
                lr_ignore_rest(lr, 0);
                break;
            }
            mapidx = 0;
            goto read_mapping;
        case tok_tolower:
            if (ignore) {
                lr_ignore_rest(lr, 0);
                break;
            }
            mapidx = 1;
            goto read_mapping;
        case tok_map:
            if (ignore) {
                lr_ignore_rest(lr, 0);
                break;
            }
            now = lr_token(lr, cm, NULL, NULL, verbose);
            if (now->tok == tok_ident || now->tok == tok_string) {
                size_t cnt;
                for (cnt = 2; cnt < c->map_collection_nr; ++cnt)
                    if (strcmp(now->str.startmb, c->mapnames[cnt]) == 0)
                        break;
                if (cnt >= c->map_collection_nr)
                    map_new(lr, c, xstrdup(now->str.startmb), cm);
                mapidx = cnt;
            } else if (now->tok < tok_toupper || now->tok > tok_tolower)
                goto err;
            else
                mapidx = (size_t)(now->tok - tok_toupper);
            now = lr_token(lr, cm, NULL, NULL, verbose);
            if (now->tok != tok_semicolon)
                goto err;
        read_mapping:
            if (c->tomap_done[mapidx]) {
                lr_error(lr, "duplicated definition for mapping `%s'", c->mapnames[mapidx]);
                lr_ignore_rest(lr, 0);
                break;
            }
            c->tomap_done[mapidx] = 1;
            now = lr_token(lr, cm, NULL, NULL, verbose);
            while (now->tok != tok_eol && now->tok != tok_eof) {
                struct charseq *from_seq, *to_seq;
                uint32_t from_wch, to_wch;
                if (now->tok != tok_open_brace)
                    goto err;
                now = lr_token(lr, cm, NULL, NULL, verbose);
                if (get_character(now, cm, rep, &from_seq, &from_wch) != 0)
                    goto err;
                now = lr_token(lr, cm, NULL, NULL, verbose);
                if (now->tok != tok_comma)
                    goto err;
                now = lr_token(lr, cm, NULL, NULL, verbose);
                if (get_character(now, cm, rep, &to_seq, &to_wch) != 0)
                    goto err;
                now = lr_token(lr, cm, NULL, NULL, verbose);
                if (now->tok != tok_close_brace)
                    goto err;
                if (!ignore) {
                    if (from_seq && from_seq->nbytes == 1 && from_seq->bytes[0] < 0x80 && to_seq &&
                        (to_seq->nbytes != 1 || to_seq->bytes[0] >= 0x80))
                        c->to_nonascii = 1;
                    if (mapidx < 2 && from_seq && to_seq && from_seq->nbytes == 1 && to_seq->nbytes == 1)
                        c->map256_collection[mapidx][from_seq->bytes[0]] = to_seq->bytes[0];
                    if (from_wch != ILLEGAL_CHAR_VALUE && to_wch != ILLEGAL_CHAR_VALUE)
                        ELEM_MAP(c, mapidx, from_wch) = to_wch;
                }
                now = lr_token(lr, cm, NULL, NULL, verbose);
                if (now->tok == tok_semicolon)
                    now = lr_token(lr, cm, NULL, NULL, verbose);
            }
            break;
        case tok_translit_start:
            if (ignore) {
                do {
                    lr_ignore_rest(lr, 0);
                    now = lr_token(lr, cm, NULL, NULL, verbose);
                } while (now->tok != tok_translit_end && now->tok != tok_eof);
                if (now->tok == tok_eof)
                    lr_error(lr, "%s: `translit_start' section does not end with `translit_end'", "LC_CTYPE");
                break;
            }
            lr_ignore_rest(lr, 1);
            lr->translate_strings = 1;
            lr->return_widestr = 1;
            while (now = lr_token(lr, cm, NULL, rep, verbose), now->tok != tok_translit_end && now->tok != tok_eof) {
                if (now->tok == tok_eol)
                    continue;
                if (now->tok == tok_include) {
                    now = lr_token(lr, cm, NULL, NULL, verbose);
                    if (now->tok != tok_string && now->tok != tok_ident) {
                    translit_syntax:
                        lr_error(lr, "%s: syntax error", "LC_CTYPE");
                        lr_ignore_rest(lr, 0);
                        continue;
                    }
                    const char *locale_name = xstrdup(now->str.startmb);
                    now = lr_token(lr, cm, NULL, NULL, verbose);
                    if (now->tok != tok_semicolon)
                        goto translit_syntax;
                    now = lr_token(lr, cm, NULL, NULL, verbose);
                    if ((now->tok != tok_string && now->tok != tok_ident) || !now->str.startmb)
                        goto translit_syntax;
                    const char *rname = now->str.startmb[0] ? xstrdup(now->str.startmb) : NULL;
                    struct translit_include *inc = xcalloc(1, sizeof *inc), **p = &c->translit_include;
                    inc->copy_locale = locale_name;
                    inc->copy_repertoire = rname;
                    while (*p)
                        p = &(*p)->next;
                    *p = inc;
                    lr_ignore_rest(lr, 1);
                    add_to_readlist(C_CTYPE, locale_name, rname, 1, NULL);
                    continue;
                } else if (now->tok == tok_default_missing) {
                    for (;;) {
                        now = lr_token(lr, cm, NULL, NULL, verbose);
                        uint32_t *w = read_widestring(lr, now, cm, rep);
                        if (w && w != (uint32_t *)-1) {
                            if (c->default_missing)
                                lr_error(lr, "%s: duplicate `default_missing' definition", "LC_CTYPE");
                            else
                                c->default_missing = w;
                            lr_ignore_rest(lr, 0);
                            break;
                        } else if (w == (uint32_t *)-1)
                            break;
                        now = lr_token(lr, cm, NULL, NULL, verbose);
                        if (now->tok == tok_eol || now->tok == tok_eof) {
                            lr_error(lr, "%s: no representable `default_missing' definition found", "LC_CTYPE");
                            break;
                        }
                        if (now->tok != tok_semicolon)
                            goto translit_syntax;
                    }
                    continue;
                } else if (now->tok == tok_translit_ignore) {
                    read_translit_ignore_entry(lr, c, cm, rep);
                    continue;
                }
                read_translit_entry(lr, c, now, cm, rep);
            }
            lr->return_widestr = 0;
            if (now->tok == tok_eof)
                lr_error(lr, "%s: `translit_start' section does not end with `translit_end'", "LC_CTYPE");
            break;
        case tok_ident: {
            if (ignore) {
                lr_ignore_rest(lr, 0);
                break;
            }
            size_t cnt;
            for (cnt = 0; cnt < c->nr_charclass; ++cnt)
                if (strcmp(now->str.startmb, c->classnames[cnt]) == 0)
                    break;
            if (cnt < c->nr_charclass) {
                class_bit = ISwbit((int)cnt);
                class256_bit = cnt <= 11 ? ISbit((int)cnt) : 0;
                goto read_charclass;
            }
            for (cnt = 0; cnt < c->map_collection_nr; ++cnt)
                if (strcmp(now->str.startmb, c->mapnames[cnt]) == 0)
                    break;
            if (cnt < c->map_collection_nr) {
                mapidx = cnt;
                goto read_mapping;
            }
            break;
        }
        case tok_end:
            now = lr_token(lr, cm, NULL, NULL, verbose);
            if (now->tok == tok_eof)
                break;
            if (now->tok == tok_eol)
                lr_error(lr, "%s: incomplete `END' line", "LC_CTYPE");
            else if (now->tok != tok_lc_ctype)
                lr_error(lr, "%1$s: definition does not end with `END %1$s'", "LC_CTYPE");
            lr_ignore_rest(lr, now->tok == tok_lc_ctype);
            free(last_str);
            return;
        default:
        err:
            if (now->tok != tok_eof)
                SYNTAX_ERROR(lr, "%s: syntax error", "LC_CTYPE");
        }
        now = lr_token(lr, cm, NULL, NULL, verbose);
        nowtok = now->tok;
    }
    free(last_str);
    lr_error(lr, "%s: premature end of file", "LC_CTYPE");
}

/* ---- the defaults and checks ---- */

static void set_one_default(struct ctype *c, const struct charmap *cm, int bitpos, int from, int to)
{
    uint32_t bit = ISbit(bitpos), bitw = ISwbit(bitpos);
    for (int ch = from; ch <= to; ++ch) {
        char tmp[2] = { (char)ch, 0 };
        struct charseq *seq = charmap_find_value(cm, tmp, 1);
        if (!seq) {
            char buf[16];
            snprintf(buf, sizeof buf, "U%08X", ch);
            seq = charmap_find_value(cm, buf, 9);
        }
        if (!seq)
            record_error(0, "%s: character `%s' not defined while needed as default value", "LC_CTYPE", tmp);
        else if (seq->nbytes != 1)
            record_error(0, "%s: character `%s' in charmap not representable with one byte", "LC_CTYPE", tmp);
        else
            c->class256_collection[seq->bytes[0]] |= bit;
        ELEM_CLASS(c, (uint32_t)ch) |= bitw;
    }
}

/* a character the defaults name: by one of its names */
static void default_char(struct ctype *c, const struct charmap *cm, const char *const *names, const char *what,
                         uint32_t wc, uint32_t bit, uint32_t bitw)
{
    struct charseq *seq = NULL;
    for (; *names && !seq; names++)
        seq = charmap_find_value(cm, *names, strlen(*names));
    if (!seq)
        record_error(0, "%s: character `%s' not defined while needed as default value", "LC_CTYPE", what);
    else if (seq->nbytes != 1)
        record_error(0, "%s: character `%s' in charmap not representable with one byte", "LC_CTYPE", what);
    else
        c->class256_collection[seq->bytes[0]] |= bit;
    ELEM_CLASS(c, wc) |= bitw;
}

static const char *const n_space[] = { "space", "SP", "U00000020", NULL };
static const char *const n_ff[] = { "form-feed", "U0000000C", NULL };
static const char *const n_nl[] = { "newline", "U0000000A", NULL };
static const char *const n_cr[] = { "carriage-return", "U0000000D", NULL };
static const char *const n_tab[] = { "tab", "U00000009", NULL };
static const char *const n_vt[] = { "vertical-tab", "U0000000B", NULL };

static void set_class_defaults(struct ctype *c, const struct charmap *cm)
{
    if (!(c->class_done & BITw(tok_upper)))
        set_one_default(c, cm, BITPOS(tok_upper), 'A', 'Z');
    if (!(c->class_done & BITw(tok_lower)))
        set_one_default(c, cm, BITPOS(tok_lower), 'a', 'z');
    if (!(c->class_done & BITw(tok_alpha))) {
        uint32_t mask = BIT(tok_upper) | BIT(tok_lower), maskw = BITw(tok_upper) | BITw(tok_lower);
        for (size_t i = 0; i < 256; ++i)
            if (c->class256_collection[i] & mask)
                c->class256_collection[i] |= BIT(tok_alpha);
        for (size_t i = 0; i < c->class_collection_act; ++i)
            if (c->class_collection[i] & maskw)
                c->class_collection[i] |= BITw(tok_alpha);
    }
    if (!(c->class_done & BITw(tok_digit)))
        set_one_default(c, cm, BITPOS(tok_digit), '0', '9');
    {
        uint32_t mask = BIT(tok_alpha) | BIT(tok_digit), maskw = BITw(tok_alpha) | BITw(tok_digit);
        for (size_t i = 0; i < 256; ++i)
            if (c->class256_collection[i] & mask)
                c->class256_collection[i] |= BIT(tok_alnum);
        for (size_t i = 0; i < c->class_collection_act; ++i)
            if (c->class_collection[i] & maskw)
                c->class_collection[i] |= BITw(tok_alnum);
    }
    if (!(c->class_done & BITw(tok_space))) {
        uint32_t b = BIT(tok_space), bw = BITw(tok_space);
        default_char(c, cm, n_space, "<space>", ' ', b, bw);
        default_char(c, cm, n_ff, "<form-feed>", '\f', b, bw);
        default_char(c, cm, n_nl, "<newline>", '\n', b, bw);
        default_char(c, cm, n_cr, "<carriage-return>", '\r', b, bw);
        default_char(c, cm, n_tab, "<tab>", '\t', b, bw);
        default_char(c, cm, n_vt, "<vertical-tab>", '\v', b, bw);
    }
    if (!(c->class_done & BITw(tok_xdigit))) {
        set_one_default(c, cm, BITPOS(tok_xdigit), '0', '9');
        set_one_default(c, cm, BITPOS(tok_xdigit), 'A', 'F');
        set_one_default(c, cm, BITPOS(tok_xdigit), 'a', 'f');
    }
    if (!(c->class_done & BITw(tok_blank))) {
        default_char(c, cm, n_space, "<space>", ' ', BIT(tok_blank), BITw(tok_blank));
        default_char(c, cm, n_tab, "<tab>", '\t', BIT(tok_blank), BITw(tok_blank));
    }
    uint32_t gmask = BIT(tok_upper) | BIT(tok_lower) | BIT(tok_alpha) | BIT(tok_digit) | BIT(tok_xdigit) | BIT(tok_punct);
    uint32_t gmaskw = BITw(tok_upper) | BITw(tok_lower) | BITw(tok_alpha) | BITw(tok_digit) | BITw(tok_xdigit) | BITw(tok_punct);
    if (!(c->class_done & BITw(tok_graph))) {
        for (size_t i = 0; i < c->class_collection_act; ++i)
            if (c->class_collection[i] & gmaskw)
                c->class_collection[i] |= BITw(tok_graph);
        for (size_t i = 0; i < 256; ++i)
            if (c->class256_collection[i] & gmask)
                c->class256_collection[i] |= BIT(tok_graph);
    }
    if (!(c->class_done & BITw(tok_print))) {
        for (size_t i = 0; i < c->class_collection_act; ++i)
            if (c->class_collection[i] & gmaskw)
                c->class_collection[i] |= BITw(tok_print);
        for (size_t i = 0; i < 256; ++i)
            if (c->class256_collection[i] & gmask)
                c->class256_collection[i] |= BIT(tok_print);
        default_char(c, cm, n_space, "<space>", ' ', BIT(tok_print), BITw(tok_print));
    }
    if (!c->tomap_done[0]) {
        for (int ch = 'a'; ch <= 'z'; ++ch) {
            char tmp[4] = { '<', (char)ch, '>', 0 };
            struct charseq *sf = charmap_find_value(cm, &tmp[1], 1), *st;
            if (!sf) {
                char buf[16];
                snprintf(buf, sizeof buf, "U%08X", ch);
                sf = charmap_find_value(cm, buf, 9);
            }
            if (!sf)
                record_error(0, "%s: character `%s' not defined while needed as default value", "LC_CTYPE", tmp);
            else if (sf->nbytes != 1)
                record_error(0, "%s: character `%s' needed as default value not representable with one byte",
                             "LC_CTYPE", tmp);
            else {
                tmp[1] = (char)(ch + ('A' - 'a'));
                st = charmap_find_value(cm, &tmp[1], 1);
                if (!st) {
                    char buf[16];
                    snprintf(buf, sizeof buf, "U%08X", ch + ('A' - 'a'));
                    st = charmap_find_value(cm, buf, 9);
                }
                if (!st)
                    record_error(0, "%s: character `%s' not defined while needed as default value", "LC_CTYPE", tmp);
                else if (st->nbytes != 1)
                    record_error(0, "%s: character `%s' needed as default value not representable with one byte",
                                 "LC_CTYPE", tmp);
                else
                    c->map256_collection[0][sf->bytes[0]] = st->bytes[0];
            }
            ELEM_MAP(c, 0, (uint32_t)ch) = (uint32_t)(ch + ('A' - 'a'));
        }
    }
    if (!c->tomap_done[1]) {
        for (size_t i = 0; i < c->map_collection_act[0]; ++i)
            if (c->map_collection[0][i] != 0)
                ELEM_MAP(c, 1, c->map_collection[0][i]) = c->charnames[i];
        for (size_t i = 0; i < 256; ++i)
            if (c->map256_collection[0][i] != 0)
                c->map256_collection[1][c->map256_collection[0][i]] = (uint32_t)i;
    }
    if (c->outdigits_act != 10) {
        if (c->outdigits_act != 0)
            record_error(0, "%s: field `%s' does not contain exactly ten entries", "LC_CTYPE", "outdigit");
        for (size_t i = c->outdigits_act; i < 10; ++i) {
            c->mboutdigits[i] = charmap_find_symbol(cm, digits + i, 1);
            if (!c->mboutdigits[i])
                c->mboutdigits[i] = charmap_find_symbol(cm, longnames[i], strlen(longnames[i]));
            if (!c->mboutdigits[i])
                c->mboutdigits[i] = charmap_find_symbol(cm, uninames[i], 9);
            if (!c->mboutdigits[i]) {
                record_error(0, "no output digits defined and none of the standard names in the charmap");
                struct charseq *s = xcalloc(1, sizeof *s + 1);
                s->bytes[0] = (unsigned char)digits[i];
                s->nbytes = 1;
                c->mboutdigits[i] = s;
            }
            c->wcoutdigits[i] = '0' + (uint32_t)i;
        }
        c->outdigits_act = 10;
    }
}

/* walk the byte sequences of a WIDTH rule */
static bool next_bytes(unsigned char *bytes, int *nbytes, int maxbytes)
{
    int inner = *nbytes - 1;
    while (inner >= 0 && bytes[inner] == 0xff)
        --inner;
    if (inner < 0) {
        if (*nbytes >= maxbytes)
            return false;
        bytes[0] = 1;
        memset(&bytes[1], 0, (size_t)*nbytes);
        ++*nbytes;
    } else {
        ++bytes[inner];
        while (++inner < *nbytes)
            bytes[inner] = 0;
    }
    return true;
}

void ctype_finish(struct localedef *locale, const struct charmap *cm)
{
    static const struct {
        const char *name;
        const char allow[13];
    } valid_table[12] = {
        { "upper", "--MX-XDDXXX-" }, { "lower", "--MX-XDDXXX-" }, { "alpha", "---X-XDDXXX-" },
        { "digit", "XXX--XDDXXX-" }, { "xdigit", "-----XDDXXX-" }, { "space", "XXXXX------X" },
        { "print", "---------X--" }, { "graph", "---------X--" }, { "blank", "XXXXXM-----X" },
        { "cntrl", "XXXXX-XX--XX" }, { "punct", "XXXXX-DD-X-X" }, { "alnum", "-----XDDXXX-" },
    };
    struct ctype *c = locale->categories[C_CTYPE];
    if (!c) {
        if (locale->copy_name[C_CTYPE]) {
            struct localedef *from = locale;
            do
                from = find_locale(C_CTYPE, from->copy_name[C_CTYPE], from->repertoire_name, cm);
            while (!from->categories[C_CTYPE] && from->copy_name[C_CTYPE]);
            c = locale->categories[C_CTYPE] = from->categories[C_CTYPE];
        }
        if (!c) {
            record_warning("No definition for %s category found", "LC_CTYPE");
            startup(NULL, locale, cm, NULL, 0);
            c = locale->categories[C_CTYPE];
        }
        const char *rn = locale->repertoire_name ? locale->repertoire_name : repertoire_global;
        if (rn)
            c->repertoire = repertoire_read(rn);
    }
    c->codeset_name = cm->code_set_name;
    if (!c->codeset_name) {
        record_error(0, "No character set name specified in charmap");
        c->codeset_name = "//UNKNOWN//";
    }
    set_class_defaults(c, cm);
    for (size_t cnt = 0; cnt < c->class_collection_act; ++cnt) {
        uint32_t tmp = c->class_collection[cnt];
        if (!tmp)
            continue;
        for (int c1 = 0; c1 < 12; ++c1)
            if (tmp & ISwbit(c1))
                for (int c2 = 0; c2 < 12; ++c2) {
                    char a = valid_table[c1].allow[c2];
                    if (a == '-')
                        continue;
                    bool eq = (tmp & ISwbit(c2)) != 0;
                    uint32_t v = c->charnames[cnt];
                    if (a == 'M' && !eq)
                        record_error(0, "character L'\\u%0*x' in class `%s' must be in class `%s'", v > 0xffff ? 8 : 4, v,
                                     valid_table[c1].name, valid_table[c2].name);
                    else if (a == 'X' && eq)
                        record_error(0, "character L'\\u%0*x' in class `%s' must not be in class `%s'", v > 0xffff ? 8 : 4,
                                     v, valid_table[c1].name, valid_table[c2].name);
                    else if (a == 'D')
                        c->class_collection[cnt] |= ISwbit(c2);
                }
    }
    for (size_t cnt = 0; cnt < 256; ++cnt) {
        uint32_t tmp = c->class256_collection[cnt];
        if (!tmp)
            continue;
        for (int c1 = 0; c1 < 12; ++c1)
            if (tmp & ISbit(c1))
                for (int c2 = 0; c2 < 12; ++c2) {
                    char a = valid_table[c1].allow[c2];
                    if (a == '-')
                        continue;
                    bool eq = (tmp & ISbit(c2)) != 0;
                    if (a == 'M' && !eq)
                        record_error(0, "character '\\%zo' in class `%s' must be in class `%s'", cnt, valid_table[c1].name,
                                     valid_table[c2].name);
                    else if (a == 'X' && eq)
                        record_error(0, "character '\\%zo' in class `%s' must not be in class `%s'", cnt,
                                     valid_table[c1].name, valid_table[c2].name);
                    else if (a == 'D')
                        c->class256_collection[cnt] |= ISbit(c2);
                }
    }
    /* <SP> */
    uint32_t sp = ELEM_CLASS(c, 32);
    if (!(sp & BITw(tok_space)))
        record_error(0, "<SP> character not in class `%s'", "space");
    else if (!(sp & BITw(tok_blank)))
        record_error(0, "<SP> character not in class `%s'", "blank");
    else if (sp & BITw(tok_punct))
        record_error(0, "<SP> character must not be in class `%s'", "punct");
    else if (sp & BITw(tok_graph))
        record_error(0, "<SP> character must not be in class `%s'", "graph");
    else
        ELEM_CLASS(c, 32) |= BITw(tok_print);
    struct charseq *space_seq = charmap_find_value(cm, "SP", 2);
    if (!space_seq)
        space_seq = charmap_find_value(cm, "space", 5);
    if (!space_seq)
        space_seq = charmap_find_value(cm, "U00000020", 9);
    if (!space_seq || space_seq->nbytes != 1)
        record_error(0, "character <SP> not defined in character map");
    else {
        uint32_t b = c->class256_collection[space_seq->bytes[0]];
        if (!(b & BIT(tok_space)))
            record_error(0, "<SP> character not in class `%s'", "space");
        else if (!(b & BIT(tok_blank)))
            record_error(0, "<SP> character not in class `%s'", "blank");
        else if (b & BIT(tok_punct))
            record_error(0, "<SP> character must not be in class `%s'", "punct");
        else if (b & BIT(tok_graph))
            record_error(0, "<SP> character must not be in class `%s'", "graph");
        else
            c->class256_collection[space_seq->bytes[0]] |= BIT(tok_print);
    }
    for (uint32_t ch = 'A'; ch <= 'Z'; ++ch) {
        uint32_t up = c->map256_collection[0][ch], low = c->map256_collection[1][ch];
        if (up != ch || low != ch + 0x20 || c->map256_collection[0][low & 0xff] != ch ||
            c->map256_collection[1][low & 0xff] != ch + 0x20)
            c->nonascii_case = 1;
    }
    for (uint32_t ch = 0; ch < 256; ++ch)
        if ((ch < 'A' || (ch > 'Z' && ch < 'a') || ch > 'z') &&
            (c->map256_collection[0][ch] != ch || c->map256_collection[1][ch] != ch))
            c->nonascii_case = 1;
    /* the characters of the WIDTH rules and of the charmap get names */
    for (size_t r = 0; r < cm->nwidth_rules; ++r) {
        const struct width_rule *w = &cm->width_rules[r];
        unsigned char bytes[16];
        int nbytes = w->from->nbytes;
        memcpy(bytes, w->from->bytes, (size_t)nbytes);
        while (nbytes < w->to->nbytes || memcmp(bytes, w->to->bytes, (size_t)nbytes) <= 0) {
            struct charseq *seq = charmap_find_symbol(cm, (const char *)bytes, (size_t)nbytes);
            uint32_t wch = !seq ? ILLEGAL_CHAR_VALUE
                           : seq->ucs4 != UNINITIALIZED_CHAR_VALUE ? seq->ucs4
                                                                   : repertoire_find_value(c->repertoire, seq->name, strlen(seq->name));
            if (wch != ILLEGAL_CHAR_VALUE)
                (void)find_idx(c, NULL, NULL, NULL, wch);
            if (!next_bytes(bytes, &nbytes, w->to->nbytes))
                break;
        }
    }
    void *curs = NULL;
    const void *key;
    size_t len;
    void *vdata;
    while (ht_iterate(&cm->char_table, &curs, &key, &len, &vdata) == 0) {
        struct charseq *d = vdata;
        if (d->ucs4 == UNINITIALIZED_CHAR_VALUE)
            d->ucs4 = repertoire_find_value(c->repertoire, d->name, len);
        if (d->ucs4 != ILLEGAL_CHAR_VALUE)
            (void)find_idx(c, NULL, NULL, NULL, d->ucs4);
    }
    if (c->mbdigits_act % 10 != 0) {
        c->wcdigits_act -= c->mbdigits_act % 10;
        c->mbdigits_act -= c->mbdigits_act % 10;
        record_error(0, "`digit' category has not entries in groups of ten");
    }
    size_t cnt = 0;
    while (cnt < c->mbdigits_act) {
        size_t in;
        for (in = 0; in < 10; ++in)
            if (!c->mbdigits[cnt + in])
                break;
        if (in == 10)
            cnt += 10;
        else {
            memmove(&c->mbdigits[cnt], &c->mbdigits[cnt + 10], (c->wcdigits_act - cnt - 10) * sizeof *c->mbdigits);
            c->mbdigits_act -= 10;
        }
    }
    if (c->mbdigits_act == 0) {
        if (c->mbdigits_max == 0) {
            c->mbdigits = xcalloc(10, sizeof *c->mbdigits);
            c->mbdigits_max = 10;
        }
        for (cnt = 0; cnt < 10; ++cnt) {
            c->mbdigits[cnt] = charmap_find_symbol(cm, digits + cnt, 1);
            if (!c->mbdigits[cnt]) {
                c->mbdigits[cnt] = charmap_find_symbol(cm, longnames[cnt], strlen(longnames[cnt]));
                if (!c->mbdigits[cnt]) {
                    record_error(0, "no input digits defined and none of the standard names in the charmap");
                    struct charseq *s = xcalloc(1, sizeof *s + 1);
                    s->bytes[0] = (unsigned char)digits[cnt];
                    s->nbytes = 1;
                    c->mbdigits[cnt] = s;
                }
            }
        }
        c->mbdigits_act = 10;
    }
    cnt = 0;
    while (cnt < c->wcdigits_act) {
        size_t in;
        for (in = 0; in < 10; ++in)
            if (c->wcdigits[cnt + in] == ILLEGAL_CHAR_VALUE)
                break;
        if (in == 10)
            cnt += 10;
        else {
            memmove(&c->wcdigits[cnt], &c->wcdigits[cnt + 10], (c->wcdigits_act - cnt - 10) * sizeof *c->wcdigits);
            c->wcdigits_act -= 10;
        }
    }
    if (c->wcdigits_act == 0) {
        if (c->wcdigits_max == 0) {
            c->wcdigits = xcalloc(10, sizeof *c->wcdigits);
            c->wcdigits_max = 10;
        }
        for (cnt = 0; cnt < 10; ++cnt)
            c->wcdigits[cnt] = '0' + (uint32_t)cnt;
        c->mbdigits_act = 10;
    }
    bool warned = false;
    for (cnt = 0; cnt < 10; ++cnt)
        if (!c->mboutdigits[cnt]) {
            if (!warned) {
                record_error(0, "not all characters used in `outdigit' are available in the charmap");
                warned = true;
            }
            struct charseq *s = xcalloc(1, sizeof *s + 1);
            s->bytes[0] = '?';
            s->nbytes = 1;
            c->mboutdigits[cnt] = s;
        }
    warned = false;
    for (cnt = 0; cnt < 10; ++cnt)
        if (c->wcoutdigits[cnt] == 0) {
            if (!warned) {
                record_error(0, "not all characters used in `outdigit' are available in the repertoire");
                warned = true;
            }
            c->wcoutdigits[cnt] = '?';
        }
    /* translit_ignore: sorted by from (as glibc's insertion does) */
    if (c->translit_ignore) {
        struct translit_ignore *first = c->translit_ignore, *runp;
        c->ntranslit_ignore = 1;
        for (runp = first->next; runp; runp = runp->next) {
            struct translit_ignore *last = NULL, *cmp;
            ++c->ntranslit_ignore;
            for (cmp = first; cmp; last = cmp, cmp = cmp->next)
                if (runp->from < cmp->from)
                    break;
            runp->next = last;
            if (!last)
                first = runp;
        }
        c->translit_ignore = first;
    }
}

/* ---- the output ---- */

static struct translit **translit_flatten(struct ctype *c, const struct charmap *cm, struct translit **cursor)
{
    while (c->translit_include) {
        const char *cl = c->translit_include->copy_locale, *cr = c->translit_include->copy_repertoire;
        c->translit_include = c->translit_include->next;
        struct localedef *other = find_locale(C_CTYPE, cl, cr, cm);
        struct ctype *oc = other ? other->categories[C_CTYPE] : NULL;
        if (!oc) {
            record_error(0, "%s: transliteration data from locale `%s' not available", "LC_CTYPE", cl);
            continue;
        }
        cursor = translit_flatten(oc, cm, cursor);
        if (oc->translit) {
            struct translit *endp = oc->translit;
            while (endp->next)
                endp = endp->next;
            endp->next = *cursor;
            *cursor = oc->translit;
            oc->translit = NULL;
            cursor = &endp->next;
        }
        if (!c->default_missing)
            c->default_missing = oc->default_missing;
    }
    return cursor;
}

static int wcscmp32(const uint32_t *a, const uint32_t *b)
{
    while (*a && *a == *b)
        a++, b++;
    return (int32_t)*a < (int32_t)*b ? -1 : (int32_t)*a > (int32_t)*b;
}

static void allocate_arrays(struct ctype *c, const struct charmap *cm)
{
    size_t idx, nr;
    c->ctype_b = xcalloc(256 + 128, sizeof *c->ctype_b);
    c->ctype32_b = xcalloc(256, sizeof *c->ctype32_b);
    c->class_b = xmalloc(c->nr_charclass * sizeof *c->class_b);
    c->class_3level = xmalloc(c->nr_charclass * sizeof *c->class_3level);
    for (idx = 0; idx < 256; ++idx)
        c->ctype_b[128 + idx] = (uint16_t)c->class256_collection[idx];
    for (idx = 0; idx < 127; ++idx)
        c->ctype_b[idx] = c->ctype_b[256 + idx];
    for (idx = 0; idx < c->class_collection_act; ++idx)
        if (c->charnames[idx] < 0x100)
            c->ctype32_b[c->charnames[idx]] = c->class_collection[idx];
    for (nr = 0; nr < c->nr_charclass; nr++) {
        c->class_b[nr] = xcalloc(256 / 32, sizeof(uint32_t));
        if (nr <= 11)
            for (idx = 0; idx < 256; ++idx)
                if (c->class256_collection[idx] & ISbit((int)nr))
                    c->class_b[nr][idx >> 5] |= (uint32_t)1 << (idx & 0x1f);
    }
    for (nr = 0; nr < c->nr_charclass; nr++) {
        struct table3 *t = &c->class_3level[nr];
        t3_init(t, 4, 7, 4, 0);
        for (idx = 0; idx < c->class_collection_act; ++idx)
            if (c->class_collection[idx] & ISwbit((int)nr))
                t3_add_bit(t, c->charnames[idx]);
    }
    c->map_b = xmalloc(2 * sizeof *c->map_b);
    c->map32_b = xmalloc(c->map_collection_nr * sizeof *c->map32_b);
    c->map_3level = xmalloc(c->map_collection_nr * sizeof *c->map_3level);
    for (idx = 0; idx < 2; ++idx) {
        c->map_b[idx] = xmalloc((256 + 128) * sizeof(uint32_t));
        for (size_t i = 0; i < 256; ++i)
            c->map_b[idx][128 + i] = c->map256_collection[idx][i];
        for (size_t i = 0; i < 127; ++i)
            c->map_b[idx][i] = c->map_b[idx][256 + i];
        c->map_b[idx][127] = (uint32_t)-1;
    }
    for (idx = 0; idx < c->map_collection_nr; ++idx) {
        c->map32_b[idx] = xmalloc(256 * sizeof(uint32_t));
        for (uint32_t i = 0; i < 256; ++i)
            c->map32_b[idx][i] = c->map_collection[idx][i] != 0 ? c->map_collection[idx][i] : i;
    }
    for (nr = 0; nr < c->map_collection_nr; nr++) {
        struct table3 *t = &c->map_3level[nr];
        t3_init(t, 7, 9, 4, 0);
        for (idx = 0; idx < c->map_collection_act[nr]; ++idx)
            if (c->map_collection[nr][idx] != 0)
                t3_add(t, c->charnames[idx], c->map_collection[nr][idx] - c->charnames[idx]);
    }
    c->class_offset = 72;
    c->map_offset = c->class_offset + (uint32_t)c->nr_charclass;
    /* the widths */
    struct table3 *t = &c->width;
    t3_init(t, 7, 9, 1, 0xff);
    void *curs = NULL;
    const void *key;
    size_t len;
    void *vdata;
    while (ht_iterate(&cm->char_table, &curs, &key, &len, &vdata) == 0) {
        struct charseq *d = vdata;
        if (d->ucs4 == UNINITIALIZED_CHAR_VALUE)
            d->ucs4 = repertoire_find_value(c->repertoire, d->name, len);
        if (d->ucs4 != ILLEGAL_CHAR_VALUE) {
            uint32_t *bits = find_idx(c, &c->class_collection, NULL, &c->class_collection_act, d->ucs4);
            if (bits && (*bits & BITw(tok_print)))
                t3_add(t, d->ucs4, cm->width_default);
        }
    }
    for (size_t r = 0; r < cm->nwidth_rules; ++r) {
        const struct width_rule *w = &cm->width_rules[r];
        unsigned char bytes[16];
        int nbytes = w->from->nbytes;
        memcpy(bytes, w->from->bytes, (size_t)nbytes);
        while (nbytes < w->to->nbytes || memcmp(bytes, w->to->bytes, (size_t)nbytes) <= 0) {
            struct charseq *seq = charmap_find_symbol(cm, (const char *)bytes, (size_t)nbytes);
            uint32_t wch = !seq ? ILLEGAL_CHAR_VALUE
                           : seq->ucs4 != UNINITIALIZED_CHAR_VALUE ? seq->ucs4
                                                                   : repertoire_find_value(c->repertoire, seq->name, strlen(seq->name));
            if (wch != ILLEGAL_CHAR_VALUE) {
                uint32_t *bits = find_idx(c, &c->class_collection, NULL, &c->class_collection_act, wch);
                if (bits && (*bits & BITw(tok_print)))
                    t3_add(t, wch, w->width);
            }
            if (!next_bytes(bytes, &nbytes, w->to->nbytes))
                break;
        }
    }
    t3_add(t, 0, 0);
    c->mb_cur_max = (uint32_t)cm->mb_cur_max;
    /* transliteration */
    if (c->translit_include)
        translit_flatten(c, cm, &c->translit);
    if (c->translit) {
        size_t number = 0;
        for (struct translit *r = c->translit; r; r = r->next)
            ++number;
        struct translit **sorted = xmalloc(number * sizeof *sorted);
        number = 0;
        for (struct translit *r = c->translit; r; r = r->next) {
            size_t i = 0;
            bool replace = false;
            while (i < number) {
                int res = wcscmp32(sorted[i]->from, r->from);
                if (res == 0) {
                    replace = true;
                    break;
                }
                if (res > 0)
                    break;
                ++i;
            }
            if (replace)
                sorted[i] = r;
            else {
                memmove(&sorted[i + 1], &sorted[i], (number - i) * sizeof *sorted);
                sorted[i] = r;
                ++number;
            }
        }
        size_t from_len = 0, to_len = 0;
        for (size_t i = 0; i < number; ++i) {
            from_len += wcslen32(sorted[i]->from) + 1;
            for (struct translit_to *s = sorted[i]->to; s; s = s->next)
                to_len += wcslen32(s->str) + 1;
            ++to_len;
        }
        c->translit_from_idx = xmalloc((number + 1) * sizeof(uint32_t));
        c->translit_from_tbl = xmalloc((from_len + 1) * sizeof(uint32_t));
        c->translit_to_idx = xmalloc((number + 1) * sizeof(uint32_t));
        c->translit_to_tbl = xmalloc((to_len + 1) * sizeof(uint32_t));
        from_len = to_len = 0;
        for (size_t i = 0; i < number; ++i) {
            c->translit_from_idx[i] = (uint32_t)from_len;
            c->translit_to_idx[i] = (uint32_t)to_len;
            size_t l = wcslen32(sorted[i]->from) + 1;
            memcpy(&c->translit_from_tbl[from_len], sorted[i]->from, l * sizeof(uint32_t));
            from_len += l;
            for (struct translit_to *s = sorted[i]->to; s; s = s->next) {
                l = wcslen32(s->str) + 1;
                memcpy(&c->translit_to_tbl[to_len], s->str, l * sizeof(uint32_t));
                to_len += l;
            }
            c->translit_to_tbl[to_len++] = 0;
        }
        c->translit_idx_size = (uint32_t)number;
        c->translit_from_tbl_size = from_len;
        c->translit_to_tbl_size = to_len;
        free(sorted);
    } else {
        c->translit_from_idx = c->translit_from_tbl = c->translit_to_idx = c->translit_to_tbl = no_str;
        c->translit_idx_size = 0;
        c->translit_from_tbl_size = c->translit_to_tbl_size = 0;
    }
}

void ctype_output(struct localedef *locale, const struct charmap *cm, const char *out)
{
    struct ctype *c = locale->categories[C_CTYPE];
    size_t nelems = 72 + c->nr_charclass + c->map_collection_nr;
    struct locfile f;
    allocate_arrays(c, cm);
    uint32_t default_missing_len = c->default_missing ? (uint32_t)wcslen32(c->default_missing) : 0;
    init_locale_data(&f, nelems);
    for (size_t elem = 0; elem < nelems; ++elem) {
        if (elem >= 72) {
            size_t nr = elem - 72;
            if (nr < c->nr_charclass) {
                start_locale_prelude(&f);
                add_locale_uint32_array(&f, c->class_b[nr], 256 / 32);
                end_locale_prelude(&f);
                add_locale_table3(&f, &c->class_3level[nr], true);
            } else {
                add_locale_table3(&f, &c->map_3level[nr - c->nr_charclass], false);
            }
            continue;
        }
        switch (elem) {
        case 0: add_locale_raw_data(&f, c->ctype_b, (256 + 128) * sizeof(uint16_t)); break;
        case 1: add_locale_uint32_array(&f, c->map_b[0], 256 + 128); break;
        case 3: add_locale_uint32_array(&f, c->map_b[1], 256 + 128); break;
        case 2: case 4: case 6: case 7: case 8: case 9: add_locale_empty(&f); break;
        case 5: add_locale_raw_data(&f, c->ctype32_b, 256 * sizeof(uint32_t)); break;
        case 10:
            start_locale_structure(&f);
            for (size_t i = 0; i < c->nr_charclass; ++i)
                add_locale_string(&f, c->classnames[i]);
            add_locale_char(&f, 0);
            align_locale_data(&f, 4);
            end_locale_structure(&f);
            break;
        case 11:
            start_locale_structure(&f);
            for (size_t i = 0; i < c->map_collection_nr; ++i)
                add_locale_string(&f, c->mapnames[i]);
            add_locale_char(&f, 0);
            align_locale_data(&f, 4);
            end_locale_structure(&f);
            break;
        case 12: add_locale_table3(&f, &c->width, false); break;
        case 13: add_locale_uint32(&f, c->mb_cur_max); break;
        case 14: add_locale_string(&f, c->codeset_name); break;
        case 15: add_locale_uint32_array(&f, c->map32_b[0], 256); break;
        case 16: add_locale_uint32_array(&f, c->map32_b[1], 256); break;
        case 17: add_locale_uint32(&f, c->class_offset); break;
        case 18: add_locale_uint32(&f, c->map_offset); break;
        case 19: add_locale_uint32(&f, (uint32_t)(c->mbdigits_act / 10)); break;
        case 30: add_locale_uint32(&f, (uint32_t)(c->wcdigits_act / 10)); break;
        case 61: add_locale_uint32(&f, c->translit_idx_size); break;
        case 62: add_locale_uint32_array(&f, c->translit_from_idx, c->translit_idx_size); break;
        case 63: add_locale_uint32_array(&f, c->translit_from_tbl, c->translit_from_tbl_size); break;
        case 64: add_locale_uint32_array(&f, c->translit_to_idx, c->translit_idx_size); break;
        case 65: add_locale_uint32_array(&f, c->translit_to_tbl, c->translit_to_tbl_size); break;
        case 66: add_locale_uint32(&f, default_missing_len); break;
        case 67: add_locale_uint32_array(&f, c->default_missing ? c->default_missing : no_str, default_missing_len); break;
        case 68: add_locale_uint32(&f, c->ntranslit_ignore); break;
        case 69:
            start_locale_structure(&f);
            for (struct translit_ignore *r = c->translit_ignore; r; r = r->next) {
                add_locale_uint32(&f, r->from);
                add_locale_uint32(&f, r->to);
                add_locale_uint32(&f, r->step);
            }
            end_locale_structure(&f);
            break;
        case 70: add_locale_uint32(&f, c->to_nonascii); break;
        case 71: add_locale_uint32(&f, c->nonascii_case); break;
        default:
            if (elem >= 20 && elem <= 29) {
                start_locale_structure(&f);
                for (size_t i = elem - 20; i < c->mbdigits_act; i += 10) {
                    add_locale_raw_data(&f, c->mbdigits[i]->bytes, (size_t)c->mbdigits[i]->nbytes);
                    add_locale_char(&f, 0);
                }
                end_locale_structure(&f);
            } else if (elem >= 31 && elem <= 40) {
                start_locale_structure(&f);
                for (size_t i = elem - 31; i < c->wcdigits_act; i += 10)
                    add_locale_uint32(&f, c->wcdigits[i]);
                end_locale_structure(&f);
            } else if (elem >= 41 && elem <= 50) {
                start_locale_structure(&f);
                size_t i = elem - 41;
                add_locale_raw_data(&f, c->mboutdigits[i]->bytes, (size_t)c->mboutdigits[i]->nbytes);
                add_locale_char(&f, 0);
                end_locale_structure(&f);
            } else if (elem >= 51 && elem <= 60) {
                add_locale_uint32(&f, c->wcoutdigits[elem - 51]);
            }
        }
    }
    write_locale_data(out, C_CTYPE, &f);
}
