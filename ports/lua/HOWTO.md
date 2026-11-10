# HOWTO: build Lua inside V86BSD

Lua 5.4 builds unmodified inside a V86BSD system, and passes Lua's
complete official test suite.  The build uses only V86BSD's own tools:
`c17` compiles and links, `ar` makes the library.

## 1. Get the sources (on the host)

    sh ports/get.sh lua

takes the developers' tree, git tag `v5.4.7` of
<https://github.com/lua/lua>, which includes the test suite, `testes/`.
Without git it downloads the release tarball,
<https://www.lua.org/ftp/lua-5.4.7.tar.gz> (sources in `src/`), and puts
the separate test tarball (`lua-5.4.7-tests.tar.gz`) inside it.  Either
way the sources land in `root/usr/src/lua-5.4.7`.

## 2. Build, test and install (inside)

    sh /usr/src/ports/lua/build.sh -t

`-t` runs the test suite after the build.  Then the script installs
`lua` (and `luac`, if the sources have `luac.c`) into `/usr/local/bin`,
`liblua.a` into `/usr/local/lib` and the headers (`lua.h`, `luaconf.h`,
`lualib.h`, `lauxlib.h`) into `/usr/local/include`; `-I` builds only.  The script does this, in the directory that holds
`lua.c`:

    for f in *.c; do
        case $f in
        lua.c|luac.c|onelua.c|ltests.c) ;;      # programs, not library
        ldo.c) c17 -O 1 -c ldo.c ;;             # see below
        *)     c17 -O 1 -D LUA_USE_POSIX -c "$f" ;;
        esac
    done
    ar -rc liblua.a *.o
    c17 -O 1 -D LUA_USE_POSIX -c lua.c
    c17 -o lua lua.o liblua.a -lm

and for the tests, in `testes/`:

    ../lua -e "_U=true" all.lua

`_U=true` selects the user-level tests (the internal ones need a
specially instrumented build).  Through all 26 files the suite ends with
`final OK !!!`; the script keeps its output in `test.log`.

Try it:

    lua -v
    lua -e 'print(_VERSION, math.pi, 7 // 2, os.date("%Y-%m-%d"))'

## Why not `make posix`?

Lua's own makefile is written for gcc and assumes tools that POSIX
doesn't have:

- **Compiler flags.** It passes `-std=gnu99`, `-Wall` and `-Wextra`.
  POSIX `c17` doesn't have those options, so it rejects them.
- **`ranlib`.** The makefile runs `ranlib` after `ar`. `ranlib` isn't
  part of POSIX, so V86BSD doesn't have it. It isn't needed either:
  V86BSD's `ar` keeps an archive's symbol table up to date itself.
- **`_setjmp` in `ldo.c`.** With `LUA_USE_POSIX`, `ldo.c` handles
  errors with `_setjmp`/`_longjmp`. POSIX.1-2024 removed those two
  functions, so V86BSD's libc doesn't have them. `ldo.c` uses
  `LUA_USE_POSIX` for nothing else. Compiled without it, it falls back
  to ISO C's `setjmp`/`longjmp`, which do the same job here. Every other
  file keeps `LUA_USE_POSIX`, which gives `io.popen`, `mkstemp`-based
  `os.tmpname` and the full `os.date`.

## What you don't get

- **C modules.** `require` can't load C modules (`.so` files): this
  build leaves out `LUA_USE_DLOPEN`.  Modules written in Lua work as
  usual.  (V86BSD has `dlopen` and `c17 -G` for shared objects, so this
  could be added; it hasn't been tried.)
- **Line editing.** The interactive prompt has no line editing or
  history, because V86BSD has no readline (it's not part of POSIX).

## Tested

Lua 5.4.7 (git tag `v5.4.7`), built by `build.sh -t` inside a freshly
bootstrapped V86BSD system, in a chroot and on V86BSD's own kernel in
QEMU: the full test suite passes.  The release tarball's layout (`src/`,
`luac.c`, the separate test tarball) is handled by the script but has
not been tested here.
