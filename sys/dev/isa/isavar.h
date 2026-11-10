/* isavar.h - the ISA bus (dev/isa/isa.c): what a driver is offered. */
#ifndef _DEV_ISA_ISAVAR_H_
#define _DEV_ISA_ISAVAR_H_

#include <sys/systm.h>

/* the aux of a device at isa: from the locators of its line of the table
   (-1: not given); its match function sets what the device uses where it
   was not (the I/O ports it takes, the interrupt) and the sizes, for the
   line dmesg shows */
struct isa_attach_args {
  int ia_iobase; /* I/O port base */
  int ia_iosize; /* ... and how many ports */
  int ia_irq;
  int ia_maddr;  /* memory address */
  int ia_msize;
};

#endif
