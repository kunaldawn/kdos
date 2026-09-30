# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------


# The autoconf build, not CMake: only it gives the library a versioned
# soname and compiles in RAWWAVE_PATH, the directory every physical model
# loads its excitation samples from. Without that path an instrument opens
# ../../rawwaves/ relative to the program and plays silence.
#
# The release carries configure.ac and hand-written Makefile.in files, so
# configure is generated here; aclocal supplies the C++ standard check from m4/.
aclocal -I m4
autoconf
RAWWAVE_PATH=/usr/share/stk/rawwaves/ \
./configure --prefix=/usr --libdir=/usr/lib \
	--enable-shared \
	--disable-static \
	--with-alsa \
	--with-pulse \
	--without-jack
make -C src
make -C src DESTDIR=$PKG install

install -d "$PKG/usr/share/stk/rawwaves"
install -m644 rawwaves/*.raw "$PKG/usr/share/stk/rawwaves/"
