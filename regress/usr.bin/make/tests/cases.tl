/*
 * cases.tl - the cases of difftest.tl.  run is a shell script (sh); $M is
 * the make under test; T N files... sets the modification time of files to
 * N seconds.  differs lists reference implementations whose behaviour
 * differs for a documented reason (the case is then skipped for that
 * reference).
 */

var P = R"(# T SECONDS file...: set the times to SECONDS (a fraction allowed) after the
# Epoch, in the form of POSIX touch -d (UTC)
T() {
    _d=$(awk -v s="$1" 'BEGIN {
        f = ""; if (index(s, ".")) { f = substr(s, index(s, ".")); s = substr(s, 1, index(s, ".") - 1) }
        days = int(s / 86400); r = s - days * 86400
        z = days + 719468; era = int(z / 146097); doe = z - era * 146097
        yoe = int((doe - int(doe / 1460) + int(doe / 36524) - int(doe / 146096)) / 365)
        y = yoe + era * 400; doy = doe - (365 * yoe + int(yoe / 4) - int(yoe / 100))
        mp = int((5 * doy + 2) / 153); d = doy - int((153 * mp + 2) / 5) + 1
        m = mp < 10 ? mp + 3 : mp - 9; if (m <= 2) y++
        printf "%04d-%02d-%02dT%02d:%02d:%02d%sZ", y, m, d, int(r / 3600), int(r % 3600 / 60), r % 60, f }')
    shift
    touch -d "$_d" "$@"
}
)";

/* c(name, run[, files[, opts]]): opts may have setup, differs and env */
func c(name, run, files, opts) {
    var d = {name: name, run: P + run, files: files != nil ? files : {},
             setup: "", differs: []};
    if (opts != nil)
        for (var k : opts)
            d[k] = opts[k];
    return d;
}

