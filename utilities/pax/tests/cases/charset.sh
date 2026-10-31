. "$LIB"
# pax archives with chosen extended header records: mkpax OUT 'k=v;k=v' NAME ...
# (one file per NAME, with the records given before it; \xNN in values;
# "g:" before the records makes a global header instead)
mkpax() { "$TLBIN" "$TARTL" mkpax "$@"; }
show() { ls "$@" 2>/dev/null | od -An -c | tr -s ' '; }
U=${U8:-}     # a UTF-8 locale, from the runner
[ -n "$U" ] || { echo SKIP; exit 0; }

echo "-- writing in a UTF-8 locale: UTF-8 names as they are, others BINARY"
mkdir w; "$TLBIN" -e 'writefile("w/caf\303\251", ""); writefile("w/caf\351", "");'
LC_ALL=$U $PAX -w -x pax -f w.pax w; hdrs w.pax | grep -E 'hdrcharset|path='
echo "-- writing in the C locale: bytes outside ASCII are BINARY"
LC_ALL=C $PAX -w -x pax -f wc.pax w; hdrs wc.pax | grep -E 'hdrcharset|path='

echo "-- -o hdrcharset:=BINARY: the names as they are"
LC_ALL=$U $PAX -w -x pax -o hdrcharset:=BINARY -f wb.pax w/caf*; hdrs wb.pax | grep -E 'hdrcharset|path=' | sort | uniq -c | sed 's/^ *//'

mkpax utf8.pax 'path=caf\xc3\xa9' x
mkpax raw.pax 'path=caf\xe9' x
mkpax bin.pax 'hdrcharset=BINARY;path=caf\xe9' x
mkpax glob.pax 'g:hdrcharset=BINARY' - 'path=a\xe9' x 'path=b\xe9' x
mkpax nul.pax 'path=a\x00b' x
mkpax long.pax "path=$(awk 'BEGIN { while (i++ < 300) printf "n" }')" x
mkpax link.pax 'linkpath=t\xe9' y
mkpax iso.pax 'hdrcharset=ISO-IR 10646 2000 UTF-8;path=caf\xc3\xa9' x
mkpax other.pax 'hdrcharset=ISO-IR 8859 1 1998;path=caf\xe9' x
mkpax ok.pax 'path=plain' x

for loc in $U C; do
  if [ "$loc" = C ]; then echo "== reading in C"; else echo "== reading in the UTF-8 locale"; fi
  for a in utf8 raw bin glob nul long iso other ok; do
    rm -rf o; mkdir o; (cd o && { LC_ALL=$loc $PAX -r -f ../$a.pax 2>&1; echo $? > ../rc; } | sed 's/^[^:]*pax: /pax: /'; echo "$a rc=$(cat ../rc)"); show o
  done
done

echo "== the actions, in the C locale, for the UTF-8 name"
for act in bypass binary UTF-8 write; do
  rm -rf o; mkdir o; (cd o && LC_ALL=C $PAX -r -o invalid=$act -f ../utf8.pax 2>&1; echo "$act rc=$?"); show o
done
echo "== the actions in a UTF-8 locale, for the name that is not UTF-8"
for act in bypass binary UTF-8 write; do
  rm -rf o; mkdir o; (cd o && LC_ALL=$U $PAX -r -o invalid=$act -f ../raw.pax 2>&1; echo "$act rc=$?"); show o
done
echo "== a name with a NUL, -o invalid=binary"
rm -rf o; mkdir o; (cd o && LC_ALL=$U $PAX -r -o invalid=binary -f ../nul.pax; echo "rc=$?"); show o
echo "== a name too long, -o invalid=write: tried, and the system refuses"
rm -rf o; mkdir o; (cd o && { LC_ALL=$U $PAX -r -o invalid=write -f ../long.pax 2>&1; echo $? > ../rc; } | sed 's/^[^:]*pax: [n]*/pax: N.../'; echo "rc=$(cat ../rc)"); show o
echo "== a symbolic link whose target cannot be translated"
mkpax sym.pax 'linkpath=t\xe9' -
"$TLBIN" "$TARTL" symlink link.pax     # the member made a symbolic link
rm -rf o; mkdir o; (cd o && LC_ALL=$U $PAX -r -f ../link.pax 2>&1; echo "rc=$?"; LC_ALL=$U $PAX -r -o invalid=write -f ../link.pax 2>&1; echo "write rc=$?"); ls -l o | sed -n 's/.* \(y -> .*\)/\1/p' | od -An -c | tr -s ' '

echo "== list mode lists every file"
for a in utf8 raw nul; do LC_ALL=C $PAX -f $a.pax | od -An -c | tr -s ' '; echo "$a rc=$?"; done

echo "== -o invalid=rename asks, as -i does"
cat > rename.tl <<'TL'
/* pax on a terminal, the new name typed at its question */
var t = term([argv[0], "-r", "-o", "invalid=rename", "-f", "../utf8.pax"], {cwd: "r", env: {LC_ALL: "C"}});
tsettle(t, 0.4);
tsend(t, "cafe\n");
var st = twait(t, 5);
printf("%q\n", replace(traw(t), "\r", ""));
printf("rc %d %q\n", st, listdir("r"));
TL
mkdir r; "$TLBIN" rename.tl "$PAX"
