# who — display who is on the system (POSIX.1-2024)

An implementation of the `who` utility of POSIX.1-2024 (XCU who), in C with
no dependencies beyond libc. It belongs to the XSI option.

    who [-mTu] [-abdHlprt] [file]
    who [-mu] -s [-bHlprt] [file]
    who -q [file]
    who am i
    who am I

## Build and test

    make           # builds ./who
    make test      # the tests (sh test_who.sh)
    make difftest  # compares with GNU who
    make install   # into $(PREFIX)/bin; PREFIX=/usr/local, DESTDIR= for staging
    make clean

## What is implemented

* The entries of the user accounting database (getutxent(), or file, a file
  of struct utmpx records); the state and idle time of a line from its
  terminal in /dev.
* Every option: -a -b -d -H -l -m -p -q -r -s -t -T -u, and "am i".
* The fields in columns: the name in 8, the line in 12, the time as date
  +"%b %e %H:%M", the idle time ("." or "old" or hours:minutes), the process
  ID, the comment ("(host)", or "id=ID") and the exit of a dead process
  ("term=T exit=E", where the C library has them).

## Choices where the standard leaves room

* The users by default; -b, -d, -l, -p, -r, -t only their entries, the users
  too with -T or -u; -a is -bdlprtTu; -s turns -T off.
* The state: + when the group or others may write to the terminal, - when
  not, ? when its file cannot be had.
* -m and "am i": the line of the first of the standard input, output and
  error that is a terminal.
* -q: the names on one line, then "# users=N".
* POSIX_UTMPX and POSIX_DEVDIR name another database and directory of
  terminals (hooks for the tests; not when set-user-ID or set-group-ID).
* Usage errors exit with status 2; a file that cannot be read, 1.

## Testing

`test_who.sh` runs the utility on prepared input and compares the output and
the exit status: databases written by tests/mkutx.c: every option on users,
login processes, dead processes, the boot, the clock, the run level; the
state and idle times of terminals; -m and "am i" on a pseudo-terminal. The
binary tested is `./who`, or `$U` if set.

`tests/whodiff.sh` compares who with GNU who on the same databases (the
blanks squeezed; the state and idle time, which GNU who takes from /dev
only, left out).

## Files

| file | contents |
|------|----------|
| `who.c` | the utility |
| `../../lib/libxcu/utx.c`, `../../lib/libxcu/utx.h` | the user accounting database (shared with lp, talk and write) |
| `tests/mkutx.c` | writes databases for the tests of who, write, talk and lp |
| `tests/whodiff.sh` | the comparison with GNU who (`make difftest`) |
| `test_who.sh` | the tests: `make test`, or `sh test_who.sh` |
| `Makefile` | build, test, install, clean |
