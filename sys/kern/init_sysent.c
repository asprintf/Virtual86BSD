/* syscall.c - the system call table, by Linux x86-64 number. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <machine/cpu.h>
#include <sys/syscallargs.h>

typedef long (*sysfn)(long, long, long, long, long, long);
#define S(n, f) [n] = (sysfn)f

static long sys_ni(void) { return -ENOSYS; }

static const sysfn table[460] = {
    S(0, sys_read), S(1, sys_write), S(2, sys_open), S(3, sys_close), S(4, sys_stat),
    S(5, sys_fstat), S(6, sys_lstat), S(7, sys_poll), S(8, sys_lseek), S(9, sys_mmap),
    S(10, sys_mprotect), S(11, sys_munmap), S(12, sys_brk), S(13, sys_rt_sigaction),
    S(14, sys_rt_sigprocmask), S(15, sys_rt_sigreturn), S(16, sys_ioctl), S(17, sys_pread64),
    S(18, sys_pwrite64), S(19, sys_readv), S(20, sys_writev), S(21, sys_access), S(22, sys_pipe),
    S(23, sys_select), S(24, sys_sched_yield), S(25, sys_mremap), S(26, sys_msync),
    S(27, sys_mincore), S(28, sys_madvise), S(29, sys_shmget), S(30, sys_shmat), S(31, sys_shmctl), S(32, sys_dup), S(33, sys_dup2), S(34, sys_pause),
    S(35, sys_nanosleep), S(36, sys_getitimer), S(37, sys_alarm), S(38, sys_setitimer),
    S(39, sys_getpid), S(41, sys_socket), S(42, sys_connect), S(43, sys_accept),
    S(44, sys_sendto), S(45, sys_recvfrom), S(46, sys_sendmsg), S(47, sys_recvmsg),
    S(48, sys_shutdown), S(49, sys_bind), S(50, sys_listen), S(51, sys_getsockname),
    S(52, sys_getpeername), S(53, sys_socketpair), S(54, sys_setsockopt), S(55, sys_getsockopt), S(56, sys_clone), S(57, sys_fork), S(58, sys_vfork), S(59, sys_execve),
    S(60, sys_exit), S(61, sys_wait4), S(62, sys_kill), S(63, sys_uname), S(64, sys_semget), S(65, sys_semop), S(66, sys_semctl),
    S(67, sys_shmdt), S(68, sys_msgget), S(69, sys_msgsnd), S(70, sys_msgrcv), S(71, sys_msgctl), S(72, sys_fcntl),
    S(73, sys_flock), S(74, sys_fsync), S(75, sys_fdatasync), S(76, sys_truncate),
    S(77, sys_ftruncate), S(79, sys_getcwd), S(80, sys_chdir), S(81, sys_fchdir),
    S(82, sys_rename), S(83, sys_mkdir), S(84, sys_rmdir), S(85, sys_creat), S(86, sys_link),
    S(87, sys_unlink), S(88, sys_symlink), S(89, sys_readlink), S(90, sys_chmod),
    S(91, sys_fchmod), S(92, sys_chown), S(93, sys_fchown), S(94, sys_lchown), S(95, sys_umask),
    S(96, sys_gettimeofday), S(97, sys_getrlimit), S(98, sys_getrusage), S(99, sys_sysinfo),
    S(100, sys_times), S(102, sys_getuid), S(103, sys_syslog), S(104, sys_getgid),
    S(105, sys_setuid), S(106, sys_setgid), S(107, sys_geteuid), S(108, sys_getegid),
    S(109, sys_setpgid), S(110, sys_getppid), S(111, sys_getpgrp), S(112, sys_setsid),
    S(113, sys_setreuid), S(114, sys_setregid), S(115, sys_getgroups), S(116, sys_setgroups),
    S(117, sys_setresuid), S(118, sys_getresuid), S(119, sys_setresgid), S(120, sys_getresgid),
    S(121, sys_getpgid), S(122, sys_setfsuid), S(123, sys_setfsgid), S(124, sys_getsid),
    S(125, sys_capget), S(126, sys_capset), S(127, sys_rt_sigpending),
    S(128, sys_rt_sigtimedwait), S(129, sys_rt_sigqueueinfo), S(130, sys_rt_sigsuspend),
    S(131, sys_sigaltstack), S(133, sys_mknod), S(135, sys_personality), S(137, sys_statfs),
    S(138, sys_fstatfs), S(140, sys_getpriority), S(141, sys_setpriority),
    S(142, sys_sched_setparam), S(143, sys_sched_getparam), S(144, sys_sched_setscheduler),
    S(145, sys_sched_getscheduler), S(146, sys_sched_get_priority_max),
    S(147, sys_sched_get_priority_min), S(148, sys_sched_rr_get_interval), S(149, sys_mlock),
    S(150, sys_munlock), S(151, sys_mlockall), S(152, sys_munlockall), S(157, sys_prctl),
    S(158, sys_arch_prctl), S(160, sys_setrlimit), S(161, sys_chroot), S(162, sys_sync),
    S(164, sys_settimeofday), S(165, sys_mount), S(166, sys_umount2), S(169, sys_reboot),
    S(170, sys_sethostname), S(171, sys_setdomainname), S(186, sys_gettid), S(200, sys_tkill),
    S(201, sys_time), S(202, sys_futex), S(203, sys_sched_setaffinity),
    S(204, sys_sched_getaffinity), S(217, sys_getdents64), S(220, sys_semtimedop), S(218, sys_set_tid_address),
    S(221, sys_fadvise64), S(222, sys_timer_create), S(223, sys_timer_settime),
    S(224, sys_timer_gettime), S(225, sys_timer_getoverrun), S(226, sys_timer_delete),
    S(227, sys_clock_settime), S(228, sys_clock_gettime), S(229, sys_clock_getres),
    S(230, sys_clock_nanosleep), S(231, sys_exit_group), S(234, sys_tgkill), S(235, sys_utimes), S(240, sys_mq_open), S(241, sys_mq_unlink),
    S(242, sys_mq_timedsend), S(243, sys_mq_timedreceive), S(244, sys_mq_notify),
    S(245, sys_mq_getsetattr),
    S(247, sys_waitid), S(257, sys_openat), S(258, sys_mkdirat), S(259, sys_mknodat),
    S(260, sys_fchownat), S(262, sys_newfstatat), S(263, sys_unlinkat), S(264, sys_renameat),
    S(265, sys_linkat), S(266, sys_symlinkat), S(267, sys_readlinkat), S(268, sys_fchmodat),
    S(269, sys_faccessat), S(270, sys_pselect6), S(271, sys_ppoll), S(273, sys_set_robust_list),
    S(274, sys_get_robust_list), S(280, sys_utimensat), S(285, sys_fallocate), S(288, sys_accept4), S(292, sys_dup3),
    S(293, sys_pipe2), S(295, sys_preadv), S(296, sys_pwritev), S(297, sys_rt_tgsigqueueinfo),
    S(302, sys_prlimit64), S(306, sys_syncfs), S(309, sys_getcpu), S(316, sys_renameat2),
    S(318, sys_getrandom), S(322, sys_execveat), S(327, sys_preadv2), S(328, sys_pwritev2),
    S(332, sys_statx), S(435, sys_clone3), S(436, sys_close_range), S(437, sys_openat2),
    S(439, sys_faccessat2), S(452, sys_fchmodat2),
};

void syscall_dispatch(struct regs *r) {
  uint64_t nr = r->rax;
  current->regs = r;
  long ret = -ENOSYS;
  if (nr < ARRAY_SIZE(table) && table[nr])
    ret = table[nr]((long)r->rdi, (long)r->rsi, (long)r->rdx, (long)r->r10, (long)r->r8, (long)r->r9);
  else if (nr == 334 || nr == 435 || 1)
    ret = sys_ni();
  if (nr == 15) /* rt_sigreturn: the restored rax stands */
    return;
  r->rax = (uint64_t)ret;
}
