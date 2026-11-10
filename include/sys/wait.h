#ifndef _SYS_WAIT_H
#define _SYS_WAIT_H
#include <bits/features.h>
#include <bits/siginfo.h>
#define __NEED_id_t
#define __NEED_pid_t
#include <bits/alltypes.h>

#include <bits/waitstatus.h>

typedef enum { P_ALL = 0, P_PID = 1, P_PGID = 2 } idtype_t;

__BEGIN_DECLS
pid_t wait(int *);
int   waitid(idtype_t, id_t, siginfo_t *, int);
pid_t waitpid(pid_t, int *, int);
__END_DECLS
#endif
