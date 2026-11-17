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

/* mount.h - file systems and mounts: FreeBSD's struct statfs, the mount
 * flags of nmount(2), and the calls. */
#ifndef _SYS_MOUNT_H_
#define _SYS_MOUNT_H_
#include <bits/features.h>
#define __NEED_uid_t
#define __NEED_int32_t
#define __NEED_uint32_t
#define __NEED_int64_t
#define __NEED_uint64_t
#define __NEED_struct_iovec
#include <bits/alltypes.h>

typedef struct fsid { int32_t val[2]; } fsid_t;

#define MFSNAMELEN 16
#define MNAMELEN   1024
#define STATFS_VERSION 0x20140518

struct statfs {
	uint32_t f_version;
	uint32_t f_type;
	uint64_t f_flags;
	uint64_t f_bsize;
	uint64_t f_iosize;
	uint64_t f_blocks;
	uint64_t f_bfree;
	int64_t  f_bavail;
	uint64_t f_files;
	int64_t  f_ffree;
	uint64_t f_syncwrites;
	uint64_t f_asyncwrites;
	uint64_t f_syncreads;
	uint64_t f_asyncreads;
	uint32_t f_nvnodelistsize;
	uint32_t f_spare0;
	uint64_t f_spare[9];
	uint32_t f_namemax;
	uid_t    f_owner;
	fsid_t   f_fsid;
	char     f_charspare[80];
	char     f_fstypename[MFSNAMELEN];
	char     f_mntfromname[MNAMELEN];
	char     f_mntonname[MNAMELEN];
};

#define MNT_RDONLY      0x0000000000000001ULL
#define MNT_SYNCHRONOUS 0x0000000000000002ULL
#define MNT_NOEXEC      0x0000000000000004ULL
#define MNT_NOSUID      0x0000000000000008ULL
#define MNT_UNION       0x0000000000000020ULL
#define MNT_ASYNC       0x0000000000000040ULL
#define MNT_NOATIME     0x0000000010000000ULL
#define MNT_UPDATE      0x0000000000010000ULL
#define MNT_RELOAD      0x0000000000040000ULL
#define MNT_FORCE       0x0000000000080000ULL
#define MNT_BYFSID      0x0000000008000000ULL
#define MNT_WAIT        1
#define MNT_NOWAIT      2

__BEGIN_DECLS
int fstatfs(int, struct statfs *);
int getfsstat(struct statfs *, long, int);
int mount(const char *, const char *, int, void *);
int nmount(struct iovec *, unsigned int, int);
int statfs(const char *, struct statfs *);
int unmount(const char *, int);
__END_DECLS
#endif
