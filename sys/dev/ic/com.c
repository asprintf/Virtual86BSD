/* com.c - 16550 UART on COM1 (/dev/tty00, /dev/cua00): console output, input to the tty.

   The console is set up early (serial_init, before the first message and
   before there is an allocator: output is polled); com0, the device, attaches
   when the machine is configured, and takes the interrupt for the input. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/tty.h>
#include <dev/isa/isavar.h>
#include <machine/cpu.h>

#define COM1 0x3f8

static bool present;

void serial_init(void) {
  outb(COM1 + 1, 0x00); /* interrupts off */
  outb(COM1 + 3, 0x80); /* DLAB */
  outb(COM1 + 0, 0x01); /* 115200 baud */
  outb(COM1 + 1, 0x00);
  outb(COM1 + 3, 0x03); /* 8N1 */
  outb(COM1 + 2, 0xc7); /* FIFO on, cleared, 14-byte threshold */
  outb(COM1 + 4, 0x0b); /* DTR, RTS, OUT2 (IRQ line) */
  present = inb(COM1 + 5) != 0xff;
}

void serial_write(const char *s, size_t n) {
  if (!present)
    return;
  for (size_t i = 0; i < n; i++) {
    for (int k = 0; k < 100000 && !(inb(COM1 + 5) & 0x20); k++)
      ;
    outb(COM1, (uint8_t)s[i]);
  }
}

static void serial_irq(void) {
  while (inb(COM1 + 5) & 1) {
    char c = (char)inb(COM1);
    tty_input(console_tty, c);
  }
}

/* only the console's port: the one UART the kernel has a tty for */
static int com_match(struct device *parent, void *match, void *aux) {
  struct isa_attach_args *ia = aux;
  (void)parent;
  (void)match;
  if (ia->ia_iobase != COM1 || !present)
    return 0;
  ia->ia_iosize = 8;
  if (ia->ia_irq == -1)
    ia->ia_irq = 4;
  return 1;
}

static void com_attach(struct device *parent, struct device *self, void *aux) {
  struct isa_attach_args *ia = aux;
  uint8_t fifo = inb(COM1 + 2) & 0xc0; /* the FIFO bits of the interrupt id */
  (void)parent;
  printf(": %s\n", fifo == 0xc0 ? "ns16550a, 16 byte fifo" : fifo == 0x80 ? "ns16550, broken fifo" : "ns16450, no fifo");
  printf("%s: console\n", self->dv_xname);
  irq_register(ia->ia_irq, serial_irq);
  outb(COM1 + 1, 0x01); /* received-data interrupt */
}

struct cfdriver com_cd = {NULL, "com", DV_TTY, 0};
struct cfattach com_ca = {sizeof(struct device), com_match, com_attach};
