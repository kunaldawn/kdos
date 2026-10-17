# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

autoreconf -fi
./configure --prefix=/usr --sysconfdir=/etc --localstatedir=/var \
	--libexecdir=/usr/lib --disable-static \
	--with-openssl \
	--with-gnutls \
	--with-seccomp \
	--without-cuse \
	--without-selinux \
	--disable-tests
make
make DESTDIR=$PKG install
rm -rf "$PKG/usr/lib/installed-tests"
