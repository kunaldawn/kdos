# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# --with-rcdir=no: the init script is ours, under fs/etc/init.d, and upstream's
# would land in a directory this init never reads.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--localstatedir=/var --disable-static \
	--enable-avahi --with-rcdir=no --disable-nls \
	--with-cups-rundir=/run/cups \
	--with-cups-domainsocket=/run/cups/cups.sock
make
make DESTDIR=$PKG install
