/* if_var.h - network interfaces, and IP as the drivers and the rest see it. */
#ifndef _NET_IF_VAR_H_
#define _NET_IF_VAR_H_

#include <sys/systm.h>

/* the interface (netinet/ip.c) */
struct netif {
  bool present, configured;
  uint8_t mac[6];
  uint32_t addr, mask, gw, dns[3]; /* host order */
  int ndns;
  char domain[64];
  bool dhcp;
  uint64_t rx_packets, rx_bytes, tx_packets, tx_bytes, rx_dropped, tx_dropped;
  int (*xmit)(const void *frame, size_t len);
};
extern struct netif eth0;
void net_init_conf(const char *ipconf, const char *loss); /* ip=, netloss= */
void net_pnp(void (*out)(void *, const char *, ...), void *sb);
void net_rx(const void *frame, size_t len);  /* a driver: a frame came (from the network thread) */
void net_wake(void);                         /* (any context) */
bool ip_is_local(uint32_t ip);               /* one of this system's addresses */
bool ip_routable(uint32_t ip);               /* reachable through eth0 */
int ip_output(int proto, uint32_t src, uint32_t dst, const void *payload, size_t len);
int ip_output_raw(void *pkt, size_t len);    /* a whole IPv4 packet */
uint16_t ip_csum(const void *p, size_t n, uint32_t sum);
uint32_t ip_pseudo(uint32_t src, uint32_t dst, int proto, size_t len);
uint64_t net_now_ms(void);

#endif
