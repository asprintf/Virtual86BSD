/*
 * mesg.c - mesg: permit or deny messages (POSIX.1-2024, XCU mesg)
 *
 * Synopsis: mesg [y|n]
 *
 * The terminal is the first of the standard input, output and error that
 * is one.  Messages are allowed when the group or others may write to it
 * (write and talk look at the same bits); y gives the group write
 * permission, n takes it away from the group and from others.
 *
 * Choices where the standard leaves room:
 *  - Without an operand: "is y" or "is n" on the standard output.
 *  - The exit status is that of the state after the change: 0 for y, 1
 *    for n; errors (no terminal, a bad operand, a failed change) are 2.
 *  - "--" before the operand is taken as the end of the options (there
 *    are none).
 */

#define _POSIX_C_SOURCE 202405L

#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");
    int i = 1;
    if (i < argc && strcmp(argv[i], "--") == 0)
        i++;
    const char *op = NULL;
    if (argc - i > 1 || (argc - i == 1 && strcmp(argv[i], "y") != 0 && strcmp(argv[i], "n") != 0)) {
        fprintf(stderr, "usage: mesg [y|n]\n");
        return 2;
    }
    if (argc - i == 1)
        op = argv[i];

    int fd = -1;
    for (int k = 0; k < 3 && fd < 0; k++)
        if (isatty(k))
            fd = k;
    if (fd < 0) {
        fprintf(stderr, "mesg: no terminal\n");
        return 2;
    }
    struct stat st;
    if (fstat(fd, &st) != 0) {
        const char *n = ttyname(fd);
        fprintf(stderr, "mesg: %s: %s\n", n ? n : "terminal", strerror(errno));
        return 2;
    }
    mode_t m = st.st_mode & 07777;
    if (op) {
        mode_t nm = op[0] == 'y' ? (m | S_IWGRP) : (m & ~(mode_t)(S_IWGRP | S_IWOTH));
        if (nm != m && fchmod(fd, nm) != 0) {
            const char *n = ttyname(fd);
            fprintf(stderr, "mesg: %s: %s\n", n ? n : "terminal", strerror(errno));
            return 2;
        }
        m = nm;
    }
    int allowed = (m & (S_IWGRP | S_IWOTH)) != 0;
    if (!op) {
        printf("is %s\n", allowed ? "y" : "n");
        if (fflush(stdout) != 0 || ferror(stdout))
            return 2;
    }
    return allowed ? 0 : 1;
}
