# talk — talk to another user (POSIX.1-2024)

An implementation of the `talk` utility of POSIX.1-2024 (XCU talk), in C
with no dependencies beyond libc. It belongs to the User Portability
Utilities option.

    talk address [terminal]

## Build and test

    make           # builds ./talk
    make test      # the tests (sh test_talk.sh)
    make install   # into $(PREFIX)/bin; PREFIX=/usr/local, DESTDIR= for staging
    make clean

## What is implemented

* Talk between the users of this system, without a daemon: the caller makes
  a socket (AF_UNIX) in the directory of meetings (/tmp) and rings the
  callee's terminal (the standard's message, again every 10 seconds); the
  callee's talk finds the socket, checks that its owner is the caller, and
  connects; the caller checks the callee in turn by a file the callee makes
  in the same directory.
* The screen split in two halves (terminfo: cup, clear, el), the state of
  the talk on the line between; the terminal in noncanonical mode: erase,
  kill and (with iexten) word-erase on both screens, <alert> alerts the
  other, <control>-L redraws, the end-of-file and interrupt characters end
  talk with status 0.
* Characters of the classes print and space are sent as they are, others as
  ^X and M-X; the other party's leaving is shown, and then only leaving is
  possible.

## Choices where the standard leaves room

* Diagnostic messages go to the standard output (the standard gives talk no
  standard error); not a terminal: status 1.
* address is a user name of this system.
* The terminal type is vt100 without TERM; one without cursor addressing
  cannot be used.
* When both call each other at the same time, the name that sorts higher
  answers.
* POSIX_UTMPX, POSIX_DEVDIR and POSIX_TALKDIR (the directory of meetings)
  are hooks for the tests. Usage errors exit with status 2.

## Testing

`test_talk.sh` runs the utility on prepared input and compares the output
and the exit status: tests/talkdrv.tl runs two talks on pseudo-terminals and
reads their screens through the terminal emulator of tl: the ring, the answer, typing,
the editing characters, conversions, UTF-8, wrapping, redrawing, leaving by
end-of-file and interrupt, a connection that cannot prove who it is, both
calling at once, the terminal operand, ringing again, errors. The binary
tested is `./talk`, or `$U` if set.

## Files

| file | contents |
|------|----------|
| `talk.c` | the utility |
| `../lib/utx.c`, `../lib/utx.h` | the user accounting database (shared with lp, who and write) |
| `../lib/terminfo.c`, `../lib/terminfo.h` | compiled terminfo entries (shared with tabs and tput) |
| `../lib/port.h` | the POSIX.1-2024 interfaces that older C libraries lack, and `NORETURN`/`PRINTF` (shared) |
| `tests/talkdrv.tl` | the scenarios of the tests, on pseudo-terminals |
| `test_talk.sh` | the tests: `make test`, or `sh test_talk.sh` |
| `Makefile` | build, test, install, clean |
