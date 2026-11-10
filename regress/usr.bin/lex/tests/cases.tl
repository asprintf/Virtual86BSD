/*
 * cases.tl - the cases of difftest.tl (and seeds of fuzz.tl): name, src (a lex
 * source), inputs (the texts to scan), and optionally args (of the scanner)
 * and differs (why flex differs).
 */

/* the source of many-keywords: one rule for each word, then identifiers */
func keyword_rules(words) {
    var rules = [];
    for (var i = 0; i < len(words); i++)
        push(rules, sprintf("%s\tprintf(\"<%d>\");", words[i], i));
    return join(rules, "\n");
}

var P = R"(
%{
#include <stdio.h>
%}
)";

var CASES = [
    {name: "copy", src: "%%\n", inputs: ["", "abc\n", "no newline", "two\nlines\n\n"]},
    {name: "pascal", src: P + R"(
DIGIT    [0-9]
ID       [a-z][a-z0-9]*
%%
{DIGIT}+ {
    printf("An integer: %s (%d)\n", yytext,
        atoi(yytext));
    }
{DIGIT}+"."{DIGIT}*        {
    printf("A float: %s\n", yytext);
    }
if|then|begin|end|procedure|function        {
    printf("A keyword: %s\n", yytext);
    }
{ID}    printf("An identifier: %s\n", yytext);
"+"|"-"|"*"|"/"        printf("An operator: %s\n", yytext);
"{"[^}\n]*"}"    /* Eat up one-line comments. */
[ \t\n]+        /* Eat up white space. */
.  printf("Unrecognized character: %s\n", yytext);
)", inputs: ["42 3.14 if foo + {comment} @\nend\n", "then1 x9y 12. {open\n", ""]},
    {name: "longest-first", src: P + R"(
%%
ab      printf("[ab]");
abc     printf("[abc]");
a       printf("[a]");
abcd|abce printf("[abc*]");
[a-c]+  printf("[+%s]", yytext);
)", inputs: ["abcdabceabcab", "aaaab", "cab abcabc", "abcde\n"]},
    {name: "classes", src: P + R"(
%%
[[:alpha:]]+    printf("<A:%s>", yytext);
[[:digit:]]+    printf("<D:%s>", yytext);
[[:space:]]     printf("<S>");
[[:punct:]]     printf("<P:%s>", yytext);
[^[:alnum:][:space:][:punct:]]  printf("<O>");
)", inputs: ["Hello, World 123!\t\n", "a1b2c3", "\x01\x7f\x80\xff"]},
    {name: "brackets", src: P + R"(
%%
[]]     printf("<rb>");
[^]a]   printf("<n:%s>", yytext);
[a-]    printf("<am>");
[-z]    printf("<mz>");
[\]\\]  printf("<esc>");
[[.x.]-z] printf("<xz>");
[[=a=]] printf("<eq>");
)", inputs: ["]a-z\\x\nyb", "[b]"]},
    {name: "quotes", src: P + R"(
%%
"a+b"   printf("<plus>");
"\n"    printf("<nl>");
"\x41""1" printf("<A1>");
"\101"  printf("<A>");
"ab"*c  printf("<abc:%s>", yytext);
\"      printf("<q>");
"/"     printf("<slash>");
)", inputs: ["a+bab", "A1A", "ababc c\n\"/", "abab"]},
    {name: "escapes", src: P + R"(
%%
\t      printf("<tab>");
\x20    printf("<sp>");
\040\041 printf("<sp!>");
\.      printf("<dot>");
\*\+\?  printf("<ops>");
\a|\b|\f|\v|\r printf("<ctl>");
\q      printf("<q>");
)", inputs: ["\t \x20!.*+?\a\b\f\v\rq", " !!"]},
    {name: "dot", src: P + R"(
%%
.+      printf("<%s>", yytext);
\n      printf("|\n");
)", inputs: ["abc\ndef\n\n", "x\x00y\n"]},
    {name: "interval-posix", src: P + R"(
%%
ab{2}       printf("<ab2:%s>", yytext);
c{2,3}      printf("<c23:%s>", yytext);
d{2,}       printf("<d2+:%s>", yytext);
(ef){1,2}g  printf("<efg:%s>", yytext);
x|y{3}      printf("<xy3:%s>", yytext);
.|\n        printf(".");
)", inputs: ["ababcccccdddefefgxyyyi", "abbccdd", "efgefefefg hi i yyy"]},
    {name: "definitions", src: P + R"(
D       [0-9]
L       [a-z]
WORD    {L}({L}|{D})*
OPT     x?
%%
foo{OPT}    printf("<foo%s>", yytext + 3);
{WORD}      printf("<w:%s>", yytext);
{D}+        printf("<n:%s>", yytext);
"{D}"       printf("<lit>");
[{D}]       printf("<br:%s>", yytext);
)", inputs: ["foo foox fooxx a1b2 123 {D} D }", ""]},
    {name: "bol", src: P + R"(
%%
^a      printf("<^a>");
a       printf("<a>");
^b$     printf("<^b$>");
b$      printf("<b$>");
\n      printf("<nl>\n");
)", inputs: ["aaa\na\nb\nbb\nab\n b\nb", "b\n"]},
    {name: "anchors-both", src: P + R"(
%%
^abc|def$   printf("<%s>", yytext);
.|\n        ECHO;
)", inputs: ["abc\ndef\nxabc def\nabcdef\ndefx\nabc", "def"]},
    {name: "trailing-fixed", src: P + R"(
%%
ab/cd       printf("<ab>");
x+/y        printf("<x+:%s>", yytext);
z/w+        printf("<z>");
[0-9]+/"."[0-9] printf("<int:%s>", yytext);
.|\n        ECHO;
)", inputs: ["abcd abce xxxy xx zwww zz 12.5 12. 3.x", "abcabcd"]},
    {name: "trailing-variable", src: P + R"(
%%
a+/b+       printf("<a:%s>", yytext);
[0-9]+/[a-z]+\. printf("<n:%s>", yytext);
.|\n        ECHO;
)", inputs: ["aaabbb ab aa b 123abc. 12ab", "aab"]},
    {name: "dollar", src: P + R"(
%%
end$        printf("<END>");
end         printf("<end>");
[a-z]+$     printf("<w$:%s>", yytext);
)", inputs: ["end\nend end\nfoo\nfoo bar\nend", "endx\n"]},
    {name: "start-inclusive", src: P + R"(
%s Q
%%
\"          { BEGIN Q; printf("<open>"); }
<Q>\"       { BEGIN INITIAL; printf("<close>"); }
<Q>[a-z]+   printf("<q:%s>", yytext);
[a-z]+      printf("<w:%s>", yytext);
<INITIAL>[0-9]+ printf("<n:%s>", yytext);
)", inputs: ["ab \"cd ef\" gh 12 \"34\"", "\"x"]},
    {name: "start-exclusive", src: P + R"(
%x COMMENT STR
%%
"/*"            BEGIN COMMENT;
<COMMENT>"*/"   BEGIN 0;
<COMMENT>.|\n   ;
\"              { BEGIN STR; printf("STR["); }
<STR>\\.        printf("%c", yytext[1]);
<STR>\"         { BEGIN INITIAL; printf("]"); }
<STR>.          ECHO;
<*>x            printf("<x>");
[a-z]+          printf("<%s>", yytext);
)", inputs: ["a /* bxc */ d \"e\\\"fx\" g", "q/*x*/r", "\"unterminated"]},
    {name: "start-list", src: P + R"(
%s A B
%x C
%%
<A,B>q      printf("<AB-q>");
<A>z        { printf("<A-z>"); BEGIN B; }
<B>z        { printf("<B-z>"); BEGIN C; }
<C>z        { printf("<C-z>"); BEGIN INITIAL; }
a           BEGIN A;
<C>.        printf("<C:%s>", yytext);
.           printf("<%s>", yytext);
)", inputs: ["qaqzqzqzqzq", "azzzz"]},
    {name: "bol-in-condition", src: P + R"(
%x X
%%
^x          { printf("<X>"); BEGIN X; }
<X>^y       { printf("<Y>"); BEGIN INITIAL; }
<X>.|\n     printf("{%s}", yytext[0] == '\n' ? "nl" : yytext);
.|\n        ECHO;
)", inputs: ["x\ny\nxy\n y\ny", "ax\nx\n"]},
    {name: "reject", src: P + R"(
%%
abc     { printf("<abc>"); REJECT; }
ab      { printf("<ab>"); REJECT; }
a       { printf("<a>"); }
b       { printf("<b>"); REJECT; }
.|\n    printf("{%s}", yytext);
)", inputs: ["abcab", "b", "xabc\n"]},
    {name: "reject-count", src: P + R"(
%{
int words, chars;
%}
%%
[a-z]+  { words++; REJECT; }
[a-z]   { chars++; }
.|\n    ;
%%
int yywrap(void) { printf("%d %d\n", words, chars); return 1; }
)", inputs: ["the quick brown fox", ""]},
    {name: "yymore", src: P + R"(
%%
a+      { printf("<a:%s>", yytext); yymore(); }
b+      { printf("<b:%s:%d>", yytext, yyleng); }
c       { yymore(); }
d       { printf("<d:%s>", yytext); }
.|\n    ECHO;
)", inputs: ["aabbabccd", "aa\nb", "cccd ab"]},
    {name: "yyless", src: P + R"(
%%
foobar  { printf("<%s>", yytext); yyless(3); }
foo     { printf("<foo>"); }
bar     { printf("<bar>"); }
x+      { if (yyleng > 1) { yyless(1); printf("<x1>"); } else printf("<x>"); }
.|\n    ECHO;
)", inputs: ["foobar foo bar xxxx", "foob"]},
    {name: "input-unput", src: P + R"(
%%
"/*"    {
            int c, prev = 0;
            while ((c = input()) != 0) {
                if (prev == '*' && c == '/')
                    break;
                prev = c;
            }
            printf("<comment>");
        }
"swap"  { unput('a'); unput('w'); unput('s'); printf("<swap>"); }
was     printf("<was>");
.|\n    ECHO;
)", inputs: ["a /* b ** c */ d /* x", "swapped swap\n"]},
    {name: "unput-many", src: P + R"(
%%
"rev"[a-z]+ { int i; char *t = strdup(yytext + 3); int n = (int)strlen(t);
              for (i = 0; i < n; i++) unput(t[i]); free(t); }
[a-z]+      printf("<%s>", yytext);
.|\n        ECHO;
)", inputs: ["revabc xy revhello\n"]},
    {name: "yywrap-files", src: R"(
%{
#include <stdio.h>
static int n;
%}
%%
[a-z]+  printf("<%s>", yytext);
%%
int yywrap(void)
{
    if (n++ < 2) {
        yyin = fopen(n == 1 ? "/dev/null" : "/etc/hostname", "r");
        printf("[wrap %d]", n);
        return yyin ? 0 : 1;
    }
    return 1;
}
)", inputs: ["abc def"]},
    {name: "return-tokens", src: R"(
%{
#include <stdio.h>
#define NUM 257
#define ID 258
%}
%%
[0-9]+      return NUM;
[a-z]+      return ID;
[ \n]       ;
.           return yytext[0];
%%
int main(void)
{
    int t;
    while ((t = yylex()) != 0)
        printf("%d:%s ", t, yytext);
    printf("end\n");
    return 0;
}
)", inputs: ["abc 12 + x9", "", "+-"]},
    {name: "array", src: P + R"(
%array
%%
[a-z]+  { printf("<%s:%d>", yytext, (int)sizeof yytext > 100); }
)", inputs: ["hello world"]},
    {name: "pointer", src: P + R"(
%pointer
%%
[a-z]+  { printf("<%s>", yytext); }
)", inputs: ["hello world"]},
    {name: "echo-default", src: P + R"(
%%
x       ECHO; ECHO;
)", inputs: ["axbxc\n"]},
    {name: "bar-action", src: P + R"(
%%
a       |
b       |
c       printf("<abc:%s>", yytext);
d       printf("<d>");
)", inputs: ["abcdx"]},
    {name: "preamble", src: P + R"(
%%
    int count = 0;
    count++;
    printf("[call %d]", count);
a       return 1;
\n      return 2;
%%
int main(void) { while (yylex()) ; return 0; }
)", inputs: ["aa\na"]},
    {name: "code-in-defs", src: R"(
  #include <stdio.h>
  static int k = 5;
%{
static int twice(int x) { return 2 * x; }
%}
%%
[0-9]   printf("%d ", twice(yytext[0] - '0') + k);
)", inputs: ["123"]},
    {name: "nul-input", src: P + R"(
%%
[a-z]+  printf("<%s>", yytext);
.       printf("{%d}", yytext[0]);
)", inputs: ["ab\x00cd", "\x00"]},
    {name: "long-token", src: P + R"(
%%
a+      printf("<%d>", yyleng);
\n      ;
)", inputs: ["a" * 100000 + "\n" + "a" * 20000]},
    {name: "many-keywords", src: P + "%%\n" + keyword_rules(split(
            "auto break case char const continue default do double else enum extern float for goto if "
            "inline int long register restrict return short signed sizeof static struct switch typedef "
            "union unsigned void volatile while")) + "\n[a-z_]+\tprintf(\"<id:%s>\", yytext);\n",
        inputs: ["int main void return if else whilex for_ ifs", "do double done"]},
    {name: "lineno", src: P + R"(
%option yylineno
%%
[a-z]+  printf("%d:%s ", yylineno, yytext);
\n      ;
)", inputs: ["a\nb c\n\nd"]},
    {name: "empty-match", src: P + R"(
%%
a*      printf("<%s>", yytext);
)", inputs: ["aab ba", "b"]},
    {name: "alt-empty", differs: "flex rejects empty alternatives", src: P + R"(
%%
x(a|)y  printf("<%s>", yytext);
(|b)c   printf("[%s]", yytext);
)", inputs: ["xyxayxby bc c"]},
    {name: "nested-ops", src: P + R"(
%%
(a|b)*c+    printf("<1:%s>", yytext);
((ab)+|d)?e printf("<2:%s>", yytext);
(x(y(z)?)?)+ printf("<3:%s>", yytext);
)", inputs: ["ababccc abe e de xyzxyx abababe"]},
    {name: "special-literal", src: P + R"(
%%
a<b>    printf("<lt>");
c^d     printf("<caret>");
e$f     printf("<dollar>");
\{x\}   printf("<bx>");
)", inputs: ["a<b>c^de$f{ {x}"]},
    {name: "begin-macro-forms", src: P + R"(
%s S1
%%
<S1>a   { printf("<S1a>"); BEGIN(INITIAL); }
a       { printf("<a:%d>", YY_START); BEGIN S1; }
)", inputs: ["aaaa"]},
    {name: "input-eof", src: P + R"(
%%
x   { int c = input(); printf("<x then %d>", c); }
)", inputs: ["ax", "xx", "x\n"]},
    {name: "yymore-then-reject-free", src: P + R"(
%%
"<"     { yymore(); }
">"     { printf("[%s]", yytext); }
[a-z]   { yymore(); }
)", inputs: ["<abc> <> >"]},
    {name: "crlf-literal", src: P + R"(
%%
\r\n    printf("<crlf>");
\n      printf("<lf>");
)", inputs: ["a\r\nb\nc\r"]},
    {name: "high-bytes", src: P + R"(
%%
[\x80-\xff]+   printf("<hi:%d>", yyleng);
[^\x80-\xff]+  printf("<lo:%d>", yyleng);
)", inputs: ["abc\xe9\xe8x\xff"]},
    {name: "hex-longest", differs: "a hex escape takes all following hex digits (the standard); flex takes two",
         src: P + R"(
%%
\x0ab   printf("<xab>");
.|\n    printf("{%s}", yytext);
)", inputs: ["\nb\xab"]},
    {name: "yymore-trailing", differs: "with %array, flex drops the yymore text when the next match has trailing context",
         src: P + R"(
%array
%%
a       { yymore(); }
[^cba]/a printf("<%s>", yytext);
.|\n    printf("{%s}", yytext);
)", inputs: ["a\na", "axa"]},
    {name: "trailing-alternation", differs: "flex misses the fixed length of an alternation of equal lengths",
         src: P + R"x(
%%
([a][b]("c")+|[abc])/("c"|b)"a"b    printf("<%s>", yytext);
.|\n    ECHO;
)x", inputs: ["abccccab"]},
];
