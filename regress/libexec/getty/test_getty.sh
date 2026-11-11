#!/bin/sh
# test_getty.sh - tests for getty (V86BSD).  Run: sh test_getty.sh
# getty runs on a pseudo-terminal that ptydrive types on (../../lib/testlib/ptydrive.c)
# and starts a login of its own (a script that says what it was given).
cd "$(dirname "$0")" || exit 2
PROG_DIR=${PROG_DIR:-$PWD/../../../libexec/getty}; export PROG_DIR
pass=0 fail=0
t() {
    _n=$1 _e=$2 _s=$3; shift 3
    _o=$("$@" 2>/dev/null); _r=$?
    if [ "$_o" = "$_e" ] && [ "$_r" = "$_s" ]; then pass=$((pass+1))
    else fail=$((fail+1)); printf 'FAIL %s\n  got:  [%s] status %s\n  want: [%s] status %s\n' "$_n" "$_o" "$_r" "$_e" "$_s"; fi
}
ok() {
    _n=$1; shift
    if "$@"; then pass=$((pass+1)); else fail=$((fail+1)); printf 'FAIL %s\n' "$_n"; fi
}
done_tests() { echo "$1: $pass passed, $fail failed"; [ "$fail" = 0 ]; }
export LC_ALL=C
G=${G:-$PROG_DIR/getty}
T=${TMPDIR:-/tmp}/test_getty.$$
mkdir -m 700 "$T" || exit 2
trap 'rm -rf "$T"' EXIT
cc=${CC:-}
[ -n "$cc" ] || for c in c17 c99 cc; do command -v $c >/dev/null 2>&1 && { cc=$c; break; }; done
$cc -o "$T/drive" ../../lib/testlib/ptydrive.c || { echo "cannot compile ../../lib/testlib/ptydrive.c"; exit 2; }
D="$T/drive"

# the login getty runs: it says what it was given
cat > "$T/login" <<'LOGIN'
#!/bin/sh
echo "LOGIN ARGS:$*"
echo "TERM=$TERM"
LOGIN
chmod 755 "$T/login"
cat > "$T/gettytab" <<TAB
# a table
default:\\
	:im=\\r\\nBANNER %h|%t|%s|%m|%r|%%|%x\\r\\n:lm=who\\72 :lo=$T/login:sp#9600:
quiet|q:\\
	:im=:lm=> :tc=default:
std.115200|fast:\\
	:sp#115200:
slow:\\
	:to#1:tc=default:
noprompt:\\
	:lm@:tc=default:
TAB
TERM=vt100; export TERM
host=$(uname -n); sys=$(uname -s); mach=$(uname -m); rel=$(uname -r)
run() { "$D" "$@"; }   # the transcript
g() { echo "$G -f $T/gettytab"; }

# the banner and the prompt
ok "the banner has the host, line, system, machine, release, a percent" sh -c '
    "$1" '"'e:who: '"' '"'d:'"' '"'w:'"' -- "$2" -f "$3" default - | grep "^BANNER $4|[^|]*|$5|$6|$7|%|%x\$" > /dev/null' sh "$D" "$G" "$T/gettytab" "$host" "$sys" "$mach" "$rel"
