/*
 * Copyright (c) 2026, Joost Orchidee Winter
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
