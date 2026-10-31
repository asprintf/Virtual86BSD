/*
 * readmo.tl - write the messages of a messages object (the GNU format),
 * for the tests of msgfmt
 *
 *     tl readmo.tl FILE
 *
 * writes one line for each message, in the order of the file:
 * ORIGINAL => TRANSLATION, NUL bytes as | and newlines as \n; then a line
 * "hash N" with the size of the hash table, and "ok" when every message is
 * found through it (as the C library's gettext() looks them up) and the
 * messages are sorted.
 */

func hash_string(b) {
    var h = 0;
    for (var i = 0; i < len(b); i++) {
        h = ((h << 4) + b[i]) & 0xffffffff;
        var g = h & 0xf0000000;
        if (g) {
            h ^= g >> 24;
            h ^= g;
        }
    }
    return h;
}

func show(b) {
    return replace(replace(b, "\0", "|"), "\n", "\\n");
}

/* the message's key: the original up to its first NUL */
func key(b) {
    return split(b, "\0")[0];
}

var data = readfile(argv[0]);
if (data == nil) {
    eprintf("readmo.tl: %s: %s\n", argv[0], strerror());
    exit(2);
}
var e = unpack("<I", data)[0] == 0x950412de ? "<" : ">";
var h = unpack(e + "7I", data);
var n = h[2], o = h[3], t = h[4], hs = h[5], ho = h[6];

func str_at(tab, i) {
    var lo = unpack(e + "2I", data, tab + 8 * i);
    return substr(data, lo[1], lo[0]);
}

var origs = [];
for (var i = 0; i < n; i++)
    push(origs, str_at(o, i));
for (var i = 0; i < n; i++)
    printf("%s => %s\n", show(origs[i]), show(str_at(t, i)));
printf("hash %d\n", hs);
var ok = true;
for (var i = 0; i < n && ok; i++) {
    var hv = hash_string(key(origs[i]));
    var idx = hv % hs, incr = 1 + hv % (hs - 2);
    for (;;) {
        var v = unpack(e + "I", data, ho + 4 * idx)[0];
        if (v == 0) {
            ok = false;
            break;
        }
        if (v - 1 == i)
            break;
        idx = idx >= hs - incr ? idx - (hs - incr) : idx + incr;
    }
}
for (var i = 1; i < n && ok; i++)
    if (key(origs[i - 1]) > key(origs[i]))
        ok = false;
print(ok ? "ok" : "bad");
