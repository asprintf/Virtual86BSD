# HOWTO: the x86_64-v86bsd cross toolchain (GNU binutils and gcc)

GNU gcc 16.2.0 (C and C++) and binutils 2.47, built on the host to make
programs for V86BSD: `x86_64-v86bsd-gcc`, `-g++`, `-as`, `-ld`, `-ar`.
The world (`make buildworld`, `obj/world`) is the sysroot: its headers and
its C library are the ones gcc uses.  With it a host builds V86BSD with gcc
(`distrib/build.sh` does that with the host's own gcc and
`lib/libc/tools/libc-cc`); this toolchain is the step toward a gcc that
runs inside V86BSD.

## Build (on the host)

    make buildworld
    sh ports/gcc/build.sh -t

downloads binutils and gcc to `obj/toolchain/dist` (the checksums are in
the script), patches them with `binutils-2.47.diff` and `gcc-16.2.0.diff`,
builds them out of the tree and installs into `obj/toolchain/prefix`; about
twenty minutes.  `-t` also cross-compiles `tests/`.  Use it with

    PATH=obj/toolchain/prefix/bin:$PATH
    x86_64-v86bsd-gcc -O2 -o hello hello.c            # dynamic: /usr/lib/libc.so
    x86_64-v86bsd-gcc -O2 -static -o hello hello.c
    x86_64-v86bsd-g++ -std=c++17 -O2 -o prog prog.cc

## Test (inside V86BSD)

Put the programs of `obj/toolchain/tests` and `tests/expect-*` in a
directory of the root, boot it with a NIC or not, and `sh tests/run.sh DIR`.
The programs check what gcc generates beyond the library: 128-bit division
(libgcc), long double, TLS, constructors and destructors, threads,
`setjmp`, variable length arrays; and in C++: iostreams, containers,
exceptions, `dynamic_cast`, `std::thread`, static constructors.  Both
linking modes pass.

## What the patches change

* `config.sub` (both trees): `v86bsd` is an operating system.
* binutils: `bfd/config.bfd`, `ld/configure.tgt`, `gas/configure.tgt`: the
  target is plain ELF64 x86-64 with the `elf_x86_64` emulation.
* gcc: `gcc/config.gcc` and the new `gcc/config/v86bsd.h`: built on the
  GNU/Linux x86-64 user-space description (`gnu-user.h`, which already has
  `crt1.o`, `Scrt1.o`, `rcrt1.o`, `-lc`), with the dynamic linker
  `/usr/lib/libc.so`, the macro `__V86BSD__`, and `-D_GNU_SOURCE` for C++.
* `libgcc/config.host`: the ELF start-up files (`crtbegin*.o`, `crtend*.o`).
* `libstdc++-v3/crossconfig.m4` and `configure`: the target uses the
  checks of the Linux/glibc branch, which probe the library instead of
  assuming it.

## Not there yet

* No shared `libgcc_s` and `libstdc++` (`--disable-shared`): programs link
  them statically, also the dynamically linked ones.
* No libatomic, libgomp, libquadmath, libsanitizer, libitm, lto.
* The unwinder uses the frame registry that `crtbegin.o` fills, not
  `PT_GNU_EH_FRAME`: the C library has no `dl_iterate_phdr`, `<link.h>`
  or `<elf.h>` yet.  Exceptions work in both linking modes.
* The C library has no `__cxa_atexit`: g++ puts the destructors of static
  objects in `.fini_array`, which the C library runs at exit.
* The compiler runs on the host; a native build (GMP, MPFR, MPC, binutils,
  make and gcc itself for V86BSD, a Canadian cross) follows.
