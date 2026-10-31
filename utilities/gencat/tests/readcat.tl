/*
 * readcat.tl - write the messages of a catalog that gencat made (the format
 * of the GNU C library), for the tests of gencat
 *
 *     tl readcat.tl CATALOG
 *
 * writes "SET MESSAGE TEXT" for each message, in the order of the sets and
 * messages, with \n for a newline and other control characters as \ooo;
 * then "size N depth D", and "ok" when every message is found where
 * catgets() looks for it and the big-endian table is the same as the
 * little-endian one.
 */

var d = readfile(argv[0]);
var e = unpack("<I", d)[0] == 0x960408de ? "<" : ">";
var h = unpack(e + "3I", d);
var size = h[1], depth = h[2];
var n = size * depth * 3;
var le = unpack(sprintf("<%dI", n), d, 12), be = unpack(sprintf(">%dI", n), d, 12 + 4 * n);
var strs = substr(d, 12 + 8 * n);
var msgs = [];
for (var i = 0; i < n; i += 3)
    if (le[i]) {
        var o = le[i + 2];
        push(msgs, [le[i] - 1, le[i + 1], substr(strs, o, index(strs, "\0", o) - o)]);
    }
for (var m : sorted(msgs)) {
    var shown = "";
    for (var c : m[2])
        shown += c == 10 ? "\\n" : c < 32 ? sprintf("\\%03o", c) : chr(c);
    print(m[0], m[1], shown);
}
print("size", size, "depth", depth);
var ok = le == be;
for (var m : msgs) {
    var idx = ((m[0] + 1) * m[1]) % size, found = false;
    for (var k = 0; k < depth; k++) {
        var j = (k * size + idx) * 3;
        if (le[j] == m[0] + 1 && le[j + 1] == m[1]) {
            found = true;
            break;
        }
    }
    ok = ok && found;
}
print(ok ? "ok" : "bad");
