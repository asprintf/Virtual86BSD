#!/bin/sh
# test_passwd.sh - tests for passwd (V86BSD).  Run as root, inside V86BSD:
#   sh test_passwd.sh
# passwd runs on a pseudo-terminal that ptydrive types on; as an ordinary
# user (asuser) it is made set-user-ID root, as it is installed.  /etc/master.passwd
# is changed for the test and put back.
cd "$(dirname "$0")" || exit 2
PROG_DIR=${PROG_DIR:-$PWD/../../../usr.bin/passwd}; export PROG_DIR
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
P=${P:-$PROG_DIR/passwd}
if [ "$(uname -s)" != V86BSD ] || [ "$(id -u)" != 0 ]; then
    echo "passwd: 0 passed, 0 failed (skipped: needs root on V86BSD)"
    exit 0
fi
T=/var/tmp/test_passwd.$$
mkdir -m 755 "$T" || exit 2
cp -p /etc/master.passwd "$T/master.passwd.save"
cleanup() { rm -f /etc/master.passwd.ptmp; cp -p "$T/master.passwd.save" /etc/master.passwd; rm -rf "$T"; }
trap cleanup EXIT
cc=${CC:-}
[ -n "$cc" ] || for c in c17 c99 cc; do command -v $c >/dev/null 2>&1 && { cc=$c; break; }; done
for s in ptydrive asuser mkhash; do
    $cc -o "$T/$s" ../../lib/testlib/$s.c || { echo "cannot compile $s.c"; exit 2; }
done
D="$T/drive"; mv "$T/ptydrive" "$D"
cp "$P" "$T/passwd" && chmod 4755 "$T/passwd"      # as installed
PW="$T/passwd"
OLD=$("$T/mkhash" secret '$6$testsalt$')
users() {
    { cat "$T/master.passwd.save"
      echo "tuser:$OLD:2001:2001::0:0:Test User:/nonexistent:/bin/sh"
      echo "tother:$OLD:2002:2002::0:0:Other:/nonexistent:/bin/sh"
      echo "tnopw::2003:2003::0:0:No Password:/nonexistent:/bin/sh"
      echo "tlock:*:2004:2004::0:0:Locked:/nonexistent:/bin/sh"; } > /etc/master.passwd
    chmod 600 /etc/master.passwd
    # the public file has the users too (getpwuid: who am I)
    :
}
hash_of() { grep "^$1:" /etc/master.passwd | cut -d: -f2; }
has() { case $1 in *"$2"*) return 0;; esac; return 1; }
hasnt() { case $1 in *"$2"*) return 1;; esac; return 0; }
users
# as tuser: the name has to be known to getpwuid: add the public entry for the test
cp -p /etc/passwd "$T/passwd.save"
echo "tuser:x:2001:2001:Test User:/nonexistent:/bin/sh" >> /etc/passwd
echo "tnopw:x:2003:2003:No Password:/nonexistent:/bin/sh" >> /etc/passwd
trap 'cp -p "$T/passwd.save" /etc/passwd; cleanup' EXIT
asuser() { "$T/asuser" "$@"; }

# root changes a password
cp /etc/master.passwd "$T/before.full"
o=$("$D" -T 30 "e:New password:" "s:newpass\n" "e:Retype new password:" "s:newpass\n" "w:" -- "$PW" tuser)
ok "root: Changing password for tuser." has "$o" "Changing password for tuser."
ok "root is not asked for the old password" hasnt "$o" "Old password:"
ok "the new password is not echoed" hasnt "$o" "newpass"
ok "exit 0" has "$o" "[exit 0]"
h=$(hash_of tuser)
ok "the hash is SHA-512 with a salt of 16 characters" sh -c 'echo "$1" | grep "^\$6\$[./0-9A-Za-z]\{16\}\$[./0-9A-Za-z]\{86\}\$" > /dev/null' sh "$h"
t "the hash is that of the new password" "$h" 0 "$T/mkhash" newpass "${h%\$*}\$"
ok "the old one is gone" test "$h" != "$OLD"
t "the mode of the file is as it was" "-rw-------" 0 sh -c 'ls -l /etc/master.passwd | cut -c1-10'
ok "the other lines are unchanged" sh -c 'grep -v "^tuser:" /etc/master.passwd > "$1/after"; grep -v "^tuser:" "$1/before.full" > "$1/before"; cmp -s "$1/before" "$1/after"' sh "$T"
ok "no temporary file is left" test ! -e /etc/master.passwd.ptmp
# two salts: the same password hashes differently
"$D" -T 30 "e:New password:" "s:newpass\n" "e:Retype new password:" "s:newpass\n" "w:" -- "$PW" tuser > /dev/null
ok "the salt is new each time" test "$(hash_of tuser)" != "$h"

