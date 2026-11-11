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

/* vga.c - 80x25 VGA text console with a small subset of the ANSI/VT100
   escape sequences (cursor movement, erasing, colours).  The console is set
   up early (vga_init); vga0, the device (the PCI adapter), attaches when the machine
   is configured. */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <machine/cpu.h>

#define COLS 80
#define ROWS 25

static uint16_t *vram;
static int row, col;
static uint8_t attr = 0x07;
static int esc_state; /* 0 normal, 1 after ESC, 2 in CSI */
static int esc_args[8], esc_n;
static bool esc_priv;

static void set_cursor(void) {
  unsigned pos = row * COLS + col;
  outb(0x3d4, 14);
  outb(0x3d5, pos >> 8);
  outb(0x3d4, 15);
  outb(0x3d5, pos & 0xff);
}

static void clear_cells(int from, int to) {
  for (int i = from; i < to; i++)
    vram[i] = (uint16_t)(attr << 8 | ' ');
}

static void scroll(void) {
  memmove(vram, vram + COLS, (ROWS - 1) * COLS * 2);
  clear_cells((ROWS - 1) * COLS, ROWS * COLS);
  row = ROWS - 1;
}

void vga_init(void) {
  vram = P2V(0xb8000);
  clear_cells(0, ROWS * COLS);
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
      clear_cells(row * COLS + col, ROWS * COLS);
    else if (a == 1)
      clear_cells(0, row * COLS + col + 1);
    else
      clear_cells(0, ROWS * COLS);
    break;
  case 'K':
    if (a == 0)
      clear_cells(row * COLS + col, row * COLS + COLS);
    else if (a == 1)
      clear_cells(row * COLS, row * COLS + col + 1);
    else
      clear_cells(row * COLS, row * COLS + COLS);
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
  if (row >= ROWS)
    row = ROWS - 1;
  if (col < 0)
    col = 0;
  if (col >= COLS)
    col = COLS - 1;
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
    if (col >= COLS) {
      col = 0;
      row++;
      if (row >= ROWS)
        scroll();
    }
    vram[row * COLS + col] = (uint16_t)(attr << 8 | (unsigned char)c);
    col++;
    return;
  }
  if (col >= COLS) {
    col = 0;
    row++;
  }
  if (row >= ROWS)
    scroll();
}

void vga_write(const char *s, size_t n) {
  if (!vram)
    return;
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

static void vga_attach(struct device *parent, struct device *self, void *aux) {
  (void)parent;
  (void)aux;
  printf(": console (%dx%d)\n", COLS, ROWS);
  config_found(self, NULL, NULL); /* wsdisplay0 */
}

struct cfdriver vga_cd = {NULL, "vga", DV_DULL, 0};
struct cfattach vga_ca = {sizeof(struct device), vga_match, vga_attach};
