/* XSI interprocess communication: ftok, message queues, semaphore
 * sets, shared memory.  The structures are the kernel's 64-bit ones. */
#include <errno.h>
#include <stdarg.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include "syscall.h"

/* as glibc: the low 8 bits of id, of the device and 16 of the inode */
key_t ftok(const char *path, int id)
{
	struct stat st;
	if (stat(path, &st) < 0)
		return -1;
	return (key_t)((st.st_ino & 0xffff) | ((st.st_dev & 0xff) << 16) | ((unsigned)(id & 0xff) << 24));
}

int msgget(key_t k, int flag) { return (int)__sys2(SYS_msgget, k, flag); }
int msgctl(int id, int cmd, struct msqid_ds *b) { return (int)__sys3(SYS_msgctl, id, cmd, b); }
int msgsnd(int id, const void *m, size_t n, int flag) { return (int)__sys_cp4(SYS_msgsnd, id, m, n, flag); }
ssize_t msgrcv(int id, void *m, size_t n, long type, int flag) { return __sys_cp5(SYS_msgrcv, id, m, n, type, flag); }

int semget(key_t k, int n, int flag)
{
	if (n > 0xffff) {     /* sem_nsems is unsigned short */
		errno = EINVAL;
		return -1;
	}
	return (int)__sys3(SYS_semget, k, n, flag);
}

int semop(int id, struct sembuf *ops, size_t n) { return (int)__sys3(SYS_semop, id, ops, n); }

/* the fourth argument, a union semun of the application's, as a long */
int semctl(int id, int num, int cmd, ...)
{
	unsigned long arg = 0;
	switch (cmd) {
	case SETVAL: case GETALL: case SETALL: case IPC_STAT: case IPC_SET: case 3: case 19: case 20: {
		va_list ap;
		va_start(ap, cmd);
		arg = va_arg(ap, unsigned long);
		va_end(ap);
	}
	}
	if (cmd == IPC_STAT) {   /* clear the high half of sem_nsems */
		struct semid_ds *d = (struct semid_ds *)arg;
		long r = __syscall4(SYS_semctl, id, num, cmd, arg);
		if (r >= 0) {
			int i;
			for (i = 0; i < 6; i++)
				d->__pad[i] = 0;
		}
		return (int)__syscall_ret((unsigned long)r);
	}
	return (int)__sys4(SYS_semctl, id, num, cmd, arg);
}

int shmget(key_t k, size_t n, int flag) { return (int)__sys3(SYS_shmget, k, n, flag); }
int shmctl(int id, int cmd, struct shmid_ds *b) { return (int)__sys3(SYS_shmctl, id, cmd, b); }
void *shmat(int id, const void *a, int flag) { return (void *)__sys3(SYS_shmat, id, a, flag); }
int shmdt(const void *a) { return (int)__sys1(SYS_shmdt, a); }
