# uux — remote command execution (POSIX.1-2024)

An implementation of the `uux` utility of POSIX.1-2024 (XCU uux), in C with
no dependencies beyond libc. It belongs to the UUCP Utilities option.

    uux [-jnp] command-string...

## Build and test

    make           # builds ./uux
    make test      # the tests (sh test_uux.sh)
    make install   # into $(PREFIX)/bin; PREFIX=/usr/local, DESTDIR= for staging
    make clean

## What is implemented

* The UUCP system of uucomm.c (see ../uucp). The command string: a pipeline
  of words (quoted with ' " \), "|", "<" and ">"; the first command may name
  the system it runs on (system!command).
* Files: system!path words of known systems, the words after < and >, and
  (system!path) output files; those of other systems are copied to where the
  command runs, output files sent where they go after it.
* The command runs with sh in the spool of its system; other systems run
  only the commands of their configuration (rmail and rnews by default). -j,
  -n, -p (or "-").

## Choices where the standard leaves room

* ">>", "<<", ">|", ">&", ";", "&", "&&" and "||" are not accepted.
* A word with an unknown system before "!" is a word as it is (a mail
  address).
* The user gets mail when the command cannot be run or fails (not with -n).
* Exit status 0 when the command was run, 1 when not, 2 for usage errors.

## Testing

`test_uux.sh` runs the utility on prepared input and compares the output and
the exit status: commands here and there, files from and to other systems,
pipelines, output files, refused commands and syntax, -j, -n, -p, mail,
systems that cannot be reached. `make` builds uucico (as in ../uucp) for
them. The binary tested is `./uux`, or `$U` if set.

## Files

| file | contents |
|------|----------|
| `uux.c` | the utility |
| `../../lib/libxcu/uucico.c` | uucico (shared with uucp) |
| `../../lib/libxcu/uucomm.c` | the UUCP system: configuration, spool, the conversation with other systems, the jobs (shared with uucp and uustat) |
| `../../lib/libxcu/uucomm.h` | its declarations |
| `test_uux.sh` | the tests: `make test`, or `sh test_uux.sh` |
| `Makefile` | build, test, install, clean |
