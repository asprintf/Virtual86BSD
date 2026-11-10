/* mainbus.h - what the mainbus offers its children (amd64/mainbus.c): the
   buses and the processor, by name; the aux of a device at mainbus. */
#ifndef _MACHINE_MAINBUS_H_
#define _MACHINE_MAINBUS_H_

struct mainbus_attach_args {
  const char *maa_name; /* "cpu", "pci" or "isa" */
  int maa_bus;          /* pci: the bus number */
};

#endif
