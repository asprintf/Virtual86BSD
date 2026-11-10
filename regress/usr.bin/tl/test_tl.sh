#!/bin/sh
# test_tl.sh - tests for tl, the test language.  Run: sh test_tl.sh
#
# The language (values, operators, statements, functions, include, errors),
# and each part of the library: strings, regular expressions, binary data,
# JSON, lists and maps, random numbers, files, processes, descriptors,
# signals, time and terminals.  The other suites of the collection use tl
# throughout, so they test it too.
cd "$(dirname "$0")" || exit 2
PROG_DIR=${PROG_DIR:-$PWD/../../../usr.bin/tl}; export PROG_DIR
pass=0 fail=0
# t NAME EXPECTED-STDOUT EXPECTED-STATUS COMMAND...  (stdout compared without trailing newlines)
t() {
    _n=$1 _e=$2 _s=$3; shift 3
    _o=$("$@" 2>/dev/null); _r=$?
    if [ "$_o" = "$_e" ] && [ "$_r" = "$_s" ]; then pass=$((pass+1))
    else fail=$((fail+1)); printf 'FAIL %s\n  got:  [%s] status %s\n  want: [%s] status %s\n' "$_n" "$_o" "$_r" "$_e" "$_s"; fi
}
done_tests() { echo "$1: $pass passed, $fail failed"; [ "$fail" = 0 ]; }
U=${U:-$PROG_DIR/tl}
[ -x "$U" ] || { echo "tl: not built (run make first)"; exit 2; }
T=${TMPDIR:-/tmp}/test_tl.$$
mkdir -m 700 "$T" || exit 2
trap 'chmod -R u+rwx "$T" 2>/dev/null; rm -rf "$T"' EXIT
cd "$T" || exit 2
export LC_ALL=C
nl='
'
# e NAME EXPECTED PROGRAM [ARGUMENT...]: tl -e PROGRAM prints EXPECTED, status 0
e() { _n=$1 _x=$2; shift 2; t "$_n" "$_x" 0 "$U" -e "$@"; }

# ---- values and operators
e "integers, doubles, strings" '42 42 42 42 4.2 0.001' 'print(42, 0x2a, 052, '"'*'"', 4.2, 1e-3);'
e "integer division truncates, as in C" '3 -3 -1 1' 'print(7 / 2, -7 / 2, -7 % 3, 7 % -3);'
e "a double makes a double" '3.5 double int' 'print(7 / 2.0, typeof(1 + 0.5), typeof(1 + 1));'
e "64-bit integers" '9223372036854775807 -9223372036854775808' 'print(0x7fffffffffffffff, -0x7fffffffffffffff - 1);'
e "strings: escapes and raw literals" '"a\tb\001\\\"" "raw \\ text"' 'print(repr("a\tb\001\\\""), repr(R"(raw \ text)"));'
e "adjacent literals are joined" 'abcd' 'print("ab" "cd");'
e "+ joins, * repeats" 'abab [1, 2, 1, 2] ababab [0, 0, 0]' 'print("ab" + "ab", [1, 2] + [1, 2], "ab" * 3, [0] * 3);'
e "in: element, key, substring" '1 0 1 1 0' 'print(2 in [1, 2], 3 in [1, 2], "a" in {a: 1}, "bc" in "abc", "x" in "abc");'
e "comparison by value" '1 1 1 1 0' 'print([1, [2]] == [1, [2]], {a: 1} == {a: 1}, "abc" < "abd", [1, 2] < [1, 3], 1 == "1");'
e "&& and || give the last value" '0 x 3 0' 'print(0 && 1, "x" || 2, 2 && 3, !5);'
e "truth" 'nil 0 0.0 empty: false; list: true' 'var s = ""; for (var v : [nil, 0, 0.0, ""]) s += v ? "?" : ""; print("nil 0 0.0 empty: " + (s == "" ? "false" : "true") + "; list: " + ([] ? "true" : "false"));'
e "bit operators and compound assignment" '28 3 -1 40 18' 'var x = 5; x <<= 3; x |= 1; x >>= 1; x ^= 8; print(x & ~3, 7 ^ 4, ~0, 5 << 3, x - 2 * 2 + 4 / 4 * 0 - 6);'
e "++ and --, ?:" '1 2 2 1 big' 'var i = 1; print(i++, i, i--, i, i > 0 ? "big" : "small");'
e "m.name is m[\"name\"], keywords as keys" 'text 3 nil' 'var r = {in: "text"}; r.n = 3; print(r.in, r["n"], r.missing);'
e "lists and maps are shared by reference, copy() makes one" '[1, 9] [1, 2]' 'var a = [1, 2], b = a, c = copy(a); b[1] = 9; print(a, c);'
e "map keys keep their order" '["z", "a", 5]' 'var m = {}; m.z = 1; m.a = 2; m[5] = 3; print(keys(m));'
e "strings are values: bytes by index" '97 3 ab' 'var s = "abc"; print(s[0], len(s), substr(s, 0, 2));'

