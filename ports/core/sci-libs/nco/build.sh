# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

./autogen.sh || autoreconf -fi
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--enable-ncap2 --enable-udunits2 --enable-dap --enable-gsl \
	--enable-openmp --enable-doc
make
make DESTDIR=$PKG install
test -x "$PKG/usr/bin/ncap2"
