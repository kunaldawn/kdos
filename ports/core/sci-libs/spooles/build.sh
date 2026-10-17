# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

mkdir -p unpacked
tar xf "$PORT_SRC/$name-$version.tgz" -C unpacked
cd unpacked

_cflags="$CFLAGS -fPIC -fpermissive"
make -j1 lib CC=gcc CFLAGS="$_cflags"
make -j1 -C MT/src spoolesMT.a CC=gcc CFLAGS="$_cflags"

gcc $LDFLAGS -shared -o libspooles.so.$version -Wl,-soname,libspooles.so.$version \
	-Wl,--whole-archive spooles.a MT/src/spoolesMT.a -Wl,--no-whole-archive \
	-Wl,--no-undefined -lm -lpthread
install -Dm755 libspooles.so.$version "$PKG/usr/lib/libspooles.so.$version"
ln -s libspooles.so.$version "$PKG/usr/lib/libspooles.so"

install -d "$PKG/usr/include/spooles"
install -m644 *.h "$PKG/usr/include/spooles/"
for d in */; do
	d=${d%/}
	ls "$d"/*.h >/dev/null 2>&1 || continue
	install -d "$PKG/usr/include/spooles/$d"
	install -m644 "$d"/*.h "$PKG/usr/include/spooles/$d/"
done
