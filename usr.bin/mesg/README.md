# mesg — permit or deny messages (POSIX.1-2024)

An implementation of the `mesg` utility of POSIX.1-2024 (XCU mesg), in C
with no dependencies beyond libc.

    mesg [y|n]

## Build and test

    make           # builds ./mesg
    make test      # the tests (sh test_mesg.sh)
    make difftest  # compares with util-linux mesg
    make install   # into $(PREFIX)/bin; PREFIX=/usr/local, DESTDIR= for staging
    make clean

## What is implemented

* The terminal is the first of the standard input, output and error that is
  one; messages are allowed when the group or others may write to it (what
  write and talk look at): y gives the group write permission, n takes it
  from the group and others.

## Choices where the standard leaves room

* Without an operand: "is y" or "is n" on the standard output.
* The exit status is that of the state after the change: 0 for y, 1 for n;
  errors are 2.

## Testing

`test_mesg.sh` runs the utility on prepared input and compares the output
and the exit status: on pseudo-terminals: y, n, the report, the exit
statuses, others' permission, errors. The binary tested is `./mesg`, or `$U`
if set.

`tests/mesgdiff.sh` compares the statuses and the permissions after
sequences of operands with util-linux mesg.

## Files

| file | contents |
|------|----------|
| `mesg.c` | the utility |
| `tests/mesgdiff.sh` | the comparison with util-linux mesg (`make difftest`) |
| `test_mesg.sh` | the tests: `make test`, or `sh test_mesg.sh` |
| `Makefile` | build, test, install, clean |
