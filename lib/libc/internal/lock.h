/* Internal: futex-based locks for library-internal state. */
#ifndef _INTERNAL_LOCK_H
#define _INTERNAL_LOCK_H

void __lock(volatile int *);
void __unlock(volatile int *);
void __futex_wait(volatile int *, int val);
void __futex_wake(volatile int *, int cnt);

#define LOCK(x)   __lock(x)
#define UNLOCK(x) __unlock(x)

#endif
