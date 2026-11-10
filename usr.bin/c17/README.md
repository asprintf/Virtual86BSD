# c17 — compile standard C programs (POSIX.1-2024)

An implementation of the `c17` utility of POSIX.1-2024 (XCU c17): a
compiler for ISO/IEC 9899:2018 (C17) for x86-64 (System V ABI, ELF,
Linux), in C11 with POSIX interfaces only. It belongs to the C-Language
Development Utilities option.

    c17 [options...] pathname [[pathname] [-I directory] [-L directory]
        [-l library] [-R directory]]...

c17 does every step itself: preprocessing, compiling, assembling (its
own assembler writes ELF relocatable objects) and link editing (its own
link editor writes static and dynamic executables). Besides the C
library it uses no other program and no file of another compiler: not
`as`, `ld`, `libgcc` or `crtbegin.o`. Its own run-time library,
`libc17rt.a`, holds complex multiplication and division and what the C
libraries' static archives expect from the compiler.

It works with the GNU C library of a Linux distribution (dynamically
linked by default, or statically) and with musl (statically by default,
or dynamically); which C library and how is a profile in `libc/`, so
another C library, such as the one being written for this system, is a
new profile.

References: the c17 page of POSIX.1-2024, and for the language WG14
N2347, the C17..C2x working draft of March 2019, which marks every change
from C17, so the C17 text can be read off it.

## Build and test

    make           # builds ./c17 and libc17rt.a
    make test      # the tests (sh test_c17.sh)
    make difftest  # compares with cc (gcc) and the GNU assembler
    make install   # into $(PREFIX)/bin; libc/ and libc17rt.a into $(PREFIX)/lib/c17
    make clean

The C library is described by a profile, a file of lines `key value`
in `libc/` (installed in `$(PREFIX)/lib/c17/libc`):

    include /usr/include/x86_64-linux-gnu   # header directories, in order
    include /usr/include
    libdir /usr/lib/x86_64-linux-gnu        # crt1.o, libc.a, libc.so; also for -l
    libdir /lib/x86_64-linux-gnu
    crt1 crt1.o                             # the start files (crti, crtn optional)
    crti crti.o
    crtn crtn.o
    interp /lib64/ld-linux-x86-64.so.2      # the dynamic linker
    link dynamic                            # the default: static or dynamic
    define NAME=VALUE                       # a macro (as -D), for one of c17's
                                            # headers: musl's __c17_int_fast16=int

`libc/glibc` and `libc/musl` are for Debian, Ubuntu and their
derivatives (musl from the package musl-dev); on other distributions
the directories may differ. `libc/v86bsd` is V86BSD's own C library
installed in /usr (a V86BSD system: see ../../bootstrap.sh). `make
LIBC=musl` makes musl the default (`LIBC=glibc` is). When c17 runs, `C17_LIBC` chooses another: the name of
a profile, the pathname of one, or a directory with `include/` and `lib/`
(a C library linked statically). `C17_LIBDIR` replaces
`$(PREFIX)/lib/c17` (in the build directory the tests set it to the
directory itself).

c17 compiles itself: `make CC=c17` with an installed c17 gives
the same compiler, and the tests check that the assembly it writes stays
the same over two more generations.

## What is implemented

The options of the standard:

