/*
 * Copyright (c) 2026, Joost Orchidee Winter
 * Copyright (c) 2026, asprintf
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

/* linux_misc.c - the system calls that are Linux's own: what a Linux
   program sees where V86BSD's answer would differ. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/syscallargs.h>
#include <uvm/uvm_extern.h>
#include <compat/linux/linux_syscallargs.h>

extern char uts_nodename[65], uts_domain[65];

/* uname: Linux, release 6.9.0, x86_64 (the Linux kernel the ABI is that of) */
long linux_sys_uname(long ubuf) {
  char u[6][65];
  memset(u, 0, sizeof(u));
  strcpy(u[0], emul_linux.e_ostype);
  strcpy(u[1], uts_nodename);
  strcpy(u[2], emul_linux.e_osrelease);
  strcpy(u[3], "#1 V86BSD");
  strcpy(u[4], "x86_64");
  strcpy(u[5], uts_domain);
  return copy_to_user((void *)ubuf, u, sizeof(u)) ? -EFAULT : 0;
}

long linux_sys_personality(long v) {
  (void)v;
  return 0;
}

/* reboot(2) of Linux: the magic numbers and LINUX_REBOOT_CMD_* */
#define LINUX_REBOOT_MAGIC1 0xfee1dead
#define LINUX_REBOOT_CMD_RESTART 0x01234567
#define LINUX_REBOOT_CMD_HALT 0xcdef0123
#define LINUX_REBOOT_CMD_CAD_ON 0x89abcdef
#define LINUX_REBOOT_CMD_CAD_OFF 0
#define LINUX_REBOOT_CMD_POWER_OFF 0x4321fedc

long linux_sys_reboot(long m1, long m2, long cmd) {
  (void)m2;
  if (!capable())
    return -EPERM;
  if ((uint32_t)m1 != LINUX_REBOOT_MAGIC1)
    return -EINVAL;
  switch ((uint32_t)cmd) {
  case LINUX_REBOOT_CMD_CAD_ON:
  case LINUX_REBOOT_CMD_CAD_OFF:
    return 0;
  case LINUX_REBOOT_CMD_RESTART:
    return sys_reboot(RB_AUTOBOOT);
  case LINUX_REBOOT_CMD_POWER_OFF:
    return sys_reboot(RB_POWERDOWN | RB_HALT);
  case LINUX_REBOOT_CMD_HALT:
    return sys_reboot(RB_HALT);
  }
  return -EINVAL;
}
