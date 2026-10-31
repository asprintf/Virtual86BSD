/*
 * mkmo.tl - write a messages object (the GNU format) for the tests of
 * gettext and ngettext, without msgfmt
 *
 *     tl mkmo.tl OUT [-b] [-n] < spec
 *
 * Each line of spec is ORIGINAL<TAB>TRANSLATION, with the escapes \n, \t,
 * \0 (a NUL byte: between msgid and msgid_plural, and between the plural
 * forms) and a doubled backslash for one; other bytes are as they are,
 * or written \xHH.  -b writes big-endian words; -n leaves the strings
 * unsorted (a file not made by msgfmt).
 */

func unesc(s) {
    var out = "", i = 0;
    var esc = {n: "\n", t: "\t", "0": "\0", "\\": "\\"};
    while (i < len(s)) {
        var c = s[i];
        if (c == '\\' && i + 1 < len(s)) {
            var k = chr(s[i + 1]);
            if (k == "x") {
                out += chr(int(substr(s, i + 2, 2), 16));
                i += 4;
                continue;
            }
            out += esc[k];
            i += 2;
            continue;
        }
        out += chr(c);
        i++;
    }
    return out;
}

var args = filter(argv, func (a) { return a != "-b" && a != "-n"; });
var out = args[0], big = "-b" in argv, nosort = "-n" in argv;
var pairs = [];
for (var line : split(readfile("-"), "\n")) {
    if (line == "")
        continue;
    var p = split(line, "\t", 1);
    push(pairs, [unesc(p[0]), unesc(p[1])]);
}
if (!nosort)
    sort(pairs, func (a, b) { return compare_strings(split(a[0], "\0")[0], split(b[0], "\0")[0]); });
func compare_strings(a, b) { return a < b ? -1 : a > b ? 1 : 0; }
var e = big ? ">" : "<";
var n = len(pairs), head = 28;
var orig = head, trans = head + 8 * n, strs = head + 16 * n;
var data = "", ot = [], tt = [];
for (var p : pairs) {
    push(ot, [len(p[0]), strs + len(data)]);
    data += p[0] + "\0";
}
for (var p : pairs) {
    push(tt, [len(p[1]), strs + len(data)]);
    data += p[1] + "\0";
}
var buf = pack(e + "7I", 0x950412de, 0, n, orig, trans, 0, strs);
for (var lo : ot + tt)
    buf += pack(e + "2I", lo[0], lo[1]);
writefile(out, buf + data);
