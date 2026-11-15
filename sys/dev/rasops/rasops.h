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

/* rasops.h - drawing text cells on a 32-bit framebuffer, in the names of
   OpenBSD's rasops. */
#ifndef _DEV_RASOPS_RASOPS_H_
#define _DEV_RASOPS_RASOPS_H_

#include <sys/types.h>
#include <dev/wsfont/wsfont.h>

/* a cell is a character (low byte) and a VGA colour attribute (high byte:
   the foreground in the low nibble, the background in bits 4-6) */
struct rasops_info {
  uint32_t *ri_bits; /* the framebuffer, 0x00RRGGBB pixels */
  int ri_width, ri_height; /* in pixels; a row of the framebuffer is ri_width long */
  int ri_cols, ri_rows;
  const struct wsdisplay_font *ri_font;
};

void rasops_init(struct rasops_info *ri, uint32_t *bits, int width, int height, const struct wsdisplay_font *font);
void rasops_putchar(struct rasops_info *ri, int row, int col, uint16_t cell, bool inverse);
void rasops_copyrows(struct rasops_info *ri, int src, int dst, int num);

#endif
