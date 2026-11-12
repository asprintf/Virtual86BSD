/*
 * Copyright (c) 2026, Joost Orchidee Winter
 * Copyright (c) 2026, asprintf
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* if_em.c - Intel 8254x gigabit Ethernet ("e1000": 82540EM, QEMU's
   default card, and its relatives): em0, em1 ... one interface per card.
   The registers are memory-mapped (BAR 0, uncached through ioremap;
   QEMU's I/O window is only a stub).  Legacy descriptors; 64 receive
   buffers of 2 KiB, 64 transmit buffers.  The interrupt only acknowledges
   and wakes the network thread, which takes the received frames
   (em_poll); PCI interrupt lines can be shared by several cards.

   A card is set up when the machine is configured (em_attach), as an
   interface that is down (net/if.c); its interrupt is taken when the
   interface goes up (em_start), after the thread's queue is there. */
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

/* a card */
struct em_softc {
  volatile uint32_t *regs;
  struct rxd *rx;
  struct txd *tx;
  uint64_t rxbuf[NRX], txbuf[NTX]; /* physical */
  unsigned rx_cur, tx_cur;
  int irq;
  bool started; /* its interrupt is on */
  struct ifnet *ifp;
};

static struct em_softc *em_cards[IF_MAXIF];
static int nem;

static uint32_t rd(struct em_softc *sc, uint32_t reg) { return sc->regs[reg / 4]; }
static void wr(struct em_softc *sc, uint32_t reg, uint32_t v) { sc->regs[reg / 4] = v; }

/* one interrupt function for every card (the lines may be shared): each
   started card looks at its own cause register */
static void em_irq(void) {
  for (int i = 0; i < nem; i++) {
    struct em_softc *sc = em_cards[i];
    if (sc->started && rd(sc, ICR)) /* reading acknowledges */
      net_wake();
  }
}

static void em_poll(struct ifnet *ifp) {
  struct em_softc *sc = ifp->if_softc;
  for (;;) {
    struct rxd *d = &sc->rx[sc->rx_cur];
    if (!(d->status & 1))
      break;
    if ((d->status & 2) && !d->errors && d->len >= 14)
      net_rx(ifp, P2V(sc->rxbuf[sc->rx_cur]), d->len);
    else
      ifp->if_idrops++;
    d->status = 0;
    wr(sc, RDT, sc->rx_cur);
    sc->rx_cur = (sc->rx_cur + 1) % NRX;
  }
}

static int em_xmit(struct ifnet *ifp, const void *frame, size_t len) {
  struct em_softc *sc = ifp->if_softc;
  if (len > BUFSZ)
    return -EMSGSIZE;
  struct txd *d = &sc->tx[sc->tx_cur];
  if (d->cmd && !(d->status & 1)) { /* the ring is full: the card is behind */
    ifp->if_odrops++;
    return -ENOBUFS;
  }
  memcpy(P2V(sc->txbuf[sc->tx_cur]), frame, len);
  if (len < 60)
    memset((char *)P2V(sc->txbuf[sc->tx_cur]) + len, 0, 60 - len), len = 60;
  d->addr = sc->txbuf[sc->tx_cur];
  d->len = (uint16_t)len;
  d->cso = d->css = 0;
  d->special = 0;
  d->status = 0;
  d->cmd = 1 | 2 | 8; /* end of packet, insert FCS, report status */
  sc->tx_cur = (sc->tx_cur + 1) % NTX;
  wr(sc, TDT, sc->tx_cur);
  return 0;
}

static uint16_t eeprom(struct em_softc *sc, int word) {
  wr(sc, EERD, (uint32_t)word << 8 | 1);
  for (int i = 0; i < 100000; i++) {
    uint32_t v = rd(sc, EERD);
    if (v & 0x10)
      return (uint16_t)(v >> 16);
  }
  return 0;
}

/* the cards the driver takes */
static int em_match(struct device *parent, void *match, void *aux) {
  static const uint16_t products[] = {PCI_PRODUCT_INTEL_82540EM, PCI_PRODUCT_INTEL_82545EM, PCI_PRODUCT_INTEL_82543GC,
                                      PCI_PRODUCT_INTEL_82544GC, PCI_PRODUCT_INTEL_82540EM_LOM,
                                      PCI_PRODUCT_INTEL_82547GI, PCI_PRODUCT_INTEL_82540EP, PCI_PRODUCT_INTEL_82574L};
  struct pci_dev *d = aux;
  (void)parent;
  (void)match;
  if (d->vendor != PCI_VENDOR_INTEL)
    return 0;
  for (size_t i = 0; i < ARRAY_SIZE(products); i++)
    if (d->device == products[i])
      return 1;
  return 0;
}

static void em_start(struct ifnet *ifp);

