# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--localstatedir=/var --disable-static \
	--disable-ppdc-utils --disable-testppdfile \
	--enable-ghostscript --enable-pdftops --enable-pdftocairo \
	--disable-mutool --disable-acroread \
	--with-pdftops=hybrid --with-cups-rundir=/run/cups
make
make DESTDIR=$PKG install
