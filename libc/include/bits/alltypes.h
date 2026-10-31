/* Internal: single point of definition for types shared between headers.
 *
 * A header defines __NEED_<type> for each type it must expose and then
 * includes this file; each type is defined at most once per translation
 * unit regardless of how many headers request it.  Layout is x86-64 LP64.
 *
 * This file intentionally has no include guard.
 */

#if defined(__NEED_size_t) && !defined(__DEFINED_size_t)
typedef unsigned long size_t;
#define __DEFINED_size_t
#endif
#if defined(__NEED_ssize_t) && !defined(__DEFINED_ssize_t)
typedef long ssize_t;
#define __DEFINED_ssize_t
#endif
#if defined(__NEED_ptrdiff_t) && !defined(__DEFINED_ptrdiff_t)
typedef long ptrdiff_t;
#define __DEFINED_ptrdiff_t
#endif
#if defined(__NEED_wchar_t) && !defined(__DEFINED_wchar_t) && !defined(__cplusplus)
typedef int wchar_t;
#define __DEFINED_wchar_t
#endif
#if defined(__NEED_wint_t) && !defined(__DEFINED_wint_t)
typedef unsigned int wint_t;
#define __DEFINED_wint_t
#endif
#if defined(__NEED_wctype_t) && !defined(__DEFINED_wctype_t)
typedef unsigned long wctype_t;
#define __DEFINED_wctype_t
#endif
#if defined(__NEED_wctrans_t) && !defined(__DEFINED_wctrans_t)
typedef unsigned long wctrans_t;
#define __DEFINED_wctrans_t
#endif
#if defined(__NEED_mbstate_t) && !defined(__DEFINED_mbstate_t)
typedef struct __mbstate { unsigned __st[2]; } mbstate_t;
#define __DEFINED_mbstate_t
#endif
#if defined(__NEED_FILE) && !defined(__DEFINED_FILE)
typedef struct __FILE FILE;
#define __DEFINED_FILE
#endif

#if defined(__NEED_va_list) && !defined(__DEFINED_va_list)
# if defined(__GNUC__) || defined(__clang__) || defined(__c17__)
typedef __builtin_va_list va_list;
# else
#  include <stdarg.h>  /* compiler-provided; defines va_list */
# endif
#define __DEFINED_va_list
#endif
#if defined(__NEED___isoc_va_list) && !defined(__DEFINED___isoc_va_list)
# if defined(__GNUC__) || defined(__clang__) || defined(__c17__)
typedef __builtin_va_list __isoc_va_list;
# else
#  include <stdarg.h>
typedef va_list __isoc_va_list;
# endif
#define __DEFINED___isoc_va_list
#endif

/* Exact-width integer types (also used by <stdint.h>). */
#if defined(__NEED_int8_t) && !defined(__DEFINED_int8_t)
typedef signed char int8_t;
#define __DEFINED_int8_t
#endif
#if defined(__NEED_int16_t) && !defined(__DEFINED_int16_t)
typedef short int16_t;
#define __DEFINED_int16_t
#endif
#if defined(__NEED_int32_t) && !defined(__DEFINED_int32_t)
typedef int int32_t;
#define __DEFINED_int32_t
#endif
#if defined(__NEED_int64_t) && !defined(__DEFINED_int64_t)
typedef long int64_t;
#define __DEFINED_int64_t
#endif
#if defined(__NEED_uint8_t) && !defined(__DEFINED_uint8_t)
typedef unsigned char uint8_t;
#define __DEFINED_uint8_t
#endif
#if defined(__NEED_uint16_t) && !defined(__DEFINED_uint16_t)
typedef unsigned short uint16_t;
#define __DEFINED_uint16_t
#endif
#if defined(__NEED_uint32_t) && !defined(__DEFINED_uint32_t)
typedef unsigned int uint32_t;
#define __DEFINED_uint32_t
#endif
#if defined(__NEED_uint64_t) && !defined(__DEFINED_uint64_t)
typedef unsigned long uint64_t;
#define __DEFINED_uint64_t
#endif
#if defined(__NEED_intptr_t) && !defined(__DEFINED_intptr_t)
typedef long intptr_t;
#define __DEFINED_intptr_t
#endif
#if defined(__NEED_uintptr_t) && !defined(__DEFINED_uintptr_t)
typedef unsigned long uintptr_t;
#define __DEFINED_uintptr_t
#endif
#if defined(__NEED_intmax_t) && !defined(__DEFINED_intmax_t)
typedef long intmax_t;
#define __DEFINED_intmax_t
#endif
#if defined(__NEED_uintmax_t) && !defined(__DEFINED_uintmax_t)
typedef unsigned long uintmax_t;
#define __DEFINED_uintmax_t
#endif

