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

# Upstream's makefiles build static archives only. The objects are compiled
# here once with -fPIC and linked into two shared libraries, so the printer
# drivers that link libjbig85 (splix) and anything linking libjbig take the
# library from this package instead of carrying a private copy.
cd libjbig
for f in jbig jbig85 jbig_ar; do
	gcc $CFLAGS -fPIC -c $f.c -o $f.o
done
gcc -shared -Wl,-soname,libjbig.so.$version -o libjbig.so.$version jbig.o jbig_ar.o $LDFLAGS
gcc -shared -Wl,-soname,libjbig85.so.$version -o libjbig85.so.$version jbig85.o jbig_ar.o $LDFLAGS
ln -sf libjbig.so.$version libjbig.so
ln -sf libjbig85.so.$version libjbig85.so
cd ../pbmtools
for t in pbmtojbg jbgtopbm; do
	gcc $CFLAGS -I../libjbig -o $t $t.c -L../libjbig -ljbig $LDFLAGS
done
for t in pbmtojbg85 jbgtopbm85; do
	gcc $CFLAGS -I../libjbig -o $t $t.c -L../libjbig -ljbig85 $LDFLAGS
done
cd ..

install -Dm644 -t "$PKG/usr/include" libjbig/jbig.h libjbig/jbig85.h libjbig/jbig_ar.h
install -d "$PKG/usr/lib"
install -m755 libjbig/libjbig.so.$version libjbig/libjbig85.so.$version "$PKG/usr/lib"
ln -s libjbig.so.$version "$PKG/usr/lib/libjbig.so"
ln -s libjbig85.so.$version "$PKG/usr/lib/libjbig85.so"
install -Dm755 -t "$PKG/usr/bin" pbmtools/pbmtojbg pbmtools/jbgtopbm \
	pbmtools/pbmtojbg85 pbmtools/jbgtopbm85
install -Dm644 -t "$PKG/usr/share/man/man1" pbmtools/pbmtojbg.1 pbmtools/jbgtopbm.1
