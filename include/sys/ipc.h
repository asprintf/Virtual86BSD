#ifndef _SYS_IPC_H
#define _SYS_IPC_H
#include <bits/features.h>
#define __NEED_uid_t
#define __NEED_gid_t
#define __NEED_mode_t
#define __NEED_key_t
#include <bits/alltypes.h>

/* the kernel's struct ipc64_perm on x86-64 */
struct ipc_perm {
	key_t __key;
	uid_t uid;
	gid_t gid;
	uid_t cuid;
	gid_t cgid;
	mode_t mode;
	unsigned short __seq;
	unsigned short __pad2;
	unsigned long __unused1, __unused2;
};

#define IPC_CREAT  01000
#define IPC_EXCL   02000
#define IPC_NOWAIT 04000

#define IPC_PRIVATE ((key_t)0)

#define IPC_RMID 0
#define IPC_SET  1
#define IPC_STAT 2
#if __EXT_VISIBLE
#define IPC_INFO 3
#endif

__BEGIN_DECLS
key_t ftok(const char *, int);
__END_DECLS

#endif
