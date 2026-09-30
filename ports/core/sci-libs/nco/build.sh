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
# udunits2 is a port, so `-u` unit conversion works — the thing that makes
# `units = "kg m-2 s-1"` a quantity rather than a string. ncap2, the expression
# language, links the ANTLR2 C++ runtime (libantlr and its antlr/ headers) and
# configure also looks for the runantlr program; antlr2 is not a port, so that
# one operator is absent and every other one — the subsetters, ncbo, ncra,
# ncea, ncflint, ncwa — is here.
#
# DAP, GSL and OpenMP are each probed and dropped in silence when the probe
# fails, so all three are spelled out and their providers are in depends: DAP
# needs a libnetcdf built with its DAP client plus curl, GSL needs gsl-config,
# and OpenMP needs gcc's libgomp. --enable-doc builds only the info manual,
# `info nco`, which is the full reference; it is skipped when makeinfo is
# missing, hence texinfo.
./autogen.sh || autoreconf -fi
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--disable-ncap2 --enable-udunits2 --enable-dap --enable-gsl \
	--enable-openmp --enable-doc
make
make DESTDIR=$PKG install
rm -f "$PKG/usr/share/man/man1/ncap2.1"
