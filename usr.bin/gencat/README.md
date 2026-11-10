# gencat — generate a formatted message catalog (POSIX.1-2024)

An implementation of the `gencat` utility of POSIX.1-2024 (XCU gencat), in C
with no dependencies beyond libc.

    gencat catfile msgfile...

## Build and test

    make           # builds ./gencat
    make test      # the tests (sh test_gencat.sh)
    make difftest  # compares with the GNU C library's gencat
    make install   # into $(PREFIX)/bin; PREFIX=/usr/local, DESTDIR= for staging
    make clean

## What is implemented

* The message text source of the standard: $set, $delset, $quote, comments,
  messages (an empty text with its blank, deletion without it), the escapes
  and continuation lines.
* The catalogs are in the format of the GNU C library, which its catopen()
  and catgets() read, laid out as GNU gencat lays them out (the size of the
  table, the order of the sets and messages, the merge of an existing
  catalog after the msgfiles), so the same input gives the same file, byte
  for byte.

## Choices where the standard leaves room

* Without $set, messages are in set NL_SETD (1).
* A number met again gives the text read last; $delset and deletions act on
  catfile and on the msgfiles read so far.
* Lines that are neither directives nor messages, invalid numbers, a quoted
  text without its closing quote and a catfile that is not a catalog are
  errors (nothing written, status 1); an unknown $ directive is a warning.
* "-" as catfile is the standard output. Usage errors exit with status 2.

## Testing

`test_gencat.sh` runs the utility on prepared input and compares the output
and the exit status: catalogs read back by tests/readcat.tl (and where
catgets() looks): sets, separators, empty messages, escapes, continuation
lines, $quote, merging, deletion, $delset, several msgfiles, "-", the size
GNU gencat chooses, the C library's catgets() on them, errors. The binary
tested is `./gencat`, or `$U` if set.

`tests/gencatdiff.tl` makes random sources, also merged into existing
catalogs, and compares the catalogs of gencat and GNU gencat byte for byte,
then reads every message back with catgets(); not $delset, which GNU gencat
does not carry out, nor what it does otherwise than the standard (empty
messages in a merge, deleting a message that is not there).

## Files

| file | contents |
|------|----------|
| `gencat.c` | the utility |
| `tests/readcat.tl` | writes the messages of a catalog, for the tests |
| `tests/catread.c` | reads a catalog with catopen() and catgets() |
| `tests/gencatdiff.tl` | the comparison with GNU gencat (`make difftest`) |
| `test_gencat.sh` | the tests: `make test`, or `sh test_gencat.sh` |
| `Makefile` | build, test, install, clean |
