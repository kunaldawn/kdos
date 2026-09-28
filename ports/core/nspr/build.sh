# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------


# nspr's _linux.h picks its libc features by testing __GLIBC__ alone, so on
# musl it assumes none of them: poll, off64_t, IPv6 and getaddrinfo are named
# here, or the library falls back to select() and IPv4-only name lookup.
export CFLAGS="$CFLAGS -D_PR_POLL_AVAILABLE -D_PR_HAVE_OFF64_T -D_PR_INET6 \
	-D_PR_HAVE_INET_NTOP -D_PR_HAVE_GETHOSTBYNAME2 -D_PR_HAVE_GETADDRINFO \
	-D_PR_INET6_PROBE"

# musl has no stat64/off64_t; with _PR_HAVE_OFF64_T set the patch maps nspr's
# 64-bit file types onto the plain ones, which are already 64-bit.
patch -p1 -i "$PORT_SRC/nspr-musl-lfs64.patch"

mkdir build && cd build
../nspr/configure --prefix=/usr --libdir=/usr/lib \
	--with-mozilla --with-pthreads --enable-64bit --enable-ipv6 \
	--enable-optimize --disable-debug
make
make DESTDIR="$PKG" install

# The install copies the static archives and two build-time helpers into the
# package; nothing links the archives and nothing runs the helpers.
rm -f "$PKG"/usr/lib/*.a "$PKG"/usr/bin/compile-et.pl "$PKG"/usr/bin/prerr.properties
