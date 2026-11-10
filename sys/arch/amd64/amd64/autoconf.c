/* autoconf.c - configuring the machine: the device tree from its root. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

/* every device of the table that is there: mainbus0 first, which finds
   the rest (cpu0, pci0 and what is on it; isa0 comes with its bridge, pcib0) */
void cpu_configure(void) {
  if (config_rootfound("mainbus", NULL) == NULL)
    panic("cpu_configure: mainbus is not configured");
  config_process_deferred();
}
