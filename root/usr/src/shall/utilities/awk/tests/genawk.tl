/*
 * genawk.tl - random awk programs for equiv.tl.
 *
 * program(depth) returns a list of tokens; render() joins them, with or
 * without blanks between tokens (blanks matter: "f(" is a call, "f (" is
 * not, "a ++b" differs from "a++ b").  mutate() deletes, repeats, swaps
 * or inserts tokens so that syntax errors land everywhere.  grid() gives
 * small templates with every token replaced by, or preceded by, each
 * token of POOL.  EDGE is a list of hand-written programs.  The random
 * numbers are tl's (srand()).
 */

var NL = "\n";

var NAMES = ["x", "y", "n", "arr", "a", "s", "i", "k", "NF", "NR", "FS", "OFS", "ORS", "RS", "SUBSEP",
    "RSTART", "RLENGTH", "FILENAME", "ENVIRON", "ARGV", "CONVFMT"];

var FUNCS = ["f", "g", "h"];

var BUILTINS1 = ["length", "int", "sqrt", "exp", "log", "sin", "cos", "tolower", "toupper", "system",
    "close", "fflush", "srand", "rand"];

var STRINGS = [R"("a")", R"("hello world")", R"("")", R"("\t|\n")", R"("x\"y")", R"("\101\7")",
    R"("\q")", R"("a\/b")", R"("%d %s\n")", R"("%5.2f")", R"("\\")", "\"tab\\\nnext\"", R"("1e3")",
    R"(" 12 ")"];

var NUMBERS = ["0", "1", "2", "10", "3.5", ".5", "1.", "1e3", "1E-2", "1e", "1e+", "1.2.3", "007",
    "0x1A", "123456789012", "2e308", "1.5e2x"];

var REGEXES = ["/a/", "/^[a-z]+$/", "/a|b/", "/[/]/", R"(/\//)", "/[[:alpha:]]+/", "/x*/", "/[]a]/",
    "/[^]a]/", "/=x/", R"(/a\.b/)", "/(ab)+c?/", "/[[:digit:].]/", "/[[.a.]]/", R"(/\$/)", "/{/",
    "/a{2}/", R"(/\t/)"];

var ASSIGNOPS = ["=", "+=", "-=", "*=", "/=", "%=", "^=", "**="];

var RELOPS = ["<", "<=", "==", "!=", ">=", ">"];

var COMMENTS = ["# comment", "#", "# } { ) /"];


/* the state of the generator: inside how many loops, inside a function */
var gen_loop = 0, gen_infunc = false;

func p(x) {
    return frand() < x;
}

/* k different elements of l, at random */
func sample(l, k) {
    var c = copy(l);
    shuffle(c);
    return slice(c, 0, k);
}

/* ---------- expressions ---------- */

func gen_lvalue(d) {
    var r = frand();
    if (r < 0.5)
        return [choice(NAMES)];
    if (r < 0.75)
        return [choice(NAMES), "["] + gen_exprlist(d - 1, 1 + p(0.3)) + ["]"];
    return ["$"] + gen_dollar_operand(d - 1);
}

func gen_exprlist(d, n) {
    var out = [];
    for (var i = 0; i < n; i++) {
        if (i)
            out += [","] + (p(0.1) ? [NL] : []);
        out += gen_expr(d);
    }
    return out;
}

func gen_dollar_operand(d) {
    var r = frand();
    if (d > 0 && r < 0.1)
        return [choice(["++", "--"])] + gen_dollar_operand(d - 1);
    if (d > 0 && r < 0.2)
        return [choice(["-", "+", "!"])] + gen_dollar_operand(d - 1);
    return gen_primary(d);
}

