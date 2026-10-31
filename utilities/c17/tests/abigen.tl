/*
 * abigen.tl - a test of the calling convention: random functions with
 * random parameter and return types (scalars, complex types, structures
 * and unions passed in registers or memory, variable arguments)
 *
 *     tl abigen.tl SEED [COUNT]
 *
 * writes callee.c, which defines COUNT functions (40) that print the
 * arguments they receive and return a value, and caller.c, which calls
 * them and prints what they return.  The output must be the same
 * whichever compiler builds each half.
 */

var seed = len(argv) ? int(argv[0]) : 1;
var nfuncs = len(argv) > 1 ? int(argv[1]) : 40;
srand(seed);

var scalars = ["char", "signed char", "unsigned char", "short", "unsigned short", "int",
               "unsigned", "long", "unsigned long", "long long", "float", "double",
               "long double", "_Bool", "void *", "float _Complex", "double _Complex",
               "long double _Complex"];
var structs = [];      /* {name, fields: [{type, name, arr}]} */
var bynames = {};

func rand_struct(id, depth) {
    var kind = choice(["struct", "struct", "struct", "union"]);
    var n = rand(1, 5);
    var fields = [];
    for (var j = 0; j < n; j++) {
        var r = frand();
        if (r < 0.15 && depth < 1) {
            var sub = rand_struct(sprintf("%s_%d", id, j), depth + 1);
            push(fields, {type: sub, name: sprintf("m%d", j), arr: 0});
        } else if (r < 0.3) {
            push(fields, {type: choice(["char", "short", "int", "float", "double", "long"]),
                          name: sprintf("m%d", j), arr: rand(1, 4)});
        } else {
            var pool = ["char", "short", "int", "long", "float", "double", "unsigned char"];
            if (frand() < 0.1)
                push(pool, "long double");
            push(fields, {type: choice(pool), name: sprintf("m%d", j), arr: 0});
        }
    }
    var name = sprintf("%s S%s", kind, id);
    var s = {name: name, fields: fields, top: depth == 0};
    push(structs, s);
    bynames[name] = s;
    return name;
}
for (var i = 0; i < 12; i++)
    rand_struct(str(i), 0);
var tops = [];
for (var s : structs)
    if (s.top)
        push(tops, s.name);
var types = scalars + tops;

/* statements printing var, of type t */
func fmt_val(t, v, out) {
    if (haskey(bynames, t)) {
        for (var f : bynames[t].fields) {
            if (f.arr)
                for (var k = 0; k < f.arr; k++)
                    fmt_val(f.type, sprintf("%s.%s[%d]", v, f.name, k), out);
            else
                fmt_val(f.type, v + "." + f.name, out);
        }
        return;
    }
    if (index(t, "_Complex") >= 0)
        push(out, sprintf("printf(\" %%La,%%La\", (long double)__real__ %s, (long double)__imag__ %s);", v, v));
    else if (t == "float" || t == "double")
        push(out, sprintf("printf(\" %%a\", (double)%s);", v));
    else if (t == "long double")
        push(out, sprintf("printf(\" %%La\", %s);", v));
    else if (t == "void *")
        push(out, sprintf("printf(\" %%lx\", (unsigned long)%s);", v));
    else
        push(out, sprintf("printf(\" %%lld\", (long long)%s);", v));
}

/* statements giving var a value */
func init_val(t, v, out, salt) {
    if (haskey(bynames, t)) {
        for (var f : bynames[t].fields) {
            if (f.arr)
                for (var k = 0; k < f.arr; k++)
                    init_val(f.type, sprintf("%s.%s[%d]", v, f.name, k), out, salt * 7 + k);
            else
                init_val(f.type, v + "." + f.name, out, salt * 3 + len(f.name));
        }
        return;
    }
    var x = rand(-120, 120) + salt % 50;
    if (index(t, "_Complex") >= 0)
        push(out, sprintf("%s = %d.25; __imag__ %s = %d.5;", v, x, v, x + 3));
    else if (t == "float" || t == "double" || t == "long double")
        push(out, sprintf("%s = %d.25;", v, x));
    else if (t == "void *")
        push(out, sprintf("%s = (void *)%dUL;", v, abs(x) * 4096 + 8));
    else if (t == "_Bool")
        push(out, sprintf("%s = %d;", v, x & 1));
    else if (startswith(t, "unsigned"))
        push(out, sprintf("%s = %d;", v, abs(x)));
    else
        push(out, sprintf("%s = %d;", v, x));
}

