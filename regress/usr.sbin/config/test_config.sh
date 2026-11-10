#!/bin/sh
# test_config.sh - tests for config, the kernel configuration tool.  Run: sh test_config.sh
cd "$(dirname "$0")" || exit 2
HERE=$(pwd)
PROG_DIR=${PROG_DIR:-$HERE/../../../usr.sbin/config}; export PROG_DIR
SYS=$HERE/../../../sys
pass=0 fail=0
ok() { pass=$((pass+1)); }
bad() { fail=$((fail+1)); printf 'FAIL %s\n' "$1"; [ -z "$2" ] || printf '%s\n' "$2"; }
# t NAME: the last command's status was 0
check() { if [ "$2" = 0 ]; then ok; else bad "$1" "$3"; fi; }
# has NAME FILE TEXT: the file contains the line
has() { if grep -F -x -q -- "$3" "$2"; then ok; else bad "$1" "  no line [$3] in $2"; fi; }
# hasnt NAME FILE TEXT
hasnt() { if grep -F -q -- "$3" "$2"; then bad "$1" "  [$3] is in $2"; else ok; fi; }
done_tests() { echo "$1: $pass passed, $fail failed"; [ "$fail" = 0 ]; }

U=${U:-$PROG_DIR/config}
T=${TMPDIR:-/tmp}/test_config.$$
mkdir -m 700 "$T" || exit 2
trap 'rm -rf "$T"' EXIT
export LC_ALL=C

# a small source tree: sys/conf/files, sys/arch/vm/conf/{files.vm,Makefile.vm},
# configurations in sys/arch/vm/conf
S=$T/sys
mkdir -p "$S/conf" "$S/arch/vm/conf" "$S/arch/vm/include" "$S/kern" "$S/sys" "$S/arch/other/include" "$S/arch/vm/stand"
echo '/* a header */' > "$S/sys/param.h"
echo '/* a header */' > "$S/arch/vm/include/cpu.h"
echo '/* not this machine' > "$S/arch/other/include/cpu.h"
echo '/* not compiled here' > "$S/arch/vm/stand/boot.h"
cat > "$S/conf/files" <<'EOT'
# the files of the test
defopt DEBUG BIGMEM
device bus {port = -1, irq = 5, size = 0}
attach bus at mainbus
device uart
attach uart at bus
device disk
attach disk at bus, ctlr
device ctlr
attach ctlr at bus
device ide
attach ide at ctlr
file kern/main.c
file kern/uart.c uart
file kern/disk.c disk
file kern/debug.c DEBUG
file kern/nodebug.c !DEBUG
file kern/both.c (uart | ide) & !BIGMEM
file kern/ide.c ide # (a comment after the condition)

EOT
cat > "$S/arch/vm/conf/files.vm" <<'EOT'
device mainbus
attach mainbus at root
file arch/vm/start.s
file arch/vm/mainbus.c mainbus
EOT
cat > "$S/arch/vm/conf/Makefile.vm" <<'EOT'
# template
S = %S%
CONF = %CONF% for %MACHINE%
%OBJS%
%HDRS%
%RULES%
EOT
cat > "$S/arch/vm/conf/GOOD" <<'EOT'
# a configuration
machine vm
option DEBUG
config bsd root on disk1a
mainbus0 at root
bus0 at mainbus0
uart* at bus? port 0x3f8 irq 4
uart1 at bus0
ctlr* at bus0
disk* at ctlr?
disk* at bus0
ide* at ctlr?
EOT
CONF=$S/arch/vm/conf
run() { (cd "$CONF" && "$U" "$@" 2>&1); }

