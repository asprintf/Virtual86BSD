/* Differential: crypt() for the traditional and extended DES, MD5,
 * SHA-256 and SHA-512 schemes (the reference is libxcrypt; a failure is
 * printed as "fail", whether it is a null pointer or libxcrypt's "*0").
 * (libxcrypt rejects rounds=N below 1000, which glibc and this library
 * raise to 1000, as the specification of the SHA schemes says.)
 * REFLIBS: -lcrypt */
#define _XOPEN_SOURCE 800
#include <unistd.h>
#include <string.h>
#include "out.h"

char *crypt(const char *, const char *);

static void t(const char *key, const char *salt)
{
	char *r = crypt(key, salt);
	os(salt); oc(' ');
	os(!r || *r == '*' ? "fail" : r);
	onl();
}

int main(void)
{
	static const char *keys[] = {
		"", "a", "password", "12345678", "123456789", "exactly8",
		"a much longer passphrase than eight characters",
		"\x80\xff\xfe high bytes", "Hello world!", "x",
	};
	static const char *salts[] = {
		"ab", "./", "zz", "Xy", "9.",
		"_J9..rasm", "_...........", "_K1..crsm", "_./..abcd", "_z...wxyz", "_zz..wxyz",
		"$1$", "$1$saltstri", "$1$saltstring$", "$1$a$", "$1$12345678", "$1$.$",
		"$5$saltstring", "$5$rounds=10000$saltstringsaltstring", "$5$rounds=5000$toolongsaltstring",
		"$5$rounds=1400$anotherlongsaltstring", "$5$short",
		"$6$saltstring", "$6$rounds=10000$saltstringsaltstring", "$6$rounds=5000$toolongsaltstring",
		"$6$rounds=1400$anotherlongsaltstring", "$6$rounds=1000$roundstoolow", "$6$",
		"$6$a$", "$5$a$", "$6$rounds=1000$", "$5$rounds=2000$x",
	};
	static const char b64[] = "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
	unsigned i, j;
	char k[64], s[24];
	for (i = 0; i < sizeof keys / sizeof *keys; i++) {
		os(keys[i]); onl();
		for (j = 0; j < sizeof salts / sizeof *salts; j++)
			t(keys[i], salts[j]);
	}
	/* random keys and salts */
	for (i = 0; i < 300; i++) {
		unsigned n = (unsigned)(rng() % 40), m;
		for (j = 0; j < n; j++)
			k[j] = (char)(rng() % 255 + 1);
		k[n] = 0;
		switch (i % 5) {
		case 0:
			s[0] = b64[rng() % 64]; s[1] = b64[rng() % 64]; s[2] = 0;
			break;
		case 1:
			s[0] = '_';
			for (j = 1; j < 9; j++) s[j] = b64[rng() % 64];
			s[1] = b64[rng() % 8 + 1]; s[2] = s[3] = s[4] = '.';   /* a small count */
			s[9] = 0;
			break;
		default:
			s[0] = '$'; s[1] = "156"[i % 5 - 2]; s[2] = '$';
			m = (unsigned)(rng() % 17);
			for (j = 0; j < m; j++) s[3 + j] = b64[rng() % 64];
			s[3 + m] = 0;
			break;
		}
		t(k, s);
	}
	oflush();
	return 0;
}
