# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# pdfio's own configure, not autotools: the shared library is opt-in and the
# default install is libpdfio.a alone, which nothing here links against.
# libpng is named so pdfioFileCreateImageObjFromFile reads PNG rather than
# refusing it, which is what it does when the probe finds nothing.
./configure --prefix=/usr --libdir=/usr/lib \
	--enable-shared --disable-static --enable-libpng
make
make DESTDIR=$PKG install
