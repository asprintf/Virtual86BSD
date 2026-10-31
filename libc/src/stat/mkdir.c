#include <sys/stat.h>
#include <fcntl.h>
#include "syscall.h"

int mkdir(const char *p, mode_t m) { return (int)__sys3(SYS_mkdirat, AT_FDCWD, p, m); }
int mkdirat(int fd, const char *p, mode_t m) { return (int)__sys3(SYS_mkdirat, fd, p, m); }
int mkfifo(const char *p, mode_t m) { return (int)__sys4(SYS_mknodat, AT_FDCWD, p, (m & 07777) | S_IFIFO, 0); }
int mkfifoat(int fd, const char *p, mode_t m) { return (int)__sys4(SYS_mknodat, fd, p, (m & 07777) | S_IFIFO, 0); }
int mknod(const char *p, mode_t m, dev_t d) { return (int)__sys4(SYS_mknodat, AT_FDCWD, p, m, d); }
int mknodat(int fd, const char *p, mode_t m, dev_t d) { return (int)__sys4(SYS_mknodat, fd, p, m, d); }
