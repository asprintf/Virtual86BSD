# lp — send files to a printer (POSIX.1-2024)

An implementation of the `lp` utility of POSIX.1-2024 (XCU lp), in C with no
dependencies beyond libc.

    lp [-c] [-d dest] [-n copies] [-msw] [-o option]... [-t title] [file...]

## Build and test

    make           # builds ./lp
    make test      # the tests (sh test_lp.sh)
    make install   # into $(PREFIX)/bin; PREFIX=/usr/local, DESTDIR= for staging
    make clean

## What is implemented

* lp prints at once, as the rationale allows: it waits for the destination,
  writes the request, and exits; the files are not needed afterwards, with
  or without -c.
* The destinations are in /etc/lp.conf (LP_CONF): printer NAME [nobanner]
  PATHNAME, printer NAME [nobanner] | COMMAND, class NAME MEMBER..., default
  NAME, lockdir DIR; without it, lp is /dev/lp0 when that is a character
  device.
* Exclusive access by a lock (fcntl) on lockdir/lp.NAME.lock, which holds
  the number of the last request: the request ID is NAME-NUMBER. A class
  takes its first free member.
* A banner page (Request, User, Title, Files, Date in LC_TIME), the copies,
  -m (mailx), -w (a message on the user's terminal, else mail), -s.

## Choices where the standard leaves room

* The destination: -d, else LPDEST, else PRINTER, else the default of the
  configuration, else its only printer.
* -o nobanner and -o banner choose the banner page; other options are errors
  for a device and passed on to a command (LP_OPTIONS).
* "-" is the standard input, read once; a file that cannot be read is left
  out (status 1), and with none nothing is printed.
* The request ID is written as "request id is ID (N file(s))". Usage errors
  exit with status 2.

## Testing

`test_lp.sh` runs the utility on prepared input and compares the output and
the exit status: printers that are files and commands: the banner, request
numbers, -d LPDEST PRINTER and their order, copies, the standard input, -t
-o -s -c, classes and waiting for a busy printer, -m and -w (a mailx of the
tests' own, a database of utx), errors. The binary tested is `./lp`, or `$U`
if set.

## Files

| file | contents |
|------|----------|
| `lp.c` | the utility |
| `../lib/utx.c`, `../lib/utx.h` | the user accounting database (shared with talk, who and write) |
| `test_lp.sh` | the tests: `make test`, or `sh test_lp.sh` |
| `Makefile` | build, test, install, clean |
