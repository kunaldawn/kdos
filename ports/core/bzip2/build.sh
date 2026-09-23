#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# Two makefiles build one set of object names: Makefile-libbz2_so compiles
# them position-independent for libbz2.so, and the clean between is what makes
# the static Makefile recompile them for libbz2.a instead of archiving the PIC
# ones. bzip2-shared, linked against the shared library, replaces the
# statically linked bzip2 the install copies three times.
make -f Makefile-libbz2_so CFLAGS="$CFLAGS -fPIC"
make clean
make CFLAGS="$CFLAGS"
make PREFIX=$PKG/usr install

install -Dm755 libbz2.so.$version -t $PKG/usr/lib
ln -sf libbz2.so.$version $PKG/usr/lib/libbz2.so.1.0
ln -sf libbz2.so.$version $PKG/usr/lib/libbz2.so
install -m755 bzip2-shared $PKG/usr/bin/bzip2
ln -sf bzip2 $PKG/usr/bin/bunzip2
ln -sf bzip2 $PKG/usr/bin/bzcat

# The tarball ships no pkg-config file; meson's dependency('bzip2') and
# pkg-config probes read this one.
install -Dm644 /dev/stdin $PKG/usr/lib/pkgconfig/bzip2.pc <<EOF
prefix=/usr
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: bzip2
Description: Block-sorting file compression library
Version: $version
Libs: -L\${libdir} -lbz2
Cflags: -I\${includedir}
EOF

install -d $PKG/usr/share
mv $PKG/usr/man $PKG/usr/share/man
for l in bzegrep:bzgrep bzfgrep:bzgrep bzless:bzmore bzcmp:bzdiff; do
	ln -sf ${l#*:} $PKG/usr/bin/${l%:*}
done
