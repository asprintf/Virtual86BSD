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

#ifndef _SYS_MMAN_H
#define _SYS_MMAN_H
#include <bits/features.h>
#include <bits/fcntl.h>
#define __NEED_mode_t
#define __NEED_off_t
#define __NEED_size_t
#include <bits/alltypes.h>

#define PROT_NONE  0
#define PROT_READ  1
#define PROT_WRITE 2
#define PROT_EXEC  4

#define MAP_SHARED    0x01
#define MAP_PRIVATE   0x02
#define MAP_FIXED     0x10
#define MAP_ANONYMOUS 0x20
#define MAP_ANON      MAP_ANONYMOUS
#define MAP_FAILED    ((void *)-1)

#define MS_ASYNC      1
#define MS_INVALIDATE 2
#define MS_SYNC       4

#define MCL_CURRENT 1
#define MCL_FUTURE  2

#define POSIX_MADV_NORMAL     0
#define POSIX_MADV_RANDOM     1
#define POSIX_MADV_SEQUENTIAL 2
#define POSIX_MADV_WILLNEED   3
#define POSIX_MADV_DONTNEED   4

/* [TYM] typed memory objects, where the system has them (sysconf) */
#define POSIX_TYPED_MEM_ALLOCATE        0x01
#define POSIX_TYPED_MEM_ALLOCATE_CONTIG 0x02
#define POSIX_TYPED_MEM_MAP_ALLOCATABLE 0x04

struct posix_typed_mem_info {
	size_t posix_tmi_length;
};

__BEGIN_DECLS
int   mlock(const void *, size_t);
int   mlockall(int);
void *mmap(void *, size_t, int, int, int, off_t);
int   mprotect(void *, size_t, int);
int   msync(void *, size_t, int);
int   munlock(const void *, size_t);
int   munlockall(void);
int   munmap(void *, size_t);
int   posix_madvise(void *, size_t, int);
int   posix_mem_offset(const void *__RESTRICT, size_t, off_t *__RESTRICT, size_t *__RESTRICT, int *__RESTRICT);
int   posix_typed_mem_get_info(int, struct posix_typed_mem_info *);
int   posix_typed_mem_open(const char *, int, int);
int   shm_open(const char *, int, mode_t);
int   shm_unlink(const char *);
__END_DECLS
#endif