# --- the output
run GOOD; check "GOOD configures" $? "$(run GOOD)"
B=$S/arch/vm/compile/GOOD
for f in Makefile ioconf.c locators.h options.h; do [ -f "$B/$f" ]; check "writes $f" $?; done
[ -L "$B/machine" ] && [ "$(readlink "$B/machine")" = ../../../../arch/vm/include ]; check "the link machine" $? "$(readlink "$B/machine")"
[ -f "$B/machine/cpu.h" ]; check "the link works" $?
has "S, relative to the compile directory" "$B/Makefile" "S = ../../../.."
has "the name and machine" "$B/Makefile" "CONF = GOOD for vm"
has "the objects, in the order of files" "$B/Makefile" "OBJS0 = main.o uart.o disk.o debug.o both.o ide.o start.o mainbus.o ioconf.o"
has "a rule for C" "$B/Makefile" "	\$(CC) \$(CFLAGS) -c -o \$@ \$(S)/kern/uart.c"
has "a rule for assembly" "$B/Makefile" "	\$(AS) -o \$@ \$(S)/arch/vm/start.s"
has "ioconf's rule" "$B/Makefile" "ioconf.o: ioconf.c"
has "a header" "$B/Makefile" "	\$(S)/sys/param.h"
has "a header of the machine" "$B/Makefile" "	\$(S)/arch/vm/include/cpu.h \\"
hasnt "no header of another machine" "$B/Makefile" "arch/other"
hasnt "no header of a stand directory" "$B/Makefile" "boot.h"
has "an option set" "$B/options.h" "#define DEBUG 1"
hasnt "an option not set" "$B/options.h" "BIGMEM"
has "a locator number" "$B/locators.h" "#define BUSCF_IRQ 1"
has "its default" "$B/locators.h" "#define BUSCF_IRQ_DEFAULT 5"
has "the number of locators" "$B/locators.h" "#define BUSCF_NLOC 3"
has "a driver" "$B/ioconf.c" "extern struct cfdriver ide_cd;"
has "the root device" "$B/ioconf.c" "const char cf_rootdev[] = \"disk1a\";"
has "the mainbus at root" "$B/ioconf.c" "    {&mainbus_ca, &mainbus_cd, 0, FSTATE_NORMAL, NULL, 0, NULL, -1, 0},"
has "a device with locators, star" "$B/ioconf.c" "    {&uart_ca, &uart_cd, 0, FSTATE_STAR, loc2, 0, &bus_cd, -1, 0},"
has "the locators, as given" "$B/ioconf.c" "static int loc2[] = {0x3f8, 4, 0};"
has "the locators, by default" "$B/ioconf.c" "static int loc4[] = {-1, 5, 0};"
has "a device at one unit" "$B/ioconf.c" "    {&uart_ca, &uart_cd, 1, FSTATE_NORMAL, loc3, 0, &bus_cd, 0, 0},"
has "two lines for one driver" "$B/ioconf.c" "    {&disk_ca, &disk_cd, 0, FSTATE_STAR, NULL, 0, &ctlr_cd, -1, 0},"
has "the table ends" "$B/ioconf.c" "    {NULL}"

# --- the same again: nothing written that did not change
touch "$T/mark"; sleep 1
(cd "$CONF" && "$U" GOOD)
n=$(find "$B" -newer "$T/mark" | wc -l | tr -d ' ')
[ "$n" = 0 ]; check "files that did not change are left alone" $? "$n newer"
# a change in the configuration: only what depends on it
sed 's/^option DEBUG$/option DEBUG=2/' "$CONF/GOOD" > "$CONF/VALUE"
run VALUE; has "an option with a value" "$S/arch/vm/compile/VALUE/options.h" "#define DEBUG 2"
sed 's/^option DEBUG$//' "$CONF/GOOD" > "$CONF/NODEBUG"
run NODEBUG
has "a file compiled for !DEBUG" "$S/arch/vm/compile/NODEBUG/Makefile" "OBJS0 = main.o uart.o disk.o nodebug.o both.o ide.o start.o mainbus.o ioconf.o"
# the same twice from other places: the same
mkdir -p "$T/elsewhere"
(cd "$CONF" && "$U" -s "$S" -b "$T/elsewhere/GOOD" GOOD); [ -f "$T/elsewhere/GOOD/ioconf.c" ]; check "-s and -b" $?
cmp -s "$B/ioconf.c" "$T/elsewhere/GOOD/ioconf.c"; check "ioconf.c is the same wherever it is made" $?
cmp -s "$B/options.h" "$T/elsewhere/GOOD/options.h"; check "options.h is the same" $?
cp -R "$S" "$T/sys2"
(cd "$T/sys2/arch/vm/conf" && "$U" GOOD)
cmp -s "$B/Makefile" "$T/sys2/arch/vm/compile/GOOD/Makefile"; check "the Makefile is the same in a copy of the tree" $?

