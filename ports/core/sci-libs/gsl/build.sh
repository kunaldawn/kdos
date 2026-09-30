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

# THE PATCH POINTS CONSUMERS AT OpenBLAS RATHER THAN GSL'S OWN CBLAS. libgsl
# leaves its cblas_* symbols undefined and each consumer supplies them at link
# time, from whatever gsl.pc and `gsl-config --libs` name. Upstream names its
# reference libgslcblas, which is correct and roughly an order of magnitude
# slower, and configure has no option to change that. libgslcblas is still
# installed, so a consumer that links -lgslcblas by name gets the slow one.
patch -p1 -i "$PORT_SRC/openblas-default-cblas.patch"
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--disable-static
make
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
