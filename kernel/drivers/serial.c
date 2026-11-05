/* serial.c - 16550 UART on COM1 (ttyS0): console output, input to the tty. */
#include "kernel.h"
#include "arch.h"
#include "dev.h"
#include "tty.h"

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

void serial_enable_irq(void) {
  irq_register(4, serial_irq);
  outb(COM1 + 1, 0x01); /* received-data interrupt */
}
