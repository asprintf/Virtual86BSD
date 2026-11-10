/* reboot.h - reboot(2): howto, and how the system is stopped. */
#ifndef _SYS_REBOOT_H_
#define _SYS_REBOOT_H_

#define RB_AUTOBOOT 0     /* restart the system */
#define RB_NOSYNC 0x0004  /* do not write the disks first */
#define RB_HALT 0x0008    /* stop, do not restart */
#define RB_POWERDOWN 0x1000 /* with RB_HALT: and turn the power off */

#ifndef _KERNEL
#ifdef __cplusplus
extern "C" {
#endif
int reboot(int);
#ifdef __cplusplus
}
#endif
#endif

#endif
