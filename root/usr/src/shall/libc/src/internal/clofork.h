/* Internal: emulation of close-on-fork (FD_CLOFORK, O_CLOFORK), which
 * Linux lacks.  A bitmap records the descriptors that have it; fork()
 * closes them in the child.  Descriptors only become flagged through the
 * library, and close()/dup2()/dup3() clear the flag of the descriptor
 * they free or replace, so a reused descriptor number starts clear. */
#ifndef _INTERNAL_CLOFORK_H
#define _INTERNAL_CLOFORK_H
extern volatile int __clofork_any;
int  __clofork_set(int fd, int on);   /* 0, or -1 (ENOMEM) */
int  __clofork_get(int fd);
void __clofork_close_all(void);       /* in the child after fork */
#endif
