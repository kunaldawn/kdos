# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

export CFLAGS="$CFLAGS -D_PR_POLL_AVAILABLE -D_PR_HAVE_OFF64_T -D_PR_INET6 \
	-D_PR_HAVE_INET_NTOP -D_PR_HAVE_GETHOSTBYNAME2 -D_PR_HAVE_GETADDRINFO \
	-D_PR_INET6_PROBE"

patch -p1 -i "$PORT_SRC/nspr-musl-lfs64.patch"

mkdir build && cd build
../nspr/configure --prefix=/usr --libdir=/usr/lib \
	--with-mozilla --with-pthreads --enable-64bit --enable-ipv6 \
	--enable-optimize --disable-debug
make
make DESTDIR="$PKG" install

rm -f "$PKG"/usr/lib/*.a "$PKG"/usr/bin/compile-et.pl "$PKG"/usr/bin/prerr.properties
