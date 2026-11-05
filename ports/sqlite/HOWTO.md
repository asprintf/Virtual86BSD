# HOWTO: build SQLite inside Shall

SQLite 3.54.1 builds inside a Shall system with its own `configure` and
makefiles, from either form of its sources:

- the **source tree** (the default): SQLite's build then makes the
  amalgamation, `sqlite3.c`, itself — with jimsh (a small Tcl, built by
  c17 at configure time), lemon (SQLite's parser generator) and
  mkkeywordhash, all built and run inside Shall.  The `sqlite3.c` made
  this way is byte for byte the one the host's gcc build makes.  The
  tree also has SQLite's test programs written in C, which `-t` runs.
- the **amalgamation** tarball, `sqlite-autoconf-3540100.tar.gz` from
  sqlite.org: `sqlite3.c` and `shell.c` ready-made.

Six small changes are needed (`posix-src.diff` for the tree,
`posix-autoconf.diff` with the same changes for the amalgamation), all
for interfaces POSIX.1-2024 doesn't have.  Three options on
`configure`'s and `make`'s command lines take care of the rest.

## 1. Get the sources (on the host)

    sh ports/get.sh sqlite

clones git tag `version-3.54.1` of <https://github.com/sqlite/sqlite>
(SQLite's official mirror: the sources of sqlite.org's
`sqlite-src-3540100.zip`) into `root/usr/src/sqlite-3.54.1`.  Without git
it downloads that tag's tarball from GitHub.  With a copy you already
have, either form: `sh ports/get.sh sqlite sqlite-src-3540100.zip` or
`sh ports/get.sh sqlite sqlite-autoconf-3540100.tar.gz` (sqlite.org's
download page has both).

## 2. Build and install (inside)

    sh /usr/src/shall/ports/sqlite/build.sh          # -t: also the tests

That applies the diff (once), then runs, with `CC=c17`:

    CC_FOR_BUILD=c17 LD=c17 CFLAGS='-O 1' ./configure --prefix=/usr/local --disable-shared
    make CFLAGS.core=
    c17 -G -O 1 -DSQLITE_ENABLE_MATH_FUNCTIONS ... -o libsqlite3.so sqlite3.c

checks the new `sqlite3` with a few statements of known result, and
installs:

| Where | What |
|---|---|
| `/usr/local/bin/sqlite3` | the command-line shell |
| `/usr/local/lib/libsqlite3.a`, `libsqlite3.so` | the library, static and shared |
| `/usr/local/include/sqlite3.h`, `sqlite3ext.h` | its headers |

Programs then build with `c17 -o prog prog.c -lsqlite3`.  As c17 links
dynamically by default, that takes `libsqlite3.so` (`c17 -B static ...`:
`libsqlite3.a`).  `-I` builds without installing; the output of
configure and make goes to `build.log`.

Try it:

    sqlite3 /tmp/test.db
    sqlite> CREATE TABLE t(x); INSERT INTO t VALUES (1), (2), (3);
    sqlite> SELECT sum(x), sqlite_version() FROM t;
    sqlite> .quit

## 3. Test (source tree)

`build.sh -t` builds SQLite's test programs that are written in C and
runs them (each one's output in `NAME.log` in the sources):

| Program | What it does | Result here |
|---|---|---|
| `fuzzcheck test/fuzzdata*.db` | 46 391 SQL and database-file cases from years of fuzzing, with invariant checks | 0 errors |
| `make mptest` | several processes writing one database at once, in the journal modes DELETE, WAL, PERSIST, TRUNCATE (file locks, shared memory) | 0 errors in 8 runs |
| `threadtest3` | 15 tests of threads sharing databases | 0 errors |
| `speedtest1 --verify --size 25` | a benchmark whose results are hashed | the same hash as with gcc on glibc |

The main test suite (`make test`, thousands of `.test` files) is written
in Tcl and needs Tcl's C library, `testfixture`; Shall has no Tcl, so it
isn't run.  `sessionfuzz`, the last part of `make fuzztest`, needs zlib.

## The changes

In the diffs:

- **`gettimeofday()`**, three times: SQLite's Unix VFS (the current
  time, for `datetime('now')` and the like), its key-value VFS, and the
  shell's `.timer`.  POSIX.1-2024 removed `gettimeofday()`; the diff uses
  `clock_gettime(CLOCK_REALTIME)` when `_POSIX_VERSION` is 202405 or more
  (SQLite already does so on VxWorks).
- **`gettimeofday()` in jimsh0.c**, the Tcl interpreter autosetup builds
  first: the same.
- **`<memory.h>`** (`ext/misc/base85.c`, part of the shell): not a
  header of ISO C or POSIX; `<string.h>`, included next, has what it
  declared.
- **`<utime.h>`** (`ext/misc/fileio.c`, part of the shell): removed by
  POSIX.1-2024, and nothing in the file uses it (it calls `utimes()`).

On the command lines:

- **`CC_FOR_BUILD=c17`**: autosetup compiles its jimsh with `cc` (not a
  POSIX utility; Shall's compiler is `c17`) unless told otherwise.
- **`LD=c17`**: configure stops when it can't find a program `ld`,
  though nothing but the compiler links.
- **`--disable-shared`, `make CFLAGS.core=`**: the makefiles' options for
  position-independent code and shared libraries are gcc's (`-fPIC`,
  `-shared`), which c17 rejects.  c17's `-G` does both; it can't simply
  take their place, because the source tree's makefile also uses the
  same options for the programs.  So `build.sh` makes `libsqlite3.so`
  itself with `c17 -G`.

## Found on the way (in Shall)

SQLite's source-tree makefile showed two differences between Shall's
make and the other makes; both are fixed in `utilities/make` (with tests
in `tests/cases.tl`, and in `make/README.md`):

- A rule whose target list expands to nothing (`$(TCLSH_CMD):`, with
  no Tcl) was an error, "missing target".  GNU and BSD make ignore such a
  rule, and so does Shall's make now.
- A file with no commands (`parse.h: parse.c`, both written by lemon)
  counted as remade, so everything depending on it was rebuilt on every
  `make`: the whole amalgamation, again.  Now an existing file with no
  commands keeps its time, as in GNU and BSD make.

And c17 warned "invalid use of structure with flexible array member"
for a union holding such a structure (`union { SrcList sSrc; u8
fromSpace[N]; }`, SQLite's way of giving one item room on the stack).
C17 forbids such a structure only as a member of a structure or an
element of an array, so the warning was wrong; it is gone (test in
`c17/test_c17.sh`).  The other warnings c17 gives on SQLite are right:
function pointers stored as `void *` in its table of math functions,
and an extra `;` after a macro.

## Time taken

On the 2-CPU machine of `../README.md`, in the chroot: the source tree
in 40 seconds (the amalgamation tarball in 25), `-t` adds 12 minutes
(building the test programs 1, fuzzcheck 1, mptest 6, threadtest3 4).
speedtest1 takes 3.9 s against 1.4 s with gcc -O2 on glibc.

## Tested

SQLite 3.54.1, built by `build.sh -t` inside a freshly bootstrapped Shall
system in a chroot, from the git tag (results above), and from an
amalgamation tarball made from that tag with SQLite's own
`make amalgamation-tarball` (sqlite.org's download wasn't reachable from
the test machine).  On Shall's own kernel in QEMU (emulation, no KVM):
the build (12 minutes), fuzzcheck (0 errors out of 46 391, in 15
minutes), and mptest's two scripts once each in WAL and DELETE mode (0
errors): several processes sharing one database through Shall's file
locks and, in WAL mode, a shared-memory index.
