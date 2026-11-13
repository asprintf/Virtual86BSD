#!/bin/sh
# Copyright (c) 2026, asprintf
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its
#    contributors may be used to endorse or promote products derived from
#    this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# gen.sh - make legacy-include from legacy/*.api, with posix-parse's test
# templates:  cd regress/os-test && sh legacy/gen.sh
# Lines: "#include <h.h>" first; "[EDITION] [optional] KIND NAME[: DECL];"
# with KIND as in os-test's .api files, plus "lvalue" (a modifiable lvalue
# that may be a macro); "% text" goes before the #include of every test;
# other "#" lines are comments.

cd "$(dirname "$0")/.." || exit 1
rm -rf legacy-include/*/
for api in legacy/*.api; do
	dir=legacy-include/$(basename "$api" .api)
	mkdir -p "$dir" || exit 1
	awk -v dir="$dir" -v api="$api" '
	function fail(m) { printf "%s:%d: %s\n", api, NR, m > "/dev/stderr"; bad = 1; exit 1 }
	function nameat(sig, name,    i, n, c) {
		n = length(name)
		for (i = 1; i <= length(sig) - n + 1; i++) {
			if (substr(sig, i, n) != name) continue
			if (i > 1 && substr(sig, i - 1, 1) ~ /[A-Za-z0-9_]/) continue
			c = substr(sig, i + n, 1)
			if (c == "" || c == "(" || c == ")" || c == "[" || c == ";" ||
			    (c == " " && substr(sig, i + n + 1, 1) == "(")) return i
		}
		return 0
	}
	function subst(sig, name, repl,    i, e) {
		i = nameat(sig, name)
		if (!i) fail("no " name " in: " sig)
		e = i + length(name)
		arr = (substr(sig, e, 1) == "[")
		if (arr) { while (e <= length(sig) && substr(sig, e, 1) != "]") e++; e++ }
		return substr(sig, 1, i - 1) repl substr(sig, e)
	}
	/^#include </ { header = $2; sub(/^</, "", header); sub(/>$/, "", header); next }
	/^% / { prelude = prelude substr($0, 3) "\n"; next }
	/^#/ || /^[ \t]*$/ { next }
	{
		line = $0
		if (header == "") fail("no #include line first")
		if (!match(line, /^\[[^]]*\] /)) fail("no [EDITION]")
		ed = substr(line, 2, RLENGTH - 3); line = substr(line, RLENGTH + 1)
		sub(/;[ \t]*$/, "", line)
		sig = ""
		if ((i = index(line, ": ")) > 0) { sig = substr(line, i + 2); line = substr(line, 1, i - 1) }
		n = split(line, w, " ")
		parent = ""; incomplete = 0; optional = 0
		if (w[1] == "optional") { optional = 1; for (k = 1; k < n; k++) w[k] = w[k + 1]; n-- }
		if (w[1] == "parent") {
			kind = w[n - 1]; name = w[n]
			parent = w[2]; for (k = 3; k < n - 1; k++) parent = parent " " w[k]
		} else if (w[1] == "incomplete") {
			incomplete = 1; kind = w[2]; name = w[3]
		} else if (w[1] == "maybe_define") {
			kind = "maybe_function"; name = w[3]
		} else { kind = w[1]; name = w[2] }
		file = (parent != "") ? parent "-" name : ((kind == "struct" || kind == "union") ? kind " " name : name)
		gsub(/[ {}]/, "-", file)
		path = dir "/" file ".c"
		if (path in seen) fail("two tests named " path)
		seen[path] = 1
		if (optional) printf "/*optional*/\n" > path
		printf "/*[%s]*/\n%s#include <%s>\n", ed, prelude, header > path
		if (kind == "define") {
			printf "#ifndef %s\n#error \"%s is not defined\"\n#endif\n", name, name > path
		} else if (kind == "typedef") {
			printf "%s* foo;\n", name > path
		} else if (kind == "struct" || kind == "union") {
			printf "%s %s%s foo;\n", kind, name, (incomplete ? "*" : "") > path
		} else if (kind == "function" || kind == "maybe_function") {
			if (kind == "maybe_function") printf "#ifdef %s\n#undef %s\n#endif\n", name, name > path
			printf "%s = %s;\n", subst(sig, name, "(*foo)"), name > path
		} else if (kind == "struct_member" || kind == "union_member") {
			d = subst(sig, name, "*qux")
			printf "void foo(%s* bar)\n{\n\t%s = %sbar->%s;\n\t(void) qux;\n}\n", parent, d, (arr ? "" : "&"), name > path
		} else if (kind == "symbolic_constant") {
			d = (sig == "") ? "int foo" : subst(sig, name, "foo")
			sub(/foo/, " const foo", d)
			printf "%s = %s;\n", d, name > path
		} else if (kind == "external") {
			d = subst(sig, name, "*foo")
			printf "%s = %s%s;\n", d, (arr ? "" : "&"), name > path
		} else if (kind == "lvalue") {
			d = subst(sig, name, "*qux")
			printf "void foo(void)\n{\n\t%s = &%s;\n\t%s = *qux;\n}\n", d, name, name > path
		} else fail("unknown kind " kind)
		printf "int main(void) { return 0; }\n" > path
		close(path)
		count++
	}
	END { if (!bad) printf "%s: %d tests\n", dir, count }
	' "$api" || exit 1
done
