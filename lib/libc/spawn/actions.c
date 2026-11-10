#include <spawn.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include "spawn_impl.h"

int posix_spawn_file_actions_init(posix_spawn_file_actions_t *fa)
{
	fa->__head = fa->__tail = 0;
	return 0;
}

int posix_spawn_file_actions_destroy(posix_spawn_file_actions_t *fa)
{
	struct fa *a = fa->__head, *n;
	for (; a; a = n) { n = a->next; free(a); }
	fa->__head = fa->__tail = 0;
	return 0;
}

static int add(posix_spawn_file_actions_t *fa, int op, int fd, int srcfd, const char *path, int oflag, mode_t mode)
{
	size_t l = path ? strlen(path) + 1 : 0;
	struct fa *a;
	long max = sysconf(_SC_OPEN_MAX);
	if (op != FA_CHDIR && (fd < 0 || (max > 0 && fd >= max))) return EBADF;
	if (op == FA_DUP2 && (srcfd < 0 || (max > 0 && srcfd >= max))) return EBADF;
	if (!(a = malloc(sizeof *a + l))) return ENOMEM;
	a->next = 0; a->op = op; a->fd = fd; a->srcfd = srcfd; a->oflag = oflag; a->mode = mode;
	if (path) memcpy(a->path, path, l);
	if (fa->__tail) ((struct fa *)fa->__tail)->next = a;
	else fa->__head = a;
	fa->__tail = a;
	return 0;
}

int posix_spawn_file_actions_addclose(posix_spawn_file_actions_t *fa, int fd)
{ return add(fa, FA_CLOSE, fd, 0, 0, 0, 0); }

int posix_spawn_file_actions_adddup2(posix_spawn_file_actions_t *fa, int src, int fd)
{ return add(fa, FA_DUP2, fd, src, 0, 0, 0); }

int posix_spawn_file_actions_addopen(posix_spawn_file_actions_t *__RESTRICT fa, int fd,
                                     const char *__RESTRICT path, int oflag, mode_t mode)
{ return add(fa, FA_OPEN, fd, 0, path, oflag, mode); }

int posix_spawn_file_actions_addchdir(posix_spawn_file_actions_t *__RESTRICT fa, const char *__RESTRICT path)
{ return add(fa, FA_CHDIR, 0, 0, path, 0, 0); }

int posix_spawn_file_actions_addfchdir(posix_spawn_file_actions_t *fa, int fd)
{ return add(fa, FA_FCHDIR, fd, 0, 0, 0, 0); }