var hdr = ["#include <stdio.h>", "#include <stdarg.h>"];
for (var s : structs) {
    var body = [];
    for (var f : s.fields)
        push(body, sprintf("%s %s%s;", f.type, f.name, f.arr ? sprintf("[%d]", f.arr) : ""));
    push(hdr, sprintf("%s { %s };", s.name, join(body, " ")));
}

var funcs = [];
for (var i = 0; i < nfuncs; i++) {
    var nparams = rand(0, 14);
    var params = [];
    for (var k = 0; k < nparams; k++)
        push(params, choice(types));
    var ret = choice(types + ["void"]);
    var variadic = nparams > 0 && frand() < 0.25;
    push(funcs, {i: i, ret: ret, params: params, variadic: variadic,
                 vstruct: variadic ? choice(tops) : nil});
}

var callee = copy(hdr), caller = copy(hdr);
for (var f : funcs) {
    var pl = [];
    for (var k = 0; k < len(f.params); k++)
        push(pl, sprintf("%s p%d", f.params[k], k));
    var plist = len(pl) ? join(pl, ", ") : "void";
    if (f.variadic)
        plist += ", ...";
    var proto = sprintf("%s f%d(%s)", f.ret, f.i, plist);
    push(callee, proto + " {");
    var body = [sprintf("printf(\"f%d:\");", f.i)];
    for (var k = 0; k < len(f.params); k++)
        fmt_val(f.params[k], sprintf("p%d", k), body);
    if (f.variadic) {
        push(body, sprintf("va_list ap; va_start(ap, p%d);", len(f.params) - 1));
        push(body, "int n = va_arg(ap, int);");
        push(body, "printf(\" |%d\", n);");
        push(body, "for (int i = 0; i < n; i++) {");
        push(body, " if (i % 3 == 0) printf(\" %d\", va_arg(ap, int));");
        push(body, " else if (i % 3 == 1) printf(\" %a\", va_arg(ap, double));");
        push(body, " else printf(\" %La\", va_arg(ap, long double)); }");
        push(body, sprintf("%s vs = va_arg(ap, %s);", f.vstruct, f.vstruct));
        fmt_val(f.vstruct, "vs", body);
        push(body, "printf(\" %d\", va_arg(ap, int));");
        push(body, "va_end(ap);");
    }
    push(body, "printf(\"\\n\");");
    if (f.ret != "void") {
        push(body, f.ret + " r;");
        init_val(f.ret, "r", body, f.i);
        push(body, "return r;");
    }
    for (var b : body)
        push(callee, "  " + b);
    push(callee, "}");
    push(caller, proto + ";");
}
push(caller, "int main(void) {");
for (var f : funcs) {
    push(caller, "  {");
    var args = [];
    for (var k = 0; k < len(f.params); k++) {
        push(caller, sprintf("    %s a%d;", f.params[k], k));
        var out = [];
        init_val(f.params[k], sprintf("a%d", k), out, f.i * 13 + k);
        for (var o : out)
            push(caller, "    " + o);
        push(args, sprintf("a%d", k));
    }
    var a = join(args, ", ");
    if (f.variadic) {
        push(caller, sprintf("    %s vs;", f.vstruct));
        var out = [];
        init_val(f.vstruct, "vs", out, f.i * 17);
        for (var o : out)
            push(caller, "    " + o);
        var nv = rand(0, 7);
        a += sprintf(", %d", nv);
        for (var j = 0; j < nv; j++)
            a += ", " + (j % 3 == 0 ? str(j * 3 + 1) : j % 3 == 1 ? sprintf("%d.5", j) : sprintf("%d.75L", j));
        a += ", vs, 4242";
    }
    if (f.ret == "void") {
        push(caller, sprintf("    f%d(%s);", f.i, a));
    } else {
        push(caller, sprintf("    %s r = f%d(%s);", f.ret, f.i, a));
        push(caller, sprintf("    printf(\"r%d:\");", f.i));
        var out = [];
        fmt_val(f.ret, "r", out);
        for (var o : out)
            push(caller, "    " + o);
        push(caller, "    printf(\"\\n\");");
    }
    push(caller, "  }");
}
push(caller, "  return 0;");
push(caller, "}");
writefile("callee.c", join(callee, "\n") + "\n");
writefile("caller.c", join(caller, "\n") + "\n");
