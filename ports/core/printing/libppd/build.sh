# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# --disable-ppdc-utils: cups 2.4 installs ppdc, ppdhtml, ppdi, ppdmerge and
# ppdpo under the same names, and two packages cannot own one file.
# --disable-acroread: there is no Adobe Reader. --disable-mutool for the same
# reason as libcupsfilters.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--localstatedir=/var --disable-static \
	--disable-ppdc-utils --disable-testppdfile \
	--enable-ghostscript --enable-pdftops --enable-pdftocairo \
	--disable-mutool --disable-acroread \
	--with-pdftops=hybrid --with-cups-rundir=/run/cups
make
make DESTDIR=$PKG install
