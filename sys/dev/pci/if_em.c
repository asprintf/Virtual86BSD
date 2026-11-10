/* if_em.c - Intel 8254x gigabit Ethernet ("e1000": 82540EM, QEMU's
   default card, and its relatives), em0.  The registers are memory-mapped
   (BAR 0, uncached through ioremap; QEMU's I/O window is only a stub).
   Legacy descriptors; 64 receive buffers of 2 KiB, 64 transmit
   buffers.  The interrupt only acknowledges and wakes the network thread,
   which takes the received frames (e1000_poll).

   The card is set up when the machine is configured (em_attach); its
   interrupt is taken when the network starts (em_start: net_init_conf),
   after the thread's queue is there. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <uvm/uvm.h>
#include <net/if_var.h>
#include <machine/cpu.h>

#define CTRL 0x0000
#define STATUS 0x0008
#define EERD 0x0014
#define ICR 0x00c0
#define IMS 0x00d0
#define IMC 0x00d8
#define RCTL 0x0100
#define TCTL 0x0400
#define TIPG 0x0410
#define RDBAL 0x2800
#define RDBAH 0x2804
#define RDLEN 0x2808
#define RDH 0x2810
#define RDT 0x2818
#define TDBAL 0x3800
#define TDBAH 0x3804
#define TDLEN 0x3808
#define TDH 0x3810
#define TDT 0x3818
#define MTA 0x5200
#define RAL0 0x5400
#define RAH0 0x5404

#define NRX 64
#define NTX 64
#define BUFSZ 2048

struct rxd {
  uint64_t addr;
  uint16_t len, csum;
  uint8_t status, errors;
  uint16_t special;
};
struct txd {
  uint64_t addr;
  uint16_t len;
  uint8_t cso, cmd, status, css;
  uint16_t special;
};

static volatile uint32_t *regs;
static struct rxd *rx;
static struct txd *tx;
static uint64_t rxbuf[NRX], txbuf[NTX]; /* physical */
static unsigned rx_cur, tx_cur;

static uint32_t rd(uint32_t reg) { return regs[reg / 4]; }
static void wr(uint32_t reg, uint32_t v) { regs[reg / 4] = v; }

static void e1000_irq(void) {
  uint32_t cause = rd(ICR); /* reading acknowledges */
  if (cause)
    net_wake();
}

void e1000_poll(void) {
  if (!rx)
    return;
  for (;;) {
    struct rxd *d = &rx[rx_cur];
    if (!(d->status & 1))
      break;
    if ((d->status & 2) && !d->errors && d->len >= 14)
      net_rx(P2V(rxbuf[rx_cur]), d->len);
    else
      eth0.rx_dropped++;
    d->status = 0;
    wr(RDT, rx_cur);
    rx_cur = (rx_cur + 1) % NRX;
  }
}

static int e1000_xmit(const void *frame, size_t len) {
  if (len > BUFSZ)
    return -EMSGSIZE;
  struct txd *d = &tx[tx_cur];
  if (d->cmd && !(d->status & 1)) { /* the ring is full: the card is behind */
    eth0.tx_dropped++;
    return -ENOBUFS;
  }
  memcpy(P2V(txbuf[tx_cur]), frame, len);
  if (len < 60)
    memset((char *)P2V(txbuf[tx_cur]) + len, 0, 60 - len), len = 60;
  d->addr = txbuf[tx_cur];
  d->len = (uint16_t)len;
  d->cso = d->css = 0;
  d->special = 0;
  d->status = 0;
  d->cmd = 1 | 2 | 8; /* end of packet, insert FCS, report status */
  tx_cur = (tx_cur + 1) % NTX;
  wr(TDT, tx_cur);
  return 0;
}

static uint16_t eeprom(int word) {
  wr(EERD, (uint32_t)word << 8 | 1);
  for (int i = 0; i < 100000; i++) {
    uint32_t v = rd(EERD);
    if (v & 0x10)
      return (uint16_t)(v >> 16);
  }
  return 0;
}

static int em_irq_line;

/* the cards the driver takes (one: there is one eth0) */
static int em_match(struct device *parent, void *match, void *aux) {
  static const uint16_t products[] = {PCI_PRODUCT_INTEL_82540EM, PCI_PRODUCT_INTEL_82545EM, PCI_PRODUCT_INTEL_82543GC,
                                      PCI_PRODUCT_INTEL_82544GC, PCI_PRODUCT_INTEL_82540EM_LOM,
                                      PCI_PRODUCT_INTEL_82547GI, PCI_PRODUCT_INTEL_82540EP, PCI_PRODUCT_INTEL_82574L};
  struct pci_dev *d = aux;
  (void)parent;
  (void)match;
  if (d->vendor != PCI_VENDOR_INTEL || eth0.present)
    return 0;
  for (size_t i = 0; i < ARRAY_SIZE(products); i++)
    if (d->device == products[i])
      return 1;
  return 0;
}

