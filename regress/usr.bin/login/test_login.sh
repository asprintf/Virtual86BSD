#!/bin/sh
# test_login.sh - tests for login (V86BSD).  Run as root, inside V86BSD:
#   sh test_login.sh
# login runs on a pseudo-terminal that ptydrive types on
# (../../lib/testlib/ptydrive.c); the users, /etc/ttys, /etc/nologin and
# /etc/motd are made for the test and put back.
cd "$(dirname "$0")" || exit 2
PROG_DIR=${PROG_DIR:-$PWD/../../../usr.bin/login}; export PROG_DIR
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
L=${L:-$PROG_DIR/login}
if [ "$(uname -s)" != V86BSD ] || [ "$(id -u)" != 0 ]; then
    echo "login: 0 passed, 0 failed (skipped: needs root on V86BSD)"
    exit 0
fi
T=/var/tmp/test_login.$$
mkdir -m 755 "$T" || exit 2
mkdir "$T/save" "$T/home" "$T/home/tuser"
files="/etc/master.passwd /etc/group /etc/ttys /etc/nologin /etc/motd"
for f in $files; do [ -e "$f" ] && cp -p "$f" "$T/save/$(basename "$f")"; done
[ -e /var/run/utmp ] || { : > /var/run/utmp; chmod 644 /var/run/utmp; }
cleanup() {
    for f in $files; do
        if [ -e "$T/save/$(basename "$f")" ]; then cp -p "$T/save/$(basename "$f")" "$f"; else rm -f "$f"; fi
    done
    rm -rf "$T"
}
trap cleanup EXIT
cc=${CC:-}
[ -n "$cc" ] || for c in c17 c99 cc; do command -v $c >/dev/null 2>&1 && { cc=$c; break; }; done
$cc -o "$T/drive" ../../lib/testlib/ptydrive.c || { echo "cannot compile ptydrive.c"; exit 2; }
$cc -o "$T/mkhash" ../../lib/testlib/mkhash.c || { echo "cannot compile mkhash.c"; exit 2; }
D="$T/drive"
HASH=$("$T/mkhash" secret '$6$testsalt$')
# the user's shell: it says who it is and what it was given
cat > "$T/tsh" <<'SH'
#!/bin/sh
echo "SHELL RAN uid=$(id -u) gid=$(id -g) groups=$(id -G | tr ' ' ,)"
echo "HOME=$HOME USER=$USER LOGNAME=$LOGNAME SHELL=$SHELL TERM=$TERM"
echo "PATH=$PATH FOO=${FOO-unset}"
echo "PWD=$(pwd)"
echo "TTY=$(ls -l "$(tty)" | awk '{print $1, $3, $4}')"
echo "WHO=$(who | awk '{print $1}' | tr '\n' ' ')"
SH
chmod 755 "$T/tsh"
# this terminal is a line of /etc/ttys (secure or not), then login
cat > "$T/wrap" <<'WRAP'
#!/bin/sh
t=$(tty); l=${t#/dev/}; sec=$1; shift
if [ "$sec" = secure ]; then echo "$l \"/usr/libexec/getty x\" vt on secure" > /etc/ttys
else echo "$l \"/usr/libexec/getty x\" vt on" > /etc/ttys; fi
exec "$@"
WRAP
chmod 755 "$T/wrap"
users() {
    { grep -v '^root:' "$T/save/master.passwd"
      echo "root:$HASH:0:0::0:0:root:/root:/bin/sh"
      echo "tuser:$HASH:2001:2001::0:0:Test User:$T/home/tuser:$T/tsh"
      echo "tnopw::2002:2002::0:0:No Password:$T/home/tuser:$T/tsh"
      echo "tlock:*:2003:2003::0:0:Locked:$T/home/tuser:$T/tsh"
      echo "texp:$HASH:2004:2004::0:1000:Expired:$T/home/tuser:$T/tsh"
      echo "tnohome:$HASH:2005:2005::0:0:No Home:$T/nowhere:$T/tsh"; } > /etc/master.passwd
    chmod 600 /etc/master.passwd
    { cat "$T/save/group"; echo "tgrp:x:3001:tuser"; } > /etc/group
    rm -f /etc/nologin "$T/home/tuser/.hushlogin"
    echo "TEST MOTD" > /etc/motd
}
TERM=vt100; FOO=bar; export TERM FOO
users
has() { case $1 in *"$2"*) return 0;; esac; return 1; }
hasnt() { case $1 in *"$2"*) return 1;; esac; return 0; }

