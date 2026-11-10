# cflow — generate a C-language flowgraph (POSIX.1-2024)

An implementation of the `cflow` utility of POSIX.1-2024 (XCU cflow), in C
with no dependencies beyond libc. It belongs to the XSI option.

    cflow [-r] [-d num] [-D name[=def]]... [-i incl] [-I dir]... [-U name]... file...

## Build and test

    make           # builds ./cflow
    make test      # the tests (sh test_cflow.sh)
    make difftest  # compares with GNU cflow
    make install   # into $(PREFIX)/bin; PREFIX=/usr/local, DESTDIR= for staging
    make clean

## What is implemented

* C files preprocessed (-D, -I, -U in their order) and parsed; .i files (c17
  -E output, its line markers honoured); .l and .y files through lex and
  yacc; .s files (labels, call instructions); ELF relocatable objects (their
  symbols and relocations).
* The graph in the format of the standard: "N name: type, <file line>", one
  space at the top level and four per level; a name written again gets only
  the line number of its first appearance. Callees in the order of
  LC_COLLATE; at the top main, then the functions no other calls, then those
  only called in cycles.
* -r inverts the graph; -d num cuts it at that level; -i x adds the data of
  file scope, -i _ the names starting with "_".

## Choices where the standard leaves room

* Static functions and objects belong to their file: two of the same name in
  two files are two. The definition written is the first.
* A file that cannot be read: a diagnostic, status 1, the graph of the
  others written. Problems in the C are warnings. Usage errors exit with
  status 2.

## Testing

`test_cflow.sh` runs the utility on prepared input and compares the output
and the exit status: the example of the standard, -r, -d, -i, -D, -I, -U,
.i, .s, lex, yacc and object files, names written again, function pointers,
missing headers and files. The binary tested is `./cflow`, or `$U` if set.

`tests/cflowdiff.sh` compares with GNU cflow on programs of this tree: the
definitions both find must be the same, and every call GNU cflow finds must
be found.

## Files

| file | contents |
|------|----------|
| `cflow.c` | the utility |
| `../../lib/libxcu/cpp.c` | a C tokenizer and preprocessor (shared with c17, ctags and cxref) |
| `../../lib/libxcu/cpp.h` | its declarations |
| `../../lib/libxcu/cparse.c` | a forgiving C parser: declarations and references (shared with ctags and cxref) |
| `../../lib/libxcu/cparse.h` | its declarations |
| `../../lib/libxcu/elf.c` | ELF files, for object files (shared with ar, nm and strip) |
| `../../lib/libxcu/elf.h` | its declarations |
| `tests/cflowdiff.sh` | the comparison with GNU cflow (`make difftest`) |
| `test_cflow.sh` | the tests: `make test`, or `sh test_cflow.sh` |
| `Makefile` | build, test, install, clean |
