/* Internal: file actions of posix_spawn. */
#ifndef _INTERNAL_SPAWN_H
#define _INTERNAL_SPAWN_H
#include <spawn.h>

enum { FA_CLOSE, FA_DUP2, FA_OPEN, FA_CHDIR, FA_FCHDIR };

struct fa {
	struct fa *next;
	int op, fd, srcfd, oflag;
	mode_t mode;
	char path[];
};

#endif