# the right password
o=$("$D" -T 40 "e:login: " "s:tuser\n" "e:Password:" "s:secret\n" "w:" -- "$T/wrap" plain "$L")
ok "a login prompt, as when no name is given" has "$o" "login: tuser"
ok "the password is asked" has "$o" "Password:"
ok "the password is not echoed" hasnt "$o" "secret"
ok "the shell runs as the user: uid, gid, the groups of the group file" has "$o" "SHELL RAN uid=2001 gid=2001 groups=2001,3001"
ok "the environment: HOME USER LOGNAME SHELL TERM" has "$o" "HOME=$T/home/tuser USER=tuser LOGNAME=tuser SHELL=$T/tsh TERM=vt100"
ok "PATH is the default and a variable of the caller is gone" has "$o" "PATH=/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin FOO=unset"
ok "in the home directory" has "$o" "PWD=$T/home/tuser"
ok "the terminal is the user's: owner, group tty, mode 0620" has "$o" "TTY=crw--w---- 2001 tty"
ok "who shows the user" has "$o" "WHO=tuser"
ok "the motd" has "$o" "TEST MOTD"
ok "the shell's status is login's" has "$o" "[exit 0]"

# the name as an argument (as getty gives it)
o=$("$D" -T 40 "e:Password:" "s:secret\n" "w:" -- "$T/wrap" plain "$L" -- tuser)
ok "login -- name asks the password only" hasnt "$o" "login: "
ok "...and runs the shell" has "$o" "SHELL RAN uid=2001"

# -p keeps the environment
o=$("$D" -T 40 "e:Password:" "s:secret\n" "w:" -- "$T/wrap" plain "$L" -p tuser)
ok "-p: the caller's FOO stays" has "$o" "FOO=bar"
ok "-p: HOME is still the user's" has "$o" "HOME=$T/home/tuser"

# wrong password, then right
o=$("$D" -T 40 "e:Password:" "s:wrong\n" "e:Login incorrect" "e:login: " "s:tuser\n" "e:Password:" "s:secret\n" "w:" -- "$T/wrap" plain "$L" tuser)
ok "a wrong password: Login incorrect, then asked again" has "$o" "Login incorrect"
ok "...and the right one after it works" has "$o" "SHELL RAN uid=2001"
# an unknown user: the same
o=$("$D" -T 40 "e:Password:" "s:whatever\n" "e:Login incorrect" "e:login: " "d:" "w:" -- "$T/wrap" plain "$L" nosuchuser)
ok "an unknown user is asked a password as well, and refused alike" has "$o" "Login incorrect"
ok "end of file at login: ends it, status 0" has "$o" "[exit 0]"
# no password
o=$("$D" -T 40 "w:" -- "$T/wrap" plain "$L" tnopw)
ok "an empty password field: no password asked" hasnt "$o" "Password:"
ok "...and the shell runs" has "$o" "SHELL RAN uid=2002"
# locked
o=$("$D" -T 40 "e:Password:" "s:*\n" "e:Login incorrect" "e:login: " "s:tlock\n" "e:Password:" "s:\n" "e:Login incorrect" "e:login: " "d:" "w:" -- "$T/wrap" plain "$L" tlock)
ok "a locked account: neither * nor an empty line gets in" hasnt "$o" "SHELL RAN"
# a name that starts with a dash, a name with a slash
o=$("$D" -T 40 "e:Login incorrect" "e:login: " "d:" "w:" -- "$T/wrap" plain "$L" -- -x)
ok "a name starting with - is refused without a password prompt" hasnt "$o" "Password:"
o=$("$D" -T 40 "e:Login incorrect" "e:login: " "d:" "w:" -- "$T/wrap" plain "$L" ../x)
ok "a name with a slash is refused" has "$o" "Login incorrect"

