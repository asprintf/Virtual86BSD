# tl, the test language

tl is the language of the tests that need more than the shell: driving an
interactive utility on a pseudo-terminal and reading its screen, comparing
with a reference implementation, generating random programs, reading binary
formats.  It is a small language in the style of C, interpreted by one C
file, `tl.c` (C99 and POSIX.1-2008, no other library), which every suite
compiles for itself.  So the tests need nothing but a C compiler and the
POSIX utilities.

    tl script.tl [argument...]
    tl -e 'program' [argument...]

The exit status is what `exit()` is given, 0 at the end of the script, and 2
after an error in the program (with its file and line on the standard error).

## A first example

```c
/* count.tl - how often each word comes in the files named */
include "../../testlib/testlib.tl";

func count(text, counts) {
    for (var w : split(text))
        counts[w] = get(counts, w, 0) + 1;
}

var counts = {};
for (var f : argv)
    count(readfile(f), counts);
for (var w : sorted(keys(counts)))
    printf("%6d %s\n", counts[w], w);
```

## Values

| type | written | notes |
| --- | --- | --- |
| nil | `nil` | the value of a variable not given one, of a missing argument |
| integer | `42`, `0x2a`, `052`, `'*'` | 64 bits; `true` is 1, `false` 0 |
| double | `4.2`, `1e-3` | |
| string | `"a\tb\n"`, `R"(raw \ text)"` | any bytes; escapes as in C (`\n \t \r \0 \033 \x1b \\ \"`); adjacent literals are joined |
| list | `[1, "two", [3]]` | shared by reference |
| map | `{name: "x", "two words": 2, 7: "seven"}` | keys strings or integers, kept in the order they were put; shared by reference |
| function | `func (a, b) { return a + b; }`, or a function's name | |
| terminal | `term(...)` | a program on a pseudo-terminal (below) |

Strings are values: `s[i]` is the byte at `i` (an integer), `len(s)` the
number of bytes.  Lists and maps are references: an assignment or a call
shares them, `copy()` makes a new one.  `==` compares by value (lists and
maps element by element); `<` and the others compare numbers by value and
strings and lists in order.  Reference counting frees what is no longer
used (a cycle, such as a node with its parent, is kept until the end).

Truth: `nil`, `0`, `0.0` and `""` are false, everything else true.

## Statements and expressions

```c
var x = 1, y;                   // a variable: global at the top, else local
if (x > 0) ...; else ...;
while (cond) ...;
do ...; while (cond);
for (var i = 0; i < n; i++) ...;
for (var e : list) ...;         // the elements; of a map: its keys; of a string: its bytes
break; continue; return v;
func name(a, b) { ... }         // at the top level only; a missing argument is nil
include "file.tl";              // relative to the including file; once
```

The operators are C's, with their precedence: `+ - * / %` (integer
division truncates, as in C; a double makes the result a double), `<< >> &
| ^ ~`, `== != < <= > >=`, `&& || !` (`!` gives 1 or 0, `&&` and `||` the last
value they evaluated), `?:`, `= += -= *= /= %= <<= >>= &= |= ^=`,
`++ --`.  Besides:

* `+` joins strings and lists; `"ab" * 3` and `[0] * 3` repeat.
* `x in c`: an element of a list, a key of a map, a substring of a string.
* `m.name` is `m["name"]`; keywords may be keys (`{in: "text"}`, `r.in`).
* `s[i]`, `l[i]`, `m[k]`: an index past the end of a list is an error, a
  key not in a map gives `nil`.

A function is a value.  It does not capture the local variables around it
(there are no closures): a function written inside another sees its own
parameters and locals and the globals.

`__FILE__` and `__LINE__` are the file and line; `argv` is the list of the
arguments, `script` the script's path, `scriptdir` its directory
(absolute), and `SIGINT` and the other signals are their numbers.

Errors are not exceptions: a wrong use (a missing file for `include`, an
operation on the wrong types, a call with too many arguments) ends the
program with exit status 2 and a message with the file and line.  The
functions of the system return `nil` or false and keep `errno()`.

## Library

The names below are the functions built in; brackets mark arguments that
may be left out.

### Output and values

* `print(v...)`: the values, separated by blanks, and a newline (strings as they are).
* `printf(fmt, v...)`, `eprintf` (to the standard error), `sprintf` (a
  string): the formats of C (`%d %i %u %x %X %o %c %s %e %f %g %%`, flags,
  width and precision, `*`), and `%q`: the value as `repr()` writes it.
* `puts(s)`, `eputs(s)`: the string as it is; `flush()`.
* `exit([status])`, `error(msg)` (an error of the program, status 2),
  `assert(cond[, msg])`.
* `str(v)`: a string; `repr(v)`: a string that reads back (strings quoted,
  control characters as `\ooo`); `typeof(v)`: `"nil"`, `"int"`, `"double"`,
  `"string"`, `"list"`, `"map"`, `"func"`, `"term"`; `nameof(f)`.
