/* vga.c - 80x25 VGA text console with a small subset of the ANSI/VT100
   escape sequences (cursor movement, erasing, colours). */
#include "kernel.h"
#include "arch.h"
#include "dev.h"

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
  /* the console is a terminal: a bare \n there is \r\n */
  size_t start = 0;
  for (size_t i = 0; i < n; i++)
    if (s[i] == '\n') {
      serial_write(s + start, i - start);
      serial_write("\r\n", 2);
      start = i + 1;
    }
  serial_write(s + start, n - start);
  vga_write(s, n);
}
