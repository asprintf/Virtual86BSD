/*
 * compressdiff.tl - compare compress with gzip and with the gzip format
 * written in tl (tests/gz.tl).
 *
 *     tl tests/compressdiff.tl ./compress
 *
 * Inputs of many kinds and sizes (random bytes, runs, repeated strings,
 * text) go each way: what compress writes in both formats must be expanded
 * by gzip -d (which reads .Z too) and, for gzip, by the inflate of gz.tl,
 * and what gzip writes at every level, and gz.tl in stored blocks and with
 * the fixed codes, must be expanded by compress -d.  Damaged input must not
 * crash it.  Needs gzip in PATH.
 */
include "../../testlib/testlib.tl";
include "gz.tl";

func z(cmd, data) {
    return run(cmd, {in: data});
}

/* the n-th input (they end at 67) */
var WORDS = nil;
func input(n) {
    if (n == 0)
        return "";
    if (n == 1)
        return "a";
    if (n == 2)
        return "hello\n";
    if (n == 3) {
        var s = "";
        for (var i = 0; i < 256; i++)
            s += chr(i);
        return s * 40;
    }
    if (n == 4)
        return "x" * 100000;
    if (n == 5)
        return randbytes(70000);
    if (n == 6) {
        var words = [];
        for (var i = 0; i < 500; i++)
            push(words, hex(randbytes(rand(1, 9))));
        var t = [];
        for (var i = 0; i < 60000; i++)
            push(t, choice(words));
        return join(t, " ");
    }
    if (n >= 67)
        return nil;
    var parts = [];
    for (var j = rand(0, 40); j > 0; j--) {
        var k = frand();
        if (k < 0.3)
            push(parts, randbytes(rand(0, 3000)));
        else if (k < 0.6)
            push(parts, chr(rand(0, 3)) * rand(1, 5000));
        else
            push(parts, randbytes(rand(1, 40)) * rand(1, 300));
    }
    return join(parts, "");
}

var prog = abspath(argv[0]);
if (which("gzip") == nil) {
    print("compressdiff: gzip is needed");
    exit(2);
}
srand(1952);
for (var n = 0; ; n++) {
    var data = input(n);
    if (data == nil)
        break;
    var nbits = rand(9, 16);
    var zd = z([prog, "-c", "-f", "-b", str(nbits)], data).out;
    check(sprintf("%d: .Z (%d bits) expanded by gzip", n, nbits), z(["gzip", "-dc"], zd).out == data);
    check(sprintf("%d: .Z (%d bits) expanded by itself", n, nbits), z([prog, "-dc"], zd).out == data);
    var level = rand(1, 9);
    var g = z([prog, "-c", "-f", "-g", "-b", str(level)], data).out;
    check(sprintf("%d: .gz (level %d) expanded by gzip", n, level), z(["gzip", "-dc"], g).out == data);
    check(sprintf("%d: .gz (level %d) expanded by gz.tl", n, level), gunzip(g) == data);
    var g2 = z(["gzip", sprintf("-%d", level), "-c"], data).out;
    check(sprintf("%d: gzip -%d expanded", n, level), z([prog, "-dc"], g2).out == data);
    var tlevel = rand(0, 9);
    var p = gzip_write(data, tlevel);
    check(sprintf("%d: gz.tl's gzip (%s) expanded", n, tlevel ? "fixed codes" : "stored"),
          z([prog, "-dc"], p).out == data);
    for (var sw : [[zd, ".Z"], [g, ".gz"]]) {
        var src = sw[0];
        if (len(src) > 12) {
            var d = [];
            for (var c : src)
                push(d, c);
            for (var k = 0; k < 3; k++)
                d[rand(2, len(d) - 1)] = rand(256);
            var r = z([prog, "-dc"], bytes_str(d));
            check(sprintf("%d: damaged %s: no crash", n, sw[1]), r.signal == 0);
        }
    }
}
exit(tests_done("compressdiff"));
