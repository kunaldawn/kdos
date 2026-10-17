# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

patch -p1 -i "$PORT_SRC/fix-neqn-wrapper.patch"

./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--disable-rpath \
	--without-x \
	--with-uchardet=yes \
	--with-gs=gs \
	--without-urw-fonts \
	--with-compatibility-wrappers=no
make
make DESTDIR=$PKG install

rm -f "$PKG/usr/bin/soelim" \
	"$PKG/usr/share/man/man1/soelim.1" \
	"$PKG/usr/share/man/man7/roff.7"
rm -f "$PKG/usr/lib/charset.alias"
