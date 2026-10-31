/*
 * arch.h - archives of the format the link editor reads (that of GNU ar
 * and System V: "!<arch>\n", 60-byte member headers, the long names in
 * the member "//", the symbol table in the member "/"), read and written
 * (shared by ar, nm and strip)
 */

#ifndef ARCH_H
#define ARCH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>

struct ar_member {
    char *name;                 /* without the trailing slash */
    long long mtime;
    long uid, gid;
    unsigned long mode;
    unsigned char *data;
    size_t size;
};

struct archive {
    struct ar_member *m;
    size_t n, cap;
    bool had_symtab;            /* it had a symbol table */
};

/* is data an archive?  (the magic string) */
bool ar_is(const unsigned char *data, size_t size);
/* read the archive in data (the members' data copied): false with *why
   set when it is not a valid archive */
bool ar_parse(struct archive *a, const unsigned char *data, size_t size, const char **why);
void ar_free(struct archive *a);

/* the archive as bytes (malloc'd), with a symbol table when one of the
   members is an object file this can read (or when force_symtab and
   there is one); symtab_time: the date in the symbol table's header */
unsigned char *ar_build(const struct archive *a, size_t *size, long long symtab_time);

/* read a whole file (malloc'd): NULL with errno set */
unsigned char *ar_slurp(const char *path, size_t *size);

#endif
