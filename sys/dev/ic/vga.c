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

/* vga.c - the console screen: 80x25 VGA text, or, on QEMU's std VGA (Bochs
   VBE), a 1024x768 framebuffer of 128x48 cells drawn with an 8x16 font; both
   with a small subset of the ANSI/VT100 escape sequences (cursor movement,
   erasing, colours).  The text console is set up early (vga_init); vga0, the
   device (the PCI adapter), attaches when the machine is configured and
   switches to the framebuffer. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <uvm/uvm.h>
#include <machine/cpu.h>
#include <dev/rasops/rasops.h>
#include <dev/wsfont/yee8x16.h>

#define TEXT_COLS 80
#define TEXT_ROWS 25
#define FB_WIDTH 1024
#define FB_HEIGHT 768
#define FB_COLS (FB_WIDTH / 8) /* the font is 8x16 */
#define FB_ROWS (FB_HEIGHT / 16)

static int cols = TEXT_COLS, rows = TEXT_ROWS;
static uint16_t *vram; /* the cells: the text screen, or fbcells */
static uint16_t fbcells[FB_COLS * FB_ROWS];
static struct rasops_info ri; /* the framebuffer, once fb_on */
static bool fb_on;
static int fb_cursor = -1; /* the cell the cursor is drawn on */
static int row, col;
static uint8_t attr = 0x07;
static int esc_state; /* 0 normal, 1 after ESC, 2 in CSI */
static int esc_args[8], esc_n;
static bool esc_priv;

static void draw_cell(int i, bool inverse) {
  rasops_putchar(&ri, i / cols, i % cols, vram[i], inverse);
}

static void set_cell(int i, uint16_t v) {
  vram[i] = v;
  if (fb_on)
    draw_cell(i, false);
}

static void set_cursor(void) {
  if (fb_on) {
    if (fb_cursor >= 0)
      draw_cell(fb_cursor, false);
    fb_cursor = row * cols + col;
    draw_cell(fb_cursor, true);
    return;
  }
  unsigned pos = row * cols + col;
  outb(0x3d4, 14);
  outb(0x3d5, pos >> 8);
  outb(0x3d4, 15);
  outb(0x3d5, pos & 0xff);
}

static void clear_cells(int from, int to) {
  for (int i = from; i < to; i++)
    set_cell(i, (uint16_t)(attr << 8 | ' '));
}

static void scroll(void) {
  memmove(vram, vram + cols, (rows - 1) * cols * 2);
  if (fb_on)
    rasops_copyrows(&ri, 1, 0, rows - 1);
  clear_cells((rows - 1) * cols, rows * cols);
  row = rows - 1;
}

void vga_init(void) {
  vram = P2V(0xb8000);
  clear_cells(0, rows * cols);
  row = col = 0;
  set_cursor();
}

static const uint8_t ansi2vga[8] = {0, 4, 2, 6, 1, 5, 3, 7};

static void csi(char c) {
  int a = esc_n ? esc_args[0] : 0;
  int b = esc_n > 1 ? esc_args[1] : 0;
  switch (c) {
  case 'H':
  case 'f':
    row = (a ? a : 1) - 1;
    col = (b ? b : 1) - 1;
    break;
  case 'A':
    row -= a ? a : 1;
    break;
  case 'B':
    row += a ? a : 1;
    break;
  case 'C':
    col += a ? a : 1;
    break;
  case 'D':
    col -= a ? a : 1;
    break;
  case 'J':
    if (a == 0)
      clear_cells(row * cols + col, rows * cols);
    else if (a == 1)
      clear_cells(0, row * cols + col + 1);
    else
      clear_cells(0, rows * cols);
    break;
  case 'K':
    if (a == 0)
      clear_cells(row * cols + col, row * cols + cols);
    else if (a == 1)
      clear_cells(row * cols, row * cols + col + 1);
    else
      clear_cells(row * cols, row * cols + cols);
    break;
  case 'm':
    if (esc_n == 0)
      attr = 0x07;
    for (int i = 0; i < esc_n; i++) {
      int v = esc_args[i];
      if (v == 0)
        attr = 0x07;
      else if (v == 1)
        attr |= 0x08;
      else if (v == 7)
        attr = (uint8_t)((attr & 0x88) | (attr & 0x70) >> 4 | (attr & 0x07) << 4);
      else if (v >= 30 && v <= 37)
        attr = (uint8_t)((attr & 0xf8) | ansi2vga[v - 30]);
      else if (v >= 40 && v <= 47)
        attr = (uint8_t)((attr & 0x8f) | ansi2vga[v - 40] << 4);
    }
    break;
  }
  if (row < 0)
    row = 0;
  if (row >= rows)
    row = rows - 1;
  if (col < 0)
    col = 0;
  if (col >= cols)
    col = cols - 1;
}

