# xgettext — extract gettext call strings from C-language source files (POSIX.1-2024)

An implementation of the `xgettext` utility of POSIX.1-2024 (XCU xgettext),
in C with no dependencies beyond libc. It belongs to the C-Language
Development Utilities option.

    xgettext [-j] [-n] [-d default-domain] [-K keyword-spec]... [-p pathname] file...
    xgettext -a [-n] [-d default-domain] [-p pathname] [-x exclude-file] file...

## Build and test

    make           # builds ./xgettext
    make test      # the tests (sh test_xgettext.sh)
    make difftest  # compares with GNU xgettext
    make install   # into $(PREFIX)/bin; PREFIX=/usr/local, DESTDIR= for staging
    make clean

## What is implemented

* The C source as tokens (comments, character constants and #include lines
  skipped, backslash-newline joined, adjacent string literals joined); the
  arguments of the keyword calls that are only string literals are the
  msgids and msgid_plurals, also in calls inside the arguments of others.
* The default keywords (gettext, ngettext, the d- and dc- functions, the _l
  ones), -K "", -K ID, ID:N, ID:N1,N2; -a, -n (#: file:line), -d, -p, -x,
  -j.

## Choices where the standard leaves room

* The d- and dc- functions with a literal domain write to DOMAIN.po (with
  its domain directive); the rest go to the default file, without a header.
* As GNU xgettext: a keyword with a plural needs both arguments literal; a
  literal in parentheses is not taken; the messages are in the order the
  calls end; -a takes every string literal, without plurals.
* A msgid met again is written as comment lines; -j keeps the existing file
  (but its domain directives) and appends.
* The empty string is never a msgid (a warning). Usage errors exit with
  status 2.

## Testing

`test_xgettext.sh` runs the utility on prepared input and compares the
output and the exit status: a source with every kind of literal and call:
the output exactly, -n, -d, -p, domains, -K in all its forms, -a, -x, -j,
the standard input, errors. The binary tested is `./xgettext`, or `$U` if
set.

`tests/xgettextdiff.tl` makes random C sources and compares the messages
that xgettext and GNU xgettext extract with the same keywords, -a and -n (a
msgid met again, merged by GNU, written as comments here, is merged for the
comparison).

## Files

| file | contents |
|------|----------|
| `xgettext.c` | the utility |
| `../lib/po.c`, `../lib/po.h` | reading dot-po files, for -x and -j (shared with msgfmt) |
| `../lib/cesc.c`, `../lib/cesc.h` | C escape sequences (shared with gettext and msgfmt) |
| `tests/xgettextdiff.tl` | the comparison with GNU xgettext (`make difftest`) |
| `test_xgettext.sh` | the tests: `make test`, or `sh test_xgettext.sh` |
| `Makefile` | build, test, install, clean |
