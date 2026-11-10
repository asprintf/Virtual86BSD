/* Internal: close-on-fork (FD_CLOFORK, O_CLOFORK).
 *
 * V86BSD's kernel has it: the flags go to the kernel as they are (open,
 * pipe2, dup3, socket, accept4, recvmsg take them; F_GETFD/F_SETFD carry
 * FD_CLOFORK; F_DUPFD_CLOFORK exists), and the kernel closes the
 * descriptors in a new process.  __clofork_native() finds out, once.
 *
 * Linux lacks it: then it is emulated.  A bitmap records the descriptors
 * that have it; fork() closes them in the child.  Descriptors only become
 * flagged through the library, and close()/dup2()/dup3() clear the flag
 * of the descriptor they free or replace, so a reused descriptor number
 * starts clear. */
#ifndef _INTERNAL_CLOFORK_H
#define _INTERNAL_CLOFORK_H
extern volatile int __clofork_any;
int  __clofork_native(void);          /* 1 if the kernel has close-on-fork */
int  __clofork_set(int fd, int on);   /* 0, or -1 (ENOMEM) */
int  __clofork_get(int fd);
void __clofork_close_all(void);       /* in the child after fork */
#endif
