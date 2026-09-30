# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# The tarball is flat, with no wrapping directory, and kpkg strips one
# component from the first source, which discards every top-level file; it
# is unpacked again here, unstripped.
mkdir -p unpacked
tar xf "$PORT_SRC/$name-$version.tgz" -C unpacked
cd unpacked

# Make.inc names a Solaris compiler, so CC and CFLAGS go on the command line,
# which reaches every recursive make. -j1: each directory's makefile updates
# one archive member per object, and parallel ar runs corrupt the archive.
# The code passes NULL where an int is expected, which GCC rejects as an
# error unless -fpermissive turns it back into a warning.
_cflags="$CFLAGS -fPIC -fpermissive"
make -j1 lib CC=gcc CFLAGS="$_cflags"
make -j1 -C MT/src spoolesMT.a CC=gcc CFLAGS="$_cflags"

# Upstream builds only static archives. One shared library carries the serial
# and the threaded (MT) solvers, so CalculiX links one name for both.
gcc $LDFLAGS -shared -o libspooles.so.$version -Wl,-soname,libspooles.so.$version \
	-Wl,--whole-archive spooles.a MT/src/spoolesMT.a -Wl,--no-whole-archive \
	-Wl,--no-undefined -lm -lpthread
install -Dm755 libspooles.so.$version "$PKG/usr/lib/libspooles.so.$version"
ln -s libspooles.so.$version "$PKG/usr/lib/libspooles.so"

# The headers include one another as <A2.h> and <A2/A2.h>, so the tree keeps
# its shape under include/spooles, which consumers put on their -I path.
install -d "$PKG/usr/include/spooles"
install -m644 *.h "$PKG/usr/include/spooles/"
for d in */; do
	d=${d%/}
	ls "$d"/*.h >/dev/null 2>&1 || continue
	install -d "$PKG/usr/include/spooles/$d"
	install -m644 "$d"/*.h "$PKG/usr/include/spooles/$d/"
done
