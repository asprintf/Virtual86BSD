/*
 * mo.c - messages objects (see mo.h)
 *
 * The format is that of GNU gettext, which the gettext functions of the
 * C libraries that have them read: a header of 32-bit words in either
 * byte order (the magic number 0x950412de, the revision, the number of
 * strings, the offsets of the tables of original and translated
 * strings, and of a hash table, not used here), then the tables of
 * (length, offset) pairs, the strings sorted by their bytes.  A message
 * with plural forms has the original "msgid\0msgid_plural" and the forms
 * as its translation, each ended by a NUL byte.  The translation of the
 * empty string is the header: its charset= and plural forms.
 */

#define _POSIX_C_SOURCE 202405L

#include "mo.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static unsigned long word(const struct mo *m, unsigned long off)
{
    const unsigned char *p = m->data + off;
    if (m->swap)
        return (unsigned long)p[0] << 24 | (unsigned long)p[1] << 16 | (unsigned long)p[2] << 8 | p[3];
    return (unsigned long)p[3] << 24 | (unsigned long)p[2] << 16 | (unsigned long)p[1] << 8 | p[0];
}

/* string i of the table at tab: false if it is out of the file */
static bool entry(const struct mo *m, unsigned long tab, unsigned long i, const char **s, size_t *len)
{
    unsigned long l = word(m, tab + 8 * i), o = word(m, tab + 8 * i + 4);
    if (o > m->size || l >= m->size - o || m->data[o + l] != '\0')
        return false;
    *s = (const char *)m->data + o;
    *len = l;
    return true;
}

bool mo_load(const char *path, struct mo *m)
{
    memset(m, 0, sizeof *m);
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return false;
    struct stat st;
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 28 || st.st_size > 0x7fffffff) {
        close(fd);
        errno = EINVAL;
        return false;
    }
    m->size = (size_t)st.st_size;
    m->data = malloc(m->size);
    if (!m->data) {
        close(fd);
        return false;
    }
    size_t got = 0;
    while (got < m->size) {
        ssize_t k = read(fd, m->data + got, m->size - got);
        if (k < 0 && errno == EINTR)
            continue;
        if (k <= 0) {
            int e = k < 0 ? errno : EINVAL;
            close(fd);
            mo_free(m);
            errno = e;
            return false;
        }
        got += (size_t)k;
    }
    close(fd);
    const unsigned char *d = m->data;
    if (d[0] == 0xde && d[1] == 0x12 && d[2] == 0x04 && d[3] == 0x95)
        m->swap = false;
    else if (d[0] == 0x95 && d[1] == 0x04 && d[2] == 0x12 && d[3] == 0xde)
        m->swap = true;
    else
        goto bad;
    if ((word(m, 4) >> 16) > 1)         /* the major revision: 0 or 1 */
        goto bad;
    m->n = word(m, 8);
    m->orig = word(m, 12);
    m->trans = word(m, 16);
    if (m->n > m->size / 8 || m->orig > m->size - 8 * m->n || m->trans > m->size - 8 * m->n)
        goto bad;
    for (unsigned long i = 0; i < m->n; i++) {
        const char *s;
        size_t l;
        if (!entry(m, m->orig, i, &s, &l) || !entry(m, m->trans, i, &s, &l))
            goto bad;
    }
    /* the header */
    size_t hl;
    const char *h = mo_find(m, "", &hl);
    if (h) {
        if (plural_header(h, &m->nplurals, &m->plural) < 0)
            m->nplurals = 0;
        if (!header_charset(h, m->charset, sizeof m->charset))
            m->charset[0] = '\0';
    }
    return true;
bad:
    mo_free(m);
    errno = EINVAL;
    return false;
}

void mo_free(struct mo *m)
{
    free(m->data);
    plural_free(m->plural);
    memset(m, 0, sizeof *m);
}

/* compare msgid with the original string s (to its first NUL) */
static int cmp(const char *msgid, const char *s)
{
    return strcmp(msgid, s);
}

const char *mo_find(const struct mo *m, const char *msgid, size_t *len)
{
    const char *s, *t;
    size_t l, tl;
    /* the strings are sorted: a binary search; else (a file not made so)
       every one in turn */
    unsigned long lo = 0, hi = m->n;
    while (lo < hi) {
        unsigned long mid = lo + (hi - lo) / 2;
        entry(m, m->orig, mid, &s, &l);
        int c = cmp(msgid, s);
        if (c == 0) {
            entry(m, m->trans, mid, &t, &tl);
            *len = tl;
            return t;
        }
        if (c < 0)
            hi = mid;
        else
            lo = mid + 1;
    }
    for (unsigned long i = 0; i < m->n; i++) {
        entry(m, m->orig, i, &s, &l);
        if (cmp(msgid, s) == 0) {
            entry(m, m->trans, i, &t, &tl);
            *len = tl;
            return t;
        }
    }
    return NULL;
}
