/* cpu_attach.c - the processor as a device: cpu0.  (There is one: the
   kernel is uniprocessor.  Its set-up is arch_init in x86.c.) */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <machine/cpu.h>
#include <machine/mainbus.h>

static int cpu_match(struct device *parent, void *match, void *aux) {
  struct mainbus_attach_args *ma = aux;
  (void)parent;
  (void)match;
  return !strcmp(ma->maa_name, "cpu");
}

static void cpu_attach(struct device *parent, struct device *self, void *aux) {
  uint32_t r[4], b[12];
  char brand[49];
  (void)parent;
  (void)aux;
  /* as OpenBSD: the APIC id of the processor, then what it is: the brand
     string, its clock and family-model-stepping in hex */
  cpuid4(1, 0, r);
  printf(": apid %u (boot processor)\n", r[1] >> 24);
  uint32_t eax = r[0];
  unsigned family = (eax >> 8) & 15, model = (eax >> 4) & 15, step = eax & 15;
  if (family == 15)
    family += (eax >> 20) & 255;
  if (family >= 6)
    model += ((eax >> 16) & 15) << 4;
  brand[0] = 0;
  cpuid4(0x80000000U, 0, r);
  if (r[0] >= 0x80000004U) {
    for (int i = 0; i < 3; i++)
      cpuid4(0x80000002U + i, 0, b + 4 * i);
    memcpy(brand, b, 48);
    brand[48] = 0;
  }
  const char *s = brand;
  while (*s == ' ')
    s++;
  uint64_t mhz100 = (tsc_frequency() + 5000) / 10000;
  printf("%s: %s, %lu.%02lu MHz, %02x-%02x-%02x\n", self->dv_xname, *s ? s : "unknown processor", mhz100 / 100,
         mhz100 % 100, family, model, step);
}

struct cfdriver cpu_cd = {NULL, "cpu", DV_CPU, 0};
struct cfattach cpu_ca = {sizeof(struct device), cpu_match, cpu_attach};
