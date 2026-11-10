/* autoconf.c - configuring the machine: the device tree from its root. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

/* every device of the table that is there: mainbus0 first, which finds
   the rest (cpu0, pci0, isa0 and what is on them) */
void cpu_configure(void) {
  if (config_rootfound("mainbus", NULL) == NULL)
    panic("cpu_configure: mainbus is not configured");
}
