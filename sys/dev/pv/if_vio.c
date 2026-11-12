/*
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

/* if_vio.c - virtio network devices (QEMU's -nic model=virtio-net-pci),
   vio0, as OpenBSD names it.  Written from the Virtio 1.2 specification,
   5.1 (the network device) on the legacy ("transitional") PCI interface of
   virtio.c.

   Features: only VIRTIO_NET_F_MAC (the address in the configuration
   space) and VIRTIO_NET_F_STATUS (the link).  No checksum or segmentation
   offload, no merged receive buffers, no control queue: every packet is
   whole and checksummed, in one descriptor behind the 10-byte
   virtio_net_hdr (5.1.6.1: without MRG_RXBUF the header has no
   num_buffers).  receiveq1 is queue 0 and transmitq1 queue 1 (5.1.2).

   Receive buffers are 2 KiB (the 1514-byte frame and the header, 5.1.6.3),
   all posted at the start and posted again as the network thread takes
   the frames (vio_poll); the interrupt (the ISR register acknowledges it)
   only wakes that thread.  A transmit buffer is copied into one of the
   transmit descriptors (a header of zeroes, then the frame) and reclaimed
   when the device is done with it; transmit interrupts are off. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <dev/pci/pcivar.h>
#include <dev/pv/virtiovar.h>
#include <uvm/uvm.h>
#include <net/if_var.h>
#include <machine/cpu.h>

/* feature bits (5.1.3) */
#define VIRTIO_NET_F_MAC (1U << 5)
#define VIRTIO_NET_F_STATUS (1U << 16)

#define VIRTIO_NET_S_LINK_UP 1

/* struct virtio_net_hdr without num_buffers (5.1.6) */
#define NET_HDR 10

#define BUFSZ 2048
#define NRX 64
#define NTX 32

struct vio_softc {
  uint16_t io;
  uint8_t irq;
  bool status; /* VIRTIO_NET_F_STATUS: the link is in the configuration */
  struct virtqueue rxq, txq;
  uint64_t rxbuf[NRX], txbuf[NTX]; /* physical; descriptor i owns buffer i */
  uint8_t txfree[NTX];             /* the transmit descriptors not in the device */
  unsigned ntxfree;
  unsigned nrx, ntx;
  struct ifnet *ifp;
};

static struct vio_softc *vio_irq_sc[4]; /* for the interrupt handler (one per card) */

static void vio_irq(void) {
  for (int i = 0; i < 4 && vio_irq_sc[i]; i++)
    if (inb((uint16_t)(vio_irq_sc[i]->io + VIRTIO_R_ISR)) & 1) /* (reading acknowledges) */
      net_wake();
}

/* transmit descriptors the device is done with */
static void vio_tx_reclaim(struct vio_softc *sc) {
  unsigned head;
  uint32_t len;
  while (virtio_queue_pop(&sc->txq, &head, &len))
    if (head < sc->ntx)
      sc->txfree[sc->ntxfree++] = (uint8_t)head;
}

static int vio_xmit(struct ifnet *ifp, const void *frame, size_t len) {
  struct vio_softc *sc = ifp->if_softc;
  if (len > BUFSZ - NET_HDR)
    return -EMSGSIZE;
  if (!sc->ntxfree)
    vio_tx_reclaim(sc);
  if (!sc->ntxfree) { /* the device is behind */
    ifp->if_odrops++;
    return -ENOBUFS;
  }
  unsigned i = sc->txfree[--sc->ntxfree];
  uint8_t *b = P2V(sc->txbuf[i]);
  memset(b, 0, NET_HDR); /* flags 0, gso_type NONE: a whole packet */
  memcpy(b + NET_HDR, frame, len);
  if (len < 60)
    memset(b + NET_HDR + len, 0, 60 - len), len = 60;
  sc->txq.desc[i].addr = sc->txbuf[i];
  sc->txq.desc[i].len = (uint32_t)(NET_HDR + len);
  sc->txq.desc[i].flags = 0;
  sc->txq.desc[i].next = 0;
  virtio_queue_push(&sc->txq, i);
  virtio_queue_notify(&sc->txq);
  return 0;
}

static void vio_poll(struct ifnet *ifp) {
  struct vio_softc *sc = ifp->if_softc;
  unsigned head;
  uint32_t len;
  bool posted = false;
  while (virtio_queue_pop(&sc->rxq, &head, &len)) {
    if (head >= sc->nrx)
      continue;
    if (len >= NET_HDR + 14 && len <= BUFSZ)
      net_rx(ifp, (uint8_t *)P2V(sc->rxbuf[head]) + NET_HDR, len - NET_HDR);
    else
      ifp->if_idrops++;
    virtio_queue_push(&sc->rxq, head); /* the buffer back to the device */
    posted = true;
  }
  if (posted)
    virtio_queue_notify(&sc->rxq);
  vio_tx_reclaim(sc);
}

static void vio_start(struct ifnet *ifp) {
  struct vio_softc *sc = ifp->if_softc;
  for (int i = 0; i < 4; i++) {
    if (vio_irq_sc[i] == sc)
      break;
    if (!vio_irq_sc[i]) {
      vio_irq_sc[i] = sc;
      break;
    }
  }
  irq_register(sc->irq, vio_irq);
  sc->rxq.avail[0] = 0; /* interrupts for received packets */
}

