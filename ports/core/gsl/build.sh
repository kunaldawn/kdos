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

# --with-blas-lib POINTS IT AT OpenBLAS RATHER THAN ITS OWN. GSL ships a
# reference CBLAS that is correct and roughly an order of magnitude slower;
# leaving it in place means every GSL consumer silently gets the slow one on a
# machine that has a tuned BLAS installed.
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--disable-static \
	--with-external-cblas
make GSL_CBLAS_LIB=-lopenblas
make DESTDIR=$PKG install

# A PUBLIC HEADER MUST BE VALID UTF-8, and upstream's is ISO-8859-1: an author
# name in gsl_complex_math.h's copyright line. Anything that reads a header AS
# TEXT rather than as bytes then fails on `invalid or incomplete multibyte or
# wide character` — brltty's Tcl dependency scanner is the one that found the
# same defect in avahi. Re-encoding is lossless and is done here rather than
# worked around in each consumer.
find "$PKG" -name '*.h' | while read -r h; do
	iconv -f UTF-8 -t UTF-8 "$h" >/dev/null 2>&1 && continue
	iconv -f ISO-8859-1 -t UTF-8 "$h" > "$h.utf8" && mv -f "$h.utf8" "$h"
done
