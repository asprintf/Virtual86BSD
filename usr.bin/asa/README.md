# asa — interpret carriage-control characters (POSIX.1-2024)

An implementation of the `asa` utility of POSIX.1-2024 (XCU asa), in C with
no dependencies beyond libc. It belongs to the FORTRAN Runtime Utilities
option.

    asa [file...]

## Build and test

    make           # builds ./asa
    make test      # the tests (sh test_asa.sh)
    make install   # into $(PREFIX)/bin; PREFIX=/usr/local, DESTDIR= for staging
    make clean

## What is implemented

* The first character of each line: <space>, the line as it is; 0, a newline
  before it; 1, a form feed (a new page); +, a carriage return instead of
  the newline of the line before (on the first line, as <space>).
* The newline that ends a line is written when the next line is read, so
  that + can replace it; each file ends with a newline. "-" is the standard
  input.

## Choices where the standard leaves room

* Another first character, and an empty line, are taken as <space>.
* A file that cannot be read: status 1, the others still written; nothing on
  the standard error (the standard gives asa none).

## Testing

`test_asa.sh` runs the utility on prepared input and compares the output and
the exit status: each control character, at the start and in the middle,
empty lines and input, a last line without a newline, several files and -, a
missing file. The binary tested is `./asa`, or `$U` if set.

## Files

| file | contents |
|------|----------|
| `asa.c` | the utility |
| `test_asa.sh` | the tests: `make test`, or `sh test_asa.sh` |
| `Makefile` | build, test, install, clean |