static void em_attach(struct device *parent, struct device *self, void *aux) {
  struct pci_dev d = *(struct pci_dev *)aux;
  (void)parent;
  (void)self;
  if (d.bar[0] & 1) {
    printk(": BAR 0 is not memory\n");
    return;
  }
  uint64_t phys = d.bar[0] & ~0xfU;
  if (((d.bar[0] >> 1) & 3) == 2) /* 64-bit */
    phys |= (uint64_t)d.bar[1] << 32;
  pci_write(&d, 0x04, pci_read(&d, 0x04) | 0x6); /* memory, bus master */
  regs = ioremap(phys, 128 * 1024);
  if (!regs) {
    printk(": cannot map the registers\n");
    return;
  }

  wr(CTRL, rd(CTRL) | (1U << 26)); /* reset */
  for (volatile int i = 0; i < 100000; i++)
    ;
  wr(IMC, 0xffffffff);
  rd(ICR);
  wr(CTRL, (rd(CTRL) | (1U << 6) | (1U << 5)) & ~(1U << 3)); /* link up, auto speed; no reset of link */

  uint32_t lo = rd(RAL0), hi = rd(RAH0);
  if (!(hi & 0x80000000U) || (lo == 0 && (hi & 0xffff) == 0)) {
    uint16_t w0 = eeprom(0), w1 = eeprom(1), w2 = eeprom(2);
    lo = (uint32_t)w0 | (uint32_t)w1 << 16;
    hi = w2;
    wr(RAL0, lo);
    wr(RAH0, hi | 0x80000000U);
  }
  for (int i = 0; i < 4; i++)
    eth0.mac[i] = (uint8_t)(lo >> (8 * i));
  eth0.mac[4] = (uint8_t)hi, eth0.mac[5] = (uint8_t)(hi >> 8);
  for (int i = 0; i < 128; i++)
    wr(MTA + 4 * (uint32_t)i, 0);

  /* rings: descriptors in one page each, buffers two to a page */
  uint64_t rp = alloc_zeroed_page(), tp = alloc_zeroed_page();
  if (!rp || !tp) {
    printk(": out of memory\n");
    return;
  }
  rx = P2V(rp);
  tx = P2V(tp);
  for (int i = 0; i < NRX; i += 2) {
    uint64_t pg = alloc_zeroed_page();
    if (!pg) {
      printk(": out of memory\n");
      return;
    }
    rxbuf[i] = pg, rxbuf[i + 1] = pg + BUFSZ;
    rx[i].addr = rxbuf[i], rx[i + 1].addr = rxbuf[i + 1];
  }
  for (int i = 0; i < NTX; i += 2) {
    uint64_t pg = alloc_zeroed_page();
    if (!pg) {
      printk(": out of memory\n");
      return;
    }
    txbuf[i] = pg, txbuf[i + 1] = pg + BUFSZ;
    tx[i].status = tx[i + 1].status = 1;
  }
  wr(RDBAL, (uint32_t)rp);
  wr(RDBAH, (uint32_t)(rp >> 32));
  wr(RDLEN, NRX * sizeof(struct rxd));
  wr(RDH, 0);
  wr(RDT, NRX - 1);
  rx_cur = 0;
  wr(TDBAL, (uint32_t)tp);
  wr(TDBAH, (uint32_t)(tp >> 32));
  wr(TDLEN, NTX * sizeof(struct txd));
  wr(TDH, 0);
  wr(TDT, 0);
  tx_cur = 0;
  /* receive: on, broadcasts, 2 KiB buffers, strip the CRC; transmit: on,
     pad short packets, collision settings for full duplex */
  wr(RCTL, (1U << 1) | (1U << 15) | (1U << 26));
  wr(TCTL, (1U << 1) | (1U << 3) | (0x10U << 4) | (0x40U << 12));
  wr(TIPG, 0x0060200a);

  eth0.xmit = e1000_xmit;
  eth0.present = true;
  em_irq_line = d.irq;
  printk(": irq %d, address %02x:%02x:%02x:%02x:%02x:%02x\n", d.irq, eth0.mac[0], eth0.mac[1], eth0.mac[2],
         eth0.mac[3], eth0.mac[4], eth0.mac[5]);
}

/* the interrupt on: the network (net_init_conf) is ready for it */
void em_start(void) {
  if (!eth0.present)
    return;
  irq_register(em_irq_line, e1000_irq);
  wr(IMS, (1U << 7) | (1U << 6) | (1U << 4) | (1U << 2)); /* received, overrun, low, link */
}

struct cfdriver em_cd = {NULL, "em", DV_IFNET, 0};
struct cfattach em_ca = {sizeof(struct device), em_match, em_attach};