| option | behaviour |
|---|---|
| `-c` | compile only: for `file.c` (or `file.i`) the object `file.o` in the current directory |
| `-o outfile` | the executable, or with `-c` and one operand the object; with `-E` the output |
| `-D name[=value]` | `#define name value` (1 by default) |
| `-U name` | `#undef name`; it wins over `-D` whatever their order |
| `-I directory` | searched for `#include "..."` after the directory of the including file, and for `#include <...>`, before the standard places |
| `-L directory` | searched by the `-l` options that follow it |
| `-l library` | in the `-L` directories given before it, then in the C library's directories, c17's own, and `$(PREFIX)/lib` (where yacc and lex install `liby.a` and `libl.a`): with dynamic linking `liblibrary.so`, or else `liblibrary.a`, in the first directory that has either; with static linking `liblibrary.a` |
| `-E` | preprocess only, with `# line "file"` markers |
| `-g` | line information (DWARF line tables) in objects and executables |
| `-s` | an executable without symbol table or debugging information |
| `-O optlevel` | `0` turns off the code generator's improvements; `1` (the default), `2`, `3`, `s` turn them on; anything else is an error |
| `-B dynamic`, `-B static` | dynamic or static link editing; the last one given counts; without either, the C library's profile decides |
| `-B standalone` | (an extension) static link editing without the C library, its start files and c17's run-time library: for a kernel, with a linker script operand (below) |
| `-R directory` | the directories, in order, in the executable's run-time search path (`DT_RUNPATH`); ignored with a warning when linking statically |
| `-G` | position-independent code; without `-c`, a shared library (see below) |

Operands: `.c` files are compiled, `.i` files (the output of `-E`)
compiled without further macro replacement, `.s` files (assembly, an
extension) assembled, and anything else (`.o`, `.a`) given to the link
editor where it appears. The order of operands, `-L` and `-l` is kept.
A compilation error in one operand does not stop the others, but then
there is no link editing and the exit status is 1. Diagnostics are
`file:line:col: error: ...` with the source line and a caret; warnings
do not change the exit status. Temporary files are made in `$TMPDIR` (or
/tmp) and removed, also on SIGINT, SIGTERM and SIGHUP.

The language: all of C17, with its optional parts.

* Variable length arrays (`__STDC_NO_VLA__` is not defined), with the
  storage released when a jump leaves their scope, and jumps into their
  scope diagnosed.
* Complex arithmetic (`__STDC_IEC_559_COMPLEX__`): the three complex
  types with the ABI's classes, conversions, `+ - * / == !=`. `*` and `/`
  call `libc17rt.a`, which follows the algorithms of Annex G (G.5.1), so
  infinities and NaNs behave as it requires; float operands are computed
  in double and rounded once. `<complex.h>` is the C library's, with
  `CMPLX`, `CMPLXF` and `CMPLXL` where it lacks them.
* Atomics (`__STDC_NO_ATOMICS__` is not defined): `_Atomic` and every
  type, macro and generic function of `<stdatomic.h>`; objects of 1, 2, 4
  and 8 bytes, always lock-free; read-modify-write operations are single
  `lock cmpxchg` loops; every operation is sequentially consistent.
* Threads: `_Thread_local` (local-exec model in executables).
* `setjmp` and the other functions that return twice also work where C17
  does not require it, as `int r = setjmp(env)` (glibc's
  `pthread_cleanup_push` does that): nothing is kept on the stack across
  the call.
* Trigraphs, digraphs, universal character names and UTF-8 in
  identifiers and literals, `u8`/`u`/`U`/`L` literals, `_Generic`,
  `_Static_assert`, `_Alignas`, `_Alignof`, `_Noreturn`, anonymous
  structures and unions, flexible array members, designated initializers,
  compound literals, old-style function definitions, tentative
  definitions, the inline semantics of C99.
* Every violation of a syntax rule or constraint is diagnosed
  (5.1.1.3). Most are errors; a few that other compilers accept are
  warnings (incompatible pointer assignment, conversion between integer
  and pointer without a cast, discarded qualifiers, excess initializers,
  `return` with or without a value in the wrong kind of function, macro
  redefinition). Calls of undeclared functions are errors.

The preprocessor follows Prosser's algorithm (hide sets), with
placemarkers, `#if` arithmetic in `intmax_t`/`uintmax_t`, and `#line`.
The code generator is a stack machine with local improvements unless
`-O 0`: operands addressed directly, immediate operands, compare and
branch fused, constants folded, jump tables for dense `switch`es. Each
translation unit is compiled in a child process.

