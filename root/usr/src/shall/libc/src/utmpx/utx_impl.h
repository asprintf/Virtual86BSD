/* Internal: the on-disk utmp record (Linux, x86-64 glibc layout). */
#ifndef _INTERNAL_UTX_H
#define _INTERNAL_UTX_H
#include <utmpx.h>
#include <stdint.h>

#define UTX_RECSIZE 384

int __utx_open(int flags);                     /* the database file, or -1 */
void __utx_decode(struct utmpx *, const unsigned char *rec);
void __utx_encode(unsigned char *rec, const struct utmpx *);

#endif
