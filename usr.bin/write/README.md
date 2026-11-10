# write — write to another user (POSIX.1-2024)

An implementation of the `write` utility of POSIX.1-2024 (XCU write), in C
with no dependencies beyond libc.

    write user_name [terminal]

## Build and test

    make           # builds ./write
    make test      # the tests (sh test_write.sh)
    make install   # into $(PREFIX)/bin; PREFIX=/usr/local, DESTDIR= for staging
    make clean

## What is implemented

* The recipient's terminals are the lines of user_name in the user
  accounting database, those that allow messages (mesg y); the first message
  is "Message from SENDER (TTY) [date]...", the sender's terminal is alerted
  twice.
* Each line is written as it comes, newline as CR LF, other control
  characters as ^X, bytes that are not characters as M-X; "EOT" at the end
  of file or on an interrupt.

## Choices where the standard leaves room

* Of several terminals, the least idle one that allows messages, not the
  sender's own unless it is the only one; a message on the standard output
  names it.
* The terminal operand as who writes it, or a pathname in /dev.
* An interrupt ignored when write starts stays ignored.
* POSIX_UTMPX and POSIX_DEVDIR as with who. Usage errors exit with status 2,
  others with 1.

## Testing

`test_write.sh` runs the utility on prepared input and compares the output
and the exit status: on terminals that are files: the first message, the
conversions, UTF-8, the choice of terminal, mesg n, the user not logged on;
on a pseudo-terminal: the sender's line, the alerts, the end-of-file and
interrupt characters. The binary tested is `./write`, or `$U` if set.

## Files

| file | contents |
|------|----------|
| `write.c` | the utility |
| `../../lib/libxcu/utx.c`, `../../lib/libxcu/utx.h` | the user accounting database (shared with lp, talk and who) |
| `test_write.sh` | the tests: `make test`, or `sh test_write.sh` |
| `Makefile` | build, test, install, clean |
