/*
 * msgfmtdiff.tl - msgfmt compared with GNU msgfmt
 *
 *     tl msgfmtdiff.tl MSGFMT [SEED [RUNS]]
 *
 * Random dot-po files (headers with plural forms and charsets, plural
 * messages, escapes, continuation lines, non-ASCII text, fuzzy and
 * untranslated messages, contexts, domains; as GNU msgfmt wants them: a
 * header (with a charset) only in the first domain, non-ASCII text only
 * with a charset, no comment ending in a backslash (GNU joins the next
 * line to it); no newlines at the ends of the
 * strings, which GNU msgfmt checks always and this one only with -c) are
 * compiled by both; the
 * messages objects must be the same, byte for byte, and so must the exit
 * status of the two.  -f, -o and the -c -v checks are mixed in.
 */
include "../../../lib/testlib/testlib.tl";

var GNU = nil;
for (var d : ["/usr/bin", "/usr/local/bin", "/bin"])
    if (GNU == nil && access(d + "/msgfmt", "x"))
        GNU = d + "/msgfmt";

var WORDS = ["file", "files", "open", "close", "error", "Hello", "world", "d\303\251j\303\240",
             "na\303\257ve", "\346\227\245\346\234\254", "\303\274n\303\257code", "a", "b", "c", "value",
             "item", "%d", "%s", "%1$s", "line\\nbreak", "\\t", "\\\"quoted\\\"", "\\\\", "\\101", "\\x41",
             " ", "100%%"];
var ASCII = filter(WORDS, func (w) { return !non_ascii(w); });
var words = WORDS;

func rstr(minw, maxw) {
    var w = [];
    for (var n = rand(minw, maxw); n > 0; n--)
        push(w, choice(words));
    return join(w, " ");
}

/* s as a value: one string, or split into continuation lines */
func quoted(s) {
    var spaces = [];
    for (var k = 1; k < len(s); k++)
        if (s[k] == ' ')
            push(spaces, k);
    if (len(spaces) && frand() < 0.3) {
        var k = choice(spaces);
        return sprintf("\"\"\n\"%s\"\n\"%s\"", substr(s, 0, k), substr(s, k));
    }
    return "\"" + s + "\"";
}

func make_po() {
    var out = [];
    var domains = ["messages"];
    if (frand() < 0.3) {
        domains = [];
        for (var i = 0, n = rand(1, 3); i < n; i++)
            push(domains, sprintf("dom%d", i));
    }
    words = WORDS;
    var header = frand() < 0.8;
    if (!header)
        words = ASCII;
    for (var d : domains) {
        if (d != "messages" || frand() < 0.2)
            push(out, sprintf("domain \"%s\"", d));
        var nplurals = 0;
        if (header && d == domains[0]) {
            var h = ["Project-Id-Version: test\\n", "Content-Type: text/plain; charset=UTF-8\\n"];
            if (frand() < 0.7) {
                var pl = choice([[2, "n != 1"], [1, "0"], [3, "n==1 ? 0 : n==2 ? 1 : 2"],
                                 [4, "n==1?0: (n>1&&n<=10)?1: (n==0)?2:3"]]);
                nplurals = pl[0];
                push(h, sprintf("Plural-Forms: nplurals=%d; plural=%s;\\n", nplurals, pl[1]));
            }
            push(out, "msgid \"\"\nmsgstr \"\"\n" + join(apply(h, func (x) { return "\"" + x + "\""; }), "\n"));
            push(out, "");
        }
        var used = {};
        for (var n = rand(0, 25); n > 0; n--) {
            var mid = rstr(1, 4);
            var ctx = nil;
            if (frand() < 0.1)
                ctx = rstr(1, 1);
            var key = repr([ctx, mid]);
            if (haskey(used, key))
                continue;
            used[key] = 1;
            if (frand() < 0.2) {
                var notes = [];
                for (var k = 0; k < 3; k++)
                    push(notes, choice(["note", "check", "x"]));
                push(out, "# translator: " + join(notes, " "));
            }
            if (frand() < 0.3)
                push(out, sprintf("#: src%d.c:%d", rand(1, 3), rand(1, 99)));
            var flags = [];
            if (frand() < 0.15)
                push(flags, "fuzzy");
            if (frand() < 0.2)
                push(flags, choice(["c-format", "no-c-format"]));
            if (len(flags))
                push(out, "#, " + join(flags, ", "));
            if (ctx != nil)
                push(out, sprintf("msgctxt \"%s\"", ctx));
            push(out, "msgid " + quoted(mid));
            if (nplurals && frand() < 0.3) {
                push(out, "msgid_plural " + quoted(rstr(1, 4)));
                for (var k = 0; k < nplurals; k++)
                    push(out, sprintf("msgstr[%d] %s", k, quoted(frand() < 0.05 ? "" : rstr(1, 4))));
            } else
                push(out, "msgstr " + quoted(frand() < 0.1 ? "" : rstr(1, 4)));
            push(out, "");
        }
    }
    return join(out, "\n") + "\n";
}

func plist(l) {
    return "[" + join(apply(l, func (s) { return "'" + s + "'"; }), ", ") + "]";
}

if (len(argv) < 1) {
    print("usage: msgfmtdiff.tl MSGFMT [SEED [RUNS]]");
    exit(2);
}
var ours = abspath(argv[0]);
var seed = len(argv) > 1 ? int(argv[1]) : 1;
var runs = len(argv) > 2 ? int(argv[2]) : 300;
if (GNU == nil) {
    print("msgfmtdiff: no GNU msgfmt");
    exit(2);
}
srand(seed);
var fails = 0;
var tmp = mkdtemp("msgfmtdiff.");
for (var i = 0; i < runs; i++) {
    var po = make_po();
    var opts = [];
    if (frand() < 0.3)
        push(opts, "-f");
    var single = frand() < 0.3;
    var res = {};
    for (var who : [["ours", ours], ["gnu", GNU]]) {
        var d = tmp + "/" + who[0];
        rmtree(d);
        mkdir(d);
        writefile(d + "/in.po", po);
        var st = run([who[1]] + opts + (single ? ["-o", "out.mo"] : []) + ["in.po"], {cwd: d}).status;
        /* the messages objects written: name -> contents */
        var files = {};
        for (var n : listdir(d))
            if (endswith(n, ".mo"))
                files[n] = readfile(d + "/" + n);
        res[who[0]] = [st, files];
    }
    if (res.ours != res.gnu) {
        fails++;
        if (fails <= 5) {
            printf("FAIL run %d (%s %s):\n", i, join(opts, " "), single ? "-o" : "");
            printf("  ours: status %d, files %s\n", res.ours[0], plist(sorted(keys(res.ours[1]))));
            printf("  gnu:  status %d, files %s\n", res.gnu[0], plist(sorted(keys(res.gnu[1]))));
            for (var n : keys(res.ours[1]))
                if (res.gnu[1][n] != res.ours[1][n])
                    printf("  %s differs\n", n);
            writefile(sprintf("msgfmtdiff.fail%d.po", i), po);
        }
    }
}
rmtree(tmp);
printf("msgfmtdiff: %d compared, %d failed\n", runs, fails);
exit(fails ? 1 : 0);
