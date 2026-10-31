/*
 * catread.c - read every message of a catalog that gencat wrote, with the
 * C library's catopen() and catgets(), and check each against the
 * catalog's own table (for tests/gencatdiff.tl)
 *
 *   catread CATALOG
 *
 * Exit status 0 when catgets() gives every message, 1 otherwise (with the
 * ones it did not give on the standard output).
 */

#define _POSIX_C_SOURCE 202405L
#define _XOPEN_SOURCE 800

#include <nl_types.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
    if (argc != 2)
        return 2;
    FILE *f = fopen(argv[1], "rb");
    if (!f)
        return 2;
    static unsigned char d[1 << 24];
    size_t n = fread(d, 1, sizeof d, f);
    fclose(f);
    uint32_t h[3];
    memcpy(h, d, 12);
    size_t ent = (size_t)h[1] * h[2];
    const unsigned char *strs = d + 12 + 24 * ent;
    nl_catd cd = catopen(argv[1], 0);
    if (cd == (nl_catd)-1) {
        printf("catopen failed\n");
        return 1;
    }
    int bad = 0;
    for (size_t i = 0; i < ent; i++) {
        const unsigned char *e = d + 12 + 12 * i;
        uint32_t s = (uint32_t)e[0] | (uint32_t)e[1] << 8 | (uint32_t)e[2] << 16 | (uint32_t)e[3] << 24;
        uint32_t m = (uint32_t)e[4] | (uint32_t)e[5] << 8 | (uint32_t)e[6] << 16 | (uint32_t)e[7] << 24;
        uint32_t o = (uint32_t)e[8] | (uint32_t)e[9] << 8 | (uint32_t)e[10] << 16 | (uint32_t)e[11] << 24;
        if (s == 0)
            continue;
        const char *want = (const char *)strs + o;
        const char *got = catgets(cd, (int)s - 1, (int)m, "\001missing");
        if (strcmp(got, want) != 0) {
            printf("set %u message %u\n", s - 1, m);
            bad = 1;
        }
    }
    (void)n;
    catclose(cd);
    return bad;
}
