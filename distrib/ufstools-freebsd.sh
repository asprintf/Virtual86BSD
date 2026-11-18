#!/bin/sh
# ufstools-freebsd.sh [-o DIR] [-l LIBCDIR]: build FreeBSD's UFS tools (libufs, the libutil pieces they use,
# fsck_ffs newfs tunefs dumpfs growfs) with clang against FreeBSD's installed headers and this tree's libc.
# Result: DIR/bin/ (default obj/ufstools).  LIBCDIR (default obj/boot/libc/lib) has libc.so and the crt files.
out=obj/ufstools lib=obj/boot/libc/lib
while getopts o:l: opt; do case $opt in o) out=$OPTARG ;; l) lib=$OPTARG ;; *) exit 2 ;; esac; done
top=$(cd "$(dirname "$0")/.." && pwd); cd "$top" || exit 1
mkdir -p "$out/bin" "$out/o" && out=$(cd "$out" && pwd) lib=$(cd "$lib" && pwd) || exit 1
inc=$(find "$top/obj/freebsd/obj" -type d -path "*/amd64.amd64/tmp/usr/include" 2>/dev/null | head -n 1)
[ -f "$inc/ufs/ffs/fs.h" ] || { echo "ufstools: no installed headers in $inc (run build-freebsd.sh loader)" >&2; exit 1; }
cc="sh $top/lib/libc/tools/libc-cc clang"
export LIBC_LIB=$lib LIBC_LINK=dynamic LIBC_INTERP=/lib/libc.so
F=$top/freebsd
mkdir -p "$out/inc" && ln -sfn "$inc/ufs" "$out/inc/ufs"   # FreeBSD's ufs headers, not the old ones of include/
# the BSD type names and limits FreeBSD's headers expect and this tree's headers do not define
cat > "$out/o/shim.h" <<'EOT'
#define _SYS_STDINT_H_
#define _SYS__TIMESPEC_H_
#define _SYS__TIMEVAL_H_
#include <stdint.h>
#include <sys/types.h>
typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;
typedef unsigned long u_long;
typedef uint8_t u_int8_t;
typedef uint16_t u_int16_t;
typedef uint32_t u_int32_t;
typedef uint64_t u_int64_t;
#define SPECNAMELEN 255
typedef int64_t daddr_t;
typedef char *caddr_t;
#include <sys/_maxphys.h>
#include <stddef.h>
typedef unsigned int uint;
typedef int64_t quad_t;
typedef uint64_t u_quad_t;
#include <time.h>
#define UF_NODUMP 0x00000001
#define _PATH_UFSSUSPEND "/dev/ufssuspend"
static inline uint16_t le16dec(const void *p) { const uint8_t *b = p; return b[0] | b[1] << 8; }
static inline uint32_t le32dec(const void *p) { const uint8_t *b = p; return b[0] | b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24; }
static inline void le16enc(void *p, uint16_t v) { uint8_t *b = p; b[0] = v; b[1] = v >> 8; }
static inline void le32enc(void *p, uint32_t v) { uint8_t *b = p; b[0] = v; b[1] = v >> 8; b[2] = v >> 16; b[3] = v >> 24; }
#define SF_IMMUTABLE 0x00020000
#define SF_NOUNLINK 0x00100000
#define SF_SNAPSHOT 0x00200000
#define NODEV ((dev_t)(-1))
#define SIGINFO 29
#define MNT_ROOTFS 0x0000000000004000ULL
#define MNT_SOFTDEP 0x0000000000200000ULL
#define P_OSREL_CK_CYLGRP 1200046
#define P_OSREL_CK_SUPERBLOCK 1300000
#define P_OSREL_CK_INODE 1300005
#define getosreldate() 1502000
#define CLOCK_REALTIME_PRECISE 9
#define setproctitle(...) ((void)0)
#define timespecadd(a, b, r) do { (r)->tv_sec = (a)->tv_sec + (b)->tv_sec; \
	(r)->tv_nsec = (a)->tv_nsec + (b)->tv_nsec; \
	if ((r)->tv_nsec >= 1000000000) { (r)->tv_sec++; (r)->tv_nsec -= 1000000000; } } while (0)
#define timespecsub(a, b, r) do { (r)->tv_sec = (a)->tv_sec - (b)->tv_sec; \
	(r)->tv_nsec = (a)->tv_nsec - (b)->tv_nsec; \
	if ((r)->tv_nsec < 0) { (r)->tv_sec--; (r)->tv_nsec += 1000000000; } } while (0)
#ifndef __offsetof
#define __offsetof(t, m) __builtin_offsetof(t, m)
#endif
#ifndef roundup2
#define roundup2(x, y) (((x) + ((y) - 1)) & (~((y) - 1)))
#define rounddown2(x, y) ((x) & (~((y) - 1)))
#endif
#define bswap16 __builtin_bswap16
#define bswap32 __builtin_bswap32
#define bswap64 __builtin_bswap64
static inline int fls(int i) { return i ? 32 - __builtin_clz(i) : 0; }
#define CLOCK_REALTIME_FAST 10
#include <strings.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ucred.h>
struct oexport_args {
	int	ex_flags;
	uid_t	ex_root;
	struct	xucred ex_anon;
	struct	sockaddr *ex_addr;
	u_char	ex_addrlen;
	struct	sockaddr *ex_mask;
	u_char	ex_masklen;
	char	*ex_indexfile;
};
EOT
CF="-include $out/o/shim.h -O2 -w -fPIE -D_DEFAULT_SOURCE -I$out/inc -isystem $inc -D_LIBUFS -I$F/lib/libufs -I$F/lib/libutil"
libufs="$F/lib/libufs/block.c $F/lib/libufs/cgroup.c $top/sys/libkern/gsb_crc32.c $F/lib/libufs/inode.c
	$F/lib/libufs/sblock.c $F/lib/libufs/type.c $top/sys/ufs/ffs/ffs_subr.c $top/sys/ufs/ffs/ffs_tables.c"
libutil="$F/lib/libutil/expand_number.c $F/lib/libutil/humanize_number.c $F/lib/libutil/mntopts.c"
for s in $libufs $libutil; do $cc $CF -c "$s" -o "$out/o/$(basename "${s%.c}").o" || exit 1; done
ar=llvm-ar; $ar rcs "$out/o/libufsutil.a" "$out"/o/*.o || exit 1
one() {	# name sources...
	n=$1; shift
	$cc $CF -I"$F/sbin/$n" -I"$top/sys/ufs/ffs" "$@" "$out/o/libufsutil.a" -o "$out/bin/$n" || echo "ufstools: $n failed"
}
S=$F/sbin
one fsck_ffs $S/fsck_ffs/*.c
# no /etc/disktab here: newfs -T finds no disk type
printf 'void *getdiskbyname(const char *n) { (void)n; return 0; }\n' > "$out/o/getdiskbyname.c"
one newfs $out/o/getdiskbyname.c $S/newfs/newfs.c $S/newfs/mkfs.c $top/sys/geom/geom_bsd_enc.c
one tunefs $S/tunefs/tunefs.c
one dumpfs $S/dumpfs/dumpfs.c
one growfs $S/growfs/growfs.c
ls "$out/bin"
