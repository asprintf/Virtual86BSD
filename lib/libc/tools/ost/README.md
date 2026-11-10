# os-test on V86BSD

[os-test](https://gitlab.com/sortix/os-test) (Sortix; ISC license; not in
this tree) is a set of suites that compare POSIX systems: headers
(`include`, `namespace`), every function at a basic level (`basic`),
`fnmatch`, `regex`, `limits`, `paths`, I/O, processes, signals,
pseudoterminals, stdio and UDP.  Its makefiles need GNU or BSD make;
these files run it with POSIX sh and c17:

* `ost.sh` — the driver (`sh ost.sh build`, `sh ost.sh run`; see its head)
* `cc` — the compiler command os-test is given: gcc's options
  (`-pthread`, `-std=c17`, `-W...`, `-shared`) turned into c17's, and
  `-Werror` as "a warning fails"
* `shim.h` — for a diagnostic run only: `usleep()` and `gettimeofday()`,
  which POSIX removed and V86BSD does not have, for the tests that call them
* `report.sh` — on the host: takes the results out of V86BSD's disk image
  and makes os-test's report (`html/index.html`, `os-test.json`)

The whole run, with os-test unpacked on the host into the tree the image
is made from (`root/root/os-test`, which is `/root/os-test` in V86BSD):

1. Build.  Inside V86BSD:

        cd /root/os-test
        sh /usr/src/libc/tools/ost/ost.sh build

   or, much faster without KVM, in a chroot of `root` before the image is
   made (there `uname` is the host's, so name the system):

        cd /root/os-test
        OS=V86BSD NO_GETIFADDRS=1 sh /usr/src/libc/tools/ost/ost.sh build

   The include and namespace suites only compile; build is all they need.

2. Run, inside V86BSD, on its own kernel (with QEMU's default network card:
   the udp suite needs a LAN), then shut V86BSD down:

        cd /root/os-test
        sh /usr/src/libc/tools/ost/ost.sh run

3. The report, on the host:

        sh libc/tools/ost/report.sh v86bsd.img root/root/os-test

   It runs `e2fsck -p` on the image, copies `/root/os-test/out/v86bsd` to
   `root/root/os-test/out/v86bsd`, builds `misc/html` there if needed and
   writes `html/index.html` and `os-test.json`.  Results of other systems
   in `out/` (e.g. `out/linux` from os-test's `make test` on Linux) are
   compared.

Both `build` and `run` skip what is done already, so a run cut short
continues where it stopped; start again from nothing with `rm -r out` and
the compiled tests removed (a test that did not compile is a small script
printing its outcome, which build also skips).

The results go to `out/v86bsd` (`OS` changes the name; on V86BSD it is
V86BSD, what `uname -s` says).  The compiler is `cc` here, not
`$CC` (V86BSD's profile sets that to `c17`, which does not take gcc's
options); `OST_CC` names another.  On V86BSD, which has no
`getifaddrs()`, `udp/udp.h` is changed (once) to find the LAN without it
(`NO_GETIFADDRS`; set it elsewhere to do the same).  A test that stops the
whole system can be left out with `SKIP=file` (lines `suite/test`).

The results are in `../STATUS.md`, stage 18, and `share/doc/conformance.md`.
