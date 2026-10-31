/* Internal: open() flags, shared by <fcntl.h>, <unistd.h> (O_CLOEXEC,
 * O_CLOFORK) and <stdlib.h> (several). Linux x86-64 values. */
#ifndef _BITS_FCNTL_H
#define _BITS_FCNTL_H
#define O_RDONLY    00
#define O_WRONLY    01
#define O_RDWR      02
#define O_PATH      010000000
#define O_EXEC      O_PATH
#define O_SEARCH    O_PATH
#define O_ACCMODE   (03 | O_PATH)
#define O_CREAT     0100
#define O_EXCL      0200
#define O_NOCTTY    0400
#define O_TRUNC     01000
#define O_APPEND    02000
#define O_NONBLOCK  04000
#define O_DSYNC     010000
#define O_SYNC      04010000
#define O_RSYNC     04010000
#define O_DIRECTORY 0200000
#define O_NOFOLLOW  0400000
#define O_CLOEXEC   02000000
#define O_TTY_INIT  0
/* Linux has no close-on-fork; the library emulates it and never passes
 * this bit to the kernel. */
#define O_CLOFORK   010000000000
#endif
