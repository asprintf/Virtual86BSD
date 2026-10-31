. "$LIB"; echo 1 > a; echo 2 > b; echo 3 > c; $PAX -w -f t.tar a b c
cat > i.tl <<'TL'
/* pax -i on a terminal: the replies typed at its questions */
var t = term([argv[0], "-r", "-i", "-f", "../t.tar"], {cwd: "o"});
for (var reply : ["renamed\n", "\n", ".\n"]) {
    tsettle(t, 0.4);
    tsend(t, reply);
}
var st = twait(t, 5);
print(replace(traw(t), "\r", ""));
printf("rc %d %q\n", st, listdir("o"));
TL
mkdir o; "$TLBIN" i.tl "$PAX"
