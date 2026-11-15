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

/* cmd.c - the boot> prompt, as OpenBSD's loader has it: the first line
   starts with a timeout, after which the default kernel is booted. */
#include "boot.h"

#define TIMEOUT (5 * 182 / 10)	/* 5 s in ticks */

static int split(char *line, char **argv, int max) {
	int argc = 0;
	for (char *p = line; *p;) {
		while (*p == ' ' || *p == '\t')
			*p++ = 0;
		if (!*p)
			break;
		if (argc < max)
			argv[argc++] = p;
		while (*p && *p != ' ' && *p != '\t')
			p++;
	}
	return argc;
}

static void cmd_boot(int argc, char **argv) {
	int i = 1;
	const char *img = "";
	if (i < argc && argv[i][0] != '-' && !strchr(argv[i], '='))
		img = argv[i++];
	struct spec sp;
	if (spec_parse(img, &sp, &defspec)) {
		printf("boot: %s: not a device and file\n", img);
		return;
	}
	if (!sp.path[0])
		snprintf(sp.path, sizeof(sp.path), "/bsd");
	char args[200];
	args[0] = 0;
	for (; i < argc; i++)
		snprintf(args + strlen(args), sizeof(args) - strlen(args), "%s%s", args[0] ? " " : "", argv[i]);
	boot_kernel(&sp, args);
}

static void cmd_ls(int argc, char **argv) {
	struct spec sp;
	if (spec_parse(argc > 1 ? argv[1] : "", &sp, &defspec)) {
		printf("ls: %s: not a device and directory\n", argv[1]);
		return;
	}
	if (!sp.path[0])
		snprintf(sp.path, sizeof(sp.path), "/");
	struct ffsfile d;
	if (spec_mount(&sp) || ffs_lookup(sp.path, &d)) {
		printf("ls: %s: no such directory\n", argc > 1 ? argv[1] : "/");
		return;
	}
	if ((d.mode & 0170000) != 0040000) {
		printf("ls: %s: not a directory\n", argv[1]);
		return;
	}
	char name[256];
	int isdir;
	for (uint32_t pos = 0; ffs_next(&d, &pos, name, &isdir);)
		printf("%s%s\n", name, isdir ? "/" : "");
}

static void cmd_machine(int argc, char **argv) {
	if (argc > 1 && !strcmp(argv[1], "memory")) {
		for (int i = 0; i < nmem; i++)
			printf("%08x%08x %08x%08x type %u%s\n", (uint32_t)(memmap[i].base >> 32), (uint32_t)memmap[i].base,
			    (uint32_t)(memmap[i].len >> 32), (uint32_t)memmap[i].len, memmap[i].type,
			    memmap[i].type == 1 ? " (usable)" : "");
	} else if (argc > 1 && !strcmp(argv[1], "diskinfo")) {
		for (int i = 0; i < ndisks; i++) {
			if (!disks[i].ok)
				continue;
			printf("disk%d: BIOS drive 0x%x, %u sectors, %s\n", i, disks[i].bios, disks[i].sectors,
			    disks[i].table ? disks[i].table : "no partition table");
			for (int k = 0; k < disks[i].nparts; k++)
				printf("  disk%ds%d: start %u, %u sectors, type 0x%x\n", i, disks[i].parts[k].index,
				    disks[i].parts[k].start, disks[i].parts[k].sectors, disks[i].parts[k].type);
		}
	} else {
		printf("machine: memory, diskinfo\n");
	}
}

static void cmd_help(void) {
	printf("commands are:\n"
	       "  boot [device:][/file] [-s] [word ...]\n"
	       "  ls [device:][/directory]\n"
	       "  machine diskinfo|memory\n"
	       "  reboot\n"
	       "  help\n"
	       "devices are disk0, disk0s1 ... as the kernel names them\n");
}

static void cmd_reboot(void) {
	printf("rebooting...\n");
	outb(0x64, 0xfe);	/* the keyboard controller resets the processor */
	for (;;)
		;
}

void cmd_loop(void) {
	char line[256], *argv[16];
	int timeout = TIMEOUT;
	for (;;) {
		printf("boot> ");
		int n = readline(line, sizeof(line), timeout);
		if (n < 0) {
			printf("\n");
			cmd_boot(1, (char *[]){"boot", NULL});
			timeout = -1;
			continue;
		}
		timeout = -1;
		int argc = split(line, argv, 16);
		if (!argc)
			continue;
		if (!strcmp(argv[0], "boot"))
			cmd_boot(argc, argv);
		else if (!strcmp(argv[0], "ls"))
			cmd_ls(argc, argv);
		else if (!strcmp(argv[0], "machine"))
			cmd_machine(argc, argv);
		else if (!strcmp(argv[0], "reboot"))
			cmd_reboot();
		else if (!strcmp(argv[0], "help") || !strcmp(argv[0], "?"))
			cmd_help();
		else
			printf("%s: unknown command; try help\n", argv[0]);
	}
}
