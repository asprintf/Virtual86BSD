# HOWTO: build curl inside Shall — and use the Internet

curl 8.23.0 builds inside a Shall system with its own `configure`
(autoconf and libtool) and makefiles, on Mbed TLS (`ports/mbedtls`) for
TLS.  With it the system reaches the Internet: in the chroot through the
host's network, and on Shall's own kernel in QEMU through its TCP/IP
stack and QEMU's user networking.

## 1. Get the sources (on the host)

    sh ports/get.sh mbedtls
    sh ports/get.sh curl

`curl` downloads the release tarball,
<https://github.com/curl/curl/releases/download/curl-8_23_0/curl-8.23.0.tar.gz>,
into `root/usr/src/curl-8.23.0` (a git clone has no `configure`).

curl checks the servers' certificates against
`/etc/ssl/certs/ca-certificates.crt`, which `bootstrap.sh` copies from the
host (`CABUNDLE=file` names another file).  For a system made before
that, copy it yourself, or take Mozilla's list as the curl project
publishes it:

    mkdir -p root/etc/ssl/certs
    cp /etc/ssl/certs/ca-certificates.crt root/etc/ssl/certs/
    # or: curl -o root/etc/ssl/certs/ca-certificates.crt https://curl.se/ca/cacert.pem

## 2. Build, test and install (inside)

    sh /usr/src/shall/ports/mbedtls/build.sh -t
    sh /usr/src/shall/ports/curl/build.sh -t

That applies `shall.diff` (once), then runs, with `CC=c17`:

    LD=c17 CFLAGS='-O 1' ./configure --prefix=/usr/local \
        --with-mbedtls=/usr/local --without-zlib --without-brotli --without-zstd \
        --without-libpsl --disable-shared \
        --with-ca-bundle=/etc/ssl/certs/ca-certificates.crt --without-ca-path
    make
    make install

`-t` tests, before installing:

- a `file:` URL;
- HTTPS with TLS 1.2 and with TLS 1.3 to `mbedtls_ssl_server2` on
  `localhost`, verified against Mbed TLS's test CA (from its sources in
  `/usr/src/mbedtls-3.6.7`) — and refused (status 60) without it;
- one request to the Internet, `https://curl.se/`: its result is shown,
  but a failure (no network) doesn't fail the test.

Installed under `/usr/local`: `bin/curl`, `bin/curl-config`,
`bin/wcurl` (a script: `wcurl URL` downloads to a file), `lib/libcurl.a`,
`include/curl/`.  `-I` builds only.  The output goes to `build.log`.

## 3. The Internet

**In the chroot** (`sh chroot.sh`) the system shares the host's network:
names are looked up with the host's name servers (`chroot.sh` copies
`/etc/resolv.conf`), connections go out as the host's do.

    curl -I https://curl.se/
    curl -LO https://github.com/curl/curl/releases/download/curl-8_23_0/curl-8.23.0.tar.gz

**On Shall's kernel in QEMU** (`sh mkdisk.sh; sh boot.sh`), the e1000
network card gets 10.0.2.15 from QEMU's DHCP server, and connections go
out through QEMU on the host ("user networking", no root needed); the
name server is QEMU's, 10.0.2.3 (`/etc/resolv.conf` is a link to the
kernel's `/proc/net/pnp`, as DHCP gave it).  The host itself is 10.0.2.2.
The same commands work.  (QEMU's user networking passes TCP and UDP, not
ping, except to 10.0.2.2.)

**Behind a proxy**, as the host may be, tell curl: `https_proxy` and
`http_proxy` in the environment, or `-x`.  In QEMU, a proxy that listens
on the host's 127.0.0.1 is at 10.0.2.2:

    https_proxy=http://10.0.2.2:3128 curl -I https://curl.se/

## The change (`shall.diff`)

- **`<curl/system.h>`** picks curl's types (`curl_off_t`,
  `curl_socklen_t`) by compiler.  c17 isn't GCC, so it fell through to a
  "generic safe guess on old 32-bit style": `curl_socklen_t` an `int`,
  where `socklen_t` is `unsigned int`, which made c17 warn about
  mismatched pointer types (harmless, but wrong).  The diff adds c17
  (LP64) with GCC's choices.

And on configure's command line, `LD=c17`: libtool stops when it finds no
link editor, though here the compiler links.

## Found on the way (in Shall)

curl's `configure` and `make install` showed three bugs in Shall's
utilities, fixed in this version (with tests):

- **`ls -t`** sorted file operands by name, not by time.  automake's
  "build environment is sane" check (in every automake-made configure)
  runs `ls -t configure conftest.file` and stopped: "newly created file is
  older than distributed files".  Operands are now sorted as directory
  entries are (`-t`, `-S`, `-r`; as given with `-f`).
- **`awk`** didn't accept a new pattern-action item after a
  backslash-newline (`BEGIN {...} \` + newline + `{...}`), which
  automake's `install-binPROGRAMS` rule uses: `make install` installed no
  programs.
- **`cp -p`** reported an error when the owner can't be set with EINVAL
  (an ID outside a user namespace's mapping, as in `chroot.sh -m
  unshare`); that is now treated as EPERM is: not an error, set-ID bits
  cleared.

## What you don't get

- **HTTP/2 and HTTP/3** (nghttp2, ngtcp2): not ported.
- **Compression** (`--compressed`: zlib, brotli, zstd): not ported.
- **IDN** (international domain names) and the **public suffix list**
  (libpsl, for cookies): not ported.
- **SSH** (scp, sftp: libssh2): not ported.
- **curl's test suite**: it needs Perl (and servers written in Perl and
  Python), which Shall doesn't have.

## Tested

Built by `build.sh -t` inside a freshly bootstrapped Shall system in a
chroot: the file: URL, TLS 1.2 (TLS-ECDHE-RSA-WITH-CHACHA20-POLY1305-SHA256)
and TLS 1.3 (TLS1-3-CHACHA20-POLY1305-SHA256) to `mbedtls_ssl_server2`,
refusal without the CA.  From the chroot and from Shall's kernel in
QEMU (emulation): HTTPS directly to github.com (names looked up by
Shall's resolver, the certificate verified), and the 4.5 MB curl tarball
over HTTPS through an HTTP proxy (CONNECT), byte-identical (cksum) to the
original; from QEMU also over plain HTTP from the host (10.0.2.2).
