# du — estimate file space usage (POSIX.1-2024)

An implementation of the `du` utility of POSIX.1-2024 (XCU du), in C with no
dependencies beyond libc.

    du [-a|-s] [-kx] [-H|-L] [file...]

## Build and test

    make           # builds ./du
    make test      # the tests (sh test_du.sh)
    make difftest  # compares with GNU du
    make install   # into $(PREFIX)/bin; PREFIX=/usr/local, DESTDIR= for staging
    make clean

## What is implemented

* The hierarchy is walked depth first by pathname; a directory is written
  after its contents; the sizes are the st_blocks of stat() in 512-byte
  units, or with -k in 1024-byte units rounded up.
* -a, -s, -k, -x (only the device of the operand), -H and -L (the last one
  given wins).

## Choices where the standard leaves room

* Every file is remembered by device and inode, so a file met again (a hard
  link, a directory named twice, a loop through -L) is neither counted nor
  written again; an operand already met writes nothing.
* The entries of a directory are taken in the order of strcmp().
* -a with -s: -s wins.
* -x leaves out the files on other devices altogether, mount points and all.
* A directory that cannot be read is reported and written with what could be
  counted; the status is then 1. Usage errors exit with status 2.

## Testing

`test_du.sh` runs the utility on prepared input and compares the output and
the exit status: a tree with hard links and symbolic links: every option and
their combinations, several operands, links met twice, unreadable
directories, -x across file systems where there is one. The binary tested is
`./du`, or `$U` if set.

`tests/dudiff.sh` makes random trees and compares du with GNU du (with
POSIXLY_CORRECT) for all the options; with hard links the entry written
depends on the walk, so only the totals of -s are compared then.

## Files

| file | contents |
|------|----------|
| `du.c` | the utility |
| `tests/dudiff.sh` | the comparison with GNU du (`make difftest`) |
| `test_du.sh` | the tests: `make test`, or `sh test_du.sh` |
| `Makefile` | build, test, install, clean |
