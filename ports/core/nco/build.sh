# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# SUBSET AND REGRID WITHOUT LOADING THE FILE. A climate or weather grid is
# routinely larger than memory, and ncks/ncra/ncwa stream through it — which is
# what makes a machine with no cluster able to answer a question about one.
#
# UDUNITS IS ON AND ncap2 IS NOT, and the split is which dependency exists.
# udunits2 is a port now, so `-u` unit conversion works — the thing that makes
# `units = "kg m-2 s-1"` a quantity rather than a string. ncap2, the expression
# language, is generated from an ANTLR2 grammar; antlr2 is not a port and is a
# dead 2008 Java-era tool, so that one operator is absent and every other one
# — the subsetters, ncbo, ncra, ncea, ncflint, ncwa — is here.
./autogen.sh || autoreconf -fi
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--disable-ncap2 --enable-udunits2 --disable-doc
make
make DESTDIR=$PKG install