static int vio_match(struct device *parent, void *match, void *aux) {
  struct virtio_attach_args *va = aux;
  (void)parent;
  (void)match;
  return va->va_devid == VIRTIO_ID_NETWORK;
}

static void vio_fail(uint16_t io, const char *why) {
  outb((uint16_t)(io + VIRTIO_R_STATUS), VIRTIO_S_FAILED);
  printf(": %s\n", why);
}

static void vio_attach(struct device *parent, struct device *self, void *aux) {
  struct virtio_attach_args *va = aux;
  struct pci_dev *d = va->va_pd;
  (void)parent;
  (void)self;
  uint16_t io = (uint16_t)(d->bar[0] & ~3U); /* (virtio_match: an I/O BAR) */
  pci_write(d, 0x04, pci_read(d, 0x04) | 0x5); /* I/O space, bus master */

  /* 3.1: reset, acknowledge, driver; then the features (2.2, legacy 32 bits) */
  outb((uint16_t)(io + VIRTIO_R_STATUS), 0);
  outb((uint16_t)(io + VIRTIO_R_STATUS), VIRTIO_S_ACK);
  outb((uint16_t)(io + VIRTIO_R_STATUS), VIRTIO_S_ACK | VIRTIO_S_DRIVER);
  uint32_t feat = inl((uint16_t)(io + VIRTIO_R_DEV_FEATURES));
  outl((uint16_t)(io + VIRTIO_R_GUEST_FEATURES), feat & (VIRTIO_NET_F_MAC | VIRTIO_NET_F_STATUS));

  struct vio_softc *sc = kzalloc(sizeof(*sc));
  if (!sc) {
    vio_fail(io, "out of memory");
    return;
  }
  sc->io = io;
  sc->irq = d->irq;
  sc->status = (feat & VIRTIO_NET_F_STATUS) != 0;
  if (virtio_queue_setup(io, 0, &sc->rxq) || virtio_queue_setup(io, 1, &sc->txq)) {
    vio_fail(io, "no queues");
    return;
  }
  sc->nrx = MIN(sc->rxq.size, NRX);
  sc->ntx = MIN(sc->txq.size, NTX);
  sc->txq.avail[0] = VRING_AVAIL_F_NO_INTERRUPT; /* the transmit path reclaims */
  sc->rxq.avail[0] = VRING_AVAIL_F_NO_INTERRUPT; /* until the interface is up */

  /* the buffers, two to a page */
  for (unsigned i = 0; i < sc->nrx; i += 2) {
    uint64_t pg = alloc_zeroed_page();
    if (!pg) {
      vio_fail(io, "out of memory");
      return;
    }
    sc->rxbuf[i] = pg;
    if (i + 1 < sc->nrx)
      sc->rxbuf[i + 1] = pg + BUFSZ;
  }
  for (unsigned i = 0; i < sc->ntx; i += 2) {
    uint64_t pg = alloc_zeroed_page();
    if (!pg) {
      vio_fail(io, "out of memory");
      return;
    }
    sc->txbuf[i] = pg;
    if (i + 1 < sc->ntx)
      sc->txbuf[i + 1] = pg + BUFSZ;
  }
  /* every receive buffer is posted */
  for (unsigned i = 0; i < sc->nrx; i++) {
    sc->rxq.desc[i].addr = sc->rxbuf[i];
    sc->rxq.desc[i].len = BUFSZ;
    sc->rxq.desc[i].flags = VRING_DESC_F_WRITE; /* the device writes it */
    sc->rxq.desc[i].next = 0;
    virtio_queue_push(&sc->rxq, i);
  }
  for (unsigned i = 0; i < sc->ntx; i++)
    sc->txfree[sc->ntxfree++] = (uint8_t)(sc->ntx - 1 - i);

  /* the address: from the device (5.1.4), else one of our own (5.1.5: a
     random locally administered address) */
  uint8_t mac[6];
  if (feat & VIRTIO_NET_F_MAC)
    for (int i = 0; i < 6; i++)
      mac[i] = inb((uint16_t)(io + VIRTIO_R_CONFIG + i));
  else {
    get_random_bytes(mac, 6);
    mac[0] = (uint8_t)((mac[0] & 0xfe) | 0x02);
  }

  outb((uint16_t)(io + VIRTIO_R_STATUS), VIRTIO_S_ACK | VIRTIO_S_DRIVER | VIRTIO_S_DRIVER_OK);
  virtio_queue_notify(&sc->rxq);

  sc->ifp = if_attach_ether("vio", mac, 1500, vio_xmit, vio_poll, vio_start, sc);
  if (!sc->ifp) {
    vio_fail(io, "no interface");
    return;
  }
  printf(": irq %d, address %02x:%02x:%02x:%02x:%02x:%02x", d->irq, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  if (sc->status && !(inw((uint16_t)(io + VIRTIO_R_CONFIG + 6)) & VIRTIO_NET_S_LINK_UP))
    printf(", link down");
  printf("\n");
}

struct cfdriver vio_cd = {NULL, "vio", DV_IFNET, 0};
struct cfattach vio_ca = {sizeof(struct device), vio_match, vio_attach};
