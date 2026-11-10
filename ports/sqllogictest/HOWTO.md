# HOWTO: test SQLite inside V86BSD with sqllogictest

sqllogictest is the SQLite project's test of SQL correctness: 622
scripts with some 5.9 million queries whose results were recorded from
several database engines (SQLite, PostgreSQL, MySQL, SQL Server,
Oracle), so that they don't depend on SQLite to say what is right.
Each script is run twice, with SQLite's query optimizer on and off, and
each result is compared with the recorded one (results of more than a
few rows by MD5 hash), with SQLite's integrity check of the database
afterwards.  On V86BSD it tests the SQLite of `ports/sqlite`, as c17
compiled it, on V86BSD's C library.

## 1. Get the sources (on the host)

    sh ports/get.sh sqllogictest

clones <https://github.com/gregrahn/sqllogictest>, a mirror of the
project's Fossil repository at <https://www.sqlite.org/sqllogictest>, into
`root/usr/src/sqllogictest`.  It is large: 1.1 GB of test scripts.  For a
system booted in QEMU, make the disk image bigger than the default:
`sh mkdisk.sh -s 4096`.

## 2. Build (inside)

SQLite first, then sqllogictest:

    sh /usr/src/ports/sqlite/build.sh
    sh /usr/src/ports/sqllogictest/build.sh

`build.sh` applies `posix.diff` (once), compiles `src/sqllogictest.c` and
`src/md5.c` with c17 and links them with the installed SQLite
(`-lsqlite3`), installs `/usr/local/bin/sqllogictest`, and runs one
script as a check.  (The project's makefile links instead the copy of
SQLite in `src/`, an amalgamation of 3.54.0; that would test that copy, not
the installed SQLite, and would need `ports/sqlite`'s changes too.)

## 3. Run the tests

    sh /usr/src/ports/sqllogictest/run.sh -j 2

`run.sh` does what the project's `run-all.tcl` does (Tcl, which V86BSD
doesn't have): every `*.test` script under `test/`, twice
(`-parameter "integrity_check;optimizer=0"` and `optimizer=-1`), and adds
up the results.  `-j N` runs N at a time (each in its own directory:
sqllogictest's database is `./test.db`).  It writes every run's output to
`run.log` in the current directory and ends with

    0 errors out of 11879758 tests and 1244 invocations in SECONDS seconds

A subset: `sh run.sh /usr/src/sqllogictest/test/select1.test` or a
directory, such as `.../test/evidence` (the SQL language, statement by
statement; seconds).

## The change (`posix.diff`)

- **`strcasecmp()`** is declared in `<strings.h>`; `sqllogictest.c`
  included only `<string.h>`, where glibc also declares it.  C99 (c17
  too) doesn't allow calling an undeclared function.

## Time taken

On the 2-CPU machine of `../README.md`, in the chroot with `-j 2`:
10½ minutes (19 minutes of CPU).  (The host's gcc build, with its own SQLite 3.45, needed 3¾
minutes for one of the two passes, single-threaded.)  On V86BSD's kernel
in QEMU without KVM the whole suite would take some 8 to 10 hours; a subset
such as `test/evidence` and `test/select*.test` takes minutes.

## Tested

Inside a freshly bootstrapped V86BSD system, in a chroot, with SQLite
3.54.1 of `ports/sqlite`: `0 errors out of 11879758 tests and 1244
invocations` — the same number of tests as the host's build runs, with
the same result.
