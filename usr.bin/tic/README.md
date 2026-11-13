# tic — the terminfo compiler

NetBSD's `tic` (`usr.bin/tic/tic.c` of NetBSD; see
`../../share/doc/credits.md`), with libcurses (`../../lib/libcurses`): it
compiles terminal descriptions in the terminfo source format of X/Open
Curses into a database of NetBSD's format, a cdb file, the one libcurses
reads.

    tic [-acSsx] [-o file] source

* `-o file`: the database (default `source.cdb`); `-c` only checks;
  `-s` says how many entries and aliases were written; `-x` keeps
  capabilities that are not X/Open's; `-a` keeps commented-out ones;
  `-S name...` writes the named entries as C source
  (`lib/libcurses/terminfo/compiled_terms.c` is made so).
* The output is the same for the same source (the cdb's seed is fixed).
* The build makes `/usr/share/misc/terminfo.cdb` with it from
  `../../share/terminfo/terminfo`.

## Build and test

    make           # builds ./tic, linked with the installed libcurses
    make test      # the tests (../../regress/usr.bin/tic/test_tic.sh)
    make install   # into $(PREFIX)/bin
    make clean
