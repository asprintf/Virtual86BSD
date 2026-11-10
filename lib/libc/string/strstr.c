/* strstr: Two-Way algorithm (Crochemore & Perrin), guaranteed linear time. */
#include <string.h>
#include <stdint.h>

#define MAX(a,b) ((a) > (b) ? (a) : (b))

static char *twoway(const unsigned char *h, const unsigned char *n)
{
	size_t l, ip, jp, k, p, ms, p0, mem, mem0, grow;
	const unsigned char *z;
	size_t shift[256];

	for (l = 0; n[l] && h[l]; l++);
	if (n[l]) return 0; /* haystack shorter than needle */

	/* shift[c]: 1 + index of the last occurrence of c in the needle, or 0. */
	for (k = 0; k < 256; k++) shift[k] = 0;
	for (k = 0; k < l; k++) shift[n[k]] = k + 1;

	/* Maximal suffix for <. */
	ip = (size_t)-1; jp = 0; k = p = 1;
	while (jp + k < l) {
		if (n[ip + k] == n[jp + k]) {
			if (k == p) { jp += p; k = 1; } else k++;
		} else if (n[ip + k] > n[jp + k]) {
			jp += k; k = 1; p = jp - ip;
		} else {
			ip = jp++; k = p = 1;
		}
	}
	ms = ip; p0 = p;
	/* Maximal suffix for >. */
	ip = (size_t)-1; jp = 0; k = p = 1;
	while (jp + k < l) {
		if (n[ip + k] == n[jp + k]) {
			if (k == p) { jp += p; k = 1; } else k++;
		} else if (n[ip + k] < n[jp + k]) {
			jp += k; k = 1; p = jp - ip;
		} else {
			ip = jp++; k = p = 1;
		}
	}
	if (ip + 1 > ms + 1) ms = ip; else p = p0;

	if (memcmp(n, n + p, ms + 1)) { mem0 = 0; p = MAX(ms, l - ms - 1) + 1; }
	else mem0 = l - p;
	mem = 0;

	z = h;
	for (;;) {
		/* Make sure at least l bytes of haystack are known to exist. */
		if (z - h < (long)l) {
			const unsigned char *z2;
			grow = l | 63;
			z2 = memchr(z, 0, grow);
			if (z2) { z = z2; if (z - h < (long)l) return 0; }
			else z += grow;
		}
		/* Bad-character shift on the last byte. */
		if (shift[h[l - 1]]) {
			k = l - shift[h[l - 1]];
			if (k) {
				if (k < mem) k = mem;
				h += k; mem = 0; continue;
			}
		} else { h += l; mem = 0; continue; }

		/* Compare right half. */
		for (k = MAX(ms + 1, mem); n[k] && n[k] == h[k]; k++);
		if (n[k]) { h += k - ms; mem = 0; continue; }
		/* Compare left half. */
		for (k = ms + 1; k > mem && n[k - 1] == h[k - 1]; k--);
		if (k <= mem) return (char *)h;
		h += p; mem = mem0;
	}
}

char *strstr(const char *h, const char *n)
{
	if (!n[0]) return (char *)h;
	h = strchr(h, *n);
	if (!h || !n[1]) return (char *)h;
	if (!h[1]) return 0;
	return twoway((const unsigned char *)h, (const unsigned char *)n);
}