# ---- statements and functions
e "if, while, do, for" '0 1 2 | 4 | 0 2 4' 'var i = 0, s = ""; while (i < 3) { s += str(i) + " "; i++; } s += "| "; do i++; while (i < 3); s += str(i) + " | "; for (var j = 0; j < 5; j += 2) s += str(j) + (j < 4 ? " " : ""); print(s);'
e "for over a list, a map, a string" '1 2 a b 104 105' 'var s = []; for (var x : [1, 2]) push(s, x); for (var k : {a: 1, b: 2}) push(s, k); for (var c : "hi") push(s, c); print(join(apply(s, str), " "));'
e "break and continue" '1 3 5' 'var s = []; for (var i = 0; ; i++) { if (i > 5) break; if (i % 2 == 0) continue; push(s, i); } print(join(apply(s, str), " "));'
e "functions: recursion, missing arguments are nil" '120 nil' 'func f(n) { return n <= 1 ? 1 : n * f(n - 1); } func g(a, b) { return b; } print(f(5), g(1));'
e "functions are values; a function in another sees the globals" '7 global' 'var x = "global"; func add(a, b) { return a + b; } func h() { return func () { return x; }; } var p = add; print(p(3, 4), h()());'
t "no closures: a local of the enclosing function is an error" "" 2 "$U" -e 'func h() { var y = 1; return func () { return y; }; } h()();'
t "int + string is an error" "" 2 "$U" -e 'print(1 + "x");'
e "nameof and typeof of a function" 'add func' 'func add(a, b) { return a + b; } print(nameof(add), typeof(add));'
cat > inc.tl <<'EOF'
var counter = 0;
func bump() { counter++; return counter; }
EOF
cat > main.tl <<'EOF'
include "inc.tl";
include "inc.tl";
bump(); bump();
print(counter, basename(script), scriptdir == getcwd(), argv);
EOF
t "include: relative, once; script, scriptdir, argv" "2 main.tl 1 [\"x\", \"y z\"]" 0 "$U" main.tl x "y z"
printf '#!%s\nprint("args", len(argv));\nexit(len(argv) + 10);\n' "$U" > run.tl
chmod +x run.tl
t "a script with #! runs as a command" "args 2" 12 ./run.tl a b
t "exit() status" "" 7 "$U" -e 'exit(7);'
t "error(): status 2" "" 2 "$U" -e 'error("boom");'
t "error(): the message has the file and line" "tl: -e:2: boom" 0 sh -c '"$1" -e "
error(\"boom\");" 2>&1; :' sh "$U"
t "a wrong use is an error: an index past the end" "" 2 "$U" -e 'var l = [1]; l[3];'
t "a wrong use is an error: too many arguments" "" 2 "$U" -e 'func f(a) { return a; } f(1, 2);'
t "a syntax error: status 2, nothing run" "" 2 "$U" -e 'print("x"); var = ;'
t "assert" "" 2 "$U" -e 'assert(1 == 2, "no");'
t "no script: usage, status 2" "" 2 "$U"
t "an unknown option: status 2" "" 2 "$U" -x
t "a missing script: status 2" "" 2 "$U" nosuch.tl

# ---- output and values
e "printf: the formats of C" '[  3.14|ab  |ff|0X1F|   -7|e|1.500000e+00|50%]' 'printf("[%6.2f|%-4s|%x|%#X|%5d|%c|%e|%d%%]\n", 3.14159, "ab", 255, 31, -7, 101, 1.5, 50);'
e "printf: * width, %q" '[   ab] ["x", 1]' 'printf("[%*s] %q\n", 5, "ab", ["x", 1]);'
e "sprintf, str, repr" '007 12 "q\"" nil' 'print(sprintf("%03d", 7), str(12), repr("q\""), str(nil));'
e "int, num, double" '31 255 nil 2.5 3 3 4.0' 'print(int("0x1f", 16), int("ff", 16), int("zz"), num("2.5"), num("3"), int(3.9), repr(double(4)));'
e "puts writes as it is" 'ab' 'puts("a"); puts("b\n");'
t "eprintf, eputs: the standard error" "e1 e2" 0 sh -c '"$1" -e "eprintf(\"%s \", \"e1\"); eputs(\"e2\n\");" 2>&1 >/dev/null' sh "$U"

# ---- strings
e "substr, slice, index, rindex" 'ell lo [3, 4] 2 3 -1' 'print(substr("hello", 1, 3), slice("hello", -2), slice([1, 2, 3, 4], 2), index("hello", "l"), rindex("hello", "l"), index("abc", "z"));'
e "startswith, endswith" '1 0 1' 'print(startswith("hello", "he"), startswith("hello", "lo"), endswith("hello", "lo"));'
e "split, lines, join" '["a", "b"] ["a", "", "c"] ["a", "b,c"] ["x", "y"] x-y' 'print(split("  a  b "), split("a,,c", ","), split("a,b,c", ",", 1), lines("x\ny\n"), join(["x", "y"], "-"));'
e "replace, trim, upper, lower, repeat" 'bcbcbc [a b] [xx] AB ab --' 'print(replace("aaa", "a", "bc"), "[" + trim("  a b ") + "]", "[" + trim("..xx..", ".") + "]", upper("ab"), lower("AB"), repeat("-", 2));'
e "chr, ord, utf8, hex" 'A 97 c3a9 4142 AB' 'print(chr(65), ord("abc"), hex(utf8(0xe9)), hex("AB"), unhex("4142"));'
e "base64 both ways" 'aGVsbG8= hello YQ==' 'print(base64("hello"), unbase64("aGVsbG8="), base64("a"));'
e "sha256" 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855' 'print(sha256("abc"), sha256(""));'
e "json: sorted keys, ASCII only" '1 {"b": [1, 2.5, null, 1]}' 'var j = json({b: [1, 2.5, nil, "x" == "x"], a: "\303\251"}); print(startswith(j, "{"+chr(34)+"a"+chr(34)+": "+chr(34)+chr(92)+"u00e9"), json({b: [1, 2.5, nil, 1]}));'
e "unjson" '[1, {"x": 1, "y": "a\"b"}, nil]' 'print(repr(unjson("[1, {\"x\": true, \"y\": \"a\\\"b\"}, null]")));'

# ---- regular expressions (POSIX EREs)
e "match: the match and the groups" '["abc123", "abc", "123"] nil' 'print(match("--abc123--", "([a-z]+)([0-9]+)"), match("abc", "[0-9]"));'
e "matchat" '[2, 5]' 'print(matchat("--abc--", "[a-z]+"));'
e "sub, gsub with & and back-references" 'he[ll]o a<X>b<X>c 21' 'print(sub("hello", "l+", "[&]"), gsub("aXbXc", "X", "<&>"), sub("12", "(.)(.)", "\\2\\1"));'
e "resplit" '["a", "b", "c"]' 'print(resplit("a1b22c", "[0-9]+"));'
e "the longest of the leftmost matches" '["abc"] ["abc", "abc"]' 'print(match("xabcd", "a|ab|abc"), match("abc", "(a|ab|abc)*"));'

# ---- binary data
e "pack: byte orders and sizes" '0102000300000003 0004 ff' 'print(hex(pack("<BHI", 1, 2, 3) + "\003"), hex(pack(">H", 4)), hex(pack("b", -1)));'
e "unpack: counts, strings, an offset" '[258, 3, 4] ["ab", 7] [-1]' 'print(unpack(">H2B", "\001\002\003\004"), unpack("2sB", "ab\007"), unpack("<b", "\000\377", 1));'
e "64 bits" '[-2, -1] 0000000000000080' 'print(unpack("<qQ", pack("<qQ", -2, -1)), hex(pack("<q", -0x7fffffffffffffff - 1)));'

# ---- lists and maps
e "list, range, push, pop, insert, find, reverse" '[0, 0] [0, 1, 2] [1, 4, 7] [1, 2, 3] 3 [9, 1, 2] 1 -1 [2, 1, 9]' 'var l = [1, 2]; push(l, 3); var s = repr(l); var p = pop(l); insert(l, 0, 9); var b = repr(l), f1 = find(l, 1), f2 = find(l, 5); reverse(l); print(list(2, 0), range(3), range(1, 10, 3), s, p, b, f1, f2, l);'
e "sort is stable, with a comparison" '[1, 2, 3] ["b", "a"] [[1, "x"], [1, "y"], [2, "z"]]' 'print(sorted([3, 1, 2]), sorted(["a", "b"], func (x, y) { return x < y ? 1 : x > y ? -1 : 0; }), sorted([[2, "z"], [1, "x"], [1, "y"]], func (a, b) { return a[0] - b[0]; }));'
e "keys, values, haskey, get, del" '["a"] [1] 1 0 5 {}' 'var m = {a: 1}; var k = keys(m), v = values(m), h1 = haskey(m, "a"), h2 = haskey(m, "b"), g = get(m, "b", 5); del(m, "a"); print(k, v, h1, h2, g, m);'
e "apply, filter, min, max, sum, abs" '[2, 4] [2] 1 9 6 3 2.5' 'func dbl(x) { return 2 * x; } func even(x) { return x % 2 == 0; } print(apply([1, 2], dbl), filter([1, 2, 3], even), min([3, 1]), max(4, 9, 2), sum([1, 2, 3]), abs(-3), abs(-2.5));'
e "random numbers: one sequence per seed" '1 1 1 1' 'srand(7); var a = [rand(), rand(10), frand()]; srand(7); var b = [rand(), rand(10), frand()]; var r = rand(3, 5); print(a == b, r >= 3 && r <= 5, len(randbytes(9)) == 9, choice([4]) == 4);'
e "shuffle keeps the elements" '[1, 2, 3, 4, 5]' 'var l = range(1, 6); shuffle(l); print(sorted(l));'

# ---- files
e "writefile, readfile, appendfile, stat" '["a", "b", "c"] 6 f 1 0 1' 'writefile("f", "a\nb\n"); appendfile("f", "c\n"); var s = stat("f"); print(lines(readfile("f")), s.size, s.type, isfile("f"), isdir("f"), exists("f"));'
e "readfile of a missing file: nil, errno" 'nil 1' 'print(readfile("nosuch"), strerror(errno()) == strerror(2));'
e "mkdirs, listdir (sorted), rmtree" "[\"a\", \"b\"]${nl}0" 'mkdirs("d/b/x"); mkdir("d/a"); print(listdir("d")); rmtree("d"); print(exists("d"));'
e "symlink, readlink, lstat, rename, remove" "target l f${nl}0 1" 'writefile("target", ""); symlink("target", "ln"); print(readlink("ln"), lstat("ln").type, stat("ln").type); rename("target", "t2"); print(exists("target"), exists("t2")); remove("ln"); remove("t2");'
e "chmod and the mode, umask" '600 18' 'writefile("m", ""); chmod("m", 0600); print(sprintf("%o", stat("m").mode), umask(022));'
e "utime" '1000000000' 'writefile("u", ""); utime("u", 1000000000, 1000000000); print(stat("u").mtime);'
e "dirname, basename, abspath" '/a/b c 1' 'print(dirname("/a/b/c"), basename("/a/b/c"), abspath("x") == getcwd() + "/x");'
e "mkdtemp" '1 1' 'var d = mkdtemp("tlx"); print(isdir(d), startswith(basename(d), "tlx")); rmtree(d);'

# ---- processes
e "run: out, err, status" '["out\n", "err\n", 3]' 'var r = run(["sh", "-c", "echo out; echo err >&2; exit 3"]); print(repr([r.out, r.err, r.status]));'
e "run: a string is for sh -c; in, env, cwd" 'xyz bar /' 'var a = run(["cat"], {in: "xyz"}).out, b = run("echo $FOO", {env: {FOO: "bar"}}).out, c = run("pwd", {cwd: "/"}).out; print(a, trim(b), trim(c));'
e "run: killed by a signal" '137 9' 'var r = run("kill -9 $$"); print(r.status, r.signal);'
e "run: timeout" '1' 'print(run("sleep 10", {timeout: 0.5}).timedout);'
e "run: err to out" 'a b' 'print(trim(replace(run("echo a; echo b >&2", {err: "out"}).out, "\n", " ")));'
e "sh, system" 'hi 4' 'print(trim(sh("echo hi")), system("exit 4"));'
e "spawn with a pipe, waitpid" 'hi 0' 'var p = pipe(); var pid = spawn(["echo", "hi"], {out: p[1]}); close(p[1]); var s = trim(fdread(p[0])); print(s, waitpid(pid));'
e "fork, _exit, waitpid" '5' 'var p = fork(); if (p == 0) _exit(5); print(waitpid(p));'
e "kill, termsig" '15' 'var p = spawn(["sleep", "10"]); kill(p, SIGTERM); waitpid(p); print(termsig());'
e "getpid, getppid, IDs" '1 1' 'print(getpid() > 0 && getppid() > 0, getuid() == int(trim(sh("id -u"))));'
e "environment" 'v nil' 'setenv("TLX", "v"); var a = getenv("TLX"); unsetenv("TLX"); print(a, getenv("TLX"));'
e "signals: catch, ign, sigdisp" 'ign dfl' 'signal(SIGUSR1, "ign"); var a = sigdisp(SIGUSR1); signal(SIGUSR1, "dfl"); print(a, sigdisp(SIGUSR1));'

# ---- descriptors
e "open, fdwrite, fdread, close" 'abc' 'var fd = open("o", "wct"); fdwrite(fd, "abc"); close(fd); fd = open("o", "r"); print(fdread(fd)); close(fd);'
e "fdread with a timeout on an empty pipe: nil" 'nil' 'var p = pipe(); print(repr(fdread(p[0], 10, 0.2)));'
e "isatty" '0' 'print(isatty(open("o", "r")));'
e "UNIX-domain sockets" 'ping' 'var l = unixlisten("sock"); var c = unixconnect("sock"); var a = accept(l); fdwrite(c, "ping"); print(fdread(a, 4));'

# ---- time
e "gmtime, timegm" '1971 1 1 31536000' 'var tm = gmtime(31536000); print(tm.year, tm.mon, tm.mday, timegm(tm));'
e "strftime in UTC, strptime" '2001-09-09 01:46:40 2001' 'print(strftime("%Y-%m-%d %H:%M:%S", 1000000000, 1), strptime("2001-09-09", "%Y-%m-%d").year);'
e "time and now" '1' 'print(abs(now() - time()) < 2);'

# ---- terminals (until: pump the output until the screen shows what is
# awaited, so that a slow machine does not change the outcome)
U8='func until(t, re) { var end = now() + 30; while (now() < end && !match(tscreen(t), re)) tpump(t, 0.05); } '
e "term: the screen, cursor addressing" '"abc" "    xy" 0' "$U8"'var t = term(["sh", "-c", "printf \"abc\\033[2;5Hxy\"; sleep 1"]); until(t, "xy"); print(repr(tline(t, 0)), repr(tline(t, 1)), twait(t, 10));'
e "term: typing, a canonical terminal, the size" '24 80 got:hello' "$U8"'var t = term(["sh", "-c", "stty size; read x; echo got:$x"]); until(t, "24 80"); tsend(t, "hello\n"); twait(t, 10); print(trim(tline(t, 0)), trim(tline(t, 2)));'
e "term: rows and cols" '5 30' 'var t = term(["stty", "size"], {rows: 5, cols: 30}); twait(t, 10); print(trim(tline(t, 0)));'
e "term: attributes" '1 1' "$U8"'var t = term(["sh", "-c", "printf \"\\033[1ma\\033[0;7mb\\033[0m\"; sleep 1"]); until(t, "ab"); print(tcell(t, 0, 0).bold, tcell(t, 0, 1).reverse);'
e "term: the controlling terminal gets the signal" '130' "$U8"'var t = term(["sh", "-c", "trap \"exit 130\" INT; echo ready; while :; do sleep 1; done"]); until(t, "ready"); tsend(t, "\003"); print(twait(t, 10));'

done_tests tl
