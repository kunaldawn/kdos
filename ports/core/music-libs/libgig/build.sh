# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------


# The libraries install into pkglibdir, /usr/lib/libgig by default, which is
# not on musl's library path: every consumer would link and then fail to load
# them. pkglibdir=/usr/lib puts libgig and libakai beside every other library;
# gig.pc's extra -L/usr/lib/libgig then names an empty directory and costs
# nothing.
#
# libsndfile is the audio-file backend; with it, configure never looks for
# libaudiofile. libuuid comes from util-linux.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib --disable-static
make pkglibdir=/usr/lib
make pkglibdir=/usr/lib DESTDIR=$PKG install

test -e "$PKG/usr/lib/libgig.so"
test -e "$PKG/usr/lib/libakai.so"
