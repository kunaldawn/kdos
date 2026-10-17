# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

autoconf
export LDFLAGS="$LDFLAGS -Wl,--strip-debug"
./configure --prefix=/usr \
	--with-solver='z3 --in' \
	--disable-jemalloc \
	--disable-tcmalloc \
	OBJCACHE=ccache
make
make DESTDIR=$PKG install
