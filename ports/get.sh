#!/bin/sh
# get.sh - fetch and unpack the sources of a port into a V86BSD system, on
# the host (V86BSD itself has no network tools and no gzip).
#
# Usage: sh get.sh [-f] [-r root] port [archive]
#
#   port     lua, ncurses, rogue, sqlite, sqllogictest, mbedtls or curl;
#            all: each of them whose sources aren't there yet
#   archive  a copy you already have (.tar.gz, .tgz, .tar.bz2 or .zip; or a
#            directory, such as a git clone); without it the sources are
#            downloaded (with git, curl or wget)
#   -r root  the system (default: root/ beside this package's bootstrap.sh)
#   -f       replace sources already there
#
# The sources go to root/usr/src/NAME, where NAME is what the port's
# build.sh looks for by default:
#
#   lua      lua-5.4.7     git tag v5.4.7 of github.com/lua/lua (it has the
#                          test suite), else lua.org's release tarball
#   ncurses  ncurses-6.6   https://ftp.gnu.org/gnu/ncurses/ncurses-6.6.tar.gz
#   rogue    rogue-5.4.4   github.com/Davidslv/rogue, branch master
#   sqlite   sqlite-3.54.1 git tag version-3.54.1 of github.com/sqlite/sqlite
#                          (the source tree, with the tests); an archive
#                          may also be sqlite.org's sqlite-src-3540100.zip
#                          or sqlite-autoconf-3540100.tar.gz
#   sqllogictest sqllogictest  github.com/gregrahn/sqllogictest, a mirror of
#                          the Fossil repository at sqlite.org/sqllogictest
#                          (1.1 GB: make the disk image with mkdisk.sh -s 4096)
#   mbedtls  mbedtls-3.6.7 the release tarball from github.com/Mbed-TLS/mbedtls
#                          (a git clone lacks the framework/ submodule)
#   curl     curl-8.23.0   the release tarball from github.com/curl/curl
#                          (a git clone lacks configure)
#
# For a system booted in QEMU: run this before mkdisk.sh, which copies
# root into the disk image.

usage() { echo "usage: sh get.sh [-f] [-r root] all|lua|ncurses|rogue|sqlite|sqllogictest|mbedtls|curl [archive]" >&2; exit 2; }
here=$(cd "$(dirname "$0")" && pwd)
root=$here/../root
force=no
while getopts fr: opt; do
	case $opt in
	f) force=yes ;;
	r) root=$OPTARG ;;
	*) usage ;;
	esac
done
shift $((OPTIND - 1))
[ $# -ge 1 ] && [ $# -le 2 ] || usage
port=$1 archive=${2:-}
fail() { echo "get.sh: $*" >&2; exit 1; }
if [ "$port" = all ]; then
	[ -z "$archive" ] || usage
	st=0
	for p in ncurses rogue lua sqlite sqllogictest mbedtls curl; do
		if [ $force = yes ]; then sh "$0" -f -r "$root" $p || st=1
		else sh "$0" -r "$root" $p 2>&1 | grep -v 'is there already' || :
		fi
	done
	echo "inside the system:  sh /usr/src/ports/all.sh"
	exit $st
fi
have() { command -v "$1" >/dev/null 2>&1; }

case $port in
lua)     name=lua-5.4.7
         git=https://github.com/lua/lua.git tag=v5.4.7
         url=https://www.lua.org/ftp/lua-5.4.7.tar.gz
         tests=https://www.lua.org/tests/lua-5.4.7-tests.tar.gz ;;
ncurses) name=ncurses-6.6 git= tag= tests=
         url=https://ftp.gnu.org/gnu/ncurses/ncurses-6.6.tar.gz ;;
rogue)   name=rogue-5.4.4
         git=https://github.com/Davidslv/rogue.git tag=master tests=
         url=https://github.com/Davidslv/rogue/archive/refs/heads/master.tar.gz ;;
