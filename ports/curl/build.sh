#!/bin/sh
# build.sh - build and install curl 8.23 with Mbed TLS inside a Shall
# system (see HOWTO.md beside it).  Needs Mbed TLS first
# (ports/mbedtls/build.sh).
#
# Usage: sh build.sh [-I] [-t] [dir]
#
#   dir   curl's sources: the release tarball curl-8.23.0.tar.gz
#         (default /usr/src/curl-8.23.0)
#   -t    test afterwards: a file: URL, HTTPS (TLS 1.2 and 1.3) to Mbed TLS's
#         test server on this machine (needs Mbed TLS's sources in
#         /usr/src/mbedtls-3.6.7, for its test CA), and one request to the
#         Internet (https://curl.se/, through $https_proxy if set), whose
#         failure is reported but doesn't fail the test
#   -I    build only, don't install
#
# Applies shall.diff (beside this script) once, then curl's own configure
# and make, with c17.  Installs, under /usr/local: bin/curl, curl-config,
# wcurl, lib/libcurl.a, include/curl/.  curl verifies servers with
# /etc/ssl/certs/ca-certificates.crt (bootstrap.sh copies the host's; see
# HOWTO.md).

usage() { echo "usage: sh build.sh [-I] [-t] [dir]" >&2; exit 2; }
inst=yes test=no
while getopts It opt; do
	case $opt in
	I) inst=no ;;
	t) test=yes ;;
	*) usage ;;
	esac
done
shift $((OPTIND - 1))
[ $# -le 1 ] || usage
here=$(cd "$(dirname "$0")" && pwd)
dir=${1:-/usr/src/curl-8.23.0}
CC=${CC:-c17}
export CC
CABUNDLE=/etc/ssl/certs/ca-certificates.crt
fail() { echo "build.sh: $*" >&2; exit 1; }
say() { echo "== $*"; }

[ -f "$dir/include/curl/curl.h" ] && [ -x "$dir/configure" ] ||
	fail "$dir: no curl sources there (sh ports/get.sh curl, on the host)"
[ -f /usr/local/include/mbedtls/ssl.h ] && [ -f /usr/local/lib/libmbedtls.a ] ||
	fail "Mbed TLS isn't installed: sh $here/../mbedtls/build.sh first"
[ -f $CABUNDLE ] ||
	echo "build.sh: warning: no $CABUNDLE: curl won't be able to verify servers (see HOWTO.md)" >&2
cd "$dir" || exit 1
log=$(pwd)/build.log
: > "$log"
run() {   # run what... : append the output to the log, stop on failure
	"$@" >> "$log" 2>&1 || fail "failed: $* (see $log)"
}

if grep "Shall's c17" include/curl/system.h >/dev/null; then
	say "shall.diff: already applied"
else
	say "applying shall.diff"
	run patch -p1 -i "$here/shall.diff"
fi
[ -f Makefile ] && run make distclean
# LD=c17: libtool insists on finding a link editor, though the compiler
# links.  No zlib, brotli, zstd, libpsl (not ported); static libcurl.
say "configure (output in $log)"
LD=$CC CFLAGS=${CFLAGS:--O 1} run ./configure --prefix=/usr/local \
	--with-mbedtls=/usr/local --without-zlib --without-brotli --without-zstd \
	--without-libpsl --disable-shared \
	--with-ca-bundle=$CABUNDLE --without-ca-path
say "make"
run make
[ -x src/curl ] || fail "no curl made (see $log)"
src/curl --version | sed -n 1p
src/curl --version | grep mbedTLS >/dev/null || fail "curl without Mbed TLS?"

if [ $test = yes ]; then
	say "test: a file: URL"
	src/curl -sS "file://$(pwd)/COPYING" | cmp - COPYING || fail "file: URL"
	echo "  ok"
	D=/usr/src/mbedtls-3.6.7/framework/data_files
	if [ -f $D/test-ca.crt ] && command -v mbedtls_ssl_server2 >/dev/null; then
		say "test: HTTPS to mbedtls_ssl_server2 on localhost"
		cat $D/test-ca.crt $D/test-ca2.crt > /tmp/curl-testca.$$
		port=14600
		mbedtls_ssl_server2 server_port=$port > /tmp/curl-srv.$$ 2>&1 &
		spid=$!
		i=0
		while [ $i -lt 30 ] && ! grep 'Waiting for a remote' /tmp/curl-srv.$$ >/dev/null; do
			sleep 1; i=$((i + 1))
		done
		ok=yes
		for v in "--tlsv1.2 --tls-max 1.2" "--tlsv1.3"; do
			# shellcheck disable=SC2086
			r=$(no_proxy=localhost src/curl -sS $v --cacert /tmp/curl-testca.$$ https://localhost:$port/ 2>&1) || ok=no
			echo "  $v: $(echo "$r" | sed -n 's/.*Successful connection using: \([^<]*\).*/\1/p')"
		done
		no_proxy=localhost src/curl -sS -o /dev/null https://localhost:$port/ 2>/dev/null
		st=$?
		echo "  without its CA: status $st (60: the certificate isn't trusted)"
		[ $st = 60 ] || ok=no
		kill $spid 2>/dev/null; wait $spid 2>/dev/null
		rm -f /tmp/curl-testca.$$ /tmp/curl-srv.$$
		[ $ok = yes ] || fail "HTTPS to localhost"
	else
		say "test: HTTPS to localhost skipped (no Mbed TLS sources or mbedtls_ssl_server2)"
	fi
	say "test: the Internet: https://curl.se/${https_proxy:+ (through $https_proxy)}"
	if r=$(src/curl -sS -o /dev/null -w '%{http_code} from %{remote_ip}' https://curl.se/ 2>/tmp/curl-err.$$); then
		echo "  status $r"
	else
		echo "  not reached: $(cat /tmp/curl-err.$$)"
	fi
	rm -f /tmp/curl-err.$$
fi

[ $inst = yes ] || { say "done: $(pwd)/src/curl"; exit 0; }
say "install"
run make install
say "done: curl (try: curl -I https://curl.se/)"
