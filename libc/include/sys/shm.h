#ifndef _SYS_SHM_H
#define _SYS_SHM_H
#include <bits/features.h>
#include <sys/ipc.h>
#define __NEED_pid_t
#define __NEED_size_t
#define __NEED_time_t
#include <bits/alltypes.h>

typedef unsigned long shmatt_t;

/* the kernel's struct shmid64_ds on x86-64 */
struct shmid_ds {
	struct ipc_perm shm_perm;
	size_t shm_segsz;
	time_t shm_atime;
	time_t shm_dtime;
	time_t shm_ctime;
	pid_t shm_cpid;
	pid_t shm_lpid;
	shmatt_t shm_nattch;
	unsigned long __unused[2];
};

#define SHM_RDONLY 010000
#define SHM_RND    020000
#define SHMLBA     4096

__BEGIN_DECLS
void *shmat(int, const void *, int);
int shmctl(int, int, struct shmid_ds *);
int shmdt(const void *);
int shmget(key_t, size_t, int);
__END_DECLS

#endif
