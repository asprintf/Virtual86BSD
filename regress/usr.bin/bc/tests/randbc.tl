/*
 * randbc.tl - random differential testing of bc against GNU bc.
 *
 * usage: tl randbc.tl BC SEED COUNT [-l] [-v] [--show N]
 *
 * Each round is a random program: settings of scale, ibase and obase,
 * expressions with + - * / % ^, unary minus, ++ and --, assignments
 * (also compound), sqrt, length, scale(), arrays, relational operators,
 * if/while/for with break, functions with parameters, auto variables
 * and arrays passed by value, recursion, strings, and with -l the math
 * library.  The standard output of both must be the same, and both must
 * report an error (on standard error) or neither.  -v shows the program
 * of each difference; --show N only prints the program of round N.
 */

if (len(argv) < 3) {
    eprintf("usage: tl randbc.tl BC SEED COUNT [-l] [-v] [--show N]\n");
    exit(2);
}
var bc = abspath(argv[0]);
srand(int(argv[1]));
var count = int(argv[2]);
var mathlib = "-l" in argv;
var verbose = "-v" in argv;
var show = 0;
if ("--show" in argv)
    show = int(argv[find(argv, "--show") + 1]);

var VARS = "abcdxyz";

/* a random character of the string s */
func pick(s) {
    return chr(s[rand(len(s))]);
}

/* n random characters of s */
func picks(s, n) {
    var t = "";
    for (var i = 0; i < n; i++)
        t += pick(s);
    return t;
}

func number(base) {
    var digits = substr("0123456789ABCDEF", 0, base);
    var r = frand();
    if (r < 0.3)
        return base > 1 ? pick(substr(digits, 1)) : "1";
    var ip = ltrim(picks(digits, rand(1, 6)), "0");
    if (ip == "")
        ip = "0";
    if (frand() < 0.5) {
        var fp = picks(digits, rand(1, 6));
        if (frand() < 0.2)
            return "." + fp;
        return ip + "." + fp;
    }
    return ip;
}

func expr(depth, base) {
    var r = frand();
    if (depth > 3 || r < 0.25)
        return number(base);
    if (r < 0.35)
        return pick(VARS);
    if (r < 0.4)
        return sprintf("%s[%d]", pick("uv"), rand(0, 5));
    if (r < 0.47) {
        var e = expr(depth + 1, base);
        return "-" + (startswith(e, "-") ? "(" + e + ")" : e);
    }
    if (r < 0.75) {
        var op = choice(["+", "-", "*", "/", "%", "*", "+"]);
        return sprintf("(%s %s %s)", expr(depth + 1, base), op, expr(depth + 1, base));
    }
    if (r < 0.8)
        return sprintf("(%s ^ %s)", expr(depth + 1, base), choice(["0", "1", "2", "3", "5", "-1", "-2", "7"]));
    if (r < 0.84)
        return sprintf("sqrt(%s)", ltrim(expr(depth + 1, base), "-"));
    if (r < 0.87)
        return sprintf("length(%s)", expr(depth + 1, base));
    if (r < 0.9)
        return sprintf("scale(%s)", expr(depth + 1, base));
    if (r < 0.93)
        return sprintf(choice(["++%s", "--%s", "%s++", "%s--"]), pick(VARS));
    if (r < 0.96)
        return sprintf("(%s %s %s)", pick(VARS), choice(["=", "+=", "-=", "*="]), expr(depth + 1, base));
    if (mathlib) {
        var f = choice(["s", "c", "a", "e", "l", "j"]);
        if (f == "j")
            return sprintf("j(%d, %s)", rand(-3, 4), expr(depth + 1, base));
        if (f == "e")
            /* (decimal digits only where ibase is 10: digits beyond ibase are undefined) */
            return sprintf("e(%s)", base == 10 ? choice(["1", "-2", ".5", "3.25", "-.75", "10"]) : number(base));
        if (f == "l")
            return sprintf("l(%s)", ltrim(number(base), "-"));
        return sprintf("%s(%s)", f, expr(depth + 1, base));
    }
    return number(base);
}

func rel() {
    return sprintf("%s %s %s", expr(2, 10), choice(["<", ">", "<=", ">=", "==", "!="]), expr(2, 10));
}

func statement(depth, infunc, inloop) {
    var r = frand();
    if (depth > 2 || r < 0.45)
        return expr(0, 10);
    if (r < 0.55)
        return sprintf("%s = %s", pick(VARS), expr(1, 10));
    if (r < 0.6)
        return sprintf("%s[%d] = %s", pick("uv"), rand(0, 5), expr(1, 10));
    if (r < 0.65)
        return sprintf("if (%s) %s", rel(), statement(depth + 1, infunc, inloop));
    if (r < 0.7) {
        var v = pick("ijk");
        var n = rand(0, 4);
        return sprintf("for (%s = 0; %s < %d; %s++) { %s }", v, v, n, v, statement(depth + 1, infunc, 1));
    }
    if (r < 0.74) {
        var v = pick("ijk");
        var n = rand(0, 3);
        return sprintf("%s = %d; while (%s > 0) { %s--; %s }", v, n, v, v, statement(depth + 1, infunc, 1));
    }
    if (r < 0.77 && inloop)
        return "break";
    if (r < 0.82)
        return sprintf("\"str%d\"", rand(0, 9));
    if (r < 0.86 && infunc)
        return sprintf("return (%s)", expr(1, 10));
    if (r < 0.9)
        return sprintf("f(%s, %s)", expr(1, 10), expr(1, 10));
    if (r < 0.93)
        return "g(u[])";
    return sprintf("{ %s; %s }", statement(depth + 1, infunc, inloop), statement(depth + 1, infunc, inloop));
}

