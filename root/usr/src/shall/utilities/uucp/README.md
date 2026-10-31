# uucp — system-to-system copy (POSIX.1-2024)

An implementation of the `uucp` utility of POSIX.1-2024 (XCU uucp), in C
with no dependencies beyond libc, with uucico, the other side of uucp and
uux and the runner of queued jobs. It belongs to the UUCP Utilities option.

    uucp [-cCdfjmr] [-n user] source-file... destination-file
    uucico -S
    uucico [-s system]

## Build and test

    make           # builds ./uucp and ./uucico
    make test      # the tests (sh test_uucp.sh)
    make install   # into $(PREFIX)/bin; PREFIX=/usr/local, DESTDIR= for staging
    make clean

## What is implemented

* The UUCP system of uucomm.c: a configuration file ($UUCP_CONF or
  /etc/uucp/config: nodename, spool, pubdir, mail, read, write, commands,
  and "system NAME COMMAND", a shell command reaching "uucico -S" there, as
  "ssh NAME uucico -S"); a spool of jobs; a conversation of lines and files
  over the command's pipes.
* A copy is a job done at once (-r: queued); one that cannot reach a system
  stays queued for a later uucp or uux, or "uucico -s". Names: system!path,
  routes (a!b!path), ~user, ~/ (the public directory), relative paths made
  absolute here.
* -c, -C, -d, -f, -j, -m, -n user, -r as the standard says.

## Choices where the standard leaves room

* Other systems may read and write only in the public directory and the
  directories of "read" and "write".
* The files made: mode 0666, or 0777 if the source is executable, less the
  umask.
* Exit status 0 when the job was done (or queued with -r), 1 when not, 2 for
  usage errors.

## Installing

make install puts uucp into $(PREFIX)/bin and uucico into $(PREFIX)/sbin;
the systems in the configuration reach it, there, by the command of their
"system" lines.

## Testing

`test_uucp.sh` runs the utility on prepared input and compares the output
and the exit status: copies here, to, from and between other systems
(systems of the test reached by uucico through env), routes, patterns,
directories, every option, mail, queued jobs run by uucico, systems that
cannot be reached, refused paths. The binary tested is `./uucp`, or `$U` if
set.

## Files

| file | contents |
|------|----------|
| `uucp.c` | the utility |
| `../lib/uucico.c` | uucico: the other side, and the runner of queued jobs (shared with uux) |
| `../lib/uucomm.c` | the UUCP system: configuration, spool, the conversation with other systems, the jobs (shared with uustat and uux) |
| `../lib/uucomm.h` | its declarations |
| `test_uucp.sh` | the tests: `make test`, or `sh test_uucp.sh` |
| `Makefile` | build, test, install, clean |
