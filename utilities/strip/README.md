# strip — remove unnecessary information from strippable files (POSIX.1-2024)

An implementation of the `strip` utility of POSIX.1-2024 (XCU strip), in C
with no dependencies beyond libc. It belongs to the Software Development
Utilities option.

    strip file...

## Build and test

    make           # builds ./strip
    make test      # the tests (sh test_strip.sh)
    make difftest  # compares with GNU strip
    make install   # into $(PREFIX)/bin; PREFIX=/usr/local, DESTDIR= for staging
    make clean

## What is implemented

* ELF files (32- or 64-bit, either byte order) and archives of them.
* Executables and shared objects: the symbol table and the debugging
  sections removed; what the program headers load stays byte for byte where
  it is.
* Relocatable objects: the debugging sections, and the local and undefined
  symbols that no relocation uses, removed; the file laid out again, the
  symbols, relocations and groups renumbered, so that it still links.
* Archives: each object member stripped, the symbol table made again. The
  result replaces the file, with its permissions.

## Choices where the standard leaves room

* The names of removed sections stay in the section name table.
* A file that is neither ELF nor an archive is reported and left as it is
  (status 1); non-ELF members of archives are kept.

## Testing

`test_strip.sh` runs the utility on prepared input and compares the output
and the exit status: objects made by ../testlib/mkelf.tl (32- and 64-bit, both
byte orders) and executables, shared objects, objects and archives made by
the compiler: what is left, that they still run and link, permissions kept,
foreign and missing files. The binary tested is `./strip`, or `$U` if set.

`tests/stripdiff.sh` compares with GNU strip (--strip-unneeded for
relocatable files) by function: the sections left, the global and dynamic
symbols, that executables run and objects link.

## Files

| file | contents |
|------|----------|
| `strip.c` | the utility |
| `../lib/arch.c` | archives (shared with ar and nm) |
| `../lib/arch.h` | its declarations |
| `../lib/elf.c` | ELF files (shared with ar, nm and cflow) |
| `../lib/elf.h` | its declarations |
| `tests/elfinfo.tl` | the sections and symbols of an ELF file, for the tests |
| `../testlib/mkelf.tl` | makes ELF objects, for the tests (shared with ar and nm) |
| `tests/stripdiff.sh` | the comparison with GNU strip (`make difftest`) |
| `test_strip.sh` | the tests: `make test`, or `sh test_strip.sh` |
| `Makefile` | build, test, install, clean |
