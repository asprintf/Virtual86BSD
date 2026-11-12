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

/* param.h - system parameters and helper macros. */
#ifndef _SYS_PARAM_H_
#define _SYS_PARAM_H_

#include <sys/types.h>
#include <machine/vmparam.h>
#ifdef _KERNEL
#include <options.h> /* the options of the configuration (config(8)) */
#endif

#define PAGE_SIZE 4096UL
#define PAGE_MASK (~(PAGE_SIZE - 1))
#define PAGE_UP(x) (((x) + PAGE_SIZE - 1) & PAGE_MASK)
#define PAGE_DOWN(x) ((x) & PAGE_MASK)

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define container_of(p, type, member) ((type *)((char *)(p) - offsetof(type, member)))

#define NORETURN _Noreturn

/* the BSD block sizes and helper macros (as the program's <sys/param.h>) */
#define NBBY       8                    /* bits per byte */
#define DEV_BSHIFT 9                    /* log2(DEV_BSIZE) */
#define DEV_BSIZE  (1 << DEV_BSHIFT)    /* the unit of disk addresses */
#define MAXBSIZE   65536                /* the largest file system block */
#define btodb(bytes) ((bytes) >> DEV_BSHIFT)
#define dbtob(db)    ((db) << DEV_BSHIFT)
#define setbit(a, i) ((a)[(i) / NBBY] |= 1 << ((i) % NBBY))
#define clrbit(a, i) ((a)[(i) / NBBY] &= ~(1 << ((i) % NBBY)))
#define isset(a, i)  ((a)[(i) / NBBY] & (1 << ((i) % NBBY)))
#define isclr(a, i)  (((a)[(i) / NBBY] & (1 << ((i) % NBBY))) == 0)
#define howmany(x, y)   (((x) + ((y) - 1)) / (y))
#define roundup(x, y)   ((((x) + ((y) - 1)) / (y)) * (y))
#define rounddown(x, y) (((x) / (y)) * (y))
#define powerof2(x)     ((((x) - 1) & (x)) == 0)

#endif