func gen_primary(d) {
    var r = frand();
    if (d <= 0 || r < 0.25)
        return [choice(NUMBERS + STRINGS + NAMES + slice(REGEXES, 0, 4))];
    if (r < 0.35)
        return gen_lvalue(d);
    if (r < 0.45)
        return ["("] + (p(0.1) ? [NL] : []) + gen_expr(d - 1) + [")"];
    if (r < 0.5)
        return ["$"] + gen_dollar_operand(d - 1);
    if (r < 0.55)
        return [choice(["++", "--"])] + gen_lvalue(d - 1);
    if (r < 0.62) {
        var f = choice(FUNCS);
        return [f + "("] + gen_exprlist(d - 1, rand(0, 2)) + [")"];
    }
    if (r < 0.72) {
        var b = choice(BUILTINS1);
        if (b == "length" && p(0.3))
            return ["length"];
        var n = (b in ["rand", "srand", "fflush", "length"]) && p(0.5) ? 0 : 1;
        return [b, "("] + gen_exprlist(d - 1, n) + [")"];
    }
    if (r < 0.8) {
        var which = choice(["substr", "index", "split", "sub", "gsub", "match", "sprintf", "atan2"]);
        var args;
        if (which == "sub" || which == "gsub") {
            args = [choice(REGEXES), ","] + gen_expr(d - 1);
            if (p(0.5))
                args += [","] + gen_lvalue(d - 1);
        } else if (which == "split") {
            args = gen_expr(d - 1) + [",", choice(NAMES)];
            if (p(0.5))
                args += [",", choice(REGEXES + STRINGS)];
        } else if (which == "match")
            args = gen_expr(d - 1) + [",", choice(REGEXES)];
        else
            args = gen_exprlist(d - 1, rand(1, 3));
        return [which, "("] + args + [")"];
    }
    if (r < 0.88) {
        var g = ["getline"];
        if (p(0.5))
            g += gen_lvalue(d - 1);
        if (p(0.5))
            g += ["<"] + gen_primary(d - 1);
        return g;
    }
    if (r < 0.94)
        return ["("] + gen_exprlist(d - 1, 2) + [")", "in", choice(NAMES)];
    return [choice(["-", "+", "!"])] + gen_primary(d - 1);
}

func gen_expr(d, print_level) {
    var r = frand();
    if (d <= 0 || r < 0.25)
        return gen_primary(d);
    if (r < 0.33)
        return gen_lvalue(d - 1) + [choice(ASSIGNOPS)] + (p(0.05) ? [NL] : []) + gen_expr(d - 1);
    if (r < 0.38)
        return gen_lvalue(d - 1) + [choice(["++", "--"])];
    if (r < 0.45)
        return gen_expr(d - 1) + ["?"] + (p(0.1) ? [NL] : []) + gen_expr(d - 1) + [":"] + gen_expr(d - 1);
    if (r < 0.5)
        return gen_expr(d - 1) + [choice(["&&", "||"])] + (p(0.2) ? [NL] : []) + gen_expr(d - 1);
    if (r < 0.54)
        return gen_expr(d - 1) + ["in", choice(NAMES)];
    if (r < 0.6) {
        if (p(0.6))
            return gen_expr(d - 1) + [choice(["~", "!~"]), choice(REGEXES)];
        return gen_expr(d - 1) + [choice(["~", "!~"])] + gen_expr(d - 1);
    }
    if (r < 0.67) {
        var op = choice(print_level ? slice(RELOPS, 0, -1) : RELOPS);
        return gen_expr(d - 1) + [op] + gen_expr(d - 1);
    }
    if (r < 0.75)
        return gen_expr(d - 1) + gen_expr(d - 1);       /* concatenation */
    if (r < 0.85)
        return gen_expr(d - 1) + [choice(["+", "-", "*", "/", "%", "^", "**"])] + gen_expr(d - 1);
    if (r < 0.9)
        return [choice(["-", "+", "!"])] + gen_expr(d - 1);
    if (r < 0.95)
        return gen_expr(d - 1) + ["|", "getline"] + (p(0.5) ? gen_lvalue(d - 1) : []);
    return gen_primary(d);
}

/* ---------- statements ---------- */

func gen_term() {
    return [choice([NL, ";", ";" + NL, NL + NL])];
}

