# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# The X11 mode keeps the Magellan protocol that the proprietary 3dxsrv driver
# speaks; the native spacenavd socket works either way. The library installs
# its pkg-config file under share/pkgconfig, which pkgconf searches.
./configure --prefix=/usr --disable-debug --enable-x11
make
make DESTDIR=$PKG install
rm -f "$PKG/usr/lib/libspnav.a"
