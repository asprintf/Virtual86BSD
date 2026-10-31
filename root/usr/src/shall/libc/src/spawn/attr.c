#include <spawn.h>
#include <errno.h>
#include <string.h>

#define ALLFLAGS (POSIX_SPAWN_RESETIDS | POSIX_SPAWN_SETPGROUP | POSIX_SPAWN_SETSIGDEF | \
                  POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSID)

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
