# msgfmt — create messages objects from portable messages object source files (POSIX.1-2024)

An implementation of the `msgfmt` utility of POSIX.1-2024 (XCU msgfmt), in C
with no dependencies beyond libc.

    msgfmt [-cfSv] [-D dir] [-o outputfile] pathname...

## Build and test

    make           # builds ./msgfmt
    make test      # the tests (sh test_msgfmt.sh)
    make difftest  # compares with GNU msgfmt
    make install   # into $(PREFIX)/bin; PREFIX=/usr/local, DESTDIR= for staging
    make clean

## What is implemented

* The dot-po format of the standard (and msgctxt of GNU gettext): domains,
  headers, plural forms, fuzzy and c-format flags, continuation lines,
  escapes.
* The messages objects are those of GNU gettext, which the C libraries'
  gettext() read: the strings sorted, and the hash table as GNU msgfmt makes
  it, so the same input gives the same file, byte for byte.
* -o, -S, -D, -f; -c -v: the standard's checks of newlines and of c-format
  conversions (by position with %n$); -v the counts.

## Choices where the standard leaves room

* Without -o, domain D goes to D.mo; a domain with nothing to write gets no
  file.
* Messages with an empty first form are left out, and fuzzy ones without -f;
  the header is kept even when fuzzy.
* Errors (nothing written, status 1): a message defined twice in a domain,
  plural forms without nplurals or not as many, a plural expression not
  valid, syntax errors.
* msgstr[0] is checked against msgid, the other forms against msgid_plural.
  Usage errors exit with status 2.

## Testing

`test_msgfmt.sh` runs the utility on prepared input and compares the output
and the exit status: objects read back by tests/readmo.tl (and through their
hash tables): domains, -o, -S, -D, headers and plural forms, escapes and
continuation lines, fuzzy and untranslated messages, msgctxt, the standard
input, every error, the checks of -c -v. The binary tested is `./msgfmt`, or
`$U` if set.

`tests/msgfmtdiff.tl` makes random dot-po files and compares the messages
objects of msgfmt and GNU msgfmt byte for byte; the files are made as GNU
msgfmt wants them (it always checks newlines, and joins a comment that ends
in a backslash with the next line).

## Files

| file | contents |
|------|----------|
| `msgfmt.c` | the utility |
| `../lib/po.c`, `../lib/po.h` | reading dot-po files (shared with xgettext) |
| `../lib/plural.c`, `../lib/plural.h` | plural expressions (shared with gettext) |
| `../lib/cesc.c`, `../lib/cesc.h` | C escape sequences (shared with gettext and xgettext) |
| `tests/readmo.tl` | writes the messages of an object, for the tests |
| `tests/msgfmtdiff.tl` | the comparison with GNU msgfmt (`make difftest`) |
| `test_msgfmt.sh` | the tests: `make test`, or `sh test_msgfmt.sh` |
| `Makefile` | build, test, install, clean |
