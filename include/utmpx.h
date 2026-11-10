#ifndef _UTMPX_H
#define _UTMPX_H
#include <bits/features.h>
#define __NEED_pid_t
#define __NEED_struct_timeval
#define __NEED_time_t
#define __NEED_suseconds_t
#include <bits/alltypes.h>

/* The POSIX structure (ut_tv is a struct timeval). The database itself
 * is the system's utmp file in the traditional Linux record format
 * (384 bytes, 32-bit times); entries are converted when read and written. */
/* the sizes, under glibc's names (a program can test __UT_HOSTSIZE for ut_host) */
#define __UT_LINESIZE 32
#define __UT_NAMESIZE 32
#define __UT_HOSTSIZE 256

struct utmpx {
	short ut_type;
	pid_t ut_pid;
	char ut_line[__UT_LINESIZE];
	char ut_id[4];
	char ut_user[__UT_NAMESIZE];
	char ut_host[__UT_HOSTSIZE];
	struct {
		short __e_termination;
		short __e_exit;
	} ut_exit;
	int ut_session;
	struct timeval ut_tv;
	unsigned ut_addr_v6[4];
	char __ut_unused[20];
};

#define EMPTY         0
#define BOOT_TIME     2
#define NEW_TIME      3
#define OLD_TIME      4
#define INIT_PROCESS  5
#define LOGIN_PROCESS 6
#define USER_PROCESS  7
#define DEAD_PROCESS  8
#if __EXT_VISIBLE
#define RUN_LVL       1     /* not POSIX */
#endif

__BEGIN_DECLS
void          endutxent(void);
struct utmpx *getutxent(void);
struct utmpx *getutxid(const struct utmpx *);
struct utmpx *getutxline(const struct utmpx *);
struct utmpx *pututxline(const struct utmpx *);
void          setutxent(void);
__END_DECLS
#endif
