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

/* Typed memory objects ([TYM]): the kernel has none (sysconf(_SC_TYPED_MEMORY_OBJECTS)
 * says -1), so no name is a typed memory object and no address is in one. */
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "syscall.h"

int posix_typed_mem_open(const char *name, int oflag, int tflag)
{
	(void)name; (void)oflag; (void)tflag;
	errno = ENOENT;
	return -1;
}

int posix_typed_mem_get_info(int fd, struct posix_typed_mem_info *info)
{
	(void)info;
	return fcntl(fd, F_GETFD) < 0 ? EBADF : ENODEV;
}

int posix_mem_offset(const void *__RESTRICT addr, size_t len, off_t *__RESTRICT off,
                     size_t *__RESTRICT contig_len, int *__RESTRICT fildes)
{
	(void)addr; (void)len; (void)off; (void)contig_len; (void)fildes;
	return EACCES;     /* no typed memory here: addr cannot be any */
}
