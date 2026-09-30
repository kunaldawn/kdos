# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# THE CLIENT HALF ONLY: libldap, liblber and the ldap* tools, which is what
# curl's ldap:// URLs, dirmngr's keyserver and certificate lookups and
# postgres's LDAP authentication link. --disable-slapd leaves out the server
# and every backend; a directory here is somebody else's. The server's manual
# pages install regardless and are removed below, since they document programs
# this package does not carry.
#
# TLS is OpenSSL's, as for every other client on the image. SASL binds go
# through cyrus-sasl and whatever mechanism plugins it carries.
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
