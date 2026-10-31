#ifndef _SYS_TIME_H
#define _SYS_TIME_H
#include <bits/features.h>
#include <sys/select.h>

__BEGIN_DECLS
int utimes(const char *, const struct timeval [2]);
__END_DECLS
#endif