var CASES = [
    c("basic-rebuild",
      "\n"
      "T 100 a.in b.in; $M; echo rc=$?; $M; echo rc=$?; T 300 b.in; $M; echo rc=$?\n",
      {"Makefile": ".POSIX:\n"
                   "out: a.mid b.mid\n"
                   "\tcat a.mid b.mid > out\n"
                   "a.mid: a.in\n"
                   "\tcp a.in a.mid\n"
                   "b.mid: b.in\n"
                   "\tcp b.in b.mid\n"},
      {setup: "echo a > a.in; echo b > b.in"}),
    c("internal-macros",
      "\n"
      "mkdir -p d; T 100 d/x.h y.c; T 50 tgt.o; $M tgt.o; $M -f m2 lib.a\n",
      {"Makefile": ".POSIX:\n"
                   "tgt.o: d/x.h y.c d/x.h\n"
                   "\t@echo '@=$@ <=$< ?=$? ^=$^ +=$+ *=$*'\n"
                   "\t@echo 'D: $(@D) $(<D) $(?D) $(^D)'\n"
                   "\t@echo 'F: $(@F) $(<F) $(?F) ${^F} ${+F}'\n",
       "m2": ".POSIX:\n"
             "lib.a: lib.a(m.o)\n"
             "lib.a(m.o): m.o\n"
             "\t@echo '@=$@ %=$% <=$< ?=$?'\n"
             "m.o: ; @echo making m.o\n"}),
    c("macro-forms",
      "\n"
      "$M; $M A=cmd\n",
      {"Makefile": ".POSIX:\n"
                   "A = 1\n"
                   "I ::= $(A)\n"
                   "D = $(A)\n"
                   "K :::= $(A) $$HOME\n"
                   "S != echo  \"  shell out\" ; printf 'x\\ny\\n'\n"
                   "Q ?= first\n"
                   "Q ?= second\n"
                   "DA = a\n"
                   "DA += $(A)\n"
                   "IA ::= i\n"
                   "IA += $(A)\n"
                   "E =\n"
                   "A = 2\n"
                   "N = A\n"
                   "all:\n"
                   "\t@echo 'I=$(I) D=$(D) K=$(K) S=[$(S)] Q=$(Q) DA=$(DA) IA=$(IA) E=[$(E)] nested=$($(N)) $${A}'\n"}),
    c("macro-substitution",
      "\n"
      "$M\n",
      {"Makefile": ".POSIX:\n"
                   "L = a.c  b.c sub/c.c d.h\n"
                   "EQ = x=y\n"
                   "P = .c\n"
                   "all:\n"
                   "\t@echo '$(L:.c=.o)|$(L:%.c=obj/%.o)|$(L:sub/%=%)|$(L:%=[%])|$(L:.c=)|$(L:$P=.o)|${L:b.c=B}'\n"
                   "\t@echo '$(L:a%=%)|$(L:%.c=%)|$(L:%h=H%)|$(EMPTY:.c=.o)|$(L:x=y)'\n"}),
    c("macro-nested-names",
      "\n"
      "$M\n",
      {"Makefile": ".POSIX:\n"
                   "V1 = one\n"
                   "V2 = two\n"
                   "N = 1\n"
                   "X = V$(N)\n"
                   "all:\n"
                   "\t@echo $($(X)) $(V$(N)) ${V$(N):one=uno} $(V$(N:1=2))\n"}),
    c("macro-precedence",
      "\n"
      "export FROMENV=env OVER=env\n"
      "$M; echo --; $M OVER=cmd; echo --; $M -e; echo --; MAKEFLAGS='OVER=mf' $M; echo --; MAKEFLAGS='OVER=mf' $M OVER=cmd\n",
      {"Makefile": ".POSIX:\n"
                   "OVER = file\n"
                   "all:\n"
                   "\t@echo \"FROMENV=$(FROMENV) OVER=$(OVER) shell=$$OVER\"\n"}),
    c("comments-and-continuation",
      "\n"
      "$M\n",
      {"Makefile": ".POSIX:\n"
                   "# a comment \\\n"
                   "  that continues\n"
                   "f=  bar baz\\\n"
                   "    biz\n"
                   "X = value # a comment\n"
                   "all: one \\\n"
                   "     two\n"
                   "\techo ==$f== \"[$(X)]\" # passed to the shell\n"
                   "\techo a \\\n"
                   "\tb \\\n"
                   "c\n"
                   "one two: ; @echo $@ # stays in the command\n"
                   "\n"
                   "\t# a comment line inside commands goes to the shell\n"}),
    c("prefixes",
      "\n"
      "$M; echo rc=$?; $M -n; echo rc=$?; $M -s; echo rc=$?; $M -i bad; echo rc=$?\n",
      {"Makefile": ".POSIX:\n"
                   "all:\n"
                   "\techo shown\n"
                   "\t@echo hidden\n"
                   "\t-false\n"
                   "\t@-false\n"
                   "\t-@echo both\n"
                   "\t+echo plus\n"
                   "bad:\n"
                   "\tfalse\n"
                   "\techo after\n"}),
    c("errors-and-k",
      "\n"
      "$M; echo rc=$?; $M -k; echo rc=$?; $M -k -S; echo rc=$?; $M -i; echo rc=$?\n",
      {"Makefile": ".POSIX:\n"
                   "all: a b c\n"
                   "a: ; false\n"
                   "b: ; echo b\n"
                   "c: a ; echo c\n"}),
    c("ignore-and-silent-targets",
      "\n"
      "$M; echo rc=$?\n",
      {"Makefile": ".POSIX:\n"
                   ".IGNORE: x\n"
                   ".SILENT: y\n"
                   "all: x y z\n"
                   "x: ; false\n"
                   "y: ; echo y\n"
                   "z: ; echo z\n"}),
    c("global-silent-ignore",
      "\n"
      "$M; echo rc=$?\n",
      {"Makefile": ".POSIX:\n"
                   ".SILENT:\n"
                   ".IGNORE:\n"
                   "all: ; false; echo still\n"}),
    c("dash-q",
      "\n"
      "T 100 src; T 200 out; $M -q; echo rc=$?; T 300 src; $M -q; echo rc=$?; cat out; $M -q nosuch; echo rc=$?\n",
      {"Makefile": ".POSIX:\n"
                   "out: src\n"
                   "\techo made > out\n"
                   "\t+echo plus line\n"},
      {setup: "echo x > src; echo y > out"}),
    c("dash-t",
      "\n"
      "T 100 src; T 50 out; $M -t; echo rc=$?; cat out; $M; $M -t ph; ls ph 2>&1\n",
      {"Makefile": ".POSIX:\n"
                   ".PHONY: ph\n"
                   "out: src\n"
                   "\techo made > out\n"
                   "\t+echo plus\n"
                   "ph: ; echo phony\n"},
      {setup: "echo x > src; echo old > out"}),
    c("dash-n-recursive",
      "\n"
      "$M -n; echo rc=$?; ls made 2>&1\n",
      {"Makefile": ".POSIX:\n"
                   "all:\n"
                   "\t@echo outer\n"
                   "\t$(MAKE) -f sub.mk\n",
       "sub.mk": "inner:\n"
                 "\ttouch made\n"
                 "\t@echo inner done\n"}),
    c("recursion-makeflags",
      "\n"
      "$M -k -s A=1 'B=x y'; $M -n -f Makefile -j 1 t2\n",
      {"Makefile": "all:\n"
                   "\t@$(MAKE) -f sub.mk\n"
                   "t2:\n"
                   "\t@$(MAKE) -f sub.mk show\n",
       "sub.mk": "show:\n"
                 "\t@echo \"A=$(A) B=$(B)\"\n"
                 "\techo 'echoed unless -s'\n"}),
    c("inference-builtin-c",
      "\n"
      "$M prog; ./prog; echo rc=$?; $M prog; T 100 prog; $M prog\n",
      {"prog.c": "int main(void){return 3;}\n",
       "Makefile": ".POSIX:\n"
                   "CC = cc\n"
                   "CFLAGS = -O\n"}),
    c("inference-c-o",
      "\n"
      "$M; $M; T 100 b.o prog; $M\n",
      {"a.c": "int main(void){return 0;}\n",
       "b.c": "int b;\n",
       "Makefile": ".POSIX:\n"
                   "CC = cc\n"
                   "CFLAGS =\n"
                   "prog: a.o b.o\n"
                   "\t$(CC) -o $@ a.o b.o\n"}),
    c("inference-custom-suffixes",
      "\n"
      "T 100 x.in y.in; $M; echo; $M -r 2>&1; echo rc=$?\n",
      {"Makefile": ".POSIX:\n"
                   ".SUFFIXES: .in .out .txt\n"
                   "all: x.out y.txt z\n"
                   ".in.out:\n"
                   "\t@echo \"in->out $< $@ $*\"; cp $< $@\n"
                   ".out.txt:\n"
                   "\t@echo \"out->txt $<\"\n"
                   ".in.txt:\n"
                   "\t@echo \"in->txt $< $@ $* $?\"; cp $< $@\n"
                   ".in:\n"
                   "\t@echo \"single $< $@ $*\"; cp $< $@\n"},
      {setup: "echo x > x.in; echo y > y.in; echo z > z.in"}),
    c("inference-suffix-order",
      "\n"
      "T 100 f.a1 f.a2; $M f.b; $M -f m2 f.b\n",
      {"Makefile": ".POSIX:\n"
                   ".SUFFIXES: .b .a1 .a2\n"
                   ".a1.b: ; @echo from a1\n"
                   ".a2.b: ; @echo from a2\n",
       "m2": ".POSIX:\n"
             ".SUFFIXES:\n"
             ".SUFFIXES: .b .a2 .a1\n"
             ".a1.b: ; @echo from a1\n"
             ".a2.b: ; @echo from a2\n"},
      {setup: "touch f.a1 f.a2"}),
    c("inference-with-target-prereqs",
      "\n"
      "T 100 foo.c; T 200 foo.o; T 300 foo.h; $M foo.o; T 400 foo.c; $M foo.o\n",
      {"Makefile": ".POSIX:\n"
                   ".SUFFIXES: .c .o\n"
                   "foo.o: foo.h\n"
                   ".c.o:\n"
                   "\t@echo '<=$< ?=$? ^=$^'\n"},
      {setup: "touch foo.c foo.o foo.h"}),
    c("empty-inference-rule",
      "\n"
      "T 100 a.x; $M a.y; echo rc=$?\n",
      {"Makefile": ".POSIX:\n"
                   ".SUFFIXES: .x .y\n"
                   ".x.y: ;\n"},
      {setup: "touch a.x"}),
    c("default-target",
      "\n"
      "$M; echo rc=$?; $M -f m2; echo rc=$?\n",
      {"Makefile": ".POSIX:\n"
                   ".SUFFIXES: .q .r\n"
                   ".q.r: ; @echo infer\n"
                   ".PHONY: first\n"
                   "first: ; @echo first\n"
                   "second: ; @echo second\n",
       "m2": ".POSIX:\n"}),
    c("dot-default",
      "\n"
      "$M; echo rc=$?\n",
      {"Makefile": ".POSIX:\n"
                   "all: missing exists\n"
                   ".DEFAULT:\n"
                   "\t@echo \"default for $@ <$<>\"\n"},
      {setup: "touch exists"}),
    c("no-rule",
      "\n"
      "$M; echo rc=$?; $M nosuch; echo rc=$?; $M -k; echo rc=$?\n",
      {"Makefile": ".POSIX:\n"
                   "all: a missing b\n"
                   "a: ; @echo a\n"
                   "b: ; @echo b\n"}),
    c("phony",
      "\n"
      "touch clean all; $M; $M clean; $M -f m2 x\n",
      {"Makefile": ".POSIX:\n"
                   ".PHONY: all clean\n"
                   "all: ; @echo all runs\n"
                   "clean: ; @echo clean runs\n",
       "m2": ".POSIX:\n"
             ".PHONY: p\n"
             "x: p ; @echo x runs\n"
             "p: ; @echo p runs\n"},
      {setup: "touch x"}),
    c("wait-serial",
      "\n"
      "$M\n",
      {"Makefile": ".POSIX:\n"
                   "all: one .WAIT two three\n"
                   "one two three: ; @echo $@\n"}),
    c("multiple-targets",
      "\n"
      "$M a b; $M -f m2 c\n",
      {"Makefile": ".POSIX:\n"
                   "a b: dep\n"
                   "\t@echo \"build $@ from $?\"\n"
                   "dep: ; @echo dep\n",
       "m2": ".POSIX:\n"
             "c: p1\n"
             "c: p2\n"
             "c: p3\n"
             "\t@echo $^\n"
             "p1 p2 p3: ; @echo $@\n"}),
    c("include",
      "\n"
      "$M; echo rc=$?; $M -f m2; echo rc=$?; $M -f m3; echo rc=$?\n",
      {"Makefile": ".POSIX:\n"
                   "include inc1.mk inc2.mk\n"
                   "-include nosuch.mk\n"
                   "all: ; @echo $(A) $(B)\n",
       "inc1.mk": "A = from-inc1\n",
       "inc2.mk": "B = from-inc2\n"
                  "include inc3.mk\n",
       "inc3.mk": "C = c\n",
       "m2": ".POSIX:\n"
             "include nosuch.mk\n"
             "all: ; @echo not reached\n",
       "m3": ".POSIX:\n"
             "FILES = inc1.mk inc2.mk # comment\n"
             "include $(FILES)\n"
             "all: ; @echo $(A) $(B) $(C)\n"}),
    c("include-generated",
      "\n"
      "$M; echo rc=$?; $M; echo rc=$?\n",
      {"Makefile": ".POSIX:\n"
                   "gen.mk: gen.src\n"
                   "\techo 'G = generated' > gen.mk\n"
                   "include gen.mk\n"
                   "all: ; @echo $(G)\n"},
      {setup: "touch gen.src"}),
    c("library-members",
      "\n"
      "$M; echo rc=$?; ar t lib.a; $M; echo rc=$?\n",
      {"a.c": "int a(void){return 1;}\n",
       "b.c": "int b(void){return 2;}\n",
       "Makefile": ".POSIX:\n"
                   "CC = cc\n"
                   "CFLAGS =\n"
                   "ARFLAGS = -rUc\n"
                   "lib.a: lib.a(a.o) lib.a(b.o)\n"
                   "\t@echo $@ is up to date\n"}),
    c("library-member-rule",
      "\n"
      "$M; ar t lib.a; $M\n",
      {"Makefile": ".POSIX:\n"
                   "ARFLAGS = rUc\n"
                   "lib.a: lib.a(x.o)\n"
                   "lib.a(x.o): x.o\n"
                   "\t@echo \"rule for $@ member $%\"\n"
                   "\tar rUc $@ $%\n"
                   "x.o: ; @echo made > x.o\n"}),
    c("circular",
      "\n"
      "$M; echo rc=$?\n",
      {"Makefile": "a: b ; @echo a\n"
                   "b: c ; @echo b\n"
                   "c: a ; @echo c\n"}),
    c("override-commands",
      "\n"
      "$M 2>/dev/null\n",
      {"Makefile": ".POSIX:\n"
                   "x: ; @echo first\n"
                   "x: ; @echo second\n"}),
    c("stdin-and-multiple-f",
      "\n"
      "printf 'all: ; @echo from stdin $(A)\\n' | $M -f -; printf 'A = a\\n' > one.mk; printf 'all: ; @echo $(A) two\\n' > two.mk; $M -f one.mk -f two.mk\n"),
    c("shell-macro",
      "\n"
      "printf '#!/bin/sh\\nshift $(($# - 1)); echo \"myshell ran: $1\"\\n' > myshell; chmod +x myshell\n"
      "SHELL=/bin/false $M; $M SHELL=$PWD/myshell | sed \"s,$PWD,PWD,\"\n",
      {"Makefile": "all:\n"
                   "\t@echo \"shell is $(SHELL)\"\n"}),
    c("curdir",
      "\n"
      "CURDIR=/elsewhere $M | sed \"s,$PWD,PWD,\"\n",
      {"Makefile": "all:\n"
                   "\t@echo $(CURDIR) $$CURDIR\n"}),
    c("make-macro",
      "\n"
      "$M; MAKE=mymake $M\n",
      {"Makefile": "all:\n"
                   "\t@echo \"$(MAKE)\" | sed 's,.*/,,'\n"},
      {differs: ["gnu"]}),
    c("target-without-commands",
      "\n"
      "T 100 x.h; T 200 x; $M; T 300 x.h; $M\n",
      {"Makefile": ".POSIX:\n"
                   "x: hdrs\n"
                   "\t@echo rebuild x\n"
                   "hdrs: x.h\n"},
      {setup: "touch x.h x"}),
    c("nonexistent-no-commands",
      "\n"
      "T 100 out; $M\n",
      {"Makefile": ".POSIX:\n"
                   "out: FORCE\n"
                   "\t@echo forced\n"
                   "FORCE:\n"},
      {setup: "touch out"}),
    c("double-colon",
      "\n"
      "T 100 a b; T 200 t; $M; T 300 a; $M; T 400 t; T 500 b; $M\n",
      {"Makefile": "t:: a\n"
                   "\t@echo from a: $?\n"
                   "t:: b\n"
                   "\t@echo from b: $?\n"},
      {setup: "touch a b t"}),
    c("pattern-rules",
      "\n"
      "T 100 x.src; $M x.dst; $M -f m2 out/y.o\n",
      {"Makefile": "%.dst: %.src\n"
                   "\t@echo \"pattern $< -> $@ stem $*\"\n",
       "m2": "out/%.o: src/%.c\n"
             "\t@echo \"$< -> $@\"\n"},
      {setup: "touch x.src; mkdir -p src out; touch src/y.c"}),
    c("parallel-jobs",
      "\n"
      "$M -j 4 2>&1 | sort; echo; $M -j4 -f m2 | tr '\\n' ' '\n",
      {"Makefile": "all: a b c d\n"
                   "a b c d:\n"
                   "\t@sleep 1; echo $@\n",
       "m2": "all: a b\n"
             "\t@echo all\n"
             "a: ; @sleep 1; echo a\n"
             "b: ; @echo b\n"}),
    c("parallel-wait",
      "\n"
      "$M -j 4\n",
      {"Makefile": "all: slow .WAIT after\n"
                   "slow: ; @sleep 1; echo slow\n"
                   "after: ; @echo after\n"}),
    c("notparallel",
      "\n"
      "$M -j 4\n",
      {"Makefile": ".NOTPARALLEL:\n"
                   "all: a b\n"
                   "a: ; @sleep 1; echo a\n"
                   "b: ; @echo b\n"}),
    c("parallel-error",
      "\n"
      "$M -j 3; echo rc=$?\n",
      {"Makefile": "all: a b\n"
                   "a: ; @sleep 1; echo a\n"
                   "b: ; @false\n"}),
    c("jobserver-submake",
      "\n"
      "$M -j 3 2>&1 | sort\n",
      {"Makefile": "all: s1 s2\n"
                   "s1 s2:\n"
                   "\t@$(MAKE) -f sub.mk N=$@\n",
       "sub.mk": "all: x y\n"
                 "x y:\n"
                 "\t@sleep 1; echo $(N) $@\n"}),
    c("cmdline-macro-forms",
      "\n"
      "$M 'A::=$(B)' B=b 'C:::=$$x $(B)' 'D=$(B)'\n",
      {"Makefile": "all:\n"
                   "\t@echo \"A=[$(A)] C=[$(C)] D=[$(D)]\"\n"},
      {differs: ["gnu"]}),
    c("macro-in-target-line",
      "\n"
      "$M\n",
      {"Makefile": ".POSIX:\n"
                   "T = goal\n"
                   "P = pre1 pre2\n"
                   "$(T): $(P)\n"
                   "\t@echo $@: $^\n"
                   "pre1 pre2: ; @echo $@\n"}),
    c("dollar-dollar",
      "\n"
      "$M\n",
      {"Makefile": ".POSIX:\n"
                   "all:\n"
                   "\t@echo $$$$ | tr 0-9 N | cut -c1; x=val; echo $$x\n"}),
    c("escaped-newline-command",
      "\n"
      "$M\n",
      {"Makefile": ".POSIX:\n"
                   "all:\n"
                   "\t@echo one \\\n"
                   "\t  two; \\\n"
                   "\techo three\n"}),
    c("env-exported",
      "\n"
      "export E=env; $M; $M E=cmd; echo \"after: $E\"\n",
      {"Makefile": "E = file\n"
                   "N = notexported\n"
                   "all:\n"
                   "\t@echo \"E=$$E N=[$$N]\"\n"}),
    c("up-to-date-messages",
      "\n"
      "T 100 a; T 200 b; $M b; $M c; $M -f m2\n",
      {"Makefile": "b: a ; @echo b\n"
                   "c:\n",
       "m2": "all: ;\n"},
      {setup: "touch a b"}),
    c("special-in-rule-list",
      "\n"
      "$M\n",
      {"Makefile": ".POSIX:\n"
                   "all: x\n"
                   "x: ; @echo x\n"
                   ".PHONY: x\n"
                   ".PRECIOUS: x\n"}),
    c("suffixes-cleared",
      "\n"
      "T 100 a.c; $M a.o; echo rc=$?\n",
      {"Makefile": ".POSIX:\n"
                   ".SUFFIXES:\n"},
      {setup: "touch a.c"}),
    c("sh-single-suffix",
      "\n"
      "$M script; ./script; ls -l script | cut -c1-10\n",
      {"script.sh": "echo running script\n"}),
    c("first-target-skips-inference",
      "\n"
      "$M\n",
      {"Makefile": ".POSIX:\n"
                   ".SUFFIXES: .a1 .b1\n"
                   ".a1.b1: ; @echo no\n"
                   ".c.o: ; @echo no\n"
                   "real: ; @echo real\n"}),
    c("dollar-at-in-prereq-list",
      "\n"
      "$M\n",
      {"Makefile": ".POSIX:\n"
                   "x = 1\n"
                   "all: a$(x) ; @echo $@ $<\n"
                   "a1: ; @echo a1\n"}),
    c("signal-delete",
      "\n"
      "( $M & sleep 1; kill -s TERM $! ; wait ) 2>&1; ls out 2>&1; ( $M -f m2 & sleep 1; kill -s TERM $!; wait ) 2>&1; ls keep 2>&1\n",
      {"Makefile": "out:\n"
                   "\techo partial > out; sleep 5\n",
       "m2": ".PRECIOUS: keep\n"
             "keep:\n"
             "\techo partial > keep; sleep 5\n"}),
    c("no-commands-keeps-time",
      "\n"
      "T 100 y; T 300 c; T 200 h; T 400 o; $M; echo rc=$?; T 500 y; $M; echo rc=$?; rm h; $M; echo rc=$?\n",
      {"Makefile": ".POSIX:\n"
                   "all: o\n"
                   "o: h\n"
                   "\t@echo making o; touch o\n"
                   "h: c\n"
                   "c: y\n"
                   "\t@echo making c; touch c\n"}),
    c("empty-target-list",
      "\n"
      "$M; echo rc=$?\n",
      {"Makefile": "E =\n"
                   "all: ; @echo all\n"
                   "$(E): x\n"
                   "\t@echo never\n"
                   "$(E) $(E):: y\n"})
];

