#!/bin/sh
# test_uucp.sh - tests for uucp (POSIX.1-2024).  Run: sh test_uucp.sh
cd "$(dirname "$0")" || exit 2
PROG_DIR=${PROG_DIR:-$PWD/../../../usr.bin/uucp}; export PROG_DIR
pass=0 fail=0
# t NAME EXPECTED-STDOUT EXPECTED-STATUS COMMAND...  (stdout compared without trailing newlines)
t() {
    _n=$1 _e=$2 _s=$3; shift 3
    _o=$("$@" 2>/dev/null); _r=$?
    if [ "$_o" = "$_e" ] && [ "$_r" = "$_s" ]; then pass=$((pass+1))
    else fail=$((fail+1)); printf 'FAIL %s\n  got:  [%s] status %s\n  want: [%s] status %s\n' "$_n" "$_o" "$_r" "$_e" "$_s"; fi
}
# ok NAME CONDITION-COMMAND...
ok() {
    _n=$1; shift
    if "$@"; then pass=$((pass+1)); else fail=$((fail+1)); printf 'FAIL %s\n' "$_n"; fi
}
done_tests() { echo "$1: $pass passed, $fail failed"; [ "$fail" = 0 ]; }
U=${U:-$PROG_DIR/uucp}
T=${TMPDIR:-/tmp}/test_uucp.$$
mkdir -m 700 "$T" || exit 2
trap 'chmod -R u+rwx "$T" 2>/dev/null; rm -rf "$T"' EXIT
cd "$T" || exit 2
export LC_ALL=C
root=false; [ "$(id -u)" = 0 ] && root=true
nl='
'
# three systems: here (this one), there (reached by "here"), far
# (reached by "there"); each has its configuration, spool and public
# directory; "down" cannot be reached
UUCICO=${UUCICO:-$PROG_DIR/uucico}
mkdir h t f h/pub t/pub f/pub t/ro
cat > h.conf <<X
nodename here
spool $T/h/spool
pubdir $T/h/pub
mail sh $T/mail.sh
system there env UUCP_CONF=$T/t.conf $UUCICO -S
system down exit 1
X
cat > t.conf <<X
nodename there
spool $T/t/spool
pubdir $T/t/pub
mail sh $T/mail.sh
read $T/t/ro
system far env UUCP_CONF=$T/f.conf $UUCICO -S
X
cat > f.conf <<X
nodename far
spool $T/f/spool
pubdir $T/f/pub
mail sh $T/mail.sh
X
printf 'printf "%%s %%s\\n" "$3" "$2" >> %s/mailbox; cat >> %s/mailbox\n' "$T" "$T" > mail.sh
UUCP_CONF=$T/h.conf; export UUCP_CONF
echo hello > a.txt; echo world > b.txt; echo secret > t/ro/r.txt
me=$(id -un 2>/dev/null || id -u)
t "to another system's public directory" 'hello' 0 sh -c '"$1" a.txt "there!~/a.txt" && cat t/pub/a.txt' sh "$U"
t "a relative path is made absolute with the current directory" 'hello' 0 sh -c '"$1" a.txt there!$PWD/t/pub/rel.txt && cat t/pub/rel.txt' sh "$U"
t "from another system" 'hello' 0 sh -c '"$1" "there!~/a.txt" back.txt && cat back.txt' sh "$U"
t "to a directory: the last part of the name" 'a.txt' 0 sh -c '"$1" a.txt "there!~/d1/" && ls t/pub/d1' sh "$U"
t "several files: the destination is a directory" 'a.txt
b.txt' 0 sh -c '"$1" a.txt b.txt "there!~/d2" && ls t/pub/d2' sh "$U"
t "a pattern expanded on the other system" 'a.txt
b.txt' 0 sh -c '"$1" "there!~/d2/*.txt" got/ && ls got' sh "$U"
t "a pattern expanded here" 'a.txt
b.txt' 0 sh -c 'mkdir pd && cp a.txt b.txt pd && "$1" "pd/*.txt" "there!~/d3/" && ls t/pub/d3' sh "$U"
t "-d (the default): directories made" 'hello' 0 sh -c '"$1" -d a.txt "there!~/x/y/z.txt" && cat t/pub/x/y/z.txt' sh "$U"
t "-f: no directories made" 'no' 1 sh -c '"$1" -f a.txt "there!~/n1/n2/z.txt" 2>/dev/null; s=$?; test -d t/pub/n1 || echo no; exit $s' sh "$U"
t "~user: the user's login directory, or the public directory" 'hello' 0 sh -c '"$1" a.txt "there!~nosuchuserxyz/u.txt" && cat t/pub/u.txt' sh "$U"
t "writing outside the public directory is refused" "uucp: there: $T/t/ro/w.txt: not permitted" 1 sh -c '"$1" a.txt there!$PWD/t/ro/w.txt 2>&1' sh "$U"
t "reading a directory of \"read\" is allowed" 'secret' 0 sh -c '"$1" there!$PWD/t/ro/r.txt r.txt && cat r.txt' sh "$U"
t "reading elsewhere is refused" "uucp: there: $T/b.txt: not permitted" 1 sh -c '"$1" there!$PWD/b.txt x.txt 2>&1' sh "$U"
t "\"..\" does not get out of the public directory" '1' 0 sh -c '"$1" a.txt "there!~/../escape.txt" 2>/dev/null; echo $?' sh "$U"
t "from this system to this system" 'hello' 0 sh -c '"$1" a.txt local.txt && cat local.txt' sh "$U"
t "this system by name, and !path" 'world' 0 sh -c '"$1" here!b.txt !copy.txt && cat copy.txt' sh "$U"
t "a route: there sends it on to far" 'hello' 0 sh -c '"$1" a.txt "there!far!~/routed.txt" && cat f/pub/routed.txt' sh "$U"
t "from one other system to another" 'hello' 0 sh -c '"$1" "there!~/a.txt" "there!~/a2.txt" && cat t/pub/a2.txt' sh "$U"
t "the files made: 0666 less the umask" '-rw-r--r--' 0 sh -c 'umask 022; "$1" a.txt "there!~/m.txt" && ls -l t/pub/m.txt | cut -c1-10' sh "$U"
t "an executable: 0777 less the umask" '-rwxr-xr-x' 0 sh -c 'umask 022; printf "#!/bin/sh\n" > x.sh; chmod 755 x.sh; "$1" x.sh "there!~/x.sh" && ls -l t/pub/x.sh | cut -c1-10' sh "$U"
t "-j: the job's id" 'there.' 0 sh -c '"$1" -j a.txt "there!~/j.txt" | sed "s/[0-9]*\$//"' sh "$U"
t "a job done is not in the spool" '' 0 sh -c '"$1" a.txt "there!~/j2.txt" && ls h/spool/jobs' sh "$U"
t "-r: queued, not done" 'no' 0 sh -c '"$1" -r a.txt "there!~/r.txt" && test ! -f t/pub/r.txt && echo no' sh "$U"
t "-r: in the spool" '1' 0 sh -c 'ls h/spool/jobs | wc -l | tr -d " "' sh "$U"
t "uucico -s: the queued job done" 'hello' 0 sh -c '"$1" -s there && cat t/pub/r.txt && test -z "$(ls h/spool/jobs)"' sh "$UUCICO"
t "-c (the default): the file read when it is sent" 'changed' 0 sh -c 'echo first > c.txt; "$1" -r -c c.txt "there!~/c.txt"; echo changed > c.txt; "$2" && cat t/pub/c.txt' sh "$U" "$UUCICO"
t "-C: the file copied to the spool" 'first' 0 sh -c 'echo first > c.txt; "$1" -r -C c.txt "there!~/cc.txt"; echo changed > c.txt; "$2" && cat t/pub/cc.txt' sh "$U" "$UUCICO"
t "-C: the copy goes when the job is done" '' 0 sh -c 'ls h/spool/data' sh "$U"
t "a system not reached: status 1, the job queued" "uucp: cannot reach down: the connection was lost; the job is queued
1" 1 sh -c 'rm -f h/spool/jobs/*; "$1" a.txt "down!~/x" 2>&1; s=$?; ls h/spool/jobs | wc -l | tr -d " "; exit $s' sh "$U"
rm -f h/spool/jobs/*
t "-m: mail to the user when it is done" "$me uucp job there.N done" 0 sh -c 'rm -f mailbox; "$1" -m a.txt "there!~/mm.txt" && head -n 1 mailbox | sed "s/[0-9][0-9]* done/N done/"' sh "$U"
t "-m: mail when it fails" "$me uucp job there.N failed" 1 sh -c 'rm -f mailbox; "$1" -m nosuch.txt "there!~/mm.txt" 2>/dev/null; s=$?; head -n 1 mailbox | sed "s/[0-9][0-9]* failed/N failed/"; exit $s' sh "$U"
t "-n user: mail to the user there" "bob uucp: a file arrived
here!$me sent $T/a.txt, now $T/t/pub/n.txt." 0 sh -c 'rm -f mailbox; "$1" -n bob a.txt "there!~/n.txt" && cat mailbox' sh "$U"
t "no mail without -m" '' 0 sh -c 'rm -f mailbox; "$1" a.txt "there!~/nm.txt" && ! test -f mailbox' sh "$U"
t "a file that is not there: status 1" "uucp: $T/nosuch.txt: No such file or directory" 1 sh -c '"$1" nosuch.txt "there!~/x" 2>&1' sh "$U"
t "a failed job is not kept" '' 0 sh -c 'ls h/spool/jobs' sh "$U"
t "an unknown system: status 1, no job" 'uucp: nowhere: unknown system' 1 sh -c '"$1" a.txt "nowhere!~/x" 2>&1; s=$?; test -z "$(ls h/spool/jobs)" || echo job; exit $s' sh "$U"
t "a configuration that cannot be read" "uucp: $T/nosuch.conf: No such file or directory" 1 sh -c 'UUCP_CONF=$PWD/nosuch.conf "$1" a.txt b.txt 2>&1' sh "$U"
t "one operand: a usage error" '' 2 "$U" a.txt
t "an unknown option: a usage error" '' 2 "$U" -x a.txt b.txt
# getopt() must not take an option after an operand for one (no permuting)
t "an option after an operand is an operand" 'hello' 0 sh -c 'cp a.txt ./-r && "$1" a.txt -r "there!~/op/" && cat t/pub/op/-r' sh "$U"
done_tests uucp
