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

/* systm.h - the kernel's own functions: memory and string routines, printf,
   panic, assertions, lists, time. */
#ifndef _SYS_SYSTM_H_
#define _SYS_SYSTM_H_

#include <stdbool.h>
#include <stdarg.h>
#include <sys/param.h>

/* lib/libkern/libkern.c */
void *memset(void *d, int c, size_t n);
void *memcpy(void *d, const void *s, size_t n);
void *memmove(void *d, const void *s, size_t n);
int memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);
size_t strnlen(const char *s, size_t n);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
char *strcpy(char *d, const char *s);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
int vsnprintf(char *buf, size_t n, const char *fmt, va_list ap);
int snprintf(char *buf, size_t n, const char *fmt, ...);

/* kern/subr_prf.c */
void printf(const char *fmt, ...);
uint64_t tsc_frequency(void); /* kern/kern_time.c */

/* the system's identity: vers.c, made by conf/newvers.sh */
extern const char ostype[], osrelease[], osversion[], version[];

/* what sysctl(2) shows (kern/kern_sysctl.c and its owners) */
extern char cpu_model[64]; /* the processor's brand string, set by cpu_attach */
extern int maxproc, maxfiles, argmax; /* kern_proc.c, kern_descrip.c, kern_exec.c */
void loadavg_init(void);
NORETURN void panic(const char *fmt, ...);

/* assertions, on with "option DIAGNOSTIC" (-DDIAGNOSTIC); KDASSERT is the
   expensive kind, on with "option DEBUG" */
#ifdef DIAGNOSTIC
#define KASSERT(e) ((e) ? (void)0 : \
    panic("kernel diagnostic assertion \"%s\" failed: file \"%s\", line %d", #e, __FILE__, __LINE__))
#else
#define KASSERT(e) ((void)0)
#endif
#ifdef DEBUG
#define KDASSERT(e) KASSERT(e)
#else
#define KDASSERT(e) ((void)0)
#endif

/* intrusive doubly linked list */
struct list {
  struct list *next, *prev;
};
#define LIST_INIT(l) { &(l), &(l) }
static inline void list_init(struct list *l) { l->next = l->prev = l; }
static inline bool list_empty(const struct list *l) { return l->next == l; }
static inline void list_add_tail(struct list *l, struct list *e) {
  e->prev = l->prev;
  e->next = l;
  l->prev->next = e;
  l->prev = e;
}
static inline void list_add(struct list *l, struct list *e) {
  e->next = l->next;
  e->prev = l;
  l->next->prev = e;
  l->next = e;
}
static inline void list_del(struct list *e) {
  e->prev->next = e->next;
  e->next->prev = e->prev;
  e->next = e->prev = e;
}
#define list_entry(p, type, member) container_of(p, type, member)
#define list_for_each(p, l) for (struct list *p = (l)->next; p != (l); p = p->next)
#define list_for_each_safe(p, n, l) \
  for (struct list *p = (l)->next, *n = p->next; p != (l); p = n, n = p->next)

/* errno values (Linux) */
#include <sys/errno.h>

/* error-carrying pointers, as Linux */
#define ERR_PTR(e) ((void *)(intptr_t)(e))
#define PTR_ERR(p) ((long)(intptr_t)(p))
#define IS_ERR(p) ((uintptr_t)(p) >= (uintptr_t)-4095)

/* timekeeping: kern/kern_time.c */
#define HZ 1000
extern volatile uint64_t jiffies;
uint64_t monotonic_ns(void);
static inline uint64_t sat_add(uint64_t a, uint64_t b) { return a + b < a ? ~0UL : a + b; }
static inline uint64_t ts_to_ns(int64_t sec, int64_t nsec) {
  if ((uint64_t)sec >= 18000000000UL)
    return ~0UL;
  return (uint64_t)sec * 1000000000UL + (uint64_t)nsec;
}
uint64_t realtime_ns(void);
void time_init(void);
void set_realtime_ns(uint64_t ns);

#endif
