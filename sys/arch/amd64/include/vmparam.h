/* vmparam.h - the layout of the address space. */
#ifndef _MACHINE_VMPARAM_H_
#define _MACHINE_VMPARAM_H_

#include <sys/types.h>

#define KERNEL_BASE 0xffffffff80000000UL  /* the image: phys + KERNEL_BASE */
#define DIRECT_MAP  0xffff888000000000UL  /* all of RAM: phys + DIRECT_MAP */
#define USER_TOP    0x0000800000000000UL  /* user addresses are below this */

#define P2V(p) ((void *)((uint64_t)(p) + DIRECT_MAP))
#define V2P(v) ((uint64_t)(v) - DIRECT_MAP)

#endif
