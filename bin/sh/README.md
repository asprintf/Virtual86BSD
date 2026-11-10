# sh — shell, the standard command language interpreter (POSIX.1-2024)

An implementation of the `sh` utility and the Shell Command Language
(XCU chapter 2) of IEEE Std 1003.1-2024, including the User Portability
Utilities option (interactive use, job control, history, `fc`, vi-mode
editing). C11, POSIX interfaces only, about 11,000 lines.

The shell grammar (2.10) is a yacc grammar, and arithmetic expansion is a
lex scanner with a yacc grammar; the C files are generated from them in
the build, by the `yacc` and `lex` of this project.

## Build and test

    make           # builds ./sh
    make test      # the tests (sh test_sh.sh): regression and terminal tests
    make difftest  # compares with dash, bash, yash, busybox sh, mksh
    make install   # into $(PREFIX)/bin; PREFIX=/usr/local, DESTDIR= for staging
    make clean

The parser and scanner are generated in the build, by the yacc and lex of
this collection: `make` builds `../yacc/yacc` and `../lex/lex` first when
they are not there yet (so the directory needs `yacc/` and `lex/`
beside it, as in the collection), and `make YACC=... LEX=...` names other
ones.  The generated files are not part of the sources; `make clean`
removes them.

## What is implemented

* **Language**: the full grammar including `case … ;&` fall-through, function
  definitions with redirections, `!` pipelines, here-documents (`<<`, `<<-`,
  several per line, inside command substitutions), `$'…'` quoting, aliases with
  the trailing-blank rule, reserved-word recognition by context.
* **Expansions** in the specified order; the 2024 field-splitting algorithm,
  `"$@"`/`$*` in every context, the `$((` arithmetic-versus-subshell rule,
  pathname expansion with its own matcher (bracket expressions, classes,
  leading-period and slash rules, collation-sorted results).
* **Execution**: command search order (special built-ins, functions, intrinsic
  utilities, PATH; regular built-ins like `echo` run only when found on PATH),
  `ENOEXEC` scripts run in a fresh shell environment, exit statuses 126/127,
  assignment semantics for special built-ins versus other commands,
  declaration-utility expansion for `export`/`readonly`/`command export`,
  `set -e` with all its exceptions (also inside functions and `eval` called
  from a condition), `set -o pipefail`, `noclobber`.
* **Errors** follow the table in 2.8.1: what exits a non-interactive shell,
  what an interactive shell survives, and `command` suppressing special
  built-in exits.
* **Built-ins**: all special built-ins and all intrinsic utilities (`alias bg cd
  command fc fg getopts hash jobs kill read type ulimit umask unalias wait`),
  plus `echo` (XSI), `printf` (including `%b` and `%n$`), `test`/`[`, `pwd`,
  `true`, `false`. New in 2024 and supported: `read -d`, `cd -e`, `trap -p`,
  `set -o pipefail`, `;&`, `$'…'`.
* **Reading**: commands are executed before the next one is parsed, and the
  shell never reads past the current command on standard input (byte-wise on
  pipes, seek-back on seekable input), so scripts piped to `sh` that read
  stdin themselves work.
