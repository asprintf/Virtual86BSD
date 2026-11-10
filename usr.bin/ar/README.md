# ar — create and maintain library archives (POSIX.1-2024)

An implementation of the `ar` utility of POSIX.1-2024 (XCU ar), in C with no
dependencies beyond libc. It belongs to the Software Development Utilities
option.

    ar -d [-v] archive file...
    ar -m [-v] [-a|-b|-i posname] archive file...
    ar -p [-v] [-s] archive [file...]
    ar -q [-cv] archive file...
    ar -r [-cuv] [-a|-b|-i posname] archive file...
    ar -t [-v] [-s] archive [file...]
    ar -x [-v] [-sCT] archive [file...]

## Build and test

    make           # builds ./ar
    make test      # the tests (sh test_ar.sh)
    make difftest  # compares with GNU ar
    make install   # into $(PREFIX)/bin; PREFIX=/usr/local, DESTDIR= for staging
    make clean

## What is implemented

* Every operation and option of the standard, on the archives the link
  editor reads: the format of GNU ar and System V (long names in the //
  member), with a symbol table of the global symbols of the ELF object
  members (32- or 64-bit, either byte order).
* The bytes are those GNU ar writes with its U modifier (real dates, owners
  and modes), but for the date of the symbol table. A changed archive is
  written to a new file in its directory that then replaces it, keeping its
  permissions.

## Choices where the standard leaves room

* The historical form, a first argument of option letters without "-" (ar rv
  lib.a x.o), is accepted.
* -s alone regenerates the symbol table (as ranlib); every change
  regenerates it.
* With operands, -p, -t and -x take them in their order and -m moves them in
  their order; an operand matches the first member of that name.
* -x: the members' modes (less the umask), the time of extraction; -C keeps
  a file that is there (a message, status 1); a name longer than {NAME_MAX}
  is an error, cut to {NAME_MAX} with -T.
* -tv: the mode string, owner/group, size, date as "%b %e %H:%M %Y", and
  name.
* An operand not in the archive is reported (status 1), the others are done;
  a posname not in the archive is the end. Usage errors exit with status 2.

## Testing

`test_ar.sh` runs the utility on prepared input and compares the output and
the exit status: every operation with its options, positioning, long names,
the symbol table of objects made by the compiler, the historical form, the
permissions kept, damaged archives. The binary tested is `./ar`, or `$U` if
set.

`tests/ardiff.tl` runs random sequences of operations on archives of random
files and objects with this ar and with GNU ar (with U) and compares the
archives (all but the date of the symbol table) and the output.

## Files

| file | contents |
|------|----------|
| `ar.c` | the utility |
| `../../lib/libxcu/arch.c` | archives: reading, writing, the symbol table (shared with nm and strip) |
| `../../lib/libxcu/arch.h` | its declarations |
| `../../lib/libxcu/elf.c` | ELF files: their sections and symbols (shared with nm, strip and cflow) |
| `../../lib/libxcu/elf.h` | its declarations |
| `../testlib/mkelf.tl` | makes ELF objects, for the tests (shared with nm and strip) |
| `tests/ardiff.tl` | the comparison with GNU ar (`make difftest`) |
| `test_ar.sh` | the tests: `make test`, or `sh test_ar.sh` |
| `Makefile` | build, test, install, clean |
