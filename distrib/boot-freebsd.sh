#!/bin/sh
# boot-freebsd.sh [-o DIR] [-n]: build init, sh, getty, login and a few utilities with clang
# against lib/libc, make a root of them and the tree's etc/, put it in a bootable image
# (mkimage-freebsd.sh) and boot that in QEMU on the serial console.  -n: stop after the image.
# DIR (default obj/boot) has libc/ src/ root/ and v86bsd.img.
out=obj/boot nobuild=
while getopts o:n opt; do case $opt in o) out=$OPTARG ;; n) nobuild=1 ;; *) exit 2 ;; esac; done
top=$(cd "$(dirname "$0")/.." && pwd); cd "$top" || exit 1
mkdir -p "$out" && out=$(cd "$out" && pwd) || exit 1
B=$out/libc src=$out/src root=$out/root
(cd lib/libc && make -s -j12 B="$B" all) > "$out/libc.log" 2>&1 || { tail "$out/libc.log"; exit 1; }
export LIBC_LIB=$B/lib LIBC_LINK=dynamic LIBC_INTERP=/lib/libc.so
cc="sh $top/lib/libc/tools/libc-cc clang"
rm -rf "$src" "$root"; mkdir -p "$src/lib" "$root"
cp -R lib/libxcu lib/libcurses "$src/lib/"
for d in yacc lex; do cp -R usr.bin/$d "$src/yl-$d"; (cd "$src/yl-$d" && make -s CC=clang) > "$out/host-$d.log" 2>&1 || echo "host $d failed"; done
YL="YACC=$src/yl-yacc/yacc LEX=$src/yl-lex/lex"
CF="-O2 -w -D_DEFAULT_SOURCE -fPIE"
(cd "$src/lib/libcurses" && make -s CC="$cc" CFLAGS="$CF -fPIC" $YL) > "$out/libcurses.log" 2>&1 || echo "libcurses failed"
: > "$out/failed"
# every program of the tree is built and installed (make install) into its directory of the root
for k in bin sbin libexec usr.bin usr.sbin; do cp -R "$k" "$src/$k"; done
cat > "$out/one.sh" <<EOT
#!/bin/sh
p=\$1; k=\${p%%/*}; n=\${p#*/}
case \$k in bin) dst=/bin ;; sbin) dst=/sbin ;; libexec) dst=/usr/libexec ;; usr.bin) dst=/usr/bin ;; usr.sbin) dst=/usr/sbin ;; esac
cd "$src/\$p" || exit 1
if [ "\$p" = sbin/init ]; then
	make -s install C17="env LIBC_LINK=static $cc" CFLAGS="$CF" DESTDIR="$root" PREFIX= BINDIR=\$dst
else
	make -s install CC="$cc" CFLAGS="$CF" LDFLAGS="-L$src/lib/libcurses" $YL DESTDIR="$root" PREFIX= BINDIR=\$dst
fi > "$out/build-\$k-\$n.log" 2>&1 || echo "\$p" >> "$out/failed"
EOT
for k in bin sbin libexec usr.bin usr.sbin; do for d in $k/*/; do [ -f "$d/Makefile" ] && echo "${d%/}"; done; done | grep -v '^usr.bin/c17$' |
	xargs -P 8 -n 1 sh "$out/one.sh"
echo "boot: failed: $(tr '\n' ' ' < "$out/failed")"
# FreeBSD's UFS tools (distrib/ufstools-freebsd.sh) against the same libc
sh distrib/ufstools-freebsd.sh -o "$out/ufstools" -l "$B/lib" > "$out/ufstools.log" 2>&1 || echo "ufstools failed"
mkdir -p "$root/sbin"
for t in dumpfs fsck_ffs growfs newfs tunefs; do cp "$out/ufstools/bin/$t" "$root/sbin/$t" || echo "no $t"; done
ln -sf fsck_ffs "$root/sbin/fsck_ufs"
mkdir -p "$root/lib" "$root/etc" "$root/root" "$root/tmp" "$root/var/run" "$root/var/log" "$root/dev" "$root/mnt" "$root/usr/share"
cp "$B/lib/libc.so" "$root/lib/libc.so"
cp -R etc/. "$root/etc/"
chmod 600 "$root/etc/master.passwd"
[ -n "$nobuild" ] && exit 0
sh distrib/mkimage-freebsd.sh -o "$out/v86bsd.img" -s 256 -d "$root" || exit 1
exec qemu-system-x86_64 -machine q35 -accel kvm -cpu host -m 512 -smp 2 -nographic \
	-drive file="$out/v86bsd.img",format=raw,if=virtio -nic user,model=virtio-net-pci
