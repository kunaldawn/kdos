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

# libR.so is built (--enable-R-shlib): RKWard embeds R through it. BLAS and
# LAPACK are OpenBLAS's, so matrix work runs threaded and vectorised instead
# of on R's reference copy. The fifteen recommended packages ship inside the
# tarball (src/library/Recommended) and are built and installed with R, so
# Matrix, MASS, lattice, survival and the rest work with no network; any other
# CRAN package is installed by the user. The X11 device and the tcltk package
# run under Xwayland; cairo and pango give the png, svg and pdf devices
# anti-aliased text with no display at all. Java is off: rJava is not
# shipped, and R's configure would otherwise probe a JDK and record its paths
# in /usr/lib/R/etc/javaconf. NLS is off: bundled data is English only.
# -D__MUSL__ is recorded in Makeconf, which every package build later reads:
# Rcpp and packages built on it pick their C library code paths by it.
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

# libRmath: R's distribution functions as a plain C library, which other
# programs link without embedding R.
make -C src/nmath/standalone shared
make -C src/nmath/standalone DESTDIR=$PKG install
rm -f "$PKG"/usr/lib/libRmath.a

test -e "$PKG"/usr/lib/R/lib/libR.so
for p in Matrix MASS lattice survival nlme mgcv; do
	test -d "$PKG/usr/lib/R/library/$p"
done
test -e "$PKG"/usr/lib/R/library/tcltk/libs/tcltk.so
test -e "$PKG"/usr/lib/R/modules/R_X11.so