static void putc(char c) {
  if (esc_state == 1) {
    if (c == '[') {
      esc_state = 2;
      esc_n = 0;
      esc_priv = false;
      memset(esc_args, 0, sizeof(esc_args));
    } else
      esc_state = 0;
    return;
  }
  if (esc_state == 2) {
    if (c >= '0' && c <= '9') {
      if (esc_n == 0)
        esc_n = 1;
      esc_args[esc_n - 1] = esc_args[esc_n - 1] * 10 + (c - '0');
    } else if (c == ';') {
      if (esc_n == 0)
        esc_n = 1;
      if (esc_n < 8)
        esc_n++;
    } else if (c == '?')
      esc_priv = true;
    else {
      esc_state = 0;
      if (!esc_priv)
        csi(c);
    }
    return;
  }
  switch (c) {
  case 033:
    esc_state = 1;
    return;
  case '\n':
    row++;
    break;
  case '\r':
    col = 0;
    break;
  case '\b':
    if (col > 0)
      col--;
    break;
  case '\t':
    col = (col + 8) & ~7;
    break;
  case '\a':
    break;
  default:
    if ((unsigned char)c < 32)
      break;
    if (col >= cols) {
      col = 0;
      row++;
      if (row >= rows)
        scroll();
    }
    set_cell(row * cols + col, (uint16_t)(attr << 8 | (unsigned char)c));
    col++;
    return;
  }
  if (col >= cols) {
    col = 0;
    row++;
  }
  if (row >= rows)
    scroll();
}

void vga_write(const char *s, size_t n) {
  if (!vram)
    return;
  if (fb_on && fb_cursor >= 0) {
    draw_cell(fb_cursor, false);
    fb_cursor = -1;
  }
  for (size_t i = 0; i < n; i++)
    putc(s[i]);
  set_cursor();
}

void console_write(const char *s, size_t n) {
  /* the console is a terminal: a bare \n there is \r\n, on the serial port
     and on the screen (whose \n only goes down a row) */
  size_t start = 0;
  for (size_t i = 0; i < n; i++)
    if (s[i] == '\n') {
      serial_write(s + start, i - start);
      serial_write("\r\n", 2);
      vga_write(s + start, i - start);
      vga_write("\r\n", 2);
      start = i + 1;
    }
  serial_write(s + start, n - start);
  vga_write(s + start, n - start);
}

/* a PCI display adapter of the VGA kind (the text console itself is the
   fixed legacy VGA: registers 0x3b0-0x3df, memory 0xa0000-0xbffff, which
   the adapter answers at; vga_init has set it up) */
static int vga_match(struct device *parent, void *match, void *aux) {
  struct pci_dev *d = aux;
  (void)parent;
  (void)match;
  return PCI_CLASS(d->class) == PCI_CLASS_DISPLAY && PCI_SUBCLASS(d->class) == PCI_SUBCLASS_DISPLAY_VGA;
}

/* Bochs VBE, the interface of QEMU's std VGA: registers at ports 0x1ce/0x1cf */
#define DISPI_XRES 1
#define DISPI_YRES 2
#define DISPI_BPP 3
#define DISPI_ENABLE 4
#define DISPI_VIRT_WIDTH 6

static void dispi_write(int reg, int v) {
  outw(0x1ce, reg);
  outw(0x1cf, v);
}

static int dispi_read(int reg) {
  outw(0x1ce, reg);
  return inw(0x1cf);
}

/* a linear 32-bit mode, then the text screen is redrawn in it */
static bool fb_init(struct pci_dev *d) {
  if (d->vendor != PCI_VENDOR_BOCHS || d->device != PCI_PRODUCT_BOCHS_VGA || (dispi_read(0) & 0xfff0) != 0xb0c0)
    return false;
  uint64_t phys = d->bar[0] & ~0xfU;
  if (((d->bar[0] >> 1) & 3) == 2) /* 64-bit */
    phys |= (uint64_t)d->bar[1] << 32;
  if (!phys)
    return false;
  /* the text screen is gone once the mode is set: take its cells first */
  for (int i = 0; i < FB_COLS * FB_ROWS; i++)
    fbcells[i] = 0x0720;
  for (int r = 0; r < TEXT_ROWS; r++)
    memcpy(fbcells + r * FB_COLS, vram + r * TEXT_COLS, TEXT_COLS * 2);
  pci_write(d, 0x04, pci_read(d, 0x04) | 0x2); /* memory */
  dispi_write(DISPI_ENABLE, 0);
  dispi_write(DISPI_XRES, FB_WIDTH);
  dispi_write(DISPI_YRES, FB_HEIGHT);
  dispi_write(DISPI_BPP, 32);
  dispi_write(DISPI_ENABLE, 0x41); /* enabled, linear */
  uint32_t *p = NULL;
  if (dispi_read(DISPI_XRES) == FB_WIDTH && dispi_read(DISPI_YRES) == FB_HEIGHT && dispi_read(DISPI_BPP) == 32 &&
      dispi_read(DISPI_VIRT_WIDTH) == FB_WIDTH)
    p = ioremap_wb(phys, FB_WIDTH * FB_HEIGHT * 4);
  if (!p) {
    dispi_write(DISPI_ENABLE, 0); /* back to the text mode */
    return false;
  }
  rasops_init(&ri, p, FB_WIDTH, FB_HEIGHT, &yee8x16);
  vram = fbcells;
  cols = ri.ri_cols;
  rows = ri.ri_rows;
  fb_on = true;
  fb_cursor = -1;
  for (int i = 0; i < cols * rows; i++)
    draw_cell(i, false);
  set_cursor();
  return true;
}

static void vga_attach(struct device *parent, struct device *self, void *aux) {
  (void)parent;
  if (fb_init(aux))
    printf(": framebuffer %dx%d, console (%dx%d)\n", FB_WIDTH, FB_HEIGHT, cols, rows);
  else
    printf(": console (%dx%d)\n", cols, rows);
  config_found(self, NULL, NULL); /* wsdisplay0 */
}

struct cfdriver vga_cd = {NULL, "vga", DV_DULL, 0};
struct cfattach vga_ca = {sizeof(struct device), vga_match, vga_attach};