t "the prompt is lm (\\72 is a colon)" "ok" 0 sh -c '"$1" "e:who: " "d:" "w:" -- "$2" -f "$3" default - | grep "^who: " > /dev/null && echo ok' sh "$D" "$G" "$T/gettytab"
t "end of file at the prompt ends getty: status 0" "[exit 0]" 0 sh -c '"$1" "e:who: " "d:" "w:" -- "$2" -f "$3" default - | tail -n 1' sh "$D" "$G" "$T/gettytab"
t "an unknown type: the default's banner" "ok" 0 sh -c '"$1" "e:who: " "d:" "w:" -- "$2" -f "$3" nosuchtype - | grep "^BANNER " > /dev/null && echo ok' sh "$D" "$G" "$T/gettytab"
t "a type whose banner is empty and prompt is >" "ok" 0 sh -c 'o=$("$1" "e:> " "d:" "w:" -- "$2" -f "$3" quiet -); case $o in "> "*) echo ok;; esac' sh "$D" "$G" "$T/gettytab"
t "lm@ cancels the prompt: the built-in one" "ok" 0 sh -c '"$1" "e:login: " "d:" "w:" -- "$2" -f "$3" noprompt - | grep "^login: " > /dev/null && echo ok' sh "$D" "$G" "$T/gettytab"
t "tc= and sp: the 115200 type gets the default's banner" "ok" 0 sh -c '"$1" "e:who: " "d:" "w:" -- "$2" -f "$3" std.115200 - | grep "^BANNER " > /dev/null && echo ok' sh "$D" "$G" "$T/gettytab"
t "a type by its alias" "ok" 0 sh -c '"$1" "e:who: " "d:" "w:" -- "$2" -f "$3" fast - | grep "^BANNER " > /dev/null && echo ok' sh "$D" "$G" "$T/gettytab"
t "no table at all: the built-in banner and prompt" "ok" 0 sh -c '"$1" "e:login: " "d:" "w:" -- "$2" -f "$3/nosuchfile" - - | grep "^$4/$5 ($6) (" > /dev/null && echo ok' sh "$D" "$G" "$T" "$sys" "$mach" "$host"

# the name goes to login
t "a name: login -- name, with the environment" "LOGIN ARGS:-- alice${nl:=
}TERM=vt100" 0 sh -c '"$1" "e:who: " "s:alice\n" "w:" -- "$2" -f "$3" default - | grep "^LOGIN\|^TERM"' sh "$D" "$G" "$T/gettytab"
t "login's status is the process's: getty is replaced by it" "[exit 0]" 0 sh -c '"$1" "e:who: " "s:bob\n" "w:" -- "$2" -f "$3" default - | tail -n 1' sh "$D" "$G" "$T/gettytab"
t "the name is echoed (the terminal does it)" "ok" 0 sh -c '"$1" "e:who: " "s:carol\n" "w:" -- "$2" -f "$3" default - | grep "^who: carol\$" > /dev/null && echo ok' sh "$D" "$G" "$T/gettytab"
t "an erase character edits the name" "LOGIN ARGS:-- dave" 0 sh -c '"$1" "e:who: " "s:dax\\x7fve\n" "w:" -- "$2" -f "$3" default - | grep "^LOGIN"' sh "$D" "$G" "$T/gettytab"
t "the kill character starts the name again" "LOGIN ARGS:-- erin" 0 sh -c '"$1" "e:who: " "s:wrong\\^Uerin\n" "w:" -- "$2" -f "$3" default - | grep "^LOGIN"' sh "$D" "$G" "$T/gettytab"
t "a name with a dot, a dash, digits" "LOGIN ARGS:-- a.b-c_9" 0 sh -c '"$1" "e:who: " "s:a.b-c_9\n" "w:" -- "$2" -f "$3" default - | grep "^LOGIN"' sh "$D" "$G" "$T/gettytab"
t "an empty line: asked again" "2" 0 sh -c '"$1" "e:who: " "s:\n" "e:who: " "d:" "w:" -- "$2" -f "$3" default - | grep -c "^who: "' sh "$D" "$G" "$T/gettytab"
t "a name starting with - is refused, asked again" "ok" 0 sh -c 'o=$("$1" "e:who: " "s:-x\n" "e:who: " "s:ok\n" "w:" -- "$2" -f "$3" default -); case $o in *"login names may not start with '"'"'-'"'"'."*"LOGIN ARGS:-- ok"*) echo ok;; esac' sh "$D" "$G" "$T/gettytab"
t "a name of 33 bytes is too long" "ok" 0 sh -c 'o=$("$1" "e:who: " "s:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n" "e:who: " "s:ok\n" "w:" -- "$2" -f "$3" default -); case $o in *"login name too long."*"LOGIN ARGS:-- ok"*) echo ok;; esac' sh "$D" "$G" "$T/gettytab"
t "32 bytes are fine" "LOGIN ARGS:-- aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" 0 sh -c '"$1" "e:who: " "s:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n" "w:" -- "$2" -f "$3" default - | grep "^LOGIN"' sh "$D" "$G" "$T/gettytab"
t "a line of 300 bytes is too long, the rest is dropped" "ok" 0 sh -c 'long=$(printf "%0300d" 0); o=$("$1" "e:who: " "s:$long\n" "e:who: " "s:ok\n" "w:" -- "$2" -f "$3" default -); case $o in *"login name too long."*"LOGIN ARGS:-- ok"*) echo ok;; esac' sh "$D" "$G" "$T/gettytab"
t "a control character: not accepted" "ok" 0 sh -c 'o=$("$1" "e:who: " "s:a\\x01b\n" "e:who: " "s:ok\n" "w:" -- "$2" -f "$3" default -); case $o in *"login name not accepted."*"LOGIN ARGS:-- ok"*) echo ok;; esac' sh "$D" "$G" "$T/gettytab"
t "^C at the prompt does not end getty" "LOGIN ARGS:-- after" 0 sh -c '"$1" "e:who: " "s:\\^C" "t:300" "s:after\n" "w:" -- "$2" -f "$3" default - | grep "^LOGIN"' sh "$D" "$G" "$T/gettytab"

