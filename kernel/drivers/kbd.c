/* kbd.c - PS/2 keyboard (scan code set 1), US layout, feeding the console
   tty.  Arrow keys etc. produce VT100 sequences. */
#include "kernel.h"
#include "arch.h"
#include "dev.h"
#include "tty.h"

static const char normal[128] = {
    0, 033, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\r', 0, 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0};
static const char shifted[128] = {
    0, 033, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\r', 0, 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ', 0};

static bool shift, ctrl, alt, caps, e0;

static void send(const char *s) {
  while (*s)
    tty_input(console_tty, *s++);
}

static void kbd_irq(void) {
  while (inb(0x64) & 1) {
    uint8_t sc = inb(0x60);
    if (sc == 0xe0) {
      e0 = true;
      continue;
    }
    bool up = sc & 0x80;
    sc &= 0x7f;
    if (e0) {
      e0 = false;
      if (sc == 0x1d)
        ctrl = !up;
      else if (sc == 0x38)
        alt = !up;
      else if (!up) {
        switch (sc) {
        case 0x48: send("\033[A"); break;
        case 0x50: send("\033[B"); break;
        case 0x4d: send("\033[C"); break;
        case 0x4b: send("\033[D"); break;
        case 0x47: send("\033[H"); break;
        case 0x4f: send("\033[F"); break;
        case 0x53: send("\033[3~"); break;
        case 0x1c: tty_input(console_tty, '\r'); break;
        }
      }
      continue;
    }
    switch (sc) {
    case 0x2a:
    case 0x36:
      shift = !up;
      continue;
    case 0x1d:
      ctrl = !up;
      continue;
    case 0x38:
      alt = !up;
      continue;
    case 0x3a:
      if (!up)
        caps = !caps;
      continue;
    }
    if (up)
      continue;
    char c = shift ? shifted[sc] : normal[sc];
    if (!c)
      continue;
    if (caps && c >= 'a' && c <= 'z')
      c -= 32;
    else if (caps && c >= 'A' && c <= 'Z' && shift)
      c += 32;
    if (c == '\b')
      c = 0x7f; /* the Backspace key sends DEL, as on Linux */
    if (ctrl) {
      if (c >= 'a' && c <= 'z')
        c -= 'a' - 1;
      else if (c >= '@' && c <= '_')
        c -= '@';
      else if (c == ' ')
        c = 0;
    }
    if (alt)
      tty_input(console_tty, 033);
    tty_input(console_tty, c);
  }
}

void kbd_init(void) {
  while (inb(0x64) & 1)
    inb(0x60);
  irq_register(1, kbd_irq);
}
