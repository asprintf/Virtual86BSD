#ifndef _STDATOMIC_H
#define _STDATOMIC_H

/* C11/C17 atomics on clang's __c11_atomic builtins; no other header is included. */

typedef enum memory_order {
	memory_order_relaxed = __ATOMIC_RELAXED,
	memory_order_consume = __ATOMIC_CONSUME,
	memory_order_acquire = __ATOMIC_ACQUIRE,
	memory_order_release = __ATOMIC_RELEASE,
	memory_order_acq_rel = __ATOMIC_ACQ_REL,
	memory_order_seq_cst = __ATOMIC_SEQ_CST
} memory_order;

typedef struct atomic_flag { _Atomic(_Bool) __v; } atomic_flag;
#define ATOMIC_FLAG_INIT { 0 }
#define ATOMIC_VAR_INIT(v) (v)

#define ATOMIC_BOOL_LOCK_FREE     __CLANG_ATOMIC_BOOL_LOCK_FREE
#define ATOMIC_CHAR_LOCK_FREE     __CLANG_ATOMIC_CHAR_LOCK_FREE
#define ATOMIC_CHAR16_T_LOCK_FREE __CLANG_ATOMIC_CHAR16_T_LOCK_FREE
#define ATOMIC_CHAR32_T_LOCK_FREE __CLANG_ATOMIC_CHAR32_T_LOCK_FREE
#define ATOMIC_WCHAR_T_LOCK_FREE  __CLANG_ATOMIC_WCHAR_T_LOCK_FREE
#define ATOMIC_SHORT_LOCK_FREE    __CLANG_ATOMIC_SHORT_LOCK_FREE
#define ATOMIC_INT_LOCK_FREE      __CLANG_ATOMIC_INT_LOCK_FREE
#define ATOMIC_LONG_LOCK_FREE     __CLANG_ATOMIC_LONG_LOCK_FREE
#define ATOMIC_LLONG_LOCK_FREE    __CLANG_ATOMIC_LLONG_LOCK_FREE
#define ATOMIC_POINTER_LOCK_FREE  __CLANG_ATOMIC_POINTER_LOCK_FREE

typedef _Atomic(_Bool)               atomic_bool;
typedef _Atomic(char)                atomic_char;
typedef _Atomic(signed char)         atomic_schar;
typedef _Atomic(unsigned char)       atomic_uchar;
typedef _Atomic(short)               atomic_short;
typedef _Atomic(unsigned short)      atomic_ushort;
typedef _Atomic(int)                 atomic_int;
typedef _Atomic(unsigned int)        atomic_uint;
typedef _Atomic(long)                atomic_long;
typedef _Atomic(unsigned long)       atomic_ulong;
typedef _Atomic(long long)           atomic_llong;
typedef _Atomic(unsigned long long)  atomic_ullong;
typedef _Atomic(__CHAR16_TYPE__)     atomic_char16_t;
typedef _Atomic(__CHAR32_TYPE__)     atomic_char32_t;
typedef _Atomic(__WCHAR_TYPE__)      atomic_wchar_t;
typedef _Atomic(__INT_LEAST8_TYPE__)   atomic_int_least8_t;
typedef _Atomic(__UINT_LEAST8_TYPE__)  atomic_uint_least8_t;
typedef _Atomic(__INT_LEAST16_TYPE__)  atomic_int_least16_t;
typedef _Atomic(__UINT_LEAST16_TYPE__) atomic_uint_least16_t;
typedef _Atomic(__INT_LEAST32_TYPE__)  atomic_int_least32_t;
typedef _Atomic(__UINT_LEAST32_TYPE__) atomic_uint_least32_t;
typedef _Atomic(__INT_LEAST64_TYPE__)  atomic_int_least64_t;
typedef _Atomic(__UINT_LEAST64_TYPE__) atomic_uint_least64_t;
typedef _Atomic(__INT_FAST8_TYPE__)    atomic_int_fast8_t;
typedef _Atomic(__UINT_FAST8_TYPE__)   atomic_uint_fast8_t;
typedef _Atomic(__INT_FAST16_TYPE__)   atomic_int_fast16_t;
typedef _Atomic(__UINT_FAST16_TYPE__)  atomic_uint_fast16_t;
typedef _Atomic(__INT_FAST32_TYPE__)   atomic_int_fast32_t;
typedef _Atomic(__UINT_FAST32_TYPE__)  atomic_uint_fast32_t;
typedef _Atomic(__INT_FAST64_TYPE__)   atomic_int_fast64_t;
typedef _Atomic(__UINT_FAST64_TYPE__)  atomic_uint_fast64_t;
typedef _Atomic(__INTPTR_TYPE__)     atomic_intptr_t;
typedef _Atomic(__UINTPTR_TYPE__)    atomic_uintptr_t;
typedef _Atomic(__SIZE_TYPE__)       atomic_size_t;
typedef _Atomic(__PTRDIFF_TYPE__)    atomic_ptrdiff_t;
typedef _Atomic(__INTMAX_TYPE__)     atomic_intmax_t;
typedef _Atomic(__UINTMAX_TYPE__)    atomic_uintmax_t;