# ten failures end it
set --
i=0
while [ $i -lt 10 ]; do set -- "$@" "e:Password:" "s:bad\n" "e:Login incorrect"; i=$((i+1)); [ $i -lt 10 ] && set -- "$@" "e:login: " "s:tuser\n"; done
o=$("$D" -T 60 "$@" "w:" -- "$T/wrap" plain "$L" tuser)
ok "ten wrong passwords end login with status 1" has "$o" "[exit 1]"

# the account's conditions
o=$("$D" -T 40 "e:Password:" "s:secret\n" "w:" -- "$T/wrap" plain "$L" texp)
ok "an expired account" has "$o" "Sorry -- your account has expired."
ok "...gets no shell" hasnt "$o" "SHELL RAN"
o=$("$D" -T 40 "e:Password:" "s:secret\n" "w:" -- "$T/wrap" plain "$L" tnohome)
ok "no home directory: said, and / is the home" has "$o" "No home directory $T/nowhere!"
ok "...with HOME=/ and PWD=/" has "$o" "HOME=/ USER=tnohome"
echo "The system is going down for the test." > /etc/nologin
o=$("$D" -T 40 "e:Password:" "s:secret\n" "w:" -- "$T/wrap" plain "$L" tuser)
ok "/etc/nologin turns a user away with its text" has "$o" "The system is going down for the test."
ok "...and gives no shell" hasnt "$o" "SHELL RAN"
ok "...with status 0" has "$o" "[exit 0]"
o=$("$D" -T 40 "w:" -- "$T/wrap" plain "$L" tnopw)
ok "nologin applies to every user that is not root" hasnt "$o" "SHELL RAN"
o=$("$D" -T 40 "e:Password:" "s:secret\n" "s:echo ROOT=\$(id -u)\n" "s:exit\n" "w:" -- "$T/wrap" secure "$L" root)
ok "root gets in in spite of /etc/nologin (on a secure line)" has "$o" "ROOT=0"
rm -f /etc/nologin
# root and the secure lines
o=$("$D" -T 40 "e:Password:" "s:secret\n" "e:Login incorrect" "e:login: " "d:" "w:" -- "$T/wrap" plain "$L" root)
ok "root is refused on a line that is not secure (the right password)" has "$o" "login: root login refused on "
ok "...and gets Login incorrect" has "$o" "Login incorrect"
o=$("$D" -T 40 "e:Password:" "s:secret\n" "s:echo ROOT=\$(id -u)\n" "s:exit\n" "w:" -- "$T/wrap" secure "$L" root)
ok "root gets in on a secure line with its password" has "$o" "ROOT=0"
o=$("$D" -T 40 "e:Password:" "s:wrong\n" "e:Login incorrect" "e:login: " "d:" "w:" -- "$T/wrap" secure "$L" root)
ok "root with a wrong password: Login incorrect" has "$o" "Login incorrect"
ok "...and no shell" hasnt "$o" "ROOT=0"

# -f, the motd and .hushlogin, and the usage
o=$("$D" -T 40 "w:" -- "$T/wrap" plain "$L" -f tuser)
ok "-f: root enters as the user without a password" has "$o" "SHELL RAN uid=2001"
ok "-f: and no password was asked" hasnt "$o" "Password:"
: > "$T/home/tuser/.hushlogin"
o=$("$D" -T 40 "e:Password:" "s:secret\n" "w:" -- "$T/wrap" plain "$L" tuser)
ok ".hushlogin hides the motd" hasnt "$o" "TEST MOTD"
ok "...and the login is the same" has "$o" "SHELL RAN uid=2001"
rm -f "$T/home/tuser/.hushlogin"
t "-f needs a name" "login: -f needs a name and is for root" 1 sh -c '"$1" -f 2>&1 < /dev/null' sh "$L"
t "not a terminal" "login: not a terminal" 1 sh -c '"$1" tuser 2>&1 < /dev/null' sh "$L"
t "too many arguments" "usage: login [-fp] [--] [name]" 1 sh -c '"$1" a b 2>&1 < /dev/null' sh "$L"
t "a bad option: usage, status 1" "" 1 "$L" -x
ok "the passwords of the file are not changed by logins" sh -c 'case $(grep "^tuser:" /etc/master.passwd | cut -d: -f2) in "$1") exit 0;; esac; exit 1' sh "$HASH"
done_tests login
