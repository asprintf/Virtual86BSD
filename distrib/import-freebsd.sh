#!/bin/sh
# import-freebsd.sh CHECKOUT [DEST]: copy the parts of a FreeBSD source checkout
# that V86BSD builds into DEST (default: this tree): sys/, stand/, freebsd/.
set -eu
src=${1:?usage: import-freebsd.sh FREEBSD-CHECKOUT [DEST]}
here=$(cd "$(dirname "$0")" && pwd)
dest=${2:-$(dirname "$here")}
lists=$here/freebsd

[ -d "$src/sys/amd64" ] || { echo "$src is not a FreeBSD source tree" >&2; exit 1; }

mkdir -p "$dest/sys" "$dest/stand" "$dest/freebsd"
rsync -a --delete --prune-empty-dirs --filter="merge $lists/sys.exclude" "$src/sys/" "$dest/sys/"
rsync -a --delete "$src/stand/" "$dest/stand/"

rm -rf "$dest/freebsd"
mkdir -p "$dest/freebsd"
rsync -a --files-from="$lists/files.list" "$src/" "$dest/freebsd/"

for p in "$lists"/patches/*.patch; do
	[ -f "$p" ] && patch -s -p1 -d "$dest/freebsd" < "$p"
done

for p in "$lists"/patches-sys/*.patch; do
	[ -f "$p" ] && patch -s -p1 -d "$dest/sys" < "$p"
done

for p in "$lists"/patches-stand/*.patch; do
	[ -f "$p" ] && patch -s -p1 -d "$dest/stand" < "$p"
done

{
	echo "commit $(git -C "$src" rev-parse HEAD)"
	echo "branch $(git -C "$src" rev-parse --abbrev-ref HEAD)"
} > "$dest/freebsd/IMPORTED"
