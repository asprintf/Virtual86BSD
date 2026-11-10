/*
 * uu.tl - the historical uuencode format, for the tests of uuencode and
 * uudecode (independent of them).
 */

/* uu_line(d, backtick): one line (with its newline) for up to 45 bytes;
   backtick: ` for a 0, as for the count of an empty line */
/* a 6-bit value as a character */
func uu_ch(c, bt) {
    return c == 0 && bt ? "`" : chr(32 + c);
}

func uu_line(d, backtick) {
    var s = uu_ch(len(d), backtick);
    for (var i = 0; i < len(d); i += 3) {
        var b0 = d[i], b1 = i + 1 < len(d) ? d[i + 1] : 0, b2 = i + 2 < len(d) ? d[i + 2] : 0;
        s += uu_ch(b0 >> 2, backtick) + uu_ch(((b0 << 4) | (b1 >> 4)) & 63, backtick)
           + uu_ch(((b1 << 2) | (b2 >> 6)) & 63, backtick) + uu_ch(b2 & 63, backtick);
    }
    return s + "\n";
}

/* uu_body(d, backtick): the lines of the data, 45 bytes to a line */
func uu_body(d, backtick) {
    var s = "";
    for (var i = 0; i < len(d); i += 45)
        s += uu_line(substr(d, i, 45), backtick);
    return s;
}

/* uu_decode_line(l): the bytes of a line */
func uu_decode_line(l) {
    if (l == "")
        return "";
    var n = ((l[0] - 32) & 63), out = "";
    for (var i = 1; len(out) < n; i += 4) {
        var c = [0, 0, 0, 0];
        for (var k = 0; k < 4; k++)
            c[k] = i + k < len(l) ? ((l[i + k] - 32) & 63) : 0;
        out += chr((c[0] << 2) | (c[1] >> 4)) + chr(((c[1] << 4) | (c[2] >> 2)) & 255) + chr(((c[2] << 6) | c[3]) & 255);
    }
    return substr(out, 0, n);
}

/* b64_body(d): the lines of Base64, 57 bytes (76 characters) to a line */
func b64_body(d) {
    var s = "";
    for (var i = 0; i < len(d); i += 57)
        s += base64(substr(d, i, 57)) + "\n";
    return s;
}
