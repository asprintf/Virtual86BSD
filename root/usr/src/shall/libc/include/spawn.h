#ifndef _SPAWN_H
#define _SPAWN_H
#include <bits/features.h>
#define __NEED_mode_t
#define __NEED_pid_t
#define __NEED_sigset_t
#include <bits/alltypes.h>

struct sched_param;

typedef struct {
	short __flags;
	pid_t __pgrp;
	sigset_t __def, __mask;
	int __pad[16];
} posix_spawnattr_t;

typedef struct {
	void *__head, *__tail;
	int __pad[16];
} posix_spawn_file_actions_t;

#define POSIX_SPAWN_RESETIDS   0x01
#define POSIX_SPAWN_SETPGROUP  0x02
#define POSIX_SPAWN_SETSIGDEF  0x04
#define POSIX_SPAWN_SETSIGMASK 0x08
#define POSIX_SPAWN_SETSID     0x80
/* POSIX_SPAWN_SETSCHEDPARAM, POSIX_SPAWN_SETSCHEDULER: with <sched.h>. */

__BEGIN_DECLS
int posix_spawn(pid_t *__RESTRICT, const char *__RESTRICT, const posix_spawn_file_actions_t *__RESTRICT,
                const posix_spawnattr_t *__RESTRICT, char *const [__RESTRICT], char *const [__RESTRICT]);
int posix_spawnp(pid_t *__RESTRICT, const char *__RESTRICT, const posix_spawn_file_actions_t *__RESTRICT,
                 const posix_spawnattr_t *__RESTRICT, char *const [__RESTRICT], char *const [__RESTRICT]);
int posix_spawn_file_actions_addchdir(posix_spawn_file_actions_t *__RESTRICT, const char *__RESTRICT);
int posix_spawn_file_actions_addclose(posix_spawn_file_actions_t *, int);
int posix_spawn_file_actions_adddup2(posix_spawn_file_actions_t *, int, int);
int posix_spawn_file_actions_addfchdir(posix_spawn_file_actions_t *, int);
int posix_spawn_file_actions_addopen(posix_spawn_file_actions_t *__RESTRICT, int, const char *__RESTRICT, int, mode_t);
int posix_spawn_file_actions_destroy(posix_spawn_file_actions_t *);
int posix_spawn_file_actions_init(posix_spawn_file_actions_t *);
int posix_spawnattr_destroy(posix_spawnattr_t *);
int posix_spawnattr_getflags(const posix_spawnattr_t *__RESTRICT, short *__RESTRICT);
int posix_spawnattr_getpgroup(const posix_spawnattr_t *__RESTRICT, pid_t *__RESTRICT);
int posix_spawnattr_getsigdefault(const posix_spawnattr_t *__RESTRICT, sigset_t *__RESTRICT);
int posix_spawnattr_getsigmask(const posix_spawnattr_t *__RESTRICT, sigset_t *__RESTRICT);
int posix_spawnattr_init(posix_spawnattr_t *);
int posix_spawnattr_setflags(posix_spawnattr_t *, short);
int posix_spawnattr_setpgroup(posix_spawnattr_t *, pid_t);
int posix_spawnattr_setsigdefault(posix_spawnattr_t *__RESTRICT, const sigset_t *__RESTRICT);
int posix_spawnattr_setsigmask(posix_spawnattr_t *__RESTRICT, const sigset_t *__RESTRICT);
__END_DECLS
#endif
