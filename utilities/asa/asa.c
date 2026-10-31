/*
 * asa.c - asa: interpret carriage-control characters (POSIX.1-2024, XCU asa)
 *
 * Synopsis: asa [file...]
 *
 * The newline that ends each output line is written only when the next
 * line is read, so that a '+' can replace it.  For the first character
 * of each line:
 *     <space>  the line as it is
 *     0        a newline, then the line
 *     1        a form feed (the next page), then the line
 *     +        a carriage return instead of the newline of the line before
 *              (as <space> on the first line)
 * Each file ends with a newline (after its last line).
 *
 * Choices where the standard leaves room:
 *  - Another first character is taken as <space>; so is an empty line
 *    (which writes an empty line).
 *  - "-" is the standard input.  A file that cannot be read makes the
 *    status 1, the others are still written; nothing is written to the
 *    standard error (the standard gives asa none).
 */

#define _POSIX_C_SOURCE 202405L

#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool asa(FILE *f, const char *name)
{
    char *line = NULL;
    size_t cap = 0;
    ssize_t n;
    bool first = true;
    while ((n = getline(&line, &cap, f)) >= 0) {
        if (n > 0 && line[n - 1] == '\n')
            line[--n] = '\0';
        char c = n > 0 ? line[0] : ' ';
        const char *rest = n > 0 ? line + 1 : "";
        if (first) {
            if (c == '0')
                putchar('\n');
            else if (c == '1')
                putchar('\f');
            first = false;
        } else {
            switch (c) {
            case '0': fputs("\n\n", stdout); break;
            case '1': fputs("\n\f", stdout); break;
            case '+': putchar('\r'); break;
            default: putchar('\n'); break;
            }
        }
        fwrite(rest, 1, strlen(rest), stdout);
    }
    bool ok = !ferror(f);
    (void)name;
    if (!first)
        putchar('\n');
    free(line);
    return ok;
}

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");
    int status = 0, i = 1;
    if (i < argc && strcmp(argv[i], "--") == 0)
        i++;
    if (i == argc) {
        if (!asa(stdin, "standard input"))
            status = 1;
    }
    for (; i < argc; i++) {
        if (strcmp(argv[i], "-") == 0) {
            if (!asa(stdin, "standard input"))
                status = 1;
            continue;
        }
        FILE *f = fopen(argv[i], "r");
        if (!f) {
            status = 1;
            continue;
        }
        if (!asa(f, argv[i]))
            status = 1;
        fclose(f);
    }
    if (fflush(stdout) != 0 || ferror(stdout))
        return 1;
    return status;
}
