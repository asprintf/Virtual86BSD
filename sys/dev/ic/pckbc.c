/* pckbc.c - the PS/2 keyboard controller (pckbc0) and the keyboard on it
   (pckbd0): scan code set 1, US layout, feeding the console tty.  Arrow
   keys etc. produce VT100 sequences. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/tty.h>
#include <dev/isa/isavar.h>
#include <machine/cpu.h>

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

/* the controller: ports 0x60 (data) and 0x64 (status, command), 5 of them
   by the usual decode; the keyboard is in its keyboard slot */
#define PCKBC_KBD_SLOT 0

struct pckbc_attach_args {
  int pa_slot;
};

static int kbd_irq_line = 1;

static int pckbc_match(struct device *parent, void *match, void *aux) {
  struct isa_attach_args *ia = aux;
  (void)parent;
  (void)match;
  if (ia->ia_iobase == -1)
    ia->ia_iobase = 0x60;
  if (ia->ia_iobase != 0x60 || inb(0x64) == 0xff)
    return 0;
  ia->ia_iosize = 5;
  if (ia->ia_irq == -1)
    ia->ia_irq = 1;
  return 1;
}

static int pckbc_print(void *aux, const char *pnp) {
  struct pckbc_attach_args *pa = aux;
  if (pnp)
    printk("device at %s", pnp);
  printk(" (%s slot)", pa->pa_slot == PCKBC_KBD_SLOT ? "kbd" : "aux");
  return UNCONF;
}

static void pckbc_attach(struct device *parent, struct device *self, void *aux) {
  struct isa_attach_args *ia = aux;
  struct pckbc_attach_args pa = {PCKBC_KBD_SLOT};
  (void)parent;
  printk("\n");
  while (inb(0x64) & 1)
    inb(0x60);
  kbd_irq_line = ia->ia_irq;
  config_found(self, &pa, pckbc_print);
}

static int pckbd_match(struct device *parent, void *match, void *aux) {
  struct pckbc_attach_args *pa = aux;
  (void)parent;
  (void)match;
  return pa->pa_slot == PCKBC_KBD_SLOT;
}

static void pckbd_attach(struct device *parent, struct device *self, void *aux) {
  (void)parent;
  (void)self;
  (void)aux;
  printk("\n");
  irq_register(kbd_irq_line, kbd_irq);
}

struct cfdriver pckbc_cd = {NULL, "pckbc", DV_DULL, 0};
struct cfattach pckbc_ca = {sizeof(struct device), pckbc_match, pckbc_attach};
struct cfdriver pckbd_cd = {NULL, "pckbd", DV_TTY, 0};
struct cfattach pckbd_ca = {sizeof(struct device), pckbd_match, pckbd_attach};
