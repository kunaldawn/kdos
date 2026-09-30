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

# configure replaces CXXFLAGS with its own "-O3 -Wall", and src/Makefile
# compiles with that alone. CFLAGS, the makefile's compile variable, is given
# whole: the exported CXXFLAGS, then upstream's -O3 -Wall, then the include
# paths and -fPIC the makefile appends, which a command-line value discards.
stkflags="$CXXFLAGS -O3 -Wall -I../include -Iinclude -fPIC"
make -C src CFLAGS="$stkflags"
make -C src CFLAGS="$stkflags" DESTDIR=$PKG install

install -d "$PKG/usr/share/stk/rawwaves"
install -m644 rawwaves/*.raw "$PKG/usr/share/stk/rawwaves/"
