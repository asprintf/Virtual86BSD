#ifndef _SYS_SEM_H
#define _SYS_SEM_H
#include <bits/features.h>
#include <sys/ipc.h>
#define __NEED_pid_t
#define __NEED_size_t
#define __NEED_time_t
#include <bits/alltypes.h>

/* the kernel's struct semid64_ds on x86-64 (sem_nsems: there an
 * unsigned long, of which this is the low half) */
struct semid_ds {
	struct ipc_perm sem_perm;
	time_t sem_otime;
	unsigned long __unused1;
	time_t sem_ctime;
	unsigned long __unused2;
	unsigned short sem_nsems;
	char __pad[6];
	unsigned long __unused3, __unused4;
};

struct sembuf {
	unsigned short sem_num;
	short sem_op;
	short sem_flg;
};

#define SEM_UNDO 0x1000

#define GETPID  11
#define GETVAL  12
#define GETALL  13
#define GETNCNT 14
#define GETZCNT 15
#define SETVAL  16
#define SETALL  17

__BEGIN_DECLS
int semctl(int, int, int, ...);
int semget(key_t, int, int);
int semop(int, struct sembuf *, size_t);
__END_DECLS

#endif
