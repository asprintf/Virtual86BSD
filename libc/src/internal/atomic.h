/* Internal: atomic operations on plain ints/pointers (full barriers).
 * gcc/clang: __atomic builtins; other compilers: arch/x86_64/atomic.s. */
#ifndef _INTERNAL_ATOMIC_H
#define _INTERNAL_ATOMIC_H

#if defined(__GNUC__) || defined(__clang__)
static inline int a_cas(volatile int *p, int t, int s)
{ __atomic_compare_exchange_n(p, &t, s, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); return t; }
static inline void *a_cas_p(void *volatile *p, void *t, void *s)
{ __atomic_compare_exchange_n(p, &t, s, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); return t; }
static inline int a_swap(volatile int *p, int v)
{ return __atomic_exchange_n(p, v, __ATOMIC_SEQ_CST); }
static inline int a_fetch_add(volatile int *p, int v)
{ return __atomic_fetch_add(p, v, __ATOMIC_SEQ_CST); }
static inline void a_store(volatile int *p, int v)
{ __atomic_store_n(p, v, __ATOMIC_SEQ_CST); }
static inline void a_barrier(void) { __atomic_thread_fence(__ATOMIC_SEQ_CST); }
static inline void a_spin(void) { __asm__ __volatile__("pause" ::: "memory"); }
static inline void a_crash(void) { __builtin_trap(); }
#else
int __a_cas(volatile int *, int, int);
void *__a_cas_p(void *volatile *, void *, void *);
int __a_swap(volatile int *, int);
int __a_fetch_add(volatile int *, int);
void __a_store(volatile int *, int);
void __a_barrier(void);
void __a_spin(void);
void __a_crash(void);
#define a_cas __a_cas
#define a_cas_p __a_cas_p
#define a_swap __a_swap
#define a_fetch_add __a_fetch_add
#define a_store __a_store
#define a_barrier __a_barrier
#define a_spin __a_spin
#define a_crash __a_crash
#endif

static inline void a_inc(volatile int *p) { a_fetch_add(p, 1); }
static inline void a_dec(volatile int *p) { a_fetch_add(p, -1); }

#endif
