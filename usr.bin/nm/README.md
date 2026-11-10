# nm — write the name list of an object file (POSIX.1-2024)

An implementation of the `nm` utility of POSIX.1-2024 (XCU nm), in C with no
dependencies beyond libc. It belongs to the Software Development Utilities
option.

    nm [-APv] [-efox] [-g|-u] [-t format] file...

## Build and test

    make           # builds ./nm
    make test      # the tests (sh test_nm.sh)
    make difftest  # compares with GNU nm
    make install   # into $(PREFIX)/bin; PREFIX=/usr/local, DESTDIR= for staging
    make clean

## What is implemented

* The symbol tables of ELF object files, executables and shared objects (32-
  or 64-bit, either byte order) and of the object members of archives.
* The types U, A a, B b, C, D d, R r, T t, N, n, W w, V v, i, u (upper case
  global); the format of -P; -t d, o, x; -A, -g, -u, -v; the XSI -e, -f, -o,
  -x.
* Sorted by name in the collation of LC_COLLATE, or with -v by value.

## Choices where the standard leaves room

* The default format is that of -P, with decimal numbers.
* Section and file symbols only with -f; -e writes all the others.
* An undefined symbol has the value and size 0; a common symbol's value is
  its size (as BSD and GNU nm).
* A file without symbols: "nm: FILE: no symbols", not an error; a file that
  is neither ELF nor an archive is an error (status 1). Usage errors exit
  with status 2.

## Testing

`test_nm.sh` runs the utility on prepared input and compares the output and
the exit status: objects made by the compiler and by ../testlib/mkelf.tl (every
type of symbol), archives, every option and format, several files, missing
and foreign files. The binary tested is `./nm`, or `$U` if set.

`tests/nmdiff.sh` compares the output with that of GNU nm (-P) on the
objects, archives and executables of the system and objects made there, with
-A, -g, -u, -v and -t d, o, x.

## Files

| file | contents |
|------|----------|
| `nm.c` | the utility |
| `../../lib/libxcu/arch.c` | archives (shared with ar and strip) |
| `../../lib/libxcu/arch.h` | its declarations |
| `../../lib/libxcu/elf.c` | ELF files (shared with ar, strip and cflow) |
| `../../lib/libxcu/elf.h` | its declarations |
| `../testlib/mkelf.tl` | makes ELF objects with every kind of symbol, for the tests (shared with ar and strip) |
| `tests/nmdiff.sh` | the comparison with GNU nm (`make difftest`) |
| `test_nm.sh` | the tests: `make test`, or `sh test_nm.sh` |
| `Makefile` | build, test, install, clean |