func program() {
    var lines = [];
    if (mathlib)
        /* (GNU bc's math library loses accuracy at small scales: s(588) at
           scale 3 is -.504, not -.498) */
        push(lines, sprintf("scale = %d", choice([20, 25, 30, 40])));
    else if (frand() < 0.7)
        push(lines, sprintf("scale = %d", choice([0, 0, 1, 2, 3, 5, 10, 20, 30])));
    push(lines, sprintf("define f(x, y) {\n  auto a, t[]\n  a = x * 2\n  t[1] = y\n  %s\n  return (a + t[1] - b)\n}",
                        statement(1, 1, 0)));
    push(lines, "define g(w[]) {\n  w[0] = w[0] + 1\n  return (w[0] + w[1])\n}");
    push(lines, "define r(n) {\n  if (n <= 0) return (1)\n  return (n * r(n - 1))\n}");
    for (var n = rand(3, 10); n > 0; n--) {
        var r = frand();
        if (r < 0.1)
            push(lines, sprintf("obase = %d", choice([2, 3, 8, 10, 16, 10, 10, 17, 25, 100, 1000])));
        else if (r < 0.15) {
            var base = choice([2, 8, 16, 10]);
            var ib = {2: "2", 8: "8", 16: "F+1", 10: "A"}[base];
            push(lines, sprintf("ibase = %s; %s; ibase = A", ib, expr(1, base)));
        } else if (r < 0.2)
            push(lines, sprintf("r(%d)", rand(0, 25)));
        else {
            var sts = [];
            for (var k = rand(1, 3); k > 0; k--)
                push(sts, statement(0, 0, 0));
            push(lines, join(sts, "; "));
        }
    }
    push(lines, "a; b; c; u[1]; v[2]; obase=10; g(v[]); v[0]");
    return join(lines, "\n") + "\n";
}

/* [standard output, whether anything was written to standard error] */
func runbc(cmd, prog, env) {
    var r = run(cmd, {in: prog, env: env, timeout: 10});
    if (r.timedout)
        return ["TIMEOUT", 0];
    /* GNU bc can print a zero as "-0" (from a negative operand that
       truncates to zero); the standard says a zero is written as 0 */
    var ls = split(r.out, "\n");
    for (var i = 0; i < len(ls); i++)
        if (ls[i] == "-0")
            ls[i] = "0";
    return [join(ls, "\n"), trim(r.err) != ""];
}

func pybool(b) {
    return b ? "True" : "False";
}

/* the counters as {'name': count, ...} */
func statstr(st) {
    return sprintf("{'rounds': %d, 'diffs': %d, 'errors': %d, 'timeouts': %d}",
                   st.rounds, st.diffs, st.errors, st.timeouts);
}

var stats = {rounds: 0, diffs: 0, errors: 0, timeouts: 0};
for (var rnd = 1; rnd <= count; rnd++) {
    var prog = program();
    if (show) {
        if (rnd == show) {
            print(prog);
            exit(0);
        }
        continue;
    }
    var flags = mathlib ? ["-l"] : [];
    var ra = runbc([bc] + flags, prog, {BC_TEST_GNU_SQRT: "1"});
    var rb = runbc(["bc"] + flags, prog, {});
    var a = ra[0], ea = ra[1], b = rb[0], eb = rb[1];
    stats.rounds++;
    if (b == "TIMEOUT") {
        /* (GNU bc is much slower on some programs, such as big powers) */
        stats.timeouts++;
        continue;
    }
    if (ea || eb)
        stats.errors++;
    if (a != b || ea != eb) {
        stats.diffs++;
        printf("DIFF (round %d) error mine %s gnu %s\n", rnd, pybool(ea), pybool(eb));
        if (verbose)
            print(prog);
        var la = split(a, "\n"), lb = split(b, "\n");
        for (var i = 0; i < max(len(la), len(lb)); i++) {
            var x = i < len(la) ? la[i] : "<none>";
            var y = i < len(lb) ? lb[i] : "<none>";
            if (x != y) {
                printf("  line %d\n   mine: %q\n   gnu:  %q\n", i + 1, substr(x, 0, 200), substr(y, 0, 200));
                break;
            }
        }
    }
    if (rnd % 100 == 0) {
        printf("progress %d %s\n", rnd, statstr(stats));
        flush();
    }
}
print(statstr(stats));
exit(stats.diffs ? 1 : 0);
