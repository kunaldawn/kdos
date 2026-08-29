# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# DIREWOLF IS THE MODEM; THIS IS THE NETWORK. kissattach turns a KISS TNC into
# an `ax0` interface the kernel routes over, and the NET/ROM and connected-mode
# tools are what make it a store-and-forward network rather than a beacon.
#
# NO X: --with-xutils would build the FLTK and GL front ends, which this host
# has by rule none of.
#
# A RULE-7 PATCH DROPPING TWO SUBDIRS, and the reason it is a patch rather than
# a `make SUBDIRS=` override is that automake's SUBDIRS is RECURSIVE: an
# override on the command line reaches every sub-Makefile too, so `ax25/` then
# tries to descend into `ax25/ax25` and the build dies on a directory that was
# never there. The patch says which two and why neither builds against musl.
#
# ONE TARBALL, THREE PROJECTS — see the libax25 recipe. The tools are their own
# autotools tree inside the same monorepo.
cd ax25tools
patch -p1 -d .. -i "$PORT_SRC/no-tcpip.patch"
./autogen.sh
./configure --prefix=/usr --sysconfdir=/etc --localstatedir=/var --libdir=/usr/lib
make
make DESTDIR=$PKG install