func gen_simple(d) {
    var r = frand();
    if (r < 0.3) {
        var out = [choice(["print", "printf"])];
        if (out[0] == "printf") {
            push(out, choice(slice(STRINGS, 8, 10) + [R"("%s %s\n")"]));
            if (p(0.5))
                out += [","] + gen_exprlist(d - 1, rand(1, 2));
        } else if (p(0.15))
            out += ["("] + gen_exprlist(d - 1, rand(1, 3)) + [")"];
        else if (p(0.8))
            out += gen_exprlist(d - 1, rand(1, 3));
        if (p(0.25))
            out += [choice([">", ">>", "|"]), choice([R"("/dev/null")", R"("cat >/dev/null")", "s", R"("o" n)"])];
        return out;
    }
    if (r < 0.38) {
        var out = ["delete", choice(NAMES)];
        if (p(0.7))
            out += ["["] + gen_exprlist(d - 1, 1 + p(0.3)) + ["]"];
        return out;
    }
    return gen_expr(d);
}

/* a loop statement (for r from 0.45 to 0.7), inside one more loop */
func gen_loopstmt(d, r) {
    if (r < 0.52) {
        var body = p(0.1) ? [";"] : (p(0.3) ? [NL] : []) + gen_stmt(d - 1);
        return ["while", "("] + gen_expr(d - 1) + [")"] + body;
    }
    if (r < 0.57)
        return ["do"] + (p(0.3) ? [NL] : []) + gen_stmt(d - 1) + ["while", "("] + gen_expr(d - 1) + [")"] +
               gen_term();
    if (r < 0.64) {
        var init = p(0.8) ? gen_simple(d - 1) : [];
        var cond = p(0.8) ? gen_expr(d - 1) : [];
        var inc = p(0.8) ? gen_simple(d - 1) : [];
        var body = p(0.1) ? [";"] : gen_stmt(d - 1);
        return ["for", "("] + init + [";"] + (p(0.1) ? [NL] : []) + cond + [";"] + inc + [")"] + body;
    }
    return ["for", "(", choice(NAMES), "in", choice(NAMES), ")"] + gen_stmt(d - 1);
}

func gen_stmt(d) {
    var r = frand();
    if (d <= 0 || r < 0.35)
        return gen_simple(d) + gen_term();
    if (r < 0.45)
        return ["if", "("] + gen_expr(d - 1) + [")"] + (p(0.3) ? [NL] : []) + gen_stmt(d - 1) +
               (p(0.5) ? ["else"] + (p(0.3) ? [NL] : []) + gen_stmt(d - 1) : []);
    if (r < 0.7) {
        gen_loop++;
        var out = gen_loopstmt(d, r);
        gen_loop--;
        return out;
    }
    if (r < 0.8) {
        var out = ["{"];
        for (var k = rand(0, 3); k > 0; k--)
            out += gen_stmt(d - 1);
        return out + ["}"] + (p(0.3) ? [NL] : []);
    }
    if (r < 0.85 && gen_loop)
        return [choice(["break", "continue"])] + gen_term();
    if (r < 0.9) {
        var kw = gen_infunc && p(0.6) ? "return" : choice(["exit", "next", "nextfile"]);
        var out = [kw];
        if ((kw == "return" || kw == "exit") && p(0.5))
            out += gen_expr(d - 1);
        return out + gen_term();
    }
    if (r < 0.93)
        return [";"];
    return [choice(COMMENTS), NL] + gen_simple(d) + gen_term();
}

func gen_block(d) {
    var out = ["{"];
    for (var k = rand(0, 4); k > 0; k--)
        out += gen_stmt(d);
    return out + ["}"];
}

func gen_item(d) {
    var r = frand();
    if (r < 0.15) {
        gen_infunc = true;
        var params = sample(["p", "q", "arr2", "loc"], rand(0, 3));
        var out = ["function", choice(FUNCS), "("];
        for (var i = 0; i < len(params); i++) {
            if (i)
                out += [","] + (p(0.1) ? [NL] : []);
            push(out, params[i]);
        }
        out += [")"] + (p(0.2) ? [NL] : []) + gen_block(d);
        gen_infunc = false;
        return out;
    }
    if (r < 0.3)
        return [choice(["BEGIN", "END"])] + (p(0.1) ? [NL] : []) + gen_block(d);
    if (r < 0.4)
        return gen_block(d);
    var pat = gen_expr(d - 1);
    if (p(0.15))
        pat += [","] + (p(0.2) ? [NL] : []) + gen_expr(d - 1);
    if (p(0.75))
        pat += gen_block(d);
    return pat;
}

