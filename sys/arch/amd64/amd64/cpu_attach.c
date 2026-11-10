/* cpu_attach.c - the processor as a device: cpu0.  (There is one: the
   kernel is uniprocessor.  Its set-up is arch_init in x86.c.) */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <machine/mainbus.h>

static int cpu_match(struct device *parent, void *match, void *aux) {
  struct mainbus_attach_args *ma = aux;
  (void)parent;
  (void)match;
  return !strcmp(ma->maa_name, "cpu");
}

static void cpu_attach(struct device *parent, struct device *self, void *aux) {
  (void)parent;
  (void)self;
  (void)aux;
  printk(": (uniprocessor)\n");
}

struct cfdriver cpu_cd = {NULL, "cpu", DV_CPU, 0};
struct cfattach cpu_ca = {sizeof(struct device), cpu_match, cpu_attach};
