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

#ifndef _SYS_TYPES_H
#define _SYS_TYPES_H
#include <bits/features.h>

#define __NEED_blkcnt_t
#define __NEED_blksize_t
#define __NEED_clock_t
#define __NEED_clockid_t
#define __NEED_dev_t
#define __NEED_fsblkcnt_t
#define __NEED_fsfilcnt_t
#define __NEED_gid_t
#define __NEED_id_t
#define __NEED_ino_t
#define __NEED_key_t
#define __NEED_mode_t
#define __NEED_nlink_t
#define __NEED_off_t
#define __NEED_pid_t
#define __NEED_reclen_t
#define __NEED_size_t
#define __NEED_ssize_t
#define __NEED_suseconds_t
#define __NEED_time_t
#define __NEED_timer_t
#define __NEED_uid_t
#define __NEED_pthread_t
#define __NEED_pthread_attr_t
#define __NEED_pthread_barrier_t
#define __NEED_pthread_barrierattr_t
#define __NEED_pthread_cond_t
#define __NEED_pthread_condattr_t
#define __NEED_pthread_key_t
#define __NEED_pthread_mutex_t
#define __NEED_pthread_mutexattr_t
#define __NEED_pthread_once_t
#define __NEED_pthread_rwlock_t
#define __NEED_pthread_rwlockattr_t
#define __NEED_pthread_spinlock_t
#include <bits/alltypes.h>

#endif