func program(depth) {
    gen_loop = 0;
    gen_infunc = false;
    var out = [NL];                     /* never an option */
    for (var k = rand(1, 4); k > 0; k--) {
        var it = gen_item(depth);
        out += it;
        if (it[len(it) - 1] == "}" && frand() < 0.3)
            continue;                   /* items after "}" need no separator */
        push(out, choice([NL, ";", NL + NL, " " + choice(COMMENTS) + NL]));
    }
    /* define the functions that are called but not defined (mostly) */
    for (var f : FUNCS) {
        var called = (f + "(") in out;
        var defined = false;
        for (var i = 0; i + 1 < len(out); i++)
            if (out[i] == "function" && out[i + 1] == f)
                defined = true;
        if (called && !defined && frand() < 0.95)
            out += [NL, "function", f, "(", "p", ",", "q", ")", "{", "return", "p", "}", NL];
    }
    return out;
}

func isword(c) {
    return c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c >= '0' && c <= '9' || c == '_' || c == '.' ||
           c == '"';
}

func render(tokens) {
    var out = [];
    var prev = "";
    for (var t : tokens) {
        if (prev != "" && !endswith(prev, "\n") && !startswith(t, "\n")) {
            var r = frand();
            if (t != "" && isword(prev[len(prev) - 1]) && isword(t[0]) && r >= 0.7 && frand() < 0.9)
                r = 0;          /* mostly keep words apart */
            if (r < 0.7)
                push(out, " ");
            else if (r < 0.75)
                push(out, " \\\n");
            else if (r < 0.78)
                push(out, "\t");
        }
        push(out, t);
        prev = t;
    }
    return join(out, "");
}

func mutate(tokens) {
    var t = copy(tokens);
    for (var k = rand(1, 3); k > 0; k--) {
        if (!len(t))
            break;
        var i = rand(len(t));
        var r = frand();
        if (r < 0.3)
            pop(t, i);
        else if (r < 0.45)
            insert(t, i, t[i]);
        else if (r < 0.6) {
            var j = rand(len(t));
            var x = t[i];
            t[i] = t[j];
            t[j] = x;
        } else
            insert(t, i, choice(POOL));
    }
    return t;
}

var POOL = ["{", "}", "(", ")", "[", "]", ";", "\n", ",", "+", "-", "*", "/", "%", "^", "!", ">", "<",
    "|", "?", ":", "~", "!~", "$", "=", "+=", "/=", "||", "&&", "==", "++", "--", ">>", "**", "in",
    "if", "else", "while", "do", "for", "break", "continue", "next", "nextfile", "exit", "return",
    "delete", "print", "printf", "getline", "function", "BEGIN", "END", "length", "substr", "x", "f(",
    "1", R"("s")", "/r/", "/r", R"("s)", "@", "&", R"(\)", "#c", ".", "\\\n", "/=r/"];