static void em_attach(struct device *parent, struct device *self, void *aux) {
  struct pci_dev d = *(struct pci_dev *)aux;
  (void)parent;
  (void)self;
  if (d.bar[0] & 1) {
    printf(": BAR 0 is not memory\n");
    return;
  }
  if (nem == IF_MAXIF) {
    printf(": too many cards\n");
    return;
  }
  struct em_softc *sc = kzalloc(sizeof(*sc));
  if (!sc) {
    printf(": out of memory\n");
    return;
  }
  uint64_t phys = d.bar[0] & ~0xfU;
  if (((d.bar[0] >> 1) & 3) == 2) /* 64-bit */
    phys |= (uint64_t)d.bar[1] << 32;
  pci_write(&d, 0x04, pci_read(&d, 0x04) | 0x6); /* memory, bus master */
  sc->regs = ioremap(phys, 128 * 1024);
  if (!sc->regs) {
    printf(": cannot map the registers\n");
    return;
  }

  wr(sc, CTRL, rd(sc, CTRL) | (1U << 26)); /* reset */
  for (volatile int i = 0; i < 100000; i++)
    ;
  wr(sc, IMC, 0xffffffff);
  rd(sc, ICR);
  wr(sc, CTRL, (rd(sc, CTRL) | (1U << 6) | (1U << 5)) & ~(1U << 3)); /* link up, auto speed; no reset of link */

  uint32_t lo = rd(sc, RAL0), hi = rd(sc, RAH0);
  if (!(hi & 0x80000000U) || (lo == 0 && (hi & 0xffff) == 0)) {
    uint16_t w0 = eeprom(sc, 0), w1 = eeprom(sc, 1), w2 = eeprom(sc, 2);
    lo = (uint32_t)w0 | (uint32_t)w1 << 16;
    hi = w2;
    wr(sc, RAL0, lo);
    wr(sc, RAH0, hi | 0x80000000U);
  }
  uint8_t mac[6];
  for (int i = 0; i < 4; i++)
    mac[i] = (uint8_t)(lo >> (8 * i));
  mac[4] = (uint8_t)hi, mac[5] = (uint8_t)(hi >> 8);
  for (int i = 0; i < 128; i++)
    wr(sc, MTA + 4 * (uint32_t)i, 0);

  /* rings: descriptors in one page each, buffers two to a page */
  uint64_t rp = alloc_zeroed_page(), tp = alloc_zeroed_page();
  if (!rp || !tp) {
    printf(": out of memory\n");
    return;
  }
  sc->rx = P2V(rp);
  sc->tx = P2V(tp);
  for (int i = 0; i < NRX; i += 2) {
    uint64_t pg = alloc_zeroed_page();
    if (!pg) {
      printf(": out of memory\n");
      return;
    }
    sc->rxbuf[i] = pg, sc->rxbuf[i + 1] = pg + BUFSZ;
    sc->rx[i].addr = sc->rxbuf[i], sc->rx[i + 1].addr = sc->rxbuf[i + 1];
  }
  for (int i = 0; i < NTX; i += 2) {
    uint64_t pg = alloc_zeroed_page();
    if (!pg) {
      printf(": out of memory\n");
      return;
    }
    sc->txbuf[i] = pg, sc->txbuf[i + 1] = pg + BUFSZ;
    sc->tx[i].status = sc->tx[i + 1].status = 1;
  }
  wr(sc, RDBAL, (uint32_t)rp);
  wr(sc, RDBAH, (uint32_t)(rp >> 32));
  wr(sc, RDLEN, NRX * sizeof(struct rxd));
  wr(sc, RDH, 0);
  wr(sc, RDT, NRX - 1);
  sc->rx_cur = 0;
  wr(sc, TDBAL, (uint32_t)tp);
  wr(sc, TDBAH, (uint32_t)(tp >> 32));
  wr(sc, TDLEN, NTX * sizeof(struct txd));
  wr(sc, TDH, 0);
  wr(sc, TDT, 0);
  sc->tx_cur = 0;
  /* receive: on, broadcasts, 2 KiB buffers, strip the CRC; transmit: on,
     pad short packets, collision settings for full duplex */
  wr(sc, RCTL, (1U << 1) | (1U << 15) | (1U << 26));
  wr(sc, TCTL, (1U << 1) | (1U << 3) | (0x10U << 4) | (0x40U << 12));
  wr(sc, TIPG, 0x0060200a);

  sc->ifp = if_attach_ether("em", mac, 1500, em_xmit, em_poll, em_start, sc);
  if (!sc->ifp) {
    printf(": no interface\n");
    return;
  }
  sc->irq = d.irq;
  em_cards[nem++] = sc;
  printf(": irq %d, address %02x:%02x:%02x:%02x:%02x:%02x\n", d.irq, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

/* the interrupt on: the network (net_init_conf, or ifconfig up) is ready for it */
static void em_start(struct ifnet *ifp) {
  struct em_softc *sc = ifp->if_softc;
  irq_register(sc->irq, em_irq);
  sc->started = true;
  wr(sc, IMS, (1U << 7) | (1U << 6) | (1U << 4) | (1U << 2)); /* received, overrun, low, link */
}

struct cfdriver em_cd = {NULL, "em", DV_IFNET, 0};
struct cfattach em_ca = {sizeof(struct device), em_match, em_attach};
