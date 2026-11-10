/*
 * quote.tl - strings in quotes with escapes, for the reports of the
 * equivalence tests: quoted(s) as text ('...', or "..." when the text
 * has ' but no "), quotedbytes(s) as bytes (b'...').  Bytes outside
 * ASCII are kept as they are in text (taken as UTF-8).
 */

func _quote(s, high) {
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

func quoted(s) {
    return _quote(s, false);
}

func quotedbytes(s) {
    return "b" + _quote(s, true);
}