The assembler reads the AT&T syntax of the GNU assembler: what the code
generator writes, and what hand-written system code uses: the integer,
SSE and x87 instructions (with the same encodings as the GNU assembler,
including its reversed x87 mnemonics), the usual directives (`.text`,
`.data`, `.bss`, `.section` with flags and types, `.globl`, `.weak`,
`.local`, `.hidden`, `.type`, `.size`, `.set`, `.comm`, `.lcomm`,
`.byte` to `.quad`, `.ascii`, `.asciz`, `.string`, `.zero`, `.skip`,
`.align`, `.balign`, `.p2align`, `.file`, `.loc`; `.cfi_*` are ignored),
local labels `1:` with `1b` and `1f`, expressions with differences of
labels (also of labels defined later), the relocation operators `@PLT`,
`@GOTPCREL`, `@tpoff`, `@gottpoff`, `@tlsgd`, `@tlsld`, `@dtpoff`,
segment prefixes (`%fs:`), and `;` between statements.  For a kernel:
the system instructions (`in`/`out` and the string forms, `cli`/`sti`,
`lgdt`/`lidt`/`ltr` and the other descriptor-table instructions,
`invlpg`, `mov` to and from control and segment registers,
`rdmsr`/`wrmsr`, `swapgs`, `iretq`, `lretq`, `ljmp`, `fxsave`/`fxrstor`,
`ldmxcsr`/`stmxcsr`), `.code32` (32-bit protected-mode code: no REX
prefixes, 32-bit address registers, absolute `disp32` addresses) and
`.code64`, macros (`.macro NAME params` ... `.endm`, with `\param`,
defaults `param=value`, `\()` and `\@`), and a parenthesized expression
as an operand (`(sym - BASE)`).  Short jumps are not relaxed: a jump
whose target is not yet known gets a 32-bit displacement.

