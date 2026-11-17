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

/* Internal: open() flags, shared by <fcntl.h>, <unistd.h> (O_CLOEXEC,
 * O_CLOFORK) and <stdlib.h> (several). FreeBSD's values. */
#ifndef _BITS_FCNTL_H
#define _BITS_FCNTL_H
#define O_RDONLY    0x0000
#define O_WRONLY    0x0001
#define O_RDWR      0x0002
#define O_EXEC      0x00040000
#define O_SEARCH    O_EXEC
#define O_PATH      0x00400000
#define O_ACCMODE   (0x0003 | O_EXEC)
#define O_NONBLOCK  0x0004
#define O_APPEND    0x0008
#define O_SYNC      0x0080
#define O_RSYNC     O_SYNC
#define O_DSYNC     0x01000000
#define O_NOFOLLOW  0x0100
#define O_CREAT     0x0200
#define O_TRUNC     0x0400
#define O_EXCL      0x0800
#define O_NOCTTY    0x8000
#define O_DIRECTORY 0x00020000
#define O_TTY_INIT  0x00080000
#define O_CLOEXEC   0x00100000
#define O_CLOFORK   0x08000000
#endif
