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

/* rasops32.c - text cells on a framebuffer of 32 bits per pixel. */
#include <sys/param.h>
#include <sys/systm.h>
#include <dev/rasops/rasops.h>

/* the 16 colours of the VGA text modes, but 7, the default text, is white */
static const uint32_t rasops_cmap[16] = {0x000000, 0x0000aa, 0x00aa00, 0x00aaaa, 0xaa0000, 0xaa00aa, 0xaa5500, 0xffffff,
                                         0x555555, 0x5555ff, 0x55ff55, 0x55ffff, 0xff5555, 0xff55ff, 0xffff55, 0xffffff};

void rasops_init(struct rasops_info *ri, uint32_t *bits, int width, int height, const struct wsdisplay_font *font) {
  ri->ri_bits = bits;
  ri->ri_width = width;
  ri->ri_height = height;
  ri->ri_font = font;
  ri->ri_cols = width / font->fontwidth;
  ri->ri_rows = height / font->fontheight;
}

void rasops_putchar(struct rasops_info *ri, int row, int col, uint16_t cell, bool inverse) {
  const struct wsdisplay_font *f = ri->ri_font;
  uint32_t fg = rasops_cmap[cell >> 8 & 15], bg = rasops_cmap[cell >> 12 & 7];
  if (inverse) {
    uint32_t t = fg;
    fg = bg;
    bg = t;
  }
  const unsigned char *g = f->data + (cell & 0xff) * f->fontheight * f->stride;
  uint32_t *p = ri->ri_bits + row * f->fontheight * ri->ri_width + col * f->fontwidth;
  for (int y = 0; y < f->fontheight; y++, g += f->stride, p += ri->ri_width)
    for (int x = 0; x < f->fontwidth; x++) {
      int bit = f->bitorder == WSDISPLAY_FONTORDER_R2L ? x : 7 - x;
      p[x] = g[0] >> bit & 1 ? fg : bg;
    }
}

void rasops_copyrows(struct rasops_info *ri, int src, int dst, int num) {
  int h = ri->ri_font->fontheight;
  memmove(ri->ri_bits + dst * h * ri->ri_width, ri->ri_bits + src * h * ri->ri_width, (size_t)num * h * ri->ri_width * 4);
}
