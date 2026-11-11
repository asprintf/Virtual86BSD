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

/* passwd - change a password (the passwd(1) of the BSDs, reduced).
 *
 *   passwd [user]
 *
 * Changes the password of the user (default: the one running it) in
 * /etc/master.passwd.  Root may change any, and is not asked for the old one;
 * anyone else only their own, after the old password (unless it is empty) and
 * with a new one of at least 6 characters.  The new password is typed twice
 * (3 tries); an empty one stops it.  The hash is SHA-512 (crypt(3) `$6$')
 * with a random salt.  The file is replaced whole, atomically; passwd is
 * installed set-user-ID root so that anyone can change their own. */
#define _POSIX_C_SOURCE 202405L
#define _XOPEN_SOURCE 800
#define _DEFAULT_SOURCE
#include <errno.h>
#include <mpasswd.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define MIN_LEN 6
#define TRIES 3

static void usage(void)
{
	fprintf(stderr, "usage: passwd [user]\n");
	exit(1);
}

static void make_salt(char *salt, size_t size)
{
	static const char alpha[] = "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
	unsigned char r[16];
	size_t i;
	if (getentropy(r, sizeof r) != 0) {
		fprintf(stderr, "passwd: no random numbers: %s\n", strerror(errno));
		exit(1);
	}
	strcpy(salt, "$6$");
	for (i = 0; i < sizeof r; i++)
		salt[3 + i] = alpha[r[i] & 63];
	salt[3 + sizeof r] = '$';
	salt[4 + sizeof r] = 0;
	(void)size;
}

int main(int argc, char *argv[])
{
	struct spw pw;
	struct passwd *me;
	const char *name;
	char first[256], salt[32], *pass, *hash;
	int tries, r;
	uid_t ruid = getuid();

	if (argc > 2 || (argc == 2 && argv[1][0] == '-' && strcmp(argv[1], "--")))
		usage();
	if (argc == 2 && !strcmp(argv[1], "--"))
		usage();
	if (argc == 2) {
		name = argv[1];
	} else {
		if (!(me = getpwuid(ruid))) {
			fprintf(stderr, "passwd: no user with your id\n");
			return 1;
		}
		name = me->pw_name;
	}
	r = spw_lookup(PATH_MASTER_PASSWD, name, &pw);
	if (r < 0) {
		fprintf(stderr, "passwd: %s: %s\n", PATH_MASTER_PASSWD, strerror(errno));
		return 1;
	}
	if (r == 0) {
		fprintf(stderr, "passwd: %s: no such user\n", name);
		return 1;
	}
	if (ruid != 0 && (uid_t)pw.uid != ruid) {
		fprintf(stderr, "passwd: you may only change your own password\n");
		return 1;
	}
	if (pw.hash[0] == '*' || pw.hash[0] == '!') {
		fprintf(stderr, "passwd: the account of %s is locked\n", name);
		return 1;
	}
	printf("Changing password for %s.\n", name);
	if (ruid != 0 && pw.hash[0]) {
		pass = getpass("Old password:");
		if (!pass || strcmp(crypt(pass, pw.hash), pw.hash) != 0) {
			fprintf(stderr, "passwd: incorrect password\n");
			return 1;
		}
		memset(pass, 0, strlen(pass));
	}
	for (tries = 0; tries < TRIES; tries++) {
		pass = getpass("New password:");
		if (!pass || !pass[0]) {
			fprintf(stderr, "passwd: password unchanged\n");
			return 1;
		}
		if (ruid != 0 && strlen(pass) < MIN_LEN) {
			fprintf(stderr, "Password is too short: at least %d characters.\n", MIN_LEN);
			continue;
		}
		snprintf(first, sizeof first, "%s", pass);
		memset(pass, 0, strlen(pass));
		pass = getpass("Retype new password:");
		if (!pass || strcmp(pass, first) != 0) {
			fprintf(stderr, "Mismatch; try again, EOF to quit.\n");
			memset(first, 0, sizeof first);
			continue;
		}
		memset(pass, 0, strlen(pass));
		make_salt(salt, sizeof salt);
		hash = crypt(first, salt);
		memset(first, 0, sizeof first);
		if (!hash || hash[0] == '*') {
			fprintf(stderr, "passwd: cannot make the hash\n");
			return 1;
		}
		if (spw_sethash(PATH_MASTER_PASSWD, name, hash) < 0) {
			if (errno == EEXIST)
				fprintf(stderr, "passwd: the password file is busy, try again\n");
			else
				fprintf(stderr, "passwd: %s: %s\n", PATH_MASTER_PASSWD, strerror(errno));
			return 1;
		}
		return 0;
	}
	fprintf(stderr, "passwd: too many tries; password unchanged\n");
	return 1;
}
