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

/* Named semaphores: files /dev/shm/sem.NAME holding a sem_t, mapped
 * shared.  A new one is made under a temporary name and linked into
 * place, so no process ever sees it uninitialised.  Opening a name again
 * returns the same address (POSIX), so the mappings are counted. */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <semaphore.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "lock.h"

#define MAXOPEN 256

static struct { dev_t dev; ino_t ino; sem_t *sem; int refs; } table[MAXOPEN];
static volatile int lock;

/* /dev/shm/sem.NAME, or 0 (errno set) */
static char *path(const char *name, char *buf)
{
	size_t n;
	while (*name == '/')
		name++;
	n = strlen(name);
	if (!n || strchr(name, '/') || !strcmp(name, ".") || !strcmp(name, "..")) {
		errno = EINVAL;
		return 0;
	}
	if (n > NAME_MAX - 4) {
		errno = ENAMETOOLONG;
		return 0;
	}
	memcpy(buf, "/dev/shm/sem.", 13);
	memcpy(buf + 13, name, n + 1);
	return buf;
}

/* map an open semaphore file, sharing an existing mapping */
static sem_t *map(int fd)
{
	struct stat st;
	sem_t *s;
	int i, slot = -1;
	if (fstat(fd, &st) < 0)
		return SEM_FAILED;
	LOCK(&lock);
	for (i = 0; i < MAXOPEN; i++) {
		if (table[i].refs && table[i].ino == st.st_ino && table[i].dev == st.st_dev) {
			table[i].refs++;
			UNLOCK(&lock);
			return table[i].sem;
		}
		if (!table[i].refs && slot < 0)
			slot = i;
	}
	if (slot < 0) {
		UNLOCK(&lock);
		errno = EMFILE;
		return SEM_FAILED;
	}
	s = mmap(0, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (s == MAP_FAILED) {
		UNLOCK(&lock);
		return SEM_FAILED;
	}
	table[slot].dev = st.st_dev;
	table[slot].ino = st.st_ino;
	table[slot].sem = s;
	table[slot].refs = 1;
	UNLOCK(&lock);
	return s;
}

sem_t *sem_open(const char *name, int flags, ...)
{
	char p[NAME_MAX + 16], tmp[64];
	mode_t mode = 0;
	unsigned value = 0;
	sem_t init, *s;
	int fd, tries;
	struct timespec ts;

	if (!path(name, p))
		return SEM_FAILED;
	if (flags & O_CREAT) {
		va_list ap;
		va_start(ap, flags);
		mode = va_arg(ap, mode_t);
		value = va_arg(ap, unsigned);
		va_end(ap);
		if (value > SEM_VALUE_MAX) {
			errno = EINVAL;
			return SEM_FAILED;
		}
	}
	for (tries = 0; tries < 100; tries++) {
		if ((flags & (O_CREAT | O_EXCL)) != (O_CREAT | O_EXCL)) {
			fd = open(p, O_RDWR | O_NOFOLLOW | O_CLOEXEC);
			if (fd >= 0) {
				s = map(fd);
				close(fd);
				return s;
			}
			if (errno != ENOENT || !(flags & O_CREAT))
				return SEM_FAILED;
		}
		/* create it under another name, then link it into place */
		clock_gettime(CLOCK_REALTIME, &ts);
		snprintf(tmp, sizeof tmp, "/dev/shm/tmp-sem.%ld.%ld.%d", (long)getpid(), ts.tv_nsec, tries);
		fd = open(tmp, O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, mode);
		if (fd < 0) {
			if (errno == EEXIST)
				continue;
			return SEM_FAILED;
		}
		sem_init(&init, 1, value);
		if (write(fd, &init, sizeof init) != sizeof init) {
			close(fd);
			unlink(tmp);
			return SEM_FAILED;
		}
		if (link(tmp, p) < 0) {
			int e = errno;
			close(fd);
			unlink(tmp);
			if (e == EEXIST && !(flags & O_EXCL))
				continue;
			errno = e;
			return SEM_FAILED;
		}
		unlink(tmp);
		s = map(fd);
		close(fd);
		return s;
	}
	errno = EAGAIN;
	return SEM_FAILED;
}

int sem_close(sem_t *s)
{
	int i;
	LOCK(&lock);
	for (i = 0; i < MAXOPEN; i++)
		if (table[i].refs && table[i].sem == s) {
			if (!--table[i].refs)
				munmap(s, sizeof(sem_t));
			UNLOCK(&lock);
			return 0;
		}
	UNLOCK(&lock);
	errno = EINVAL;
	return -1;
}

int sem_unlink(const char *name)
{
	char p[NAME_MAX + 16];
	if (!path(name, p))
		return -1;
	if (unlink(p) == 0)
		return 0;
	if (errno == EPERM)
		errno = EACCES;     /* the sticky /dev/shm: POSIX says EACCES */
	return -1;
}