/* Known, deliberate differences: case -> {reference: reason} */
var KNOWN = {
    "macro-forms": {gnu: "GNU make 4.3 has no :::="},
    "cmdline-macro-forms": {gnu: "GNU make 4.3 has no :::="},
    "make-macro": {gnu: "GNU make ignores MAKE from the environment"},
    "global-silent-ignore": {gnu: "GNU make keeps sh -e when errors are ignored by .IGNORE"},
    "dash-q": {gnu: "GNU make does not run + lines under -q; the standard requires it"},
    "inference-builtin-c": {gnu: "GNU built-in rules are written differently"},
    "inference-c-o": {gnu: "GNU built-in rules are written differently"},
    "library-members": {gnu: "GNU built-in .c.a rule differs from the standard text"},
    "sh-single-suffix": {gnu: "GNU .sh rule uses cat instead of cp"},
    "wait-serial": {gnu: "GNU make 4.3 has no .WAIT"},
    "parallel-wait": {gnu: "GNU make 4.3 has no .WAIT"},
    "multiple-targets": {gnu: "GNU make lists the prerequisites of the rule with commands first"},
    "include": {gnu: "GNU make remakes include files after reading (other diagnostic)"}
};
for (var d : CASES)
    for (var r : get(KNOWN, d.name, {}))
        if (!(r in d.differs))
            push(d.differs, r);