void atomic_thread_fence(memory_order);
void atomic_signal_fence(memory_order);

#define kill_dependency(y) (y)
#define atomic_init(o, v) __c11_atomic_init(o, v)
#define atomic_thread_fence(m) __c11_atomic_thread_fence(m)
#define atomic_signal_fence(m) __c11_atomic_signal_fence(m)
#define atomic_is_lock_free(o) __c11_atomic_is_lock_free(sizeof(*(o)))

#define atomic_store_explicit(o, v, m) __c11_atomic_store(o, v, m)
#define atomic_load_explicit(o, m) __c11_atomic_load(o, m)
#define atomic_exchange_explicit(o, v, m) __c11_atomic_exchange(o, v, m)
#define atomic_compare_exchange_strong_explicit(o, e, d, s, f) __c11_atomic_compare_exchange_strong(o, e, d, s, f)
#define atomic_compare_exchange_weak_explicit(o, e, d, s, f) __c11_atomic_compare_exchange_weak(o, e, d, s, f)
#define atomic_fetch_add_explicit(o, v, m) __c11_atomic_fetch_add(o, v, m)
#define atomic_fetch_sub_explicit(o, v, m) __c11_atomic_fetch_sub(o, v, m)
#define atomic_fetch_or_explicit(o, v, m) __c11_atomic_fetch_or(o, v, m)
#define atomic_fetch_xor_explicit(o, v, m) __c11_atomic_fetch_xor(o, v, m)
#define atomic_fetch_and_explicit(o, v, m) __c11_atomic_fetch_and(o, v, m)

#define atomic_store(o, v) atomic_store_explicit(o, v, memory_order_seq_cst)
#define atomic_load(o) atomic_load_explicit(o, memory_order_seq_cst)
#define atomic_exchange(o, v) atomic_exchange_explicit(o, v, memory_order_seq_cst)
#define atomic_compare_exchange_strong(o, e, d) \
	atomic_compare_exchange_strong_explicit(o, e, d, memory_order_seq_cst, memory_order_seq_cst)
#define atomic_compare_exchange_weak(o, e, d) \
	atomic_compare_exchange_weak_explicit(o, e, d, memory_order_seq_cst, memory_order_seq_cst)
#define atomic_fetch_add(o, v) atomic_fetch_add_explicit(o, v, memory_order_seq_cst)
#define atomic_fetch_sub(o, v) atomic_fetch_sub_explicit(o, v, memory_order_seq_cst)
#define atomic_fetch_or(o, v) atomic_fetch_or_explicit(o, v, memory_order_seq_cst)
#define atomic_fetch_xor(o, v) atomic_fetch_xor_explicit(o, v, memory_order_seq_cst)
#define atomic_fetch_and(o, v) atomic_fetch_and_explicit(o, v, memory_order_seq_cst)

_Bool atomic_flag_test_and_set(volatile atomic_flag *);
_Bool atomic_flag_test_and_set_explicit(volatile atomic_flag *, memory_order);
void atomic_flag_clear(volatile atomic_flag *);
void atomic_flag_clear_explicit(volatile atomic_flag *, memory_order);

#endif
