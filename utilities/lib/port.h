/*
 * port.h - what the utilities need beyond the C library: the interfaces
 * of POSIX.1-2024 on C libraries of POSIX.1-2008, and GNU attributes
 * where the compiler has them
 *
 * The utilities are written for POSIX.1-2024 (_POSIX_C_SOURCE 202405L,
 * _XOPEN_SOURCE 800).  A C library that implements it (_POSIX_VERSION
 * 202405L in <unistd.h>) needs nothing from here but the attribute
 * macros.  For the C libraries of POSIX.1-2008, which have the new
 * interfaces under other names, as extensions, or not at all (glibc 2.39,
 * musl 1.2.4), this file supplies those the utilities use:
 *   tcgetwinsize(), tcsetwinsize()  through the TIOCGWINSZ and TIOCSWINSZ
 *                                   requests of ioctl(), which they stand
 *                                   for (glibc 2.39 lacks them; musl has
 *                                   them, but works with the ioctl too)
 *   tm_gmtoff, tm_zone              members of struct tm that glibc and
 *                                   musl name __tm_gmtoff and __tm_zone
 *                                   unless _DEFAULT_SOURCE or _BSD_SOURCE
 *   setresuid(), setresgid()        declared by glibc and musl only for
 *                                   _GNU_SOURCE
 * Include it after the feature test macros (it includes <sys/types.h>,
 * <termios.h>, <time.h> and <unistd.h>).
 */

#ifndef PORT_H
#define PORT_H

/* noreturn functions, and printf-like format checking: GNU attributes
   with GCC and Clang, _Noreturn elsewhere (C11; nothing in C99) */
#if defined(__GNUC__) || defined(__clang__)
#define NORETURN __attribute__((noreturn))
#define PRINTF(fmt, args) __attribute__((format(printf, fmt, args)))
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define NORETURN _Noreturn
#define PRINTF(fmt, args)
#else
#define NORETURN
#define PRINTF(fmt, args)
#endif

#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#if !defined(_POSIX_VERSION) || _POSIX_VERSION < 202405L

#include <sys/ioctl.h>

static inline int port_tcgetwinsize(int fd, struct winsize *ws)
{
    return ioctl(fd, TIOCGWINSZ, ws);
}

static inline int port_tcsetwinsize(int fd, const struct winsize *ws)
{
    return ioctl(fd, TIOCSWINSZ, ws);
}

#define tcgetwinsize port_tcgetwinsize
#define tcsetwinsize port_tcsetwinsize

#if (defined(__GLIBC__) && !defined(__USE_MISC)) || \
    (!defined(__GLIBC__) && defined(__linux__) && !defined(__tm_gmtoff))
#define tm_gmtoff __tm_gmtoff
#define tm_zone __tm_zone
#endif

int setresuid(uid_t, uid_t, uid_t);
int setresgid(gid_t, gid_t, gid_t);

#endif

#endif
