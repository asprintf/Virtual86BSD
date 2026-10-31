#ifndef _STRING_H
#define _STRING_H
#include <bits/features.h>
#include <bits/null.h>
#define __NEED_size_t
#if __POSIX_VISIBLE
#define __NEED_locale_t
#endif
#include <bits/alltypes.h>

__BEGIN_DECLS
void   *memchr(const void *, int, size_t);
int     memcmp(const void *, const void *, size_t);
void   *memcpy(void *__RESTRICT, const void *__RESTRICT, size_t);
void   *memmove(void *, const void *, size_t);
void   *memset(void *, int, size_t);
char   *strcat(char *__RESTRICT, const char *__RESTRICT);
char   *strchr(const char *, int);
int     strcmp(const char *, const char *);
int     strcoll(const char *, const char *);
char   *strcpy(char *__RESTRICT, const char *__RESTRICT);
size_t  strcspn(const char *, const char *);
char   *strerror(int);
size_t  strlen(const char *);
char   *strncat(char *__RESTRICT, const char *__RESTRICT, size_t);
int     strncmp(const char *, const char *, size_t);
char   *strncpy(char *__RESTRICT, const char *__RESTRICT, size_t);
char   *strpbrk(const char *, const char *);
char   *strrchr(const char *, int);
size_t  strspn(const char *, const char *);
char   *strstr(const char *, const char *);
char   *strtok(char *__RESTRICT, const char *__RESTRICT);
size_t  strxfrm(char *__RESTRICT, const char *__RESTRICT, size_t);

#if __XSI_VISIBLE
void   *memccpy(void *__RESTRICT, const void *__RESTRICT, int, size_t);
#endif
#if __POSIX_VISIBLE
void   *memmem(const void *, size_t, const void *, size_t);
char   *stpcpy(char *__RESTRICT, const char *__RESTRICT);
char   *stpncpy(char *__RESTRICT, const char *__RESTRICT, size_t);
int     strcoll_l(const char *, const char *, locale_t);
char   *strdup(const char *);
char   *strerror_l(int, locale_t);
int     strerror_r(int, char *, size_t);
size_t  strlcat(char *__RESTRICT, const char *__RESTRICT, size_t);
size_t  strlcpy(char *__RESTRICT, const char *__RESTRICT, size_t);
char   *strndup(const char *, size_t);
size_t  strnlen(const char *, size_t);
char   *strsignal(int);
char   *strtok_r(char *__RESTRICT, const char *__RESTRICT, char **__RESTRICT);
size_t  strxfrm_l(char *__RESTRICT, const char *__RESTRICT, size_t, locale_t);
#endif
__END_DECLS
#endif
