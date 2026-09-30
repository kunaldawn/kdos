# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# --without-libxml2: expat is already a port and the smaller of the two
# parsers. Without the flag, configure falls back to libxml2 when it cannot
# find expat, so a missing expat would change the parser rather than fail.
./bootstrap.sh
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--without-libxml2 --disable-examples
make
make DESTDIR=$PKG install