/* <sys/types.h> family. */
#if defined(__NEED_off_t) && !defined(__DEFINED_off_t)
typedef long off_t;
#define __DEFINED_off_t
#endif
#if defined(__NEED_pid_t) && !defined(__DEFINED_pid_t)
typedef int pid_t;
#define __DEFINED_pid_t
#endif
#if defined(__NEED_uid_t) && !defined(__DEFINED_uid_t)
typedef unsigned int uid_t;
#define __DEFINED_uid_t
#endif
#if defined(__NEED_gid_t) && !defined(__DEFINED_gid_t)
typedef unsigned int gid_t;
#define __DEFINED_gid_t
#endif
#if defined(__NEED_id_t) && !defined(__DEFINED_id_t)
typedef unsigned int id_t;
#define __DEFINED_id_t
#endif
#if defined(__NEED_mode_t) && !defined(__DEFINED_mode_t)
typedef unsigned int mode_t;
#define __DEFINED_mode_t
#endif
#if defined(__NEED_dev_t) && !defined(__DEFINED_dev_t)
typedef unsigned long dev_t;
#define __DEFINED_dev_t
#endif
#if defined(__NEED_ino_t) && !defined(__DEFINED_ino_t)
typedef unsigned long ino_t;
#define __DEFINED_ino_t
#endif
#if defined(__NEED_nlink_t) && !defined(__DEFINED_nlink_t)
typedef unsigned long nlink_t;
#define __DEFINED_nlink_t
#endif
#if defined(__NEED_blksize_t) && !defined(__DEFINED_blksize_t)
typedef long blksize_t;
#define __DEFINED_blksize_t
#endif
#if defined(__NEED_blkcnt_t) && !defined(__DEFINED_blkcnt_t)
typedef long blkcnt_t;
#define __DEFINED_blkcnt_t
#endif
#if defined(__NEED_fsblkcnt_t) && !defined(__DEFINED_fsblkcnt_t)
typedef unsigned long fsblkcnt_t;
#define __DEFINED_fsblkcnt_t
#endif
#if defined(__NEED_fsfilcnt_t) && !defined(__DEFINED_fsfilcnt_t)
typedef unsigned long fsfilcnt_t;
#define __DEFINED_fsfilcnt_t
#endif
#if defined(__NEED_reclen_t) && !defined(__DEFINED_reclen_t)
typedef unsigned short reclen_t;
#define __DEFINED_reclen_t
#endif
#if defined(__NEED_key_t) && !defined(__DEFINED_key_t)
typedef int key_t;
#define __DEFINED_key_t
#endif
#if defined(__NEED_useconds_t) && !defined(__DEFINED_useconds_t)
typedef unsigned int useconds_t;
#define __DEFINED_useconds_t
#endif

/* Time. */
#if defined(__NEED_time_t) && !defined(__DEFINED_time_t)
typedef long time_t;
#define __DEFINED_time_t
#endif
#if defined(__NEED_suseconds_t) && !defined(__DEFINED_suseconds_t)
typedef long suseconds_t;
#define __DEFINED_suseconds_t
#endif
#if defined(__NEED_clock_t) && !defined(__DEFINED_clock_t)
typedef long clock_t;
#define __DEFINED_clock_t
#endif
#if defined(__NEED_clockid_t) && !defined(__DEFINED_clockid_t)
typedef int clockid_t;
#define __DEFINED_clockid_t
#endif
#if defined(__NEED_timer_t) && !defined(__DEFINED_timer_t)
typedef void *timer_t;
#define __DEFINED_timer_t
#endif
#if defined(__NEED_struct_winsize) && !defined(__DEFINED_struct_winsize)
struct winsize { unsigned short ws_row, ws_col, ws_xpixel, ws_ypixel; };
#define __DEFINED_struct_winsize
#endif
#if defined(__NEED_struct_iovec) && !defined(__DEFINED_struct_iovec)
struct iovec { void *iov_base; unsigned long iov_len; };
#define __DEFINED_struct_iovec
#endif
#if defined(__NEED_socklen_t) && !defined(__DEFINED_socklen_t)
typedef unsigned int socklen_t;
#define __DEFINED_socklen_t
#endif
#if defined(__NEED_sa_family_t) && !defined(__DEFINED_sa_family_t)
typedef unsigned short sa_family_t;
#define __DEFINED_sa_family_t
#endif
#if defined(__NEED_in_port_t) && !defined(__DEFINED_in_port_t)
typedef unsigned short in_port_t;
#define __DEFINED_in_port_t
#endif
#if defined(__NEED_in_addr_t) && !defined(__DEFINED_in_addr_t)
typedef unsigned int in_addr_t;
#define __DEFINED_in_addr_t
#endif
#if defined(__NEED_struct_in_addr) && !defined(__DEFINED_struct_in_addr)
struct in_addr { unsigned int s_addr; };
#define __DEFINED_struct_in_addr
#endif
#if defined(__NEED_struct_timespec) && !defined(__DEFINED_struct_timespec)
struct timespec { long tv_sec; long tv_nsec; };
#define __DEFINED_struct_timespec
#endif
#if defined(__NEED_struct_timeval) && !defined(__DEFINED_struct_timeval)
struct timeval { long tv_sec; long tv_usec; };
#define __DEFINED_struct_timeval
#endif

