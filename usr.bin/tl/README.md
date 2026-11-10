# tl — the test language

tl is a small interpreted language in the style of C, with the interfaces
of POSIX and pseudo-terminals (with a terminal emulator) as its library.
It is not a utility of POSIX: it is the language the test suites of this
collection are written in where the shell is not enough, and a program of
V86BSD in its own right (`/usr/bin/tl`), so that tests in the same manner
can be written for anything else on the system.  One file, `tl.c`, C99
with POSIX interfaces, about 6700 lines.

    tl script.tl [argument...]
    tl -e 'program' [argument...]

The language and its library are described in [tl.md](tl.md), which is
also its page of `man` (`man tl`).

## Build and test

    make           # builds ./tl
    make test      # the tests (sh test_tl.sh)
    make install   # tl into $(PREFIX)/bin, its page (tl.md) into
                   # $(PREFIX)/share/posixutilities/man, with a line in whatis
    make clean

The test suites of the other utilities do not use an installed tl: each
compiles `../tl/tl.c` for itself (with `$CC`, else the first of c17, c99
and cc), so they run on any host with a C compiler and test the tl of the
same sources.

## What is implemented

* Values: nil, 64-bit integers, doubles, strings of bytes, lists and maps
  (by reference, keys in the order put), functions, terminals; reference
  counting.
* C's statements and operators, `for (var x : c)`, `in`, `include` (once,
  relative to the including file), `#!` on the first line.
* The library: output and formatting (C's printf conversions and `%q`),
  strings, POSIX extended regular expressions, binary packing, JSON,
  SHA-256 and base64, lists and maps, a seeded random generator
  (splitmix64), files, processes (`run`, `spawn`, `fork`, the user and
  group IDs), signals, descriptors and UNIX-domain sockets, time, and
  terminals: a program on a new pseudo-terminal as its controlling
  terminal, with a screen kept by an emulator of the xterm and ANSI
  sequences.

## Choices

* Errors are not exceptions: a wrong use ends the program with status 2
  and a message with the file and line; the functions of the system
  return nil or false and keep `errno()`.
* No closures: a function sees its parameters, its locals and the
  globals; using a local of an enclosing function is an error (so a
  mistake is not silently a global).
* The random numbers are the same sequence for a seed on every system, so
  a random test that fails can be repeated anywhere.
* `json()` writes keys sorted and only ASCII, so its output can be
  compared as text.

## Testing

`test_tl.sh` (98 cases) goes through the language (values, operators,
statements, functions, include, the errors and exit statuses) and every
part of the library, with a script run by `#!`.  The suites of the other
utilities use tl throughout (some 50 of them build it), so they test it
as well, under gcc and c17, with glibc and with V86BSD's C library.

## Files

| file | contents |
|------|----------|
| `tl.c` | the interpreter: lexer, parser to a tree, evaluator, library, terminal emulator |
| `tl.md` | the language and its library (also the page of `man tl`) |
| `test_tl.sh` | the tests |
| `Makefile` | all, test, difftest, install, clean |
