# config

`config` makes the compile directory of a kernel from its configuration,
as BSD's config(8) does (a small subset of OpenBSD's).

    config [-s srcdir] [-b builddir] configfile

It is meant to be run in the directory of the configurations, `sys/arch/amd64/conf`:

    cd sys/arch/amd64/conf
    config GENERIC            # writes ../compile/GENERIC
    cd ../compile/GENERIC
    make                      # bsd

`-s` is the `sys/` directory (default `../../..`), `-b` the directory to write
(default `../compile/NAME`; it is made if it is not there).  `make buildkernel
KERNCONF=NAME` (distrib/build.sh) does this with the config built by the stage
before.

## What it reads

* `srcdir/conf/files`, the machine-independent sources and devices;
* `srcdir/arch/MACHINE/conf/files.MACHINE`, those of the machine, where
  MACHINE is the `machine` line of the configuration;
* the configuration file.

`files` and `files.MACHINE` have these lines (`#` starts a comment):

    defopt NAME...               options a configuration may set
    device NAME [{loc = default, loc...}]
                                 a driver.  The devices that attach to it can be
                                 given the locators (default -1)
    attach NAME at PARENT...     where NAME attaches: devices (their name is
                                 the name of what they offer), or root.  The
                                 driver defines NAME_cd (struct cfdriver) and
                                 NAME_ca (struct cfattach)
    file PATH [CONDITION]        a .c or .s source, from srcdir

A condition is a device that is in the configuration, an option that is set,
`!`, `&`, `|` and parentheses: `file dev/pv/virtio.c virtio | vioblk`.  A file
without one is always compiled.  The sources are compiled in the order of the
lines, the files of `conf/files` first.

The configuration file (`#` starts a comment):

    machine NAME                 first: the machine
    option NAME                  an option (declared by defopt): #define NAME 1
    option NAME=VALUE            ... #define NAME VALUE
    config bsd [root on DEVICE]  the kernel is bsd; DEVICE is written to
                                 ioconf.c as cf_rootdev
    NAME0 at PARENT0 [locator number]...
    NAME* at PARENT?             a device: a unit, or * (as many as are found,
                                 any unit); a parent unit, or ? (any)

`com0 at isa0 port 0x3f8 irq 4` is the serial port at that address and
interrupt (the locators `port` and `irq` of `isa`).  `disk*` can be at more
than one parent (two lines): the units are one series, as they attach.

## What it writes

In the compile directory:

* `Makefile`: the template `srcdir/arch/MACHINE/conf/Makefile.MACHINE`, in which
  the lines `%OBJS%` (the objects), `%HDRS%` (every header of the tree and of
  the machine, for the dependencies) and `%RULES%` (the rule for each source)
  and the words `%S%` (the relative path of `srcdir`), `%CONF%` and `%MACHINE%`
  are replaced;
* `ioconf.c`: the table of devices, `cfdata[]` (`struct cfdata`,
  `sys/sys/device.h`);
* `locators.h`: `ISACF_PORT` and the rest, the index of each locator;
* `options.h`: the options as `#define`s, which `<sys/param.h>` includes;
* `machine`: a link to `arch/MACHINE/include`, for `<machine/...>`.

Nothing in them depends on the date, the host or an absolute path: the same
three inputs make the same files wherever config runs (the kernel is
byte-identical when built in the tree on the host and inside the system),
and a file that would not change is not written again, so that make does
not compile what did not change.  The sources are named in the Makefile as
`$(S)/kern/...` with a relative `S`, which is what `__FILE__` is in the kernel.

Errors are reported with the file and line, and the status is 1; usage
errors give 2.
