/* User and group IDs.  The set*id calls act on every thread
 * (src/thread/__setxid.c). */
#include <unistd.h>
#include "syscall.h"

long __setxid(long, long, long, long);

uid_t getuid(void) { return (uid_t)__syscall0(SYS_getuid); }
uid_t geteuid(void) { return (uid_t)__syscall0(SYS_geteuid); }
gid_t getgid(void) { return (gid_t)__syscall0(SYS_getgid); }
gid_t getegid(void) { return (gid_t)__syscall0(SYS_getegid); }
int getgroups(int n, gid_t list[]) { return (int)__sys2(SYS_getgroups, n, list); }
int setuid(uid_t u) { return (int)__setxid(SYS_setuid, (long)u, (long)0, (long)0); }
int setgid(gid_t g) { return (int)__setxid(SYS_setgid, (long)g, (long)0, (long)0); }
int seteuid(uid_t u) { return (int)__setxid(SYS_setresuid, (long)-1, (long)u, (long)-1); }
int setegid(gid_t g) { return (int)__setxid(SYS_setresgid, (long)-1, (long)g, (long)-1); }
int setreuid(uid_t r, uid_t e) { return (int)__setxid(SYS_setreuid, (long)r, (long)e, (long)0); }
int setregid(gid_t r, gid_t e) { return (int)__setxid(SYS_setregid, (long)r, (long)e, (long)0); }
int setresuid(uid_t r, uid_t e, uid_t s) { return (int)__setxid(SYS_setresuid, (long)r, (long)e, (long)s); }
int setresgid(gid_t r, gid_t e, gid_t s) { return (int)__setxid(SYS_setresgid, (long)r, (long)e, (long)s); }
int getresuid(uid_t *__RESTRICT r, uid_t *__RESTRICT e, uid_t *__RESTRICT s) { return (int)__sys3(SYS_getresuid, r, e, s); }
int getresgid(gid_t *__RESTRICT r, gid_t *__RESTRICT e, gid_t *__RESTRICT s) { return (int)__sys3(SYS_getresgid, r, e, s); }
