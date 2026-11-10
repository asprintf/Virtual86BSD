# ctags — create a tags file (POSIX.1-2024)

An implementation of the `ctags` utility of POSIX.1-2024 (XCU ctags), in C
with no dependencies beyond libc. It belongs to the C-Language Development
Utilities and the Software Development Utilities options.

    ctags [-a] [-f tagsfile] pathname...
    ctags -x pathname...

## Build and test

    make           # builds ./ctags
    make test      # the tests (sh test_ctags.sh)
    make difftest  # compares with Universal Ctags
    make install   # into $(PREFIX)/bin; PREFIX=/usr/local, DESTDIR= for staging
    make clean

## What is implemented

* The files read as C (not preprocessed): function definitions (main as M
  and the file's name), typedef names, macros, and structure, union and
  enumeration tags with their members. Of each #if, #ifdef and #ifndef the
  first group is read (after "#if 0", the next); a #define is tagged in any
  group.
* The tags file: identifier, file, and the search pattern of the whole line
  ("\" and "/" escaped), sorted in the order of the POSIX locale; -a keeps
  the lines there; -f - is the standard output.
* -x: identifier, line number, file and line, in the order of LC_COLLATE.

## Choices where the standard leaves room

* An identifier in several places is written for each.
* Any file is read as C, whatever its suffix.
* A file that cannot be read: a diagnostic, status 1, the others tagged. -x
  with -a or -f, and no pathname, are usage errors (status 2).

## Testing

`test_ctags.sh` runs the utility on prepared input and compares the output
and the exit status: each kind of tag, K&R definitions, function pointers,
conditionals, macros over several lines, comments and strings, -a, -f, -x,
the order, files that cannot be read. The binary tested is `./ctags`, or
`$U` if set.

`tests/ctagsdiff.sh` compares the tags of -x with those of Universal Ctags
(its kinds d, f, g, s, t, u) on the C files of these utilities.

## Files

| file | contents |
|------|----------|
| `ctags.c` | the utility |
| `../../lib/libxcu/cpp.c` | a C tokenizer and preprocessor (shared with c17, cflow and cxref) |
| `../../lib/libxcu/cpp.h` | its declarations |
| `../../lib/libxcu/cparse.c` | a forgiving C parser: declarations and references (shared with cflow and cxref) |
| `../../lib/libxcu/cparse.h` | its declarations |
| `tests/ctagsdiff.sh` | the comparison with Universal Ctags (`make difftest`) |
| `test_ctags.sh` | the tests: `make test`, or `sh test_ctags.sh` |
| `Makefile` | build, test, install, clean |