var GRID_TEMPLATES = ["BEGIN { x = 1 ; print x }", R"(/re/ , $2 ~ "y" { n ++ } END { print n })",
    "function f ( a , b ) { return a + b } BEGIN { print f( 1 , 2 ) }",
    "BEGIN { if ( x ) y = 1 ; else { z = 2 } }",
    "BEGIN { while ( i < 3 ) i ++ ; do x -- ; while ( x > 0 ) }",
    "BEGIN { for ( i = 0 ; i < 2 ; i ++ ) print i ; for ( k in a ) delete a [ k ] }",
    R"(BEGIN { print ( 1 , 2 ) > "/dev/null" ; printf "%s" , x | "cat" })",
    R"(BEGIN { "echo" | getline v ; getline < "f" ; while ( ( getline l ) > 0 ) n ++ })",
    "BEGIN { x = a ? b : c ; y = $ ++ i ; z = - $ 1 ^ 2 ; w = ! a ~ b }",
    R"(BEGIN { s = substr( "abc" , 2 ) length ; t = ( 1 , 2 ) in a ; u = a b c })",
    "{ a [ $1 , $2 ] += 1 } END { for ( k in a ) print k , a [ k ] }",
    "NR == 1 , NR == 2 ; { next } END { exit 1 }", "BEGIN { x = 1 ;\n y = 2 &&\n z }",
    R"(BEGIN { x = $ ++ y ; z = a in b | "c" ; delete d | e })",
    R"(BEGIN { print a | "cat" ; x ++ | getline ; $ 1 ++ | getline })",
    "BEGIN { if ( x ) /r/ ; else /s/ ; while ( y ) /t/ }",
    "function f ( a ) { return a | getline } BEGIN { x = length / 2 }"];

/* [name, program text] pairs: every template token replaced by, and
   preceded by, each pool token */
func grid() {
    var res = [];
    for (var ti = 0; ti < len(GRID_TEMPLATES); ti++) {
        var toks = filter(split(replace(GRID_TEMPLATES[ti], "\n", " \n "), " "), func (x) { return x != ""; });
        push(res, [sprintf("t%d.orig", ti), join(toks, " ")]);
        for (var i = 0; i < len(toks); i++) {
            var before = slice(toks, 0, i), after = slice(toks, i + 1);
            for (var pi = 0; pi < len(POOL); pi++) {
                push(res, [sprintf("t%d.rep%d.%d", ti, i, pi), join(before + [POOL[pi]] + after, " ")]);
                push(res, [sprintf("t%d.ins%d.%d", ti, i, pi), join(before + [POOL[pi]] + slice(toks, i), " ")]);
            }
            push(res, [sprintf("t%d.del%d", ti, i), join(before + after, " ")]);
            /* no blank before the token */
            if (i)
                push(res, [sprintf("t%d.tight%d", ti, i), join(slice(toks, 0, i - 1), " ") + " " + toks[i - 1] +
                           toks[i] + " " + join(after, " ")]);
        }
    }
    return res;
}

/* hand-written programs for what the generator does not make: bytes
   outside ASCII, NULs, carriage returns, ends in the middle of a token,
   continuations in odd places, and the corners of regular expressions */
