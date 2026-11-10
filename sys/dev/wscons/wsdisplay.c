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

/* wsdisplay.c - the console display as a device, wsdisplay0: it only says
   what the console is (the VGA text screen with a vt100 terminal, /dev/ttyC0,
   is vga.c and kern/tty.c), and the keyboard it is used with. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

extern struct cfdriver wskbd_cd;

static int wsdisplay_match(struct device *parent, void *match, void *aux) {
  (void)parent;
  (void)match;
  (void)aux;
  return 1; /* only a display (vga) offers one */
}

static void wsdisplay_attach(struct device *parent, struct device *self, void *aux) {
  (void)parent;
  (void)self;
  (void)aux;
  printf(": console (std, vt100 emulation)");
  if (wskbd_cd.cd_ndevs > 0 && wskbd_cd.cd_devs[0])
    printf(", using %s", wskbd_cd.cd_devs[0]->dv_xname);
  printf("\n");
}

struct cfdriver wsdisplay_cd = {NULL, "wsdisplay", DV_DULL, 0};
struct cfattach wsdisplay_ca = {sizeof(struct device), wsdisplay_match, wsdisplay_attach};