# --- errors: each a message with the line, and status 1
err() { # NAME EXPECTED-MESSAGE-PART CONFIG-TEXT...
    _n=$1 _m=$2; shift 2
    printf '%s\n' "$@" > "$CONF/BAD"
    _o=$(cd "$CONF" && "$U" BAD 2>&1); _r=$?
    case $_o in *"$_m"*) [ "$_r" = 1 ] && ok || bad "$_n" "  status $_r";; *) bad "$_n" "  got [$_o]  want [$_m]";; esac
}
err "no machine" "no \`machine' line" "config bsd" "mainbus0 at root"
err "no config line" "no \`config bsd' line" "machine vm" "mainbus0 at root"
err "a device that is not there" "no device nosuch" "machine vm" "config bsd" "mainbus0 at root" "nosuch0 at mainbus0"
err "the wrong parent" "uart does not attach at mainbus" "machine vm" "config bsd" "mainbus0 at root" "uart0 at mainbus0"
err "no root parent" "bus does not attach at root" "machine vm" "config bsd" "bus0 at root"
err "a unit twice" "bus0 is there twice" "machine vm" "config bsd" "mainbus0 at root" "bus0 at mainbus0" "bus0 at mainbus0"
err "no such parent unit" "there is no bus3" "machine vm" "config bsd" "mainbus0 at root" "bus0 at mainbus0" "uart0 at bus3"
err "no parent at all" "no bus in the configuration" "machine vm" "config bsd" "mainbus0 at root" "uart0 at bus?"
err "a locator that is not" "bus has no locator dma" "machine vm" "config bsd" "mainbus0 at root" "bus0 at mainbus0" "uart0 at bus0 dma 3"
err "a locator without a number" "a number is needed" "machine vm" "config bsd" "mainbus0 at root" "bus0 at mainbus0" "uart0 at bus0 port"
err "no unit" "written with its unit" "machine vm" "config bsd" "mainbus at root"
err "an unknown option" "unknown option NOPE" "machine vm" "config bsd" "option NOPE"
err "an option twice" "option DEBUG is given twice" "machine vm" "config bsd" "option DEBUG" "option DEBUG"
err "a bad kernel name" "the kernel is named bsd" "machine vm" "config vmunix"
err "an unknown line" "unknown line" "machine vm" "config bsd" "frobnicate"
err "machine twice" "machine is given twice" "machine vm" "machine vm" "config bsd"
printf 'machine nowhere\nconfig bsd\n' > "$CONF/BAD"
(cd "$CONF" && "$U" BAD) >/dev/null 2>&1; [ $? = 1 ]; check "a machine without files" $?
(cd "$CONF" && "$U" NOSUCH) >/dev/null 2>&1; [ $? = 1 ]; check "a configuration that is not there" $?
(cd "$CONF" && "$U") >/dev/null 2>&1; [ $? = 2 ]; check "usage" $?
(cd "$CONF" && "$U" -s /nonexistent GOOD) >/dev/null 2>&1; [ $? = 1 ]; check "no source directory" $?

