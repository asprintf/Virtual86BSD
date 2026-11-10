# man — display system documentation (POSIX.1-2024)

An implementation of the `man` utility of POSIX.1-2024 (XCU man), in C with
no dependencies beyond libc. It belongs to the User Portability Utilities
option.

    man [-k] name...

## Build and test

    make           # builds ./man
    make test      # the tests (sh test_man.sh)
    make install   # into $(PREFIX)/bin; PREFIX=/usr/local, DESTDIR= for staging
    make clean

## What is implemented

* The pages are plain text files, one for each name, in the directories of
  MANPATH, else in $(PREFIX)/share/posixutilities/man; each directory may
  have a summary database, whatis, of lines "name - purpose".
* mkpages.tl makes both from the HTML edition of the standard (make pages
  SPEC=its directory): for every standard utility its synopsis, options,
  operands, environment variables, exit status and the rest of its page,
  filled to 72 columns. They are the standard's text, so they are made where
  the standard is and not distributed; make install installs them with man.
* -k: the lines of the whatis files that any name matches, as an extended
  regular expression without regard to case (as grep -Ei).
* On a terminal the output goes through PAGER (sh -c), or more.

## Choices where the standard leaves room

* A name with no page is reported and the status is 1; the other names are
  written.
* man -k with nothing found: "nothing appropriate" and status 1.
* A pager that exits with a status other than 0 makes the status 1. Usage
  errors exit with status 2.

## Testing

`test_man.sh` runs the utility on prepared input and compares the output and
the exit status: pages of the tests' own in two directories, -k with
expressions, the pager on a pseudo-terminal; and when the standard's HTML
edition is found (SPEC, or next to the tree), the pages made from it: every
utility has the sections the standard requires. The binary tested is
`./man`, or `$U` if set.

## Files

| file | contents |
|------|----------|
| `man.c` | the utility |
| `mkpages.tl` | the pages and the summary database, made from the standard |
| `test_man.sh` | the tests: `make test`, or `sh test_man.sh` |
| `Makefile` | build, test, install, clean |