sqlite)  name=sqlite-3.54.1
         git=https://github.com/sqlite/sqlite.git tag=version-3.54.1 tests=
         url=https://github.com/sqlite/sqlite/archive/refs/tags/version-3.54.1.tar.gz ;;
sqllogictest) name=sqllogictest
         git=https://github.com/gregrahn/sqllogictest.git tag=master tests=
         url=https://github.com/gregrahn/sqllogictest/archive/refs/heads/master.tar.gz ;;
mbedtls) name=mbedtls-3.6.7 git= tag= tests=
         url=https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-3.6.7/mbedtls-3.6.7.tar.bz2 ;;
curl)    name=curl-8.23.0 git= tag= tests=
         url=https://github.com/curl/curl/releases/download/curl-8_23_0/curl-8.23.0.tar.gz ;;
*)       usage ;;
esac

[ -x "$root/bin/sh" ] || fail "$root: no V86BSD system there (sh bootstrap.sh)"
src=$(cd "$root/usr/src" && pwd) || exit 1
dest=$src/$name
if [ -e "$dest" ]; then
	[ $force = yes ] || fail "$dest is there already (-f replaces it)"
	rm -rf "$dest"
fi
tmp=$src/.get.$$
rm -rf "$tmp" && mkdir "$tmp" || exit 1
trap 'rm -rf "$tmp"' EXIT

download() {   # download url file
	if have curl; then curl -fL -o "$2" "$1"
	elif have wget; then wget -O "$2" "$1"
	else echo "get.sh: neither curl nor wget is installed" >&2; return 1
	fi
}
unpack() {     # unpack archive dir: into dir, which then holds one directory
	case $1 in
	*.zip) unzip -q "$1" -d "$2" ;;
	*.bz2|*.tbz2) bzip2 -dc "$1" | (cd "$2" && tar -xf -) ;;
	*)     gzip -dc "$1" | (cd "$2" && tar -xf -) ;;
	esac
}
only() {       # only dir: the one directory in dir
	set -- "$1"/*
	[ $# -eq 1 ] && [ -d "$1" ] || { echo "get.sh: the archive doesn't hold one directory" >&2; return 1; }
	echo "$1"
}

if [ -n "$archive" ] && [ -d "$archive" ]; then
	cp -pR "$archive" "$tmp/x" || exit 1
	rm -rf "$tmp/x/.git"
	mv "$tmp/x" "$dest" || exit 1
elif [ -n "$archive" ]; then
	[ -f "$archive" ] || fail "$archive: no such file"
	mkdir "$tmp/u" && unpack "$archive" "$tmp/u" || fail "$archive: can't unpack it"
	d=$(only "$tmp/u") && mv "$d" "$dest" || exit 1
elif [ -n "$git" ] && have git; then
	echo "get.sh: git clone $git ($tag)"
	git -c advice.detachedHead=false clone -q --depth 1 -b "$tag" "$git" "$tmp/x" ||
		fail "git clone failed"
	rm -rf "$tmp/x/.git"
	mv "$tmp/x" "$dest" || exit 1
else
	echo "get.sh: downloading $url"
	a=$tmp/a.${url##*.}
	download "$url" "$a" || fail "download failed"
	mkdir "$tmp/u" && unpack "$a" "$tmp/u" || fail "can't unpack $url"
	d=$(only "$tmp/u") && mv "$d" "$dest" || exit 1
	if [ -n "$tests" ]; then   # lua's release tarball has no tests: beside it
		rm -rf "$src/$name-tests"
		download "$tests" "$tmp/t.tar.gz" && mkdir "$tmp/t" && unpack "$tmp/t.tar.gz" "$tmp/t" &&
			d=$(only "$tmp/t") && mv "$d" "$dest/$name-tests" ||
			echo "get.sh: warning: no test suite ($tests)" >&2
	fi
fi
echo "get.sh: $port in $dest"
echo "inside the system:  sh /usr/src/ports/$port/build.sh"
