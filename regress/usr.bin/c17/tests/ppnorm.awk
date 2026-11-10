# ppnorm.awk - the token text of preprocessed C, for comparing the output
# of c17 -E: line markers ("# 12 ...") dropped, and all white space outside
# string literals and character constants removed.
/^# [0-9]+ / { next }
{ buf = buf $0 "\n" }
END {
    n = length(buf); out = ""; i = 1
    while (i <= n) {
        c = substr(buf, i, 1)
        if (c == "\"" || c == "'") {
            j = i + 1
            while (j <= n && substr(buf, j, 1) != c)
                j += substr(buf, j, 1) == "\\" ? 2 : 1
            out = out substr(buf, i, j - i + 1); i = j + 1
        } else if (c == " " || c == "\t" || c == "\n" || c == "\r" || c == "\f" || c == "\v") {
            i++
        } else {
            out = out c; i++
        }
    }
    print out
}