/* Locales. */
#if defined(__NEED_locale_t) && !defined(__DEFINED_locale_t)
typedef struct __locale_struct *locale_t;
#define __DEFINED_locale_t
#endif

/* Signals. 1024-bit set, as glibc; the kernel uses the first 64 bits. */
#if defined(__NEED_sigset_t) && !defined(__DEFINED_sigset_t)
typedef struct { unsigned long __bits[16]; } sigset_t;
#define __DEFINED_sigset_t
#endif

/* Threads.  Opaque objects; sizes follow the x86-64 glibc ABI so that
 * structures embedding them keep familiar layouts. */
#if defined(__NEED_pthread_t) && !defined(__DEFINED_pthread_t)
typedef struct __pthread *pthread_t;
#define __DEFINED_pthread_t
#endif
#if defined(__NEED_pthread_attr_t) && !defined(__DEFINED_pthread_attr_t)
typedef union { long __l[7]; void *__p[7]; } pthread_attr_t;
#define __DEFINED_pthread_attr_t
#endif
#if defined(__NEED_pthread_mutex_t) && !defined(__DEFINED_pthread_mutex_t)
typedef union { int __i[10]; volatile int __vi[10]; void *__p[5]; } pthread_mutex_t;
#define __DEFINED_pthread_mutex_t
#endif
#if defined(__NEED_pthread_mutexattr_t) && !defined(__DEFINED_pthread_mutexattr_t)
typedef struct { unsigned int __attr; } pthread_mutexattr_t;
#define __DEFINED_pthread_mutexattr_t
#endif
#if defined(__NEED_pthread_cond_t) && !defined(__DEFINED_pthread_cond_t)
typedef union { int __i[12]; volatile int __vi[12]; void *__p[6]; } pthread_cond_t;
#define __DEFINED_pthread_cond_t
#endif
#if defined(__NEED_pthread_condattr_t) && !defined(__DEFINED_pthread_condattr_t)
typedef struct { unsigned int __attr; } pthread_condattr_t;
#define __DEFINED_pthread_condattr_t
#endif
#if defined(__NEED_pthread_rwlock_t) && !defined(__DEFINED_pthread_rwlock_t)
typedef union { int __i[14]; volatile int __vi[14]; void *__p[7]; } pthread_rwlock_t;
#define __DEFINED_pthread_rwlock_t
#endif
#if defined(__NEED_pthread_rwlockattr_t) && !defined(__DEFINED_pthread_rwlockattr_t)
typedef struct { unsigned int __attr[2]; } pthread_rwlockattr_t;
#define __DEFINED_pthread_rwlockattr_t
#endif
#if defined(__NEED_pthread_barrier_t) && !defined(__DEFINED_pthread_barrier_t)
typedef union { int __i[8]; volatile int __vi[8]; void *__p[4]; } pthread_barrier_t;
#define __DEFINED_pthread_barrier_t
#endif
#if defined(__NEED_pthread_barrierattr_t) && !defined(__DEFINED_pthread_barrierattr_t)
typedef struct { unsigned int __attr; } pthread_barrierattr_t;
#define __DEFINED_pthread_barrierattr_t
#endif
#if defined(__NEED_pthread_spinlock_t) && !defined(__DEFINED_pthread_spinlock_t)
typedef volatile int pthread_spinlock_t;
#define __DEFINED_pthread_spinlock_t
#endif
#if defined(__NEED_pthread_key_t) && !defined(__DEFINED_pthread_key_t)
typedef unsigned int pthread_key_t;
#define __DEFINED_pthread_key_t
#endif
#if defined(__NEED_pthread_once_t) && !defined(__DEFINED_pthread_once_t)
typedef int pthread_once_t;
#define __DEFINED_pthread_once_t
#endif
