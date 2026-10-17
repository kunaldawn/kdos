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

export CXXFLAGS="$CXXFLAGS -D__MUSL__"
./configure --prefix=/usr --libdir=/usr/lib --sysconfdir=/etc/R \
	--localstatedir=/var --mandir=/usr/share/man \
	rdocdir=/usr/share/doc/R \
	rincludedir=/usr/include/R \
	rsharedir=/usr/share/R \
	--enable-R-shlib \
	--disable-R-static-lib \
	--disable-java \
	--disable-nls \
	--enable-openmp \
	--with-recommended-packages \
	--with-blas=-lopenblas \
	--with-lapack \
	--with-readline \
	--with-pcre2 \
	--with-ICU \
	--with-cairo \
	--with-libpng \
	--with-jpeglib \
	--with-libtiff \
	--with-libdeflate-compression \
	--with-x \
	--with-tcltk \
	--with-tcl-config=/usr/lib/tclConfig.sh \
	--with-tk-config=/usr/lib/tkConfig.sh
make
make DESTDIR=$PKG install
make -C src/unix DESTDIR=$PKG LIBR_LDFLAGS= install-pc

make -C src/nmath/standalone shared
make -C src/nmath/standalone DESTDIR=$PKG install
rm -f "$PKG"/usr/lib/libRmath.a

test -e "$PKG"/usr/lib/R/lib/libR.so
ln -s R/lib/libR.so "$PKG"/usr/lib/libR.so
for p in Matrix MASS lattice survival nlme mgcv; do
	test -d "$PKG/usr/lib/R/library/$p"
done
test -e "$PKG"/usr/lib/R/library/tcltk/libs/tcltk.so
test -e "$PKG"/usr/lib/R/modules/R_X11.so
