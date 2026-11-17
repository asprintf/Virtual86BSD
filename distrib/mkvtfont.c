/* mkvtfont.c - write the XNU iso_font as a vt(4) font file (VFNT0002, as vtfontcvt(1) writes it):
 *   cc -o mkvtfont mkvtfont.c && ./mkvtfont xnu8x16.fnt
 * The font is ISO Latin-1, so the glyph of byte N is the one of the code point U+00N. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "fonts/iso_font_8x16.h"

static void be32(unsigned char *p, uint32_t v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }
static void be16(unsigned char *p, uint16_t v) { p[0] = v >> 8; p[1] = v; }

int main(int argc, char **argv)
{
	unsigned char hdr[32], map[8], g[256 * 16];
	FILE *f;
	int i, b;
	if (argc != 2) { fprintf(stderr, "usage: mkvtfont file.fnt\n"); return 2; }
	memset(hdr, 0, sizeof hdr);
	memcpy(hdr, "VFNT0002", 8);
	hdr[8] = 8; hdr[9] = 16;
	be32(hdr + 12, 256);	/* glyphs */
	be32(hdr + 16, 1);	/* one run in the normal map, none in the others */
	for (i = 0; i < 256 * 16; i++) {	/* vt wants the leftmost pixel in the top bit */
		unsigned char c = iso_font_8x16[i], r = 0;
		for (b = 0; b < 8; b++)
			if (c & (1 << b)) r |= 0x80 >> b;
		g[i] = r;
	}
	be32(map, 0); be16(map + 4, 0); be16(map + 6, 255);	/* U+0000.. -> glyph 0.., 256 of them */
	if (!(f = fopen(argv[1], "wb"))) { perror(argv[1]); return 1; }
	if (fwrite(hdr, sizeof hdr, 1, f) != 1 || fwrite(g, sizeof g, 1, f) != 1 || fwrite(map, sizeof map, 1, f) != 1 || fclose(f)) { perror(argv[1]); return 1; }
	return 0;
}