# root: a short password is fine, an empty one stops it, a mismatch asks again
users
o=$("$D" -T 30 "e:New password:" "s:ab\n" "e:Retype new password:" "s:ab\n" "w:" -- "$PW" tuser)
ok "root may set a short password" has "$o" "[exit 0]"
users
o=$("$D" -T 30 "e:New password:" "s:\n" "w:" -- "$PW" tuser)
ok "an empty new password: unchanged" has "$o" "passwd: password unchanged"
t "...and the file is as it was" "$OLD" 0 sh -c 'grep "^tuser:" /etc/master.passwd | cut -d: -f2'
o=$("$D" -T 30 "e:New password:" "s:one\n" "e:Retype new password:" "s:two\n" "e:New password:" "s:same\n" "e:Retype new password:" "s:same\n" "w:" -- "$PW" tuser)
ok "a mismatch: Mismatch; try again" has "$o" "Mismatch; try again, EOF to quit."
ok "...and then it goes through" has "$o" "[exit 0]"
users
o=$("$D" -T 30 "e:New password:" "s:a\n" "e:Retype new password:" "s:b\n" "e:New password:" "s:a\n" "e:Retype new password:" "s:b\n" "e:New password:" "s:a\n" "e:Retype new password:" "s:b\n" "w:" -- "$PW" tuser)
ok "three mismatches: too many tries" has "$o" "passwd: too many tries; password unchanged"
ok "...status 1" has "$o" "[exit 1]"
t "...and the file is as it was" "$OLD" 0 sh -c 'grep "^tuser:" /etc/master.passwd | cut -d: -f2'

# an ordinary user
users
o=$("$D" -T 30 "e:Old password:" "s:secret\n" "e:New password:" "s:goodpass\n" "e:Retype new password:" "s:goodpass\n" "w:" -- "$T/asuser" 2001 2001 "$PW")
ok "a user: the old password, then the new one twice" has "$o" "[exit 0]"
ok "...asked for by name: Changing password for tuser." has "$o" "Changing password for tuser."
h=$(hash_of tuser)
t "...and it is set" "$h" 0 "$T/mkhash" goodpass "${h%\$*}\$"
users
o=$("$D" -T 30 "e:Old password:" "s:wrong\n" "w:" -- "$T/asuser" 2001 2001 "$PW")
ok "a wrong old password: refused" has "$o" "passwd: incorrect password"
t "...nothing changed" "$OLD" 0 sh -c 'grep "^tuser:" /etc/master.passwd | cut -d: -f2'
o=$("$D" -T 30 "e:Old password:" "s:secret\n" "e:New password:" "s:short\n" "e:New password:" "s:\n" "w:" -- "$T/asuser" 2001 2001 "$PW")
ok "a user: at least 6 characters" has "$o" "Password is too short: at least 6 characters."
t "...and nothing changed" "$OLD" 0 sh -c 'grep "^tuser:" /etc/master.passwd | cut -d: -f2'
o=$("$D" -T 30 "w:" -- "$T/asuser" 2001 2001 "$PW" tother)
ok "a user may not change another's" has "$o" "passwd: you may only change your own password"
o=$("$D" -T 30 "e:New password:" "s:newpass1\n" "e:Retype new password:" "s:newpass1\n" "w:" -- "$T/asuser" 2003 2003 "$PW")
ok "an empty old password: not asked" hasnt "$o" "Old password:"
ok "...the new one is set" has "$o" "[exit 0]"

# errors
users
o=$("$D" -T 30 "w:" -- "$PW" nosuchuser)
ok "no such user" has "$o" "passwd: nosuchuser: no such user"
o=$("$D" -T 30 "w:" -- "$PW" tlock)
ok "a locked account" has "$o" "passwd: the account of tlock is locked"
o=$("$D" -T 30 "w:" -- "$PW" a b)
ok "too many arguments" has "$o" "usage: passwd [user]"
o=$("$D" -T 30 "w:" -- "$PW" -x)
ok "a bad option" has "$o" "usage: passwd [user]"
: > /etc/master.passwd.ptmp
o=$("$D" -T 30 "e:New password:" "s:newpass\n" "e:Retype new password:" "s:newpass\n" "w:" -- "$PW" tuser)
ok "the file is busy (someone else at it)" has "$o" "passwd: the password file is busy, try again"
rm -f /etc/master.passwd.ptmp
t "...nothing changed" "$OLD" 0 sh -c 'grep "^tuser:" /etc/master.passwd | cut -d: -f2'
# the helper and the program agree with login: a password set by passwd gets in
done_tests passwd
