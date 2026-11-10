/* setgroups(): not POSIX; declared in <grp.h> as on other systems. */
#include <sys/types.h>
#include "syscall.h"

int setgroups(size_t n, const gid_t *list);
long __setxid(long, long, long, long);
int setgroups(size_t n, const gid_t *list) { return (int)__setxid(SYS_setgroups, (long)n, (long)list, 0); }
