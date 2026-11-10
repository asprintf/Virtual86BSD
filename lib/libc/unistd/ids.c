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

/* User and group IDs.  The set*id calls act on every thread
 * (src/thread/__setxid.c). */
#include <unistd.h>
#include "syscall.h"

long __setxid(long, long, long, long);

uid_t getuid(void) { return (uid_t)__syscall0(SYS_getuid); }
uid_t geteuid(void) { return (uid_t)__syscall0(SYS_geteuid); }
gid_t getgid(void) { return (gid_t)__syscall0(SYS_getgid); }
gid_t getegid(void) { return (gid_t)__syscall0(SYS_getegid); }
int getgroups(int n, gid_t list[]) { return (int)__sys2(SYS_getgroups, n, list); }
int setuid(uid_t u) { return (int)__setxid(SYS_setuid, (long)u, (long)0, (long)0); }
int setgid(gid_t g) { return (int)__setxid(SYS_setgid, (long)g, (long)0, (long)0); }
int seteuid(uid_t u) { return (int)__setxid(SYS_setresuid, (long)-1, (long)u, (long)-1); }
int setegid(gid_t g) { return (int)__setxid(SYS_setresgid, (long)-1, (long)g, (long)-1); }
int setreuid(uid_t r, uid_t e) { return (int)__setxid(SYS_setreuid, (long)r, (long)e, (long)0); }
int setregid(gid_t r, gid_t e) { return (int)__setxid(SYS_setregid, (long)r, (long)e, (long)0); }
int setresuid(uid_t r, uid_t e, uid_t s) { return (int)__setxid(SYS_setresuid, (long)r, (long)e, (long)s); }
int setresgid(gid_t r, gid_t e, gid_t s) { return (int)__setxid(SYS_setresgid, (long)r, (long)e, (long)s); }
int getresuid(uid_t *__RESTRICT r, uid_t *__RESTRICT e, uid_t *__RESTRICT s) { return (int)__sys3(SYS_getresuid, r, e, s); }
int getresgid(gid_t *__RESTRICT r, gid_t *__RESTRICT e, gid_t *__RESTRICT s) { return (int)__sys3(SYS_getresgid, r, e, s); }
