. "$LIB"
echo "-- size, uid, mtime records override the header"
"$TLBIN" "$TARTL" huge huge.tar     # as a common tar library makes it
$PAX -f huge.tar -o 'listopt=%(path)s %(size)d %(uid)u %(uname)s %(mtime)T %(comment)s'; echo rc=$?
echo "-- truncated data is detected"; $PAX -v -f huge.tar > /dev/null; echo rc=$?
echo "-- records with an empty value delete the field"
"$TLBIN" "$TARTL" records r.tar     # as a common tar library makes it
$PAX -v -f r.tar | cut -c1-30,52-
echo "-- global records apply to later members"
"$TLBIN" "$TARTL" global g.tar     # as a common tar library makes it
$PAX -v -f g.tar | cut -c1-30,52-
