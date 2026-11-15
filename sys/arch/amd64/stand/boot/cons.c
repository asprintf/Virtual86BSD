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

/* cons.c - the console: the screen (by the BIOS) and the first serial port
   take the output; the keyboard and the serial port give the input.  Both
   always work, so the loader is there on the window of QEMU and on the
   terminal of -nographic. */
#include "boot.h"

#define COM1 0x3f8

static int have_serial;
static void screen_init(void);

uint8_t inb(uint16_t p) {
	uint8_t v;
	__asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(p));
	return v;
}

void outb(uint16_t p, uint8_t v) {
	__asm__ volatile("outb %0, %1" : : "a"(v), "Nd"(p));
}

void cons_init(void) {
	outb(COM1 + 7, 0xaa);
	have_serial = inb(COM1 + 7) == 0xaa;
	screen_init();
	if (!have_serial)
		return;
	outb(COM1 + 1, 0);
	outb(COM1 + 3, 0x80);	/* the divisor: 115200 baud */
	outb(COM1 + 0, 1);
	outb(COM1 + 1, 0);
	outb(COM1 + 3, 3);	/* 8N1 */
	outb(COM1 + 2, 0xc7);
	outb(COM1 + 4, 3);
}

/* the screen: the text memory of the VGA, written here and not through the
   BIOS (which, with a serial console, would echo to the port as well); the
   cursor starts where the BIOS has it (its data area, which stays as it is:
   a BIOS that mirrors the screen to the port would send cursor movements) */
#define VGAMEM ((volatile uint16_t *)phys(0xb8000))
#define BDA_MODE (*(volatile uint8_t *)phys(0x449))
#define BDA_COLS (*(volatile uint16_t *)phys(0x44a))
#define BDA_COL (*(volatile uint8_t *)phys(0x450))
#define BDA_ROW (*(volatile uint8_t *)phys(0x451))
#define BDA_CRTC (*(volatile uint16_t *)phys(0x463))
#define ROWS 25
#define COLS 80

static int row, col, screen;

static void screen_init(void) {
	if (BDA_MODE != 3 || BDA_COLS != COLS) {
		struct biosregs r = {.eax = 3};	/* 80x25 text */
		bioscall(0x10, &r);
	}
	col = BDA_COL < COLS ? BDA_COL : 0;
	row = BDA_ROW < ROWS ? BDA_ROW : 0;
	screen = 1;
}

static void cursor(void) {
	uint16_t pos = (uint16_t)(row * COLS + col), crtc = BDA_CRTC ? BDA_CRTC : 0x3d4;
	outb(crtc, 0x0e);
	outb(crtc + 1, (uint8_t)(pos >> 8));
	outb(crtc, 0x0f);
	outb(crtc + 1, (uint8_t)pos);
}

static void newline(void) {
	col = 0;
	if (++row < ROWS)
		return;
	row = ROWS - 1;
	for (int i = 0; i < (ROWS - 1) * COLS; i++)
		VGAMEM[i] = VGAMEM[i + COLS];
	for (int i = 0; i < COLS; i++)
		VGAMEM[(ROWS - 1) * COLS + i] = 0x0720;
}

static void screen_put(int c) {
	if (c == '\n')
		newline();
	else if (c == '\r')
		col = 0;
	else if (c == '\b') {
		if (col)
			col--;
	} else {
		VGAMEM[row * COLS + col] = (uint16_t)(0x0700 | (uint8_t)c);
		if (++col == COLS)
			newline();
	}
}

/* what comes in on the port while the loader is busy: its FIFO has 16
   bytes, and a line typed ahead (or pasted) would be lost, with some of QEMU's
   serial consoles, once the FIFO is full */
static uint8_t rxq[256];
static unsigned rxhead, rxtail;

static void rx_drain(void) {
	while (have_serial && (inb(COM1 + 5) & 1)) {
		uint8_t c = inb(COM1);
		if (rxtail - rxhead < sizeof(rxq))
			rxq[rxtail++ % sizeof(rxq)] = c;
	}
}

static void put1(int c) {
	if (have_serial) {
		for (int i = 0; i < 100000 && !(inb(COM1 + 5) & 0x20); i++)
			rx_drain();
		outb(COM1, (uint8_t)c);
		rx_drain();
	}
	if (screen) {
		screen_put(c);
		cursor();
	}
}

void putchar(int c) {
	if (c == '\t') {
		do
			putchar(' ');
		while (col & 7);
		return;
	}
	if (c == '\n')
		put1('\r');
	put1(c);
}

int trygetchar(void) {
	rx_drain();
	if (rxhead != rxtail)
		return rxq[rxhead++ % sizeof(rxq)];
	struct biosregs r = {.eax = 0x0100};
	bioscall(0x16, &r);
	if (r.flags & ZF)
		return -1;
	r.eax = 0;
	bioscall(0x16, &r);
	return (int)(r.eax & 0xff) ? (int)(r.eax & 0xff) : -1;
}

/* the BIOS's clock: ticks of 1/18.2 s since midnight */
uint32_t ticks(void) {
	struct biosregs r = {.eax = 0};
	bioscall(0x1a, &r);
	return r.ecx << 16 | (r.edx & 0xffff);
}

/* a line of at most n - 1 characters; -1 if timeout ticks pass without a
   key (timeout < 0: wait for ever) */
int readline(char *buf, int n, int timeout) {
	int len = 0;
	uint32_t t0 = ticks();
	for (;;) {
		int c = trygetchar();
		if (c < 0) {
			if (timeout >= 0 && ticks() - t0 > (uint32_t)timeout)
				return -1;
			continue;
		}
		timeout = -1;
		if (c == '\r' || c == '\n') {
			putchar('\n');
			buf[len] = 0;
			return len;
		}
		if (c == 8 || c == 127) {
			if (len) {
				len--;
				printf("\b \b");
			}
		} else if (c == 21) {
			while (len) {
				len--;
				printf("\b \b");
			}
		} else if (c >= ' ' && c < 127 && len < n - 1) {
			buf[len++] = (char)c;
			putchar(c);
		}
	}
}