# errors in the files
ferr() { # NAME EXPECTED FILES-LINE
    _n=$1 _m=$2 _l=$3
    cp "$S/conf/files" "$T/files.save"
    printf '%s\n' "$_l" >> "$S/conf/files"
    _o=$(cd "$CONF" && "$U" GOOD 2>&1); _r=$?
    cp "$T/files.save" "$S/conf/files"
    case $_o in *"$_m"*) [ "$_r" = 1 ] && ok || bad "$_n" "  status $_r";; *) bad "$_n" "  got [$_o]  want [$_m]";; esac
}
ferr "a condition on what is not there" "\`nothing', which is not a device or an option" "file kern/x.c nothing"
ferr "a condition not closed" "lacks a \`)'" "file kern/x.c (uart"
ferr "a condition with a rest" "after the condition" "file kern/x.c uart )"
ferr "two sources, one object" "would both make main.o" "file arch/x/main.c"
ferr "a source that is not C" "is not a .c or .s file" "file kern/x.y"
ferr "a device twice" "device uart is declared twice" "device uart"
ferr "an unknown keyword" "unknown keyword" "frobnicate x"
ferr "attach for nothing" "attach: nosuch is not a device" "attach nosuch at bus"
ferr "attach twice" "is attached twice" "attach uart at bus"
ferr "an option twice" "option DEBUG is declared twice" "defopt DEBUG"
ferr "a locator list not closed" "\`}' is missing" "device broken {a = 1"

# --- the real tree
if [ -f "$SYS/arch/amd64/conf/GENERIC" ]; then
    R=$T/real
    cp -R "$SYS" "$R" && rm -rf "$R"/arch/amd64/compile/*
    (cd "$R/arch/amd64/conf" && "$U" GENERIC); check "GENERIC configures" $?
    RB=$R/arch/amd64/compile/GENERIC
    missing=
    for f in $(sed -n 's/^[ 	]*\$(\(CC\|AS\)).*\$(S)\/\(.*\)$/\2/p' "$RB/Makefile" | sed 's/ .*//'); do
        [ -f "$R/$f" ] || missing="$missing $f"
    done
    [ -z "$missing" ]; check "every source of GENERIC is in the tree" $? "missing:$missing"
    has "GENERIC: the root" "$RB/ioconf.c" "    {&mainbus_ca, &mainbus_cd, 0, FSTATE_NORMAL, NULL, 0, NULL, -1, 0},"
    has "GENERIC: DIAGNOSTIC" "$RB/options.h" "#define DIAGNOSTIC 1"
    has "GENERIC: COMPAT_LINUX" "$RB/options.h" "#define COMPAT_LINUX 1"
    has "GENERIC: the serial port" "$RB/ioconf.c" "    {&com_ca, &com_cd, 0, FSTATE_NORMAL, loc12, 0, &isa_cd, 0, 0},"
    has "GENERIC: its port and irq" "$RB/ioconf.c" "static int loc12[] = {0x3f8, -1, 0, 4};"
    has "GENERIC: root on disk0s1" "$RB/ioconf.c" "const char cf_rootdev[] = \"disk0s1\";"
    has "GENERIC: the Makefile's S" "$RB/Makefile" "S = ../../../.."
    # the kernel is compiled from $(S) paths of one form: what __FILE__ is
    hasnt "GENERIC: no absolute path in the Makefile" "$RB/Makefile" "$R"
    # a GENERIC without the network: no driver of it
    grep -v '^em\*' "$R/arch/amd64/conf/GENERIC" > "$R/arch/amd64/conf/NONET"
    (cd "$R/arch/amd64/conf" && "$U" NONET); check "a configuration without em" $?
    hasnt "NONET: no em.o" "$R/arch/amd64/compile/NONET/Makefile" "if_em.o"
    hasnt "NONET: no em in ioconf.c" "$R/arch/amd64/compile/NONET/ioconf.c" "em_cd"
    has "GENERIC: with em.o" "$RB/Makefile" "if_em.o: \$(S)/dev/pci/if_em.c"
fi

done_tests config
