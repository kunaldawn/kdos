# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# THE KERNEL HAS AX.25 SOCKETS AND NOTHING IN USERLAND SPEAKS THEM. This is the
# library that reads /etc/ax25/axports, resolves a callsign to an address and
# opens the socket; ax25-tools is what uses it.
# ONE TARBALL, THREE PROJECTS. `linuxax25` is a monorepo — libax25, ax25tools
# and ax25apps each with their own autotools tree — and the release tag only
# says which of the three the version number refers to. Each port builds its
# own subdirectory.
cd libax25
./autogen.sh
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib --disable-static
make
make DESTDIR=$PKG install