# the timeout
t "to#1: no answer in a second ends getty with status 1" "[exit 1]" 0 sh -c '"$1" "e:who: " "w:" -- "$2" -f "$3" slow - | tail -n 1' sh "$D" "$G" "$T/gettytab"
t "to is not a limit on login: an answer in time" "LOGIN ARGS:-- quick" 0 sh -c '"$1" "e:who: " "s:quick\n" "w:" -- "$2" -f "$3" slow - | grep "^LOGIN"' sh "$D" "$G" "$T/gettytab"
t "(and login lives longer than the timeout)" "LOGIN ARGS:-- late" 0 sh -c 'printf "#!/bin/sh\nsleep 2\necho LOGIN ARGS:\$*\n" > "$3/slowlogin"; chmod 755 "$3/slowlogin"; sed "s|lo=[^:]*|lo=$3/slowlogin|" "$3/gettytab" > "$3/tab2"; "$1" "e:who: " "s:late\n" "w:" -- "$2" -f "$3/tab2" slow - | grep "^LOGIN"' sh "$D" "$G" "$T"

# usage and errors
t "a third argument" "usage: getty [-f gettytab] [type [line]]" 1 sh -c '"$1" a b c 2>&1' sh "$G"
t "a bad option: status 1" "" 1 "$G" -x
t "...and the usage line" "usage: getty [-f gettytab] [type [line]]" 0 sh -c '"$1" -x 2>&1 | grep "^usage"' sh "$G"
t "stdin is not a terminal" "getty: stdin: not a terminal" 1 sh -c '"$1" -f "$2" default - < /dev/null 2>&1' sh "$G" "$T/gettytab"
t "a line with a slash" "getty: ../x: not a terminal line name" 1 sh -c '"$1" -f "$2" default ../x 2>&1' sh "$G" "$T/gettytab"
t "a hidden line" "getty: .x: not a terminal line name" 1 sh -c '"$1" -f "$2" default .x 2>&1' sh "$G" "$T/gettytab"
t "a line that is not there" "getty: /dev/nosuchline: No such file or directory" 1 sh -c '"$1" -f "$2" default nosuchline 2>&1' sh "$G" "$T/gettytab"
t "a login program that is not there" "ok" 0 sh -c 'sed "s|lo=[^:]*|lo=$2/nologin-here|" "$2/gettytab" > "$2/tab3"; o=$("$1" "e:who: " "s:x\n" "w:" -- "$3" -f "$2/tab3" default -); case $o in *"getty: $2/nologin-here: No such file or directory"*"[exit 1]") echo ok;; esac' sh "$D" "$T" "$G"
done_tests getty