* **Interactive**: PS1/PS2/PS4 with `!` expansion, `ENV`, job control with
  process groups and terminal ownership (Ctrl-Z, `fg`, `bg`, `jobs`, `%n`
  job IDs, `set -b` notifications), SIGINT handling, `ignoreeof`, history
  file (`HISTFILE`, `HISTSIZE`), `fc -l/-s/-e`, `MAIL`/`MAILPATH`/`MAILCHECK`,
  and `set -o vi` editing with motions, counts, `c d y p x r R ~ u U .`,
  history search `/ ? n N`, `#`, `=`, `\`, `*`, `@letter`, `v`.

## Where yacc and lex are used

* **The shell grammar** is `sh.y`, following the grammar in 2.10 with
  precedence-free rules and no conflicts. Each construct is split into
  parts (`if_then`, `while_do`, ...) so that its error rules report the
  missing keyword (`expecting "then"`, `unexpected end of file (expecting
  "done")`) exactly as before. Each kind of input has its own start
  token: a complete command from the shell's input (accepted at its
  newline, so the shell still never reads past the current command), the
  commands inside `$(...)`, and a string (backquotes, `eval`-style text).
* **Token recognition stays hand-written** (`parser.c`), because it is
  not a regular-expression problem: what a character means depends on
  quoting nested to any depth, `$(` and `${` nesting, here-document bodies
  that start at the next newline, alias substitution that pushes new
  input, and reading one byte at a time from standard input with PS2
  prompts. Rule 1–9 classification of 2.10.2 (reserved words, assignment
  words, `NAME` in a function definition, `in`, case patterns) is done by
  a classifier between the word scanner and the grammar, which knows the
  position in the grammar and does alias substitution where the old
  parser did, so that aliases behave as before, even in edge cases such as
  an alias that expands to nothing before a newline.
* **Arithmetic expansion** is a lex scanner (`arith.l`) and a yacc
  grammar (`arith.y`) with C's precedence. Evaluation happens in the
  parser's actions, left to right as before; mid-rule actions turn
  evaluation off for the operand that `&&`, `||` or `?:` skips. `++`/`--`
  depend on context as in C, which the scanner decides from the previous
  token. Error rules give the same messages as the old evaluator
  (`unexpected end of expression`, `expected ')'`, `expected ':'`, ...).

## Where it follows the 2024 text rather than common practice

* `HOME=/h/; echo ~/x` gives `/h/x`: the spec says the trailing slash
  "should be omitted" (mksh agrees; the others print `/h//x`).
* `echo -n` prints `-n` (XSI-conformant `echo` has no options; yash agrees).
* `cd ""` is an error, as required.
* `printf '%2$s %1$s'` numbered arguments and `trap -p` are supported.
* Traps for signals ignored on entry are listed by `trap`.
* `shift n` beyond `$#` exits a non-interactive shell (permitted: "may exit"),
  and `command shift` reports status 2 instead of 1.
* `read` with fewer variables than fields gives the last one the rest of
  the line "unsplit", with only IFS white space removed at its ends:
  `IFS=' -' read a b c` of `A-B-C - ` sets `c` to `C -` (the others, and
  yash's test suite, give `C`).

## Choices where the standard leaves room

* Arithmetic uses `long`; a variable used in an expression must hold an
  integer constant (no recursive evaluation); `++`/`--` are supported.
* Variable assignments before a function call are temporary.
* All commands of a pipeline run in subshells.
* Asynchronous lists are always given job numbers.
* Signal-terminated commands have status 128+signal.
* Here-documents use a pipe when small and an unlinked temporary file otherwise.
* `[[`, `function`, `select` and `time` are not reserved words.
* With `ignoreeof`, each end-of-file typed at the terminal gives "Use
  "exit" to leave the shell." and the shell reads on. A terminal that has
  gone away (a hangup with SIGHUP ignored, which `poll()` reports as
  `POLLHUP`) ends the shell as an end-of-file would, instead of an endless
  series of end-of-files.

## Testing

### The POSIX Shell Tests (yash's test suite)

[posix-shell-tests](https://github.com/posix-shell-tests/posix-shell-tests)
(yash's POSIX test suite, GPL-2, not in this tree; 120 files, 10,979
cases, most of them the actions of every signal in every context) is run
by `tests/posix_shell_tests.sh SUITE SHELL` on the host: its runner needs
yash, and its job-control tests a terminal (each file gets a
pseudo-terminal of its own from `script`).  With this shell built by c17
and linked statically with V86BSD's C library (November 2026):

| shell | passed | failed | skipped |
|---|---|---|---|
| this sh | 10,940 | 8 | 31 |
| yash 2.55 | 10,957 | 0 | 22 |
| dash 0.5.12 | 9,674 | 1,271 | 31 |
| bash 5.2 as sh | 9,166 | 1,634 | 29 (some files cut at 300 s) |

The first run had 970 failures.  Fixed after it: a signal ignored on
entry to an interactive shell, or SIGINT/SIGQUIT in an asynchronous list
without job control, could not be reset by `trap` (and stayed ignored in
subshells and children); `sh -m` made children lose signals ignored on
entry; job control did not give the terminal back to the process group
that had it when the shell ended (the next program found itself in the
background); a subshell with a trap ran its last command in place of
itself (the trap was lost), and a signal caught just before the shell
exited never ran its trap; `exit` from within an EXIT trap's subshell
skipped the subshell's own EXIT trap; `unset` of a read-only variable
did not end a non-interactive shell (a special built-in's error); a
function named `command` did not override the built-in; `a=b command`
did not report the read-only error; `command exec 3<file` did not keep
the redirection; redirections without a command name were not performed
in a subshell, and their command substitutions gave no exit status;
redirections were done after the expansion of assignments (2.9.1.1 has
them first); `>&3` and `<&3` did not check that the descriptor is open
in that direction; line continuations inside operators (`&\<newline>&`)
and before the closing `))` of an arithmetic expansion; `$((` that
reaches the end of the input is an incomplete arithmetic expansion;
an alias whose value is blanks followed by a newline; `alias --`;
`command -V ./x` gave a relative pathname; `bg` wrote ` &` after the
command; `umask +r` without who did nothing under `umask 777`; `set -h`
did not locate a function's utilities when it is defined; `set -v`
echoed input read ahead more than once.

0.9.1 fixed the alias cases that POSIX specifies: an alias value is read
as input, so a here-document in it continues in the rest of the value and
a backslash at its end joins the following input (the end of an alias
value no longer reads as a space: the lexer ends the word there unless a
backslash or a quote reaches across, and digits the value ends with are
not an IO_NUMBER); the token that replaces one following an alias whose
value ends in a blank is examined too (`alias e='echo ' w=x` makes `e w`
look at `x`), also where a function definition's `()` may come from an
alias (`alias f='f ' p='()'`).

What is left (8): four alias cases that substitute `in`, `do`, `for` or
`case` (POSIX: unspecified for a word that could be a reserved word),
three where the commands of a command substitution are parsed at once
rather than line by line (unspecified in POSIX.1-2024; dash and bash do
the same), and `read` (above).  `wait` interrupted by a trap passes or
fails with the timing of the signal (dash fails it too).

### The regression suite

`tests/run_diff.tl` runs every case in `tests/cases*.txt` under this shell
and under dash, bash `--posix`, yash `--posix`, BusyBox ash and mksh, and
reports where this shell disagrees with the majority (stdout and exit
status). Of 369 cases, 7 differ, all deliberately (below). The suite also
runs clean under AddressSanitizer and UBSan, including the interactive tests.
`tests/regress.tl` freezes the verified results so `make test` needs no
other shells.

The move to yacc and lex was checked against the previous release (the
hand-written parser and evaluator), comparing standard output, standard
error and exit status byte for byte:

* `tests/equiv.tl OLD NEW cases`: every regression case, given as `-c`, as
  a script file and on standard input (1107 runs);
* `gen`: random programs with nested compound commands, functions,
  aliases, here-documents and substitutions, mutated by deleting,
  repeating, swapping and inserting tokens so that syntax errors land
  everywhere;
* `grid`: 33,742 variants of templates for every construct, with every
  token replaced by every reserved word, operator and alias (including
  aliases that expand to keywords or to nothing);
* `arith`: random arithmetic expressions, valid and invalid;
* `tests/equiv_pty.tl`: 40 interactive sessions through a pseudo-terminal,
  compared byte for byte including the PS1/PS2 prompts (which show when
  the parser asks for more input).

All are identical (nearly 50,000 runs in all). The regression suite and the
interactive tests also pass under AddressSanitizer and UBSan, and a build
whose parsers and scanner are regenerated with bison and flex instead
passes the regression suite too.

One difference is intentional: very deep nesting. The old recursive
parser and evaluator ran out of stack and crashed at about 10,000 nested
compound commands and between 20,000 and 50,000 nested parentheses in
arithmetic; the yacc parsers keep their stacks on the heap and handle
these.

## Files

| file | contents |
|------|----------|
| `sh.h` | shared types: encoded words, AST, variables, jobs |
| `util.c` | memory, stack allocator, buffers, exceptions, buffered output, options |
| `var.c` | variables, environment, positional parameters, aliases, functions, locale |
| `input.c` | input source stack (files, strings, aliases), no read-ahead on stdin, `MAIL` checks |
| `parser.c` | token recognition (2.3), word encoding, here-documents, alias substitution, and the token classifier that feeds the grammar |
| `sh.y` | the shell grammar (2.10), building the AST (`sh.tab.c`, `sh.tab.h` generated, prefix `shy`) |
| `parse.h` | the interface between `sh.y` and the classifier |
| `expand.c` | tilde/parameter/command/arithmetic expansion, field splitting, pathname expansion, pattern matching |
| `arith.l` | the tokens of arithmetic expansion (`arith_scan.c` generated) |
| `arith.y` | arithmetic expansion (`long`, all C operators, short-circuit, assignments), evaluated as it is parsed (`arith.tab.c`, `arith.tab.h` generated) |
| `arith.h` | values shared by `arith.l` and `arith.y` |
| `exec.c` | simple commands, redirections, pipelines, subshells, functions, command search, `eval`/`.` |
| `jobs.c` | job control, waiting, signals and traps, command deparsing for job listings |
| `builtins.c` | special built-ins and intrinsic utilities |
| `bltin2.c` | `echo`, `printf`, `test`/`[` |
| `edit.c` | history, `fc`, vi-mode line editing |
| `main.c` | invocation and the top-level loop |
| `tests/` | the test suites and their data (see Testing) |
| `test_sh.sh` | runs the tests: `make test`, or `sh test_sh.sh` |
| `Makefile` | build, test, difftest, install, clean |
