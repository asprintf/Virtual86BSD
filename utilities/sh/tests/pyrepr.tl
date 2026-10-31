/*
 * pyrepr.tl - strings written as Python's repr() writes them, for reports
 * that keep the form the Python versions of these tests had:
 * pyrepr(s) as a str ('...'), pybytes(s) as bytes (b'...').  Bytes
 * outside ASCII are kept as they are in a str (taken as UTF-8 text).
 */

func _pyquote(s, high) {
    var q = "'" in s && !("\"" in s) ? "\"" : "'";
    var out = q;
    for (var c : s) {
        if (c == '\\')
            out += "\\\\";
        else if (c == ord(q))
            out += "\\" + q;
        else if (c == '\t')
            out += "\\t";
        else if (c == '\n')
            out += "\\n";
        else if (c == '\r')
            out += "\\r";
        else if (c < 32 || c == 127 || (c > 127 && high))
            out += sprintf("\\x%02x", c);
        else
            out += chr(c);
    }
    return out + q;
}

func pyrepr(s) {
    return _pyquote(s, false);
}

func pybytes(s) {
    return "b" + _pyquote(s, true);
}
