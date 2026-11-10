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

/* Internal: close-on-fork (FD_CLOFORK, O_CLOFORK).
 *
 * V86BSD's kernel has it: the flags go to the kernel as they are (open,
 * pipe2, dup3, socket, accept4, recvmsg take them; F_GETFD/F_SETFD carry
 * FD_CLOFORK; F_DUPFD_CLOFORK exists), and the kernel closes the
 * descriptors in a new process.  __clofork_native() finds out, once.
 *
 * Linux lacks it: then it is emulated.  A bitmap records the descriptors
 * that have it; fork() closes them in the child.  Descriptors only become
 * flagged through the library, and close()/dup2()/dup3() clear the flag
 * of the descriptor they free or replace, so a reused descriptor number
 * starts clear. */
#ifndef _INTERNAL_CLOFORK_H
#define _INTERNAL_CLOFORK_H
extern volatile int __clofork_any;
int  __clofork_native(void);          /* 1 if the kernel has close-on-fork */
int  __clofork_set(int fd, int on);   /* 0, or -1 (ENOMEM) */
int  __clofork_get(int fd);
void __clofork_close_all(void);       /* in the child after fork */
#endif
