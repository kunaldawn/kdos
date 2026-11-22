# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

export LDFLAGS="$LDFLAGS -Wl,-z,stack-size=8388608"

./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--localstatedir=/var --disable-static \
	--enable-avahi --with-rcdir=no --disable-nls \
	--with-cups-rundir=/run/cups \
	--with-cups-domainsocket=/run/cups/cups.sock
make
make DESTDIR=$PKG install
