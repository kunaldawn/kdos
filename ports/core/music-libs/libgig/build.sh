# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

patch -p1 -i "$PORT_SRC/pc-libdir.patch"
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib --disable-static
make pkglibdir=/usr/lib
make pkglibdir=/usr/lib DESTDIR=$PKG install

test -e "$PKG/usr/lib/libgig.so"
test -e "$PKG/usr/lib/libakai.so"
grep -q "^libdir=/usr/lib$" "$PKG/usr/lib/pkgconfig/gig.pc"
