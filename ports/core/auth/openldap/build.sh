# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--localstatedir=/var \
	--disable-static \
	--enable-shared \
	--enable-dynamic \
	--disable-slapd \
	--with-tls=openssl \
	--with-cyrus-sasl \
	--without-systemd \
	--without-fetch
make depend
make
make DESTDIR=$PKG install

rm -f "$PKG"/usr/share/man/man8/*.8 \
	"$PKG"/usr/share/man/man5/slap*.5 \
	"$PKG"/usr/share/man/man5/lloadd*.5
rmdir "$PKG/usr/share/man/man8"
