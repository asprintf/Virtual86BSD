/*
 * dec.tl - decodes the output of uuencode on the standard input,
 * independently of uudecode (historical uuencode and RFC 4648 base64)
 *
 *     tl dec.tl < FILE
 */
include "../../../lib/testlib/uu.tl";

var l = split(readfile("/dev/stdin"), "\n");
var out = "";
if (startswith(l[0], "begin-base64 ")) {
    var b = "";
    for (var i = 1; l[i] != "===="; i++)
        b += l[i];
    out = unbase64(b);
} else {
    var end = find(l, "end");
    for (var i = 1; i < end - 1; i++)
        out += uu_decode_line(l[i]);
}
puts(out);
