# HOWTO: build Mbed TLS inside Shall

Mbed TLS 3.6.7 (the long-term-support branch) — a TLS 1.2/1.3, X.509 and
cryptography library in portable C — builds inside a Shall system with
c17 and ar, and passes its own tests: the self-tests of every algorithm,
TLS handshakes between its test client and server, and, if you generate
them on the host, its 140 unit test suites (44 000 test cases).  It is
the TLS library of `ports/curl`.

## 1. Get the sources (on the host)

    sh ports/get.sh mbedtls

downloads the release tarball,
<https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-3.6.7/mbedtls-3.6.7.tar.bz2>,
into `root/usr/src/mbedtls-3.6.7`.  Take the tarball rather than a git
clone: the clone lacks the `framework/` submodule (and the files
generated from it).

## 2. Build, test and install (inside)

    sh /usr/src/shall/ports/mbedtls/build.sh -t

applies `shall.diff` (once) and compiles, with `c17 -O 1`, everything as
Mbed TLS's makefiles would: `library/*.c` into `libmbedcrypto.a`,
`libmbedx509.a` and `libmbedtls.a` (the files of each as `library/Makefile`
lists them), the bundled `3rdparty/` code, the test helpers, and some
programs.  `-t` then runs:

- `selftest`: the self-tests of every algorithm in the configuration
  (AES, ChaCha20-Poly1305, SHA-2/3, RSA, ECP, ECDSA, the DRBGs, ...):
  `[ All tests PASS ]`.
- a TLS 1.2 and a TLS 1.3 handshake, and an HTTP exchange over each,
  between `ssl_server2` and `ssl_client2` on the loopback, with the test
  certificates built into them.

and the script installs:

| Where | What |
|---|---|
| `/usr/local/lib` | `libmbedtls.a`, `libmbedx509.a`, `libmbedcrypto.a` |
| `/usr/local/include` | `mbedtls/`, `psa/` |
| `/usr/local/bin` | `mbedtls_selftest`, `mbedtls_ssl_client2`, `mbedtls_ssl_server2` (test client and server, with many options: `mbedtls_ssl_client2 server_name=example.org server_port=443`), `mbedtls_gen_key`, `mbedtls_cert_app` |

Link programs with `-lmbedtls -lmbedx509 -lmbedcrypto` (in that order).
`-I` builds only.  The output goes to `build.log`.

## 3. The unit test suites (optional; Python on the host)

Mbed TLS keeps its unit tests as `.function` and `.data` files that a
Python script turns into C.  Shall has no Python (it isn't POSIX), so
generate them on the host, then build and run them inside:

    sh ports/mbedtls/gentests.sh                             # on the host: python3
    sh /usr/src/shall/ports/mbedtls/build.sh -t -T           # inside

`-T` compiles each of the 140 suites with c17 and runs it (one log each,
`tests/test_suite_NAME.log`).  One suite is not built:
`test_suite_platform_unix` tests `gettimeofday()` itself, which
POSIX.1-2024 removed.  Result here: 139 suites passed, none failed: of
their 43 985 test cases 31 291 ran and passed, and 12 694 were skipped
because they need options this configuration hasn't (other
configurations, hardware acceleration).  It takes about 20 minutes.

## The changes (`shall.diff`)

- **No inline assembly** (`include/mbedtls/mbedtls_config.h`): c17
  doesn't have `asm`, so the configuration leaves out
  `MBEDTLS_HAVE_ASM` (the multiply-accumulate loops of the bignum code
  then use plain C), `MBEDTLS_AESNI_C` (AES with the CPU's AES-NI
  instructions; the table-driven C version is used) and
  `MBEDTLS_PADLOCK_C` (VIA's).  The results are the same, only slower:
  AES and RSA most.
- **`gettimeofday()`** (`library/timing.c`, `mbedtls_timing_get_timer`):
  removed by POSIX.1-2024; `clock_gettime(CLOCK_MONOTONIC)` instead, when
  `_POSIX_VERSION` is 202405 or more.  (The `benchmark` program also uses
  `gettimeofday()` and isn't built.)

Mbed TLS's makefiles aren't used: they are GNU make's (`ifdef`,
`$(wildcard)`, `$(patsubst)`, `$(shell)`), and they would regenerate files
with Python and Perl.  The tarball has those files already.

## Tested

Built by `build.sh -t -T` inside a freshly bootstrapped Shall system in
a chroot: selftest passes, TLS 1.2 (TLS-ECDHE-RSA-WITH-CHACHA20-POLY1305-SHA256)
and TLS 1.3 (TLS1-3-CHACHA20-POLY1305-SHA256) handshakes between
`ssl_client2` and `ssl_server2`, and the unit suites as above.  With
`ports/curl` it fetched files over HTTPS from the Internet, in the chroot
and on Shall's own kernel in QEMU.