* `int(x[, base])` (a string that is no number gives nil; a double is cut),
  `num(s)` (an integer or a double), `double(x)`, `len(x)`.

### Strings

* `substr(s, i[, n])`, `slice(x, i[, j])` (a string or list; negative
  positions count from the end), `index(s, t[, from])`, `rindex(s, t)`
  (-1: not there), `startswith`, `endswith`.
* `split(s)` (at blanks), `split(s, sep[, max])`, `lines(s)`, `join(list,
  sep)`, `replace(s, old, new)`, `trim`, `ltrim`, `rtrim` (blanks, or the
  characters of a second argument), `upper`, `lower`, `repeat(s, n)`.
* `chr(byte)`, `ord(s)` (the first byte), `utf8(codepoint)`, `hex(s)`,
  `unhex(s)`, `base64(s)`, `unbase64(s)`, `sha256(s)` (hexadecimal).
* `json(v[, indent])`: JSON with the keys sorted and only ASCII (the same
  text as Python's `json.dumps(v, indent=indent, sort_keys=True)`);
  `unjson(text)`.
* Regular expressions are POSIX extended ones: `match(s, re)` gives `[the
  match, group 1, ...]` or nil; `matchat(s, re)` gives `[start, end]`;
  `sub(s, re, repl)` and `gsub(s, re, repl)` (`&` and `\1` ... in repl);
  `resplit(s, re)`.
* `pack(fmt, v...)` and `unpack(fmt, s[, offset])`: binary data; fmt has
  `<` (little-endian), `>` or `!` (big-endian), then `b B` (8 bits), `h H`
  (16), `i I` (32), `q Q` (64), `x` (a pad byte) and `s` (a string, `4s`),
  each with an optional count (`3I`).

### Lists and maps

* `list(n[, v])`, `range([from,] to[, step])`, `push(l, v...)`, `pop(l[,
  i])`, `insert(l, i, v)`, `find(l, v)` (-1: not there), `reverse(l)`,
  `copy(x)`.
* `sort(l[, cmp])` (in place, stable; `cmp(a, b)` negative, 0 or positive),
  `sorted(l[, cmp])`.
* `keys(m)`, `values(m)`, `haskey(m, k)`, `get(m, k, default)`, `del(m, k)`.
* `apply(l, f)`, `filter(l, f)`, `min`, `max`, `sum` (of a list, or of
  their arguments), `abs`.
* Random numbers (splitmix64, the same sequence everywhere for a seed):
  `srand(seed)`, `rand()`, `rand(n)` (0 to n-1), `rand(i, j)` (i to j),
  `frand()`, `choice(l)`, `shuffle(l)`, `randbytes(n)`.

### Files

* `readfile(path)` (nil if it cannot be read), `writefile(path, s[, mode])`,
  `appendfile(path, s)`.
* `stat(path)`, `lstat(path)`: `{type ("f", "d", "l", "p", "c", "b", "s"),
  mode (the permissions), uid, gid, size, nlink, ino, dev, rdev, atime,
  mtime, ctime, atime_ns, mtime_ns}`, or nil.
* `exists`, `isdir`, `isfile`, `remove`, `rmtree`, `mkdir(path[, mode])`,
  `mkdirs`, `listdir` (sorted), `rename`, `symlink`, `link`, `readlink`,
  `dirname`, `basename`, `abspath`, `chmod`, `chown`, `utime(path, atime,
  mtime)`, `mkfifo`, `access(path, how)`, `getcwd`, `chdir`, `umask`,
  `truncate`, `mkdtemp([prefix])` (in `$TMPDIR` or /tmp), `errno()`,
  `strerror([n])`.

### Processes

* `run(cmd[, opts])`: runs a command and waits for it.  cmd is a list (the
  program and its arguments, found in `PATH`) or a string (for `sh -c`).
  The result is `{out, err, status, signal, timedout}`; status is the exit
  status, or 128 plus the number of the signal that ended it.  opts: `in`
  (the standard input, a string), `stdin: "null"`, `out` and `err`
  (`"pipe"`, the default: caught; `"inherit"`, `"null"`, `">file"`; and
  `err: "out"` for both together), `env` (a map of variables set, `nil`
  for those unset), `cwd`, `timeout` (seconds; then the process is killed, or its
  whole group when it has its own: `setsid` or `setpgid`), `umask`,
  `setsid`, `setpgid`.
* `sh(cmd[, input])`: the output of `sh -c cmd`; `system(cmd)`: its status.
* `spawn(cmd[, opts])`: starts a command without waiting: its process ID
  (opts `in`, `out` and `err` may also be descriptors, such as those of
  `pipe()`); `waitpid(pid[, nohang])` (the status as run() gives it),
  `termsig()` (the signal that ended the process waited for, or 0);
  `kill(pid, sig)`.
* `fork()`, `exec(argv)`, `_exit(n)`, `getpid`, `getppid`, `getpgrp`,
  `setsid`, `getsid`, `setpgid`, `nice(incr)`, `times()` (`{user, sys,
  cuser, csys}` in seconds), and the user and group IDs: `getuid`,
  `geteuid`, `getgid`, `getegid`, `setuid`, `seteuid`, `setgid`, `setegid`,
  `setreuid`, `setregid`, `setresuid`, `setresgid`, `getgroups`,
  `setgroups`, `getpwuid`, `getpwnam`, `getgrgid`, `getgrnam`.
* `signal(sig, how)` (`"dfl"`, `"ign"`, `"catch"`), `sigdisp(sig)`,
  `sigmask(sig, block)`, `pause()`, `sleep(secs)` (a double).
* `getenv`, `setenv`, `unsetenv`, `environ()`, `setlocale(name[,
  category])`, `iswctype(wc, class)`.

A program that switches its IDs and runs another is one line in a test
script:

    "$TLBIN" -e 'setresuid(65534, 0, 0) || exit(1); exec(argv); exit(1);' ./id -u

### Descriptors

`open(path, how[, mode])` (how: letters `r w a c t x n e`), `close`,
`dup2`, `pipe()` (closed on exec), `fdread(fd[, max[, timeout]])`, `fdwrite(fd, s)`,
`isatty`, `ttyname`, `mmap(path)`, `lockf(fd, how)`, and sockets of the
UNIX domain: `unixlisten(path)`, `accept(fd)`, `unixconnect(path)`,
`unixdgram(path)`.

### Time

`time()`, `now()` (a double), `gmtime([t])` and `localtime([t])` (a map
`{year, mon, mday, hour, min, sec, wday, yday, isdst}`), `timegm(map)`,
`mktime(map)`, `strftime(fmt[, t[, utc]])`, `strptime(s, fmt)`.

### Terminals

`term(cmd[, opts])` starts a program on a new pseudo-terminal, which is its
controlling terminal, and keeps a screen of what the program writes (an
emulator of the xterm and ANSI sequences: the cursor, erasing, scrolling
regions, insertion and deletion, the attributes bold, underline and
reverse).  opts: `rows` and `cols` (24 and 80; 0 leaves the terminal
without a size), `fds` (the descriptors on the terminal, `"012"`), `in`,
`out` and `err` for the others (`"inherit"`, `"null"`, `out: "pipe"`),
`env`, `cwd`, `umask`, and `termios` (a map: the characters `erase`,
`kill`, `werase`, `eof`, `intr`, `quit`, `susp`; the modes `icanon`,
`echo`, `isig`, `iexten`).

* Reading: `tpump(t, secs)`, `tsettle(t[, quiet[, max]])` (until the output
  stops), `twait(t[, secs])` (the exit status, or nil), `talive`, `teof`.
* Typing: `tsend(t, s)`; `tkill(t[, sig])`, `tclose(t)` (a hangup).
* The screen: `tline(t, row)`, `tscreen(t)`, `tcell(t, row, col)`
  (`{ch, bold, underline, reverse}`), `trow`, `tcol`, `tbells`, `tfill`,
  `tresize(t, rows, cols)` (and SIGWINCH).
* The rest: `traw(t)` (all the output), `tpiped(t)`, `tattr(t)` (the modes
  now), `tsignal`, `tpid`, `tfd`.

## The libraries of the tests

* `testlib.tl`: `utf8_locale()`, `compiler()`, `cc_env()`, `build(name,
  dir)` (a helper of this directory compiled), `which`, `shquote`,
  `non_ascii`, `check(name, cond)` and `tests_done(name)`.
* `cases.tl`: `run_cases(opts)`, the regression suites of case scripts
  with their expected output in files.
* `expect.tl`: sessions on a terminal in the manner of expect: `session`,
  `expect(s, re[, secs])`, `send`, `wait`, `verify`, `pty_tests(tests)`.
* `ptyrun.tl`: `tl ptyrun.tl FDS COMMAND...` runs a command with some of
  its descriptors on a terminal (`PTYRUN_INPUT` typed first).
* `mkterminfo.tl`: compiles terminfo entries for the tests.
* `mkelf.tl`: `tl mkelf.tl OUT [-32] [-be] < spec` writes a relocatable
  ELF object (ar, nm, strip).
* `uu.tl`: the historical uuencode format.

## In a test script

A test script compiles tl and uses it through `TLBIN`:

```sh
cc=${CC:-}
[ -n "$cc" ] || for c in c17 c99 cc; do command -v $c >/dev/null 2>&1 && { cc=$c; break; }; done
$cc -o "$T.tl" ../testlib/tl.c || { echo "cannot compile ../testlib/tl.c"; exit 2; }
TLBIN=$T.tl; export TLBIN
"$TLBIN" tests/pty_tests.tl ./mailx
```

`tl.sh` does the same for a single run: `sh ../testlib/tl.sh script.tl
argument...` (it is what the `difftest` targets use).
