#include <spawn.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <sched.h>

#define ALLFLAGS (POSIX_SPAWN_RESETIDS | POSIX_SPAWN_SETPGROUP | POSIX_SPAWN_SETSIGDEF | \
                  POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSID | \
                  POSIX_SPAWN_SETSCHEDPARAM | POSIX_SPAWN_SETSCHEDULER)

int posix_spawnattr_init(posix_spawnattr_t *a) { memset(a, 0, sizeof *a); return 0; }
int posix_spawnattr_destroy(posix_spawnattr_t *a) { (void)a; return 0; }

int posix_spawnattr_setflags(posix_spawnattr_t *a, short f)
{
	if (f & ~ALLFLAGS) return EINVAL;
	a->__flags = f;
	return 0;
}

int posix_spawnattr_getflags(const posix_spawnattr_t *__RESTRICT a, short *__RESTRICT f) { *f = a->__flags; return 0; }
int posix_spawnattr_setpgroup(posix_spawnattr_t *a, pid_t p) { a->__pgrp = p; return 0; }
int posix_spawnattr_getpgroup(const posix_spawnattr_t *__RESTRICT a, pid_t *__RESTRICT p) { *p = a->__pgrp; return 0; }
int posix_spawnattr_setsigdefault(posix_spawnattr_t *__RESTRICT a, const sigset_t *__RESTRICT s) { a->__def = *s; return 0; }
int posix_spawnattr_getsigdefault(const posix_spawnattr_t *__RESTRICT a, sigset_t *__RESTRICT s) { *s = a->__def; return 0; }
int posix_spawnattr_setsigmask(posix_spawnattr_t *__RESTRICT a, const sigset_t *__RESTRICT s) { a->__mask = *s; return 0; }
int posix_spawnattr_getsigmask(const posix_spawnattr_t *__RESTRICT a, sigset_t *__RESTRICT s) { *s = a->__mask; return 0; }

/* [PS] the policy and parameters the child gets with POSIX_SPAWN_SETSCHEDULER
 * or POSIX_SPAWN_SETSCHEDPARAM; checked against the system when it is spawned */
int posix_spawnattr_setschedpolicy(posix_spawnattr_t *a, int pol)
{
	if (pol != SCHED_OTHER && pol != SCHED_FIFO && pol != SCHED_RR && pol != SCHED_SPORADIC) return EINVAL;
	a->__policy = pol;
	return 0;
}
int posix_spawnattr_getschedpolicy(const posix_spawnattr_t *__RESTRICT a, int *__RESTRICT pol) { *pol = a->__policy; return 0; }
/* (a sporadic server's other members are kept in __pad) */
#define SS_OFF offsetof(struct sched_param, sched_ss_low_priority)
int posix_spawnattr_setschedparam(posix_spawnattr_t *__RESTRICT a, const struct sched_param *__RESTRICT p)
{
	a->__prio = p->sched_priority;
	memcpy(a->__pad, (const char *)p + SS_OFF, sizeof *p - SS_OFF);
	return 0;
}
int posix_spawnattr_getschedparam(const posix_spawnattr_t *__RESTRICT a, struct sched_param *__RESTRICT p)
{
	memset(p, 0, sizeof *p);
	p->sched_priority = a->__prio;
	memcpy((char *)p + SS_OFF, a->__pad, sizeof *p - SS_OFF);
	return 0;
}
