/*
 * xgettextdiff.tl - xgettext compared with GNU xgettext
 *
 *     tl xgettextdiff.tl XGETTEXT [SEED [RUNS]]
 *
 * Random C sources (keyword calls with literal and other arguments, nested
 * calls, joined string literals with escapes, comments, character
 * constants, #include and #define lines, lines joined by backslash-newline)
 * are read by both, with the same keywords (-K "" -K ... here, -k -k... for
 * GNU) and the same -a, -n and -x.  The msgids written (with their
 * msgid_plural, and with -n the places of their first occurrence) must be
 * the same, in the same order; GNU merges a msgid met again into the first
 * (taking its msgid_plural when the first has none), this one writes it
 * as comment lines, which are merged so for the comparison.
 */

var GNU = nil;
for (var d : ["/usr/bin", "/usr/local/bin", "/bin"])
    if (GNU == nil && access(d + "/xgettext", "x"))
        GNU = d + "/xgettext";

var STRS = ["hello", "Hello, world", "a\\tb", "two\\nlines", "x\\101y", "q\\\"uote", "back\\\\slash", "c%d",
            "%s and %s", "na\303\257ve", "percent 100%%", "one", "two", "many"];
var KEYWORDS = [["gettext", "1"], ["ngettext", "1,2"], ["i18n", "1"], ["dg", "2"], ["pl", "1,3"], ["N_", "1"]];

func lit() {
    var s = "\"" + choice(STRS) + "\"";
    if (frand() < 0.2)
        s += " \"" + choice(STRS) + "\"";
    if (frand() < 0.1)
        s = "(" + s + ")";
    return s;
}

func expr(depth) {
    var c = frand();
    if (c < 0.4 || depth > 2)
        return lit();
    if (c < 0.55)
        return choice(["x", "argv[1]", "n + 1", "f(x)", "'\"'", "'a'", "0"]);
    if (c < 0.65)
        return "other(" + lit() + ")";
    return call(depth + 1);
}

func call(depth) {
    var kw = choice(KEYWORDS);
    var nargs = max(apply(split(kw[1], ","), int)) + rand(0, 1);
    var args = [];
    for (var k = 0; k < nargs; k++)
        push(args, expr(depth));
    return kw[0] + "(" + join(args, ", ") + ")";
}

func source() {
    var out = ["#include <stdio.h>", "#include \"header.h\""];
    for (var i = 0, n = rand(3, 25); i < n; i++) {
        var c = frand();
        if (c < 0.6)
            push(out, "    puts(" + call(0) + ");");
        else if (c < 0.7)
            push(out, "    /* " + call(0) + " */");
        else if (c < 0.75)
            push(out, "    // " + call(0));
        else if (c < 0.8)
            push(out, sprintf("#define M%d %s", i, call(0)));
        else if (c < 0.85)
            push(out, "    s = " + lit() + ";");
        else if (c < 0.9)
            push(out, sprintf("    puts(%s(\\\n\"%s\"));", choice(["gettext", "i18n"]), choice(STRS)));
        else
            push(out, "    c = '\"'; d = " + call(0) + ";");
    }
    return "int main(void)\n{\n" + join(out, "\n") + "\n}\n";
}

/* the value of the msgid or msgid_plural at line i (without the prefix
   pre), with its continuation lines: [value, the next line] */
func value(lines, i, pre) {
    var m = match(substr(lines[i], len(pre)), "^[^[:space:]]+ \"(.*)\"$");
    var v = m[1];
    i++;
    while (i < len(lines) && startswith(substr(lines[i], len(pre)), "\"")) {
        var l = substr(lines[i], len(pre));
        v += substr(l, 1, len(l) - 2);
        i++;
    }
    return [v, i];
}

/*
 * the [reference, msgid, msgid_plural] of the messages of a dot-po file,
 * each msgid once: a msgid met again (written again as comment lines here,
 * merged by GNU) gives its plural to the first when that has none
 */
func entries(path) {
    var text = readfile(path);
    if (text == nil)
        return [];
    var lines = split(text, "\n");
    var res = [], seen = {}, refs = [], cur = nil;
    var i = 0;
    while (i < len(lines)) {
        var l = lines[i];
        var pre = startswith(l, "# msg") ? "# " : "";
        var body = substr(l, len(pre));
        if (startswith(l, "#:")) {
            refs += split(substr(l, 2));
            i++;
        } else if (startswith(body, "msgid ")) {
            var vi = value(lines, i, pre);
            i = vi[1];
            if (haskey(seen, vi[0]))
                cur = seen[vi[0]];
            else {
                cur = [slice(refs, 0, min(1, len(refs))), vi[0], nil];
                seen[vi[0]] = cur;
                push(res, cur);
            }
            refs = [];
        } else if (startswith(body, "msgid_plural ")) {
            var vi = value(lines, i, pre);
            i = vi[1];
            if (cur[2] == nil)
                cur[2] = vi[0];
        } else
            i++;
    }
    return filter(res, func (e) { return e[1] != ""; });
}

/* k distinct elements of l, in random order */
func sample(l, k) {
    var c = copy(l);
    shuffle(c);
    return slice(c, 0, k);
}

if (len(argv) < 1) {
    print("usage: xgettextdiff.tl XGETTEXT [SEED [RUNS]]");
    exit(2);
}
var ours = abspath(argv[0]);
var seed = len(argv) > 1 ? int(argv[1]) : 1;
var runs = len(argv) > 2 ? int(argv[2]) : 300;
if (GNU == nil) {
    print("xgettextdiff: no GNU xgettext");
    exit(2);
}
srand(seed);
var fails = 0;
var tmp = mkdtemp("xgettextdiff.");
for (var i = 0; i < runs; i++) {
    var src = tmp + "/a.c";
    writefile(src, source());
    var kws = sample(KEYWORDS, rand(1, len(KEYWORDS)));
    var o_a = frand() < 0.2;
    var o_n = frand() < 0.5;
    var ourcmd = [ours, "-K", ""], gnucmd = [GNU, "--from-code=UTF-8", "-k"];
    for (var k : kws) {
        push(ourcmd, "-K", k[0] + ":" + k[1]);
        push(gnucmd, "-k" + k[0] + ":" + k[1]);
    }
    if (o_a) {
        push(ourcmd, "-a");
        push(gnucmd, "-a");
    }
    if (!o_n)
        push(gnucmd, "--no-location");
    else
        push(ourcmd, "-n");
    var res = {};
    for (var who : [["ours", ourcmd], ["gnu", gnucmd]]) {
        var d = tmp + "/" + who[0];
        rmtree(d);
        mkdir(d);
        var p = run(who[1] + ["-p", d, src], {cwd: tmp});
        res[who[0]] = [p.status, entries(d + "/messages.po")];
    }
    if (res.ours != res.gnu) {
        fails++;
        if (fails <= 5) {
            printf("FAIL run %d: %s\n", i, join(slice(ourcmd, 1), " "));
            var a = res.ours[1], b = res.gnu[1];
            for (var k = 0; k < max(len(a), len(b)); k++) {
                var x = k < len(a) ? a[k] : nil, y = k < len(b) ? b[k] : nil;
                if (x != y) {
                    printf("  first difference at %d:\n    ours: %q\n    gnu:  %q\n", k, x, y);
                    break;
                }
            }
            printf("  statuses %d %d\n", res.ours[0], res.gnu[0]);
            writefile(sprintf("xgettextdiff.fail%d.c", i), readfile(src));
        }
    }
}
rmtree(tmp);
printf("xgettextdiff: %d compared, %d failed\n", runs, fails);
exit(fails ? 1 : 0);