var EDGE = [
    "", " ", "\n", "#", "# only a comment", "\\", "\\\n", "\\\r\n", "\r\n",
    "BEGIN{print 1}\r\n", "BEGIN{print 1}\r", "BEGIN\r\n{print 1}",
    "BEGIN{print \"a\x00b\"}", "BEGIN{print 1}\x00", "\x00", "BEGIN{x=1 # c\x00c\nprint x}",
    "BEGIN{print /a\x00b/}", "BEGIN{caf\xe9=1; print caf\xe9}", "BEGIN{\xe9=2; print \xe9}",
    "BEGIN{x\xe9y=3}", "BEGIN{print \"\xe9\"}", "BEGIN{f\xe9(1)}",
    "function f\xe9(a){return a} BEGIN{print f\xe9(2)}",
    "BEGIN{print \"abc", "BEGIN{print \"abc\\", "BEGIN{print \"abc\\\n", "BEGIN{print \"a\\\nb\"}",
    "BEGIN{print \"a\nb\"}", "BEGIN{print /abc", "BEGIN{print /abc\\", "BEGIN{print /a\\/b/}",
    "BEGIN{print /a\\\nb/}", "BEGIN{print /[/]/}", "BEGIN{print /[[:alpha:]/]/}",
    "BEGIN{print /[[:alpha:/]/}", "BEGIN{print /[[:alpha/:]]/}", "BEGIN{print /[]/]/}",
    "BEGIN{print /[^]/]/}", "BEGIN{print /[[.a.]/]/}", "BEGIN{print /[[=a=]/]/}",
    "BEGIN{print /[/", "BEGIN{print /[[:a", "BEGIN{x=/=/; print x}", "BEGIN{x/=2; print x}",
    "BEGIN{x=4; x/=2; print x}", "BEGIN{print 1/2/1}", "BEGIN{print (1)/2/(1)}",
    "BEGIN{if (1) /x/; print \"ok\"}", "BEGIN{while (0) /x/; print \"ok\"}",
    "BEGIN{for (;0;) /x/; print \"ok\"}", "BEGIN{for (k in a) /x/; print \"ok\"}",
    "BEGIN{print length/2}", "BEGIN{getline/2}", "BEGIN{x[1]/2}", "BEGIN{x++/2}",
    "BEGIN{print $/x/}", "BEGIN{print 1 /x/}", "BEGIN{print \"a\" /x/}",
    "BEGIN { \"echo a\" |getline; print }", "BEGIN { \"echo a\" |\\\ngetline; print }",
    "BEGIN { \"echo a\" | getline x; print x }", "BEGIN { \"echo a\" | getlinex; print }",
    "BEGIN { print \"x\" | getline }", "BEGIN { print | getline }", "BEGIN { printf | getline }",
    "BEGIN { print > \"/dev/null\" | getline }", "BEGIN { \"echo a\" | getline < \"f\" }",
    "BEGIN{}END{}", "BEGIN{}\\\nEND{}", "BEGIN{} END{}", "BEGIN{}\tEND{}", "BEGIN{}\fEND{}",
    "BEGIN{}\vEND{}", "BEGIN{}\rEND{}", "BEGIN{}}", "BEGIN{} }", "BEGIN{}{}", "BEGIN{}x",
    "{}x", "x{}y", "x y", "x\ny", "x;y", "x }", "x )", "x {", "{ x", "{ x;", "{ x\n",
    "{ if (x) y", "{ do x", "{ do x;", "{ do x; while", "{ do x; while (y)", "{ while (x)\n",
    "{ print", "{ printf", "{ printf }", "{ printf; }", "{ printf > \"x\" }", "{ exit", "{ exit }",
    "{ exit ) }", "{ return }", "function f(){ return }", "function f(){ return ) }",
    "{ break }", "{ while(1) break }", "{ while(1) { break } }", "{ for(;;) continue }",
    "function f(a,){} BEGIN{f(1)}", "function f(,a){}", "function f(a a){}", "function f(a,a){}",
    "function f(f){}", "function f(){} function f(){}", "function f()\n\n{}", "function f() x",
    "function (a){}", "function f", "function f(", "function f(a", "function f(a,", "func f(){}",
    "function f(a){ a(1) }", "function f(a){ a[1]; return a } BEGIN { f(x) }",
    "BEGIN { f = 1 } function f(){}", "BEGIN { f() }", "BEGIN { g() } function f(){}",
    "BEGIN { $++1 }", "BEGIN { $++x }", "BEGIN { $--$x }", "BEGIN { $++1 @ }", "BEGIN { ++1 }",
    "BEGIN { ++$1 }", "BEGIN { x++ ++ }", "BEGIN { 1 ++ y }", "BEGIN { x ++y }", "BEGIN{x+++y}",
    "BEGIN { $x++ }", "BEGIN { $$0 }", "BEGIN { $-1 }", "BEGIN { $!x }", "BEGIN { -2^2 }",
    "BEGIN { 2^-1 }", "BEGIN { 2^3^2 }", "BEGIN { !x^2 }", "BEGIN { ! x ~ y }",
    "BEGIN { a < b < c }", "BEGIN { a ~ b ~ c }", "BEGIN { a in b in c }", "BEGIN { (a,b) in c }",
    "BEGIN { ((a,b)) }", "BEGIN { (a,b) }", "BEGIN { print (a,b) }", "BEGIN { print (a,b), c }",
    "BEGIN { print (a,b) > \"/dev/null\" }", "BEGIN { print (a,b) c }", "BEGIN { x = (a,b) }",
    "BEGIN { print (a,b)(c,d) }", "BEGIN { (a,b) / 2 }", "BEGIN { for ((k) in a) ; }",
    "BEGIN { for (k in a in b) ; }", "BEGIN { for ((i,j) in a) ; }", "BEGIN { for (print; ; ) break }",
    "BEGIN { for (print ) ; }", "BEGIN { for (;;print) break }", "BEGIN { for (\n;;) ; }",
    "BEGIN { for (;\n;) break }", "BEGIN { for (;;\n) break }", "BEGIN { if (\nx) ; }",
    "BEGIN { x = 1 ? 2 : 3 = 4 }", "BEGIN { x = 1 ?\n2\n:\n3 }", "BEGIN { x = a &&\n b ||\n c }",
    "BEGIN { f(\n1\n,\n2\n) } function f(a,b){}", "BEGIN { x[1,\n2] }", "BEGIN { x[\n1] }",
    "BEGIN { delete x[1,\n2] }", "BEGIN { delete x[\n1] }", "BEGIN { delete f(1) }", "BEGIN { delete }",
    "BEGIN { delete x[] }", "BEGIN { length length }", "BEGIN { length() }", "BEGIN { length 1 }",
    "BEGIN { substr }", "BEGIN { substr (\"abc\", 2) }", "BEGIN { getline x y }",
    "BEGIN { getline < \"a\" \"b\" }", "BEGIN { getline < \"a\" < 2 }", "BEGIN { getline x < \"a\" * 2 }",
    "BEGIN { getline $1 < \"a\" }", "BEGIN { getline $1++ }", "BEGIN { getline x = 5 }",
    "BEGIN { print > \"a\" > \"b\" }", "BEGIN { print >> \"/dev/null\" \"x\" }", "BEGIN { print a >= b }",
    "BEGIN { print a > b ? c : d }", "BEGIN { print x ? (a,b) : c }", "BEGIN { print a = b > \"/dev/null\" }",
    "BEGIN { if (1) print; else print }", "BEGIN { if (1) print else print }",
    "BEGIN { if (1) print;; else print }", "BEGIN { if (1) {print}\n\nelse print 2 }",
    "BEGIN { if (1) print 1\n\n\nelse print 2 }", "BEGIN { if (1) ; else ; }",
    "BEGIN { while (0)\n;\nprint \"x\" }", "BEGIN { do ; while (0) }", "BEGIN { do\n\nx++\n\nwhile (x < 3) }",
    "BEGIN { do x++ while (x < 3) }", "BEGIN { do {x++} while (x < 3) print x }",
    "BEGIN { 1e; 1e+; 1.2.3; .5.; 08; 0x1A; 1.5e2x }", "BEGIN { print 1e }", "BEGIN { print .}",
    "BEGIN { print 1. }", "BEGIN { x = \"\\q\\/\\\"\\101\\7777\" ; print x }",
    "BEGIN { print a !b }", "BEGIN { print a ! b }", "BEGIN { print a - b }", "BEGIN { print a -b }",
    "BEGIN { print a \" \" -b }", "BEGIN { print 1 \" \" +2 }", "BEGIN { x = y\n= 2 }",
    "BEGIN { x =\n 2; print x }", "BEGIN { print 1,\n2 }", "BEGIN { print 1\n, 2 }",
    "NR==1,\nNR==2", "NR==1,", "NR==1, {", "BEGIN\n{ print 1 }", "BEGIN\n\nx", "END", "BEGIN",
    "BEGIN {} ; ; END {}", ";;;", "\n\n;\n", "{ ;;; }", "{ \n\n }", "{ ; x }",
    "@", "&", "BEGIN { x & y }", "BEGIN { a && }", "BEGIN { a || }", "BEGIN { ? }",
    "BEGIN{print 1}" * 3, "{" * 50 + "}" * 50, "BEGIN{x=" + "(" * 200 + "1" + ")" * 200 + "}",
    "BEGIN{x=1" + "+1" * 2000 + "}", "BEGIN{x=\"" + "a" * 100000 + "\"}",
];

