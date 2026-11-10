/*
 * Copyright (c) 2026, Joost Orchidee Winter
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
