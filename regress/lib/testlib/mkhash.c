/* mkhash - print crypt(3) of a password with a salt: mkhash PASSWORD SALT
 * (for tests that need an account with a password). */
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
	char *h;
	if (argc != 3) {
		fprintf(stderr, "usage: mkhash PASSWORD SALT\n");
		return 2;
	}
	h = crypt(argv[1], argv[2]);
	if (!h) {
		fprintf(stderr, "mkhash: crypt failed\n");
		return 1;
	}
	puts(h);
	return 0;
}
