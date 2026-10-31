# HOWTO: build Lua inside Shall

Lua 5.4 builds unmodified inside a Shall system. The build uses only
Shall's own tools: `c17` compiles and links, and `ar` makes the library.
The result passes Lua's complete official test suite. The build takes
about 6 seconds and the test suite about 3.

You need a system made by `bootstrap.sh` (see `README.md`). Below,
`root` is that system's directory, as seen from the host.

## 1. Get the sources (on the host)

Shall has no network tools and no `gzip`, so fetch and unpack Lua on the
host, into the system's `/usr/src`:

    cd root/usr/src
    curl -O https://www.lua.org/ftp/lua-5.4.7.tar.gz
    tar -xzf lua-5.4.7.tar.gz             # sources in lua-5.4.7/src

Or take the developers' tree. It has the sources in its top directory
and includes the test suite, `testes/`:

    git clone --depth 1 -b v5.4.7 https://github.com/lua/lua.git

The release tarball has no tests. They are a separate download:
`lua-5.4.7-tests.tar.gz` from <https://www.lua.org/tests/>.

If you built the system without root privileges, the files are yours.
Inside the user namespace they then belong to root, as they should.

## 2. Enter the system

    sh chroot.sh root

## 3. Build

Go to the directory that holds `lua.c`:

    cd /usr/src/lua-5.4.7/src                # release tarball
    cd /usr/src/lua                          # git clone

and run:

    rm -f *.o liblua.a
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
    if [ -f luac.c ]; then                   # release tarball only
        c17 -O 1 -D LUA_USE_POSIX -c luac.c
        c17 -o luac luac.o liblua.a -lm
    fi

Try it:

    ./lua -v
    ./lua -e 'print(_VERSION, math.pi, 7 // 2, os.date("%Y-%m-%d"))'

### Why not `make posix`?

Lua's own makefile is written for gcc and assumes tools that POSIX
doesn't have:

- **Compiler flags.** It passes `-std=gnu99`, `-Wall` and `-Wextra`.
  POSIX `c17` doesn't have those options, so it rejects them.
- **`ranlib`.** The makefile runs `ranlib` after `ar`. `ranlib` isn't
  part of POSIX, so Shall doesn't have it. It isn't needed either:
  Shall's `ar` keeps an archive's symbol table up to date itself.
- **`_setjmp` in `ldo.c`.** With `LUA_USE_POSIX`, `ldo.c` handles
  errors with `_setjmp`/`_longjmp`. POSIX.1-2024 removed those two
  functions, so Shall's libc doesn't have them. `ldo.c` uses
  `LUA_USE_POSIX` for nothing else. Compiled without it, it falls back
  to ISO C's `setjmp`/`longjmp`, which do the same job here. Every other
  file keeps `LUA_USE_POSIX`, which gives `io.popen`, `mkstemp`-based
  `os.tmpname` and the full `os.date`.

## 4. Test (git clone, or with the test tarball)

    cd testes
    ../lua -e "_U=true" all.lua

`_U=true` selects the user-level tests; the internal tests need a
specially instrumented build. Run through all 26 files, the suite ends
with `final OK !!!`.

## 5. Install (optional)

    cp lua /usr/bin/        # and luac, if you built it

## What you don't get

- **C modules.** `require` can't load C modules (`.so` files). Shall
  builds its programs statically, and this build leaves out
  `LUA_USE_DLOPEN`. Modules written in Lua work as usual.
- **Line editing.** The interactive prompt has no line editing or
  history, because Shall has no readline (it's not part of POSIX).

## Tested

Lua 5.4.7 (git tag `v5.4.7`), built inside a freshly bootstrapped Shall
system, passed the full test suite. The `luac` part of step 3 applies to
the release tarball, whose `src/` also holds `luac.c`; that part is
built the same way but has not been tested here.
