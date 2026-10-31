# uustat — uucp status enquiry and job control (POSIX.1-2024)

An implementation of the `uustat` utility of POSIX.1-2024 (XCU uustat), in C
with no dependencies beyond libc. It belongs to the UUCP Utilities option.

    uustat [-q|-k jobid|-r jobid]
    uustat [-s system] [-u user]

## Build and test

    make           # builds ./uustat
    make test      # the tests (sh test_uustat.sh)
    make install   # into $(PREFIX)/bin; PREFIX=/usr/local, DESTDIR= for staging
    make clean

## What is implemented

* The jobs of the spool of the UUCP system of uucomm.c (see ../uucp): those
  of uucp -r and uux, and those not done because a system could not be
  reached.
* Each job: its id, user, system, when it was made, what it does, and its
  failed attempts; the user's jobs, -s system, -u user.
* -q: per system its jobs and last attempt; -k jobid removes a job, -r jobid
  touches it.

## Choices where the standard leaves room

* The lines: "id user system YYYY-MM-DD HH:MM description", -q "system count
  status".
* -k: "Job ID killed"; -r: "Job ID rejuvenated". A job that is not there or
  not the user's: status 1. Usage errors exit with status 2.

## Testing

`test_uustat.sh` runs the utility on prepared input and compares the output
and the exit status: job files written in a spool of the test: the listings,
-s, -u, -q, -k and -r, other users' jobs, an empty or missing spool. The
binary tested is `./uustat`, or `$U` if set.

## Files

| file | contents |
|------|----------|
| `uustat.c` | the utility |
| `../lib/uucomm.c` | the UUCP system: configuration, spool, the conversation with other systems, the jobs (shared with uucp and uux) |
| `../lib/uucomm.h` | its declarations |
| `test_uustat.sh` | the tests: `make test`, or `sh test_uustat.sh` |
| `Makefile` | build, test, install, clean |
