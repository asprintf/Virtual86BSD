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

/* sockio.h - the socket ioctls (interfaces, routes).  The names are the
   BSDs'; the numbers of the names Linux has too are Linux's (so a Linux
   program works under the compat layer), SIOCAIFADDR and SIOCDIFADDR are
   in the range Linux leaves to protocols.  The argument is a struct ifreq
   (<net/if.h>) unless said otherwise; the setting ones are for root. */
#ifndef _SYS_SOCKIO_H_
#define _SYS_SOCKIO_H_

#define SIOCADDRT 0x890b      /* add a route: struct rtentry (<net/route.h>) */
#define SIOCDELRT 0x890c      /* delete a route: struct rtentry */
#define SIOCATMARK 0x8905     /* int: at the out-of-band mark */
#define SIOCGIFNAME 0x8910    /* ifr_index -> ifr_name */
#define SIOCGIFCONF 0x8912    /* struct ifconf: an entry per address */
#define SIOCGIFFLAGS 0x8913   /* ifr_flags */
#define SIOCSIFFLAGS 0x8914   /* only IFF_UP, IFF_DEBUG and IFF_LINK0-2 change */
#define SIOCGIFADDR 0x8915    /* ifr_addr: the first address */
#define SIOCSIFADDR 0x8916    /* replace the first address (keeps its mask) */
#define SIOCGIFDSTADDR 0x8917 /* (no point-to-point interfaces: EOPNOTSUPP) */
#define SIOCSIFDSTADDR 0x8918
#define SIOCGIFBRDADDR 0x8919 /* ifr_broadaddr: address | ~mask */
#define SIOCSIFBRDADDR 0x891a /* only that same address is accepted */
#define SIOCGIFNETMASK 0x891b /* ifr_addr: the mask of the first address */
#define SIOCSIFNETMASK 0x891c
#define SIOCGIFMETRIC 0x891d  /* ifr_metric: that of its connected routes */
#define SIOCSIFMETRIC 0x891e
#define SIOCGIFMTU 0x8921     /* ifr_mtu */
#define SIOCSIFMTU 0x8922     /* 68 up to the card's own */
#define SIOCGIFHWADDR 0x8927  /* ifr_hwaddr: family 1 (Ethernet) and the address */
#define SIOCGIFINDEX 0x8933   /* ifr_name -> ifr_index */
#define SIOCAIFADDR 0x89e0    /* struct ifaliasreq: add an address */
#define SIOCDIFADDR 0x89e1    /* ifr_addr: delete that address */

#endif
