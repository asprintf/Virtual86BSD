# cxref — generate a C-language program cross-reference table (POSIX.1-2024)

An implementation of the `cxref` utility of POSIX.1-2024 (XCU cxref), in C
with no dependencies beyond libc. It belongs to the XSI option.

    cxref [-cs] [-o file] [-w num] [-D name[=def]]... [-I dir]... [-U name]... file...

## Build and test

    make           # builds ./cxref
    make test      # the tests (sh test_cxref.sh)
    make install   # into $(PREFIX)/bin; PREFIX=/usr/local, DESTDIR= for staging
    make clean

## What is implemented

* Each file preprocessed (-D, -I, -U in their order) and parsed: macros,
  functions, objects (external, static, automatic, parameters), typedef
  names and enumeration constants, with their references: the lines, "*"
  where declared, "=" where assigned to; for macros where expanded or
  tested.
* The listing "SYMBOL FILE FUNCTION LINE", sorted by symbol (LC_COLLATE),
  file and function ("--" outside functions); columns as wide as their
  entries, the references wrapped to -w columns (80 by default).
* A listing per file after its name (-s: without it), or with -c one for
  all; -o file.

## Choices where the standard leaves room

* The symbols of included headers are listed too, under the headers' names.
* A name declared in several scopes of a function is one symbol there.
* A file that cannot be read: a diagnostic, status 1, the others listed.
  Usage errors exit with status 2.

## Testing

`test_cxref.sh` runs the utility on prepared input and compares the output
and the exit status: each kind of symbol, declarations and assignments,
macros (also from headers and in #ifdef), -c, -s, -o, -w and the wrapping,
-D, -I, -U, files that cannot be read. The binary tested is `./cxref`, or
`$U` if set.

## Files

| file | contents |
|------|----------|
| `cxref.c` | the utility |
| `../../lib/libxcu/cpp.c` | a C tokenizer and preprocessor (shared with c17, cflow and ctags) |
| `../../lib/libxcu/cpp.h` | its declarations |
| `../../lib/libxcu/cparse.c` | a forgiving C parser: declarations and references (shared with cflow and ctags) |
| `../../lib/libxcu/cparse.h` | its declarations |
| `test_cxref.sh` | the tests: `make test`, or `sh test_cxref.sh` |
| `Makefile` | build, test, install, clean |
