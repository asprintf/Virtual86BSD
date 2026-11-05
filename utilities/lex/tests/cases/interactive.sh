# the scanner reads only as far as it must: with a terminal or a pipe it
# acts on a line before the next one is typed
cat > g.l <<'L'
%{
#include <stdio.h>
%}
%%
[a-z]+\n    { printf("word %.*s\n", yyleng - 1, yytext); fflush(stdout); }
.|\n        ;
L
$LEX g.l && $CC -o g lex.yy.c -L"$LIBDIR" -ll || exit 1
"$TLBIN" -e '
/* ./g on pipes: each line typed has its answer before the next */
var in_ = pipe(), out = pipe();
var pid = spawn(["./g"], {in: in_[0], out: out[1]});
close(in_[0]);
close(out[1]);
var buf = "";
for (var w : ["one\n", "two\n"]) {
    fdwrite(in_[1], w);
    while (index(buf, "\n") < 0) {
        var r = fdread(out[0], 4096, 5);
        if (r == nil || r == "")
            break;
        buf += r;
    }
    var nl = index(buf, "\n");
    print(trim(substr(buf, 0, nl)));
    buf = substr(buf, nl + 1);
}
close(in_[1]);
printf("exit %d\n", waitpid(pid));'