The link editor reads ELF relocatable objects, `ar` archives (GNU and
BSD long names), shared objects, and the linker scripts C libraries
install as `libc.so` and `libm.so` (`GROUP`, `INPUT`, `AS_NEEDED`).
Of a kernel's linker script it takes `ENTRY(symbol)` and from `SECTIONS`
the address of the image (`. = ADDRESS;` before the first output
section) and symbols defined at its end (`NAME = .;` after the last
one, perhaps after `. = ALIGN(N);`); the output section descriptions
themselves are not interpreted (sections go where c17 puts them), but a
section named `.boot` comes first, right after the ELF headers, for a
boot loader's header. It
writes an executable (`ET_EXEC`, not position-independent; with `-G` a
shared library, below): a read-only, an executable and a writable
segment at 0x400000, a TLS segment, a non-executable stack. It resolves strong, weak and common symbols (the
largest size and alignment), keeps the first of COMDAT groups, sorts
`.init_array.N` and `.fini_array.N` by priority, defines
`__init_array_start` and the other symbols the C libraries look for
(`__fini_array_*`, `__preinit_array_*`, `_etext`, `_edata`, `_end`,
`__bss_start`, `__ehdr_start`, `_GLOBAL_OFFSET_TABLE_`, `_DYNAMIC`,
`__dso_handle`, `__rela_iplt_start` and `__rela_iplt_end`, and
`__start_NAME` and `__stop_NAME` for sections named like C identifiers),
gives symbols reached through the GOT an entry, and rewrites the
general- and local-dynamic TLS sequences of position-independent objects
into the local-exec (or initial-exec) form. Indirect functions
(`STT_GNU_IFUNC`, in glibc's `libc.a`) get a PLT entry and an
`R_X86_64_IRELATIVE` relocation for the C library's start-up code.
Undefined symbols and multiple definitions are errors naming the
operands.

A dynamic executable has the C library's dynamic linker (`PT_INTERP`),
the shared objects it needs (`DT_NEEDED`, in the order given; those of
an `AS_NEEDED` list only if they provide a symbol), `DT_RUNPATH` for
`-R`, a hash table, the version requirements of the symbols bound
(`DT_VERNEED`, for glibc's versioned symbols), and the constructors of
`DT_INIT_ARRAY`. Functions of shared objects are called through PLT
entries; when the program takes the address of one, the PLT entry is
that address everywhere (canonical). Variables of shared objects that
the program uses directly (`environ`, `stdout`) are copied into it
(`R_X86_64_COPY`), together with their aliases. Symbols of the program
that shared objects refer to, or also define, are exported. All
bindings are made when the program is loaded (`DF_BIND_NOW`).

`-G` compiles position-independent code: variables and the addresses of
functions are reached through the GOT, calls through the PLT, except for
static symbols and those of hidden visibility; thread-local variables use
the general-dynamic model (`__tls_get_addr`), or initial-exec with
`__attribute__((tls_model("initial-exec")))`. Without `-c`, `-G` makes a
shared library (`ET_DYN`, loaded anywhere), whose `DT_SONAME` is the
name of the output file. Its functions bind to the library itself (as
ld's `-Bsymbolic-functions` would make them); its other variables, and
the addresses of its functions, may be taken over by a program (copy
relocations, canonical PLT entries), so they stay symbolic. Symbols of
hidden visibility, and c17's run-time support (linked in), are not
exported. Absolute addresses in data become `R_X86_64_RELATIVE`
relocations (ahead of the others), or `R_X86_64_64` against a symbol;
GOT entries `R_X86_64_RELATIVE`, `R_X86_64_GLOB_DAT`, `R_X86_64_TPOFF64`
or the pair `R_X86_64_DTPMOD64`/`R_X86_64_DTPOFF64`. Undefined symbols
are left to the dynamic linker (an error if they are hidden). The
library needs the C library's shared library (`DT_NEEDED`) when there is
one, unless it is that library (an output named `libc.so...`); it gets no
start files. An object not compiled with `-G` that uses a symbol of
another object directly, or an absolute address in code, is an error. A
linker script operand with `ENTRY(symbol)` gives the library an entry
point (a C library that is also its dynamic linker: V86BSD's `libc.so`).

GNU extensions accepted (only syntax that is invalid in C17, or reserved
identifiers, so strictly conforming programs are not affected):
`__attribute__` with `aligned`, `packed`, `noreturn`, `weak`, `alias`,
`visibility`, `section`, `constructor` and `destructor` (with priority),
`tls_model` and `used` (others are ignored), `__extension__`, `__typeof__`, `asm`
labels and basic `asm` statements, statement expressions, `a ?: b`, case
ranges, range designators, zero-length arrays, `, ## __VA_ARGS__`, named
variadic macro parameters, `#include_next`, `#pragma once`,
`push_macro`/`pop_macro`, `__has_include`, `__COUNTER__`, and the
builtins `__builtin_va_*`, `__builtin_offsetof`,
`__builtin_types_compatible_p`, `__builtin_expect`,
`__builtin_constant_p`, `__builtin_unreachable`, `__builtin_alloca`,
`__builtin_complex`, `__builtin_return_address` and
`__builtin_frame_address` (any constant level: c17 always keeps a frame
pointer), and `__builtin_inf`, `__builtin_huge_val`, `__builtin_nan("")`
(with `f` and `l`: constants, which raise no floating-point exception
when used, for `<math.h>`'s `INFINITY`, `HUGE_VAL` and `NAN`). `tls_model` is honoured as described for `-G`. c17 does not
define `__GNUC__`.

## Choices where the standard leaves room

* The default linking is the C library's (its profile): dynamic for
  glibc, static for musl. Executables are not position-independent;
  shared libraries (`-G`) are.
* A static executable with glibc: what `libc.a` expects from gcc's
  `libgcc` is in `libc17rt.a`: binary128 arithmetic and comparison
  (rounded to nearest, without the floating-point environment's rounding
  mode or flags), `__popcountdi2`, and the unwinding interface without an
  unwinder, since c17 writes no unwinding tables: `backtrace()` finds no
  frames, and thread exit and cancellation run the cleanup handlers and
  end the thread at once.
* glibc's `<sys/cdefs.h>` removes `__attribute__` for compilers that do
  not define `__GNUC__`, which c17 does not; with glibc, a program that
  needs an attribute has to `#undef __attribute__` after the headers.
* `-l c`, `-l m`, `-l pthread`, `-l rt`, `-l xnet` and `-l trace` (and
  `dl`, `util`, `resolv`, `crypt`, `iconv`) are satisfied by the C library
  when it has no such library.
* `-o` with `-c` or `-S` and more than one compiled operand is an error.
  Without `-o`, an operand `dir/file.c` gives `file.o` in the current
  directory.
* `-g`: line tables only (no variables or types); `-g` with `-s` strips.
* `-O`: only `0` changes anything.
* Extensions: `-S` (write the assembly to `file.s`), `-w` (no
  warnings), `-v` (show the link editor's inputs), `-dM` with `-E`
  (instead of the preprocessed text, a `#define` line for each macro
  defined at the end of the file), `.s` operands.
* Implementation-defined behaviour: `char` is signed; `short` 2, `int`
  4, `long`, `long long` and pointers 8 bytes; `long double` is the x87
  80-bit type in 16 bytes; two's complement, conversions to narrower
  signed types wrap, `>>` of a negative value is arithmetic; IEC 60559
  floating point, `FLT_EVAL_METHOD` 0; `wchar_t` is `int` (UTF-32), the
  execution character set UTF-8; plain `int` bit-fields are signed; an
  enumerated type is `unsigned int` unless an enumerator is negative.
* `#include` searches the directory of the including file (for "..."),
  the `-I` directories, the headers built into the preprocessor
  (`stddef.h`, `stdarg.h`, `float.h`, `stdbool.h`, `stdalign.h`,
  `stdnoreturn.h`, `iso646.h`, `stdatomic.h`, `complex.h`; named
  `<built-in>/stddef.h` and so on in diagnostics and `-E` output), then the
  C library's headers.
* `#pragma once`, `push_macro`, `pop_macro` and `GCC system_header` are
  recognised; the `STDC` pragmas are accepted (the default states
  apply); other pragmas are ignored (and kept by `-E`).
* Tentative definitions are definitions in `.bss`, not common symbols.
* `__DATE__` and `__TIME__` are the local time, or `SOURCE_DATE_EPOCH`.
* Not supported: `_Atomic` objects of other sizes than 1, 2, 4 and 8
  bytes (diagnosed), GNU extended `asm` with operands, computed `goto`.

## Testing

`test_c17.sh` (302 tests with glibc and musl both present, about
fifteen seconds) needs only the POSIX utilities and a C compiler for tl.
It finds which configurations work here (each profile in `libc/`, static
and dynamic) and runs the programs in all of them:

* the command line: every option and the order of operands, `-L` and
  `-l`, `.i`, `.a` and `.s` operands, errors (compilation continues, no
  link editing, the status), diagnostics naming files, the permissions of
  `a.out`, `TMPDIR`, `C17_LIBC` and profiles, `-B` (the dynamic linker
  and `DT_NEEDED`), `-R` (`DT_RUNPATH`), `-l y` and `-l l` with the
  libraries of this collection's yacc and lex;
* the programs of `tests/run` (complex arithmetic, atomics with threads,
  `long double`, program start with constructors, TLS in threads and weak
  references, thread exit and cancellation, copied variables and
  function addresses) at `-O 0` and `-O 1` with the default C library and
  in every other configuration, compared with their expected output;
* the chibicc test suite (39 files, adapted to C17: see
  `tests/chibicc/README`), 130 programs that each break a constraint
  (`tests/diag`), the examples of C17 6.10.3.3, 6.10.3.5 and 6.10.9 and
  corner cases of the preprocessor (`tests/pp`);
* the assembler: `tests/asm/insns.s` (some 270 instructions and 30
  directives) must give the bytes and relocations the GNU assembler gives
  (`tests/asm/insns.expected`); local labels, jump tables, data, `.comm`,
  expressions on labels defined later; the errors;
* the link editor: TLS in all models, the GOT, weak and strong symbols,
  common symbols, archives searched until nothing more is needed, the
  layout of the executable (read with `tests/elfinfo.tl`);
* the calling convention between the two code generators (`-O 0` with
  `-O 1`), on random functions of `tests/abigen.tl`;
* self-hosting: c17 compiles itself, the result compiles itself again,
  and all three write the same assembly for every source.

`make difftest` (`tests/difftest.sh`) compares with other
implementations: the programs of `tests/run` with `cc`; the calling
convention with `cc` in all four combinations of caller and callee
(`ABI_SEEDS`, 20 seeds of 40 functions); `tests/asm/insns.s` with the GNU
assembler, and c17's own assembly output assembled by it; with
`CTESTSUITE=dir` the 220 programs of c-testsuite, and with
`CSMITH=prefix` (`CSMITH_COUNT`) random programs of Csmith compared with
`cc -O0`.

Beyond these: this whole collection, built with `make CC=c17`
against glibc (dynamically linked, the default), passes its tests
exactly as the same sources built by gcc do (6874 tests; the 9 failures
of bc, diff and talk come from this test machine and are the same with
gcc). Built against musl it passes them as a build by gcc against musl
does (the differences from glibc are those of the C library: utmpx,
locales, iconv, regular expression messages, `REG_STARTEND`). Lua 5.5
built by c17 passes its whole test suite (`all.lua`) with glibc, and with
musl when `os.setlocale` is limited to "C"; SQLite's `speedtest1` gives
the same verification hash as with gcc, with glibc (with threads) and
with musl; Csmith programs agree with gcc. The binary128 functions of
`libc17rt.a` give the same results as gcc's `libgcc` for two million
random operands, special values among them.

Bugs found on the way: `long double` subtraction and division had their
operands reversed (now `tests/run/ldouble.c`); the x87 mnemonics with
`%st(i)` as destination encode as the GNU assembler has them (reversed
relative to Intel's names); a parameter could not be used in the type
of a later parameter (`int a[n][n]`, as glibc's `<regex.h>` does; now
`tests/run/vlaparam.c`); `x = setjmp(env)` lost `x` at `-O 0`.

## Files

| file | contents |
|------|----------|
| `c17.c` | the utility: options and operands, the C library, temporary files, link editing |
| `c17.h`, `parse.h`, `elf.h` | declarations; the parts of ELF the assembler and link editor use (`<elf.h>` is not POSIX) |
| `../../lib/libxcu/cpp.c`, `../../lib/libxcu/cpp.h` | translation phases 1 to 4: the tokenizer and preprocessor, with the headers c17 provides built in; memory, `Buf` and hash maps (shared with cflow, ctags and cxref) |
| `../../lib/libxcu/port.h` | the POSIX.1-2024 interfaces that older C libraries lack, and `NORETURN`/`PRINTF` (shared) |
| `util.c` | strings, paths, diagnostics |
| `tokenize.c` | c17's tokens from the preprocessor's: literals, keywords, string concatenation (phases 5 to 7) |
| `type.c` | types, compatibility, composite types, conversions |
| `decl.c`, `init.c`, `stmt.c`, `expr.c`, `eval.c` | declarations and attributes, initializers, statements, expressions, constant expressions |
| `codegen.c` | x86-64 code generation, as assembly text |
| `asm.c` | the assembler |
| `link.c` | the link editor |
| `libc/` | the profiles of the C libraries: `glibc`, `musl` |
| `rt_complex.c` | `libc17rt.a`: complex multiplication and division |
| `rt_float128.c`, `rt_float128.s` | `libc17rt.a`: binary128 arithmetic and comparison |
| `rt_unwind.c`, `rt_bits.c`, `rt_atomic.c` | `libc17rt.a`: the unwinding interface without an unwinder; `__popcountdi2`; the non-generic functions of `<stdatomic.h>` |
| `test_c17.sh` | the tests |
| `tests/run/` | programs with their expected output |
| `tests/chibicc/`, `tests/diag/`, `tests/pp/`, `tests/asm/` | test suites (see Testing) |
| `tests/elfinfo.tl` | reads ELF files for the tests |
| `tests/abigen.tl` | random functions for the calling convention |
| `tests/ppnorm.awk` | the token text of `-E` output |
| `tests/difftest.sh` | `make difftest` |
