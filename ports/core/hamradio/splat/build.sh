# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

gen_parms() {
	printf '#define MAXPAGES %s\n#define HD_MODE %s\n' "$2" "$3" > "$1"
}
gen_parms std-parms.h 9 0
gen_parms hd-parms.h  4 1

build_one() {
	cp "$1" splat.h
	g++ $CXXFLAGS $LDFLAGS -ffast-math -mcmodel=medium \
	    itwom3.0.cpp splat.cpp -lm -lbz2 -o "$2"
}
build_one std-parms.h splat
build_one hd-parms.h  splat-hd

install -Dm755 splat    $PKG/usr/bin/splat
install -Dm755 splat-hd $PKG/usr/bin/splat-hd

cd utils
for u in citydecoder usgs2sdf bearing; do
	cc $CFLAGS $LDFLAGS $u.c -lm -o $u
	install -Dm755 $u $PKG/usr/bin/$u
done
cc $CFLAGS $LDFLAGS srtm2sdf.c -lbz2 -lm -o srtm2sdf
install -Dm755 srtm2sdf $PKG/usr/bin/srtm2sdf
cc $CFLAGS $LDFLAGS fontdata.c -lz -o fontdata
install -Dm755 fontdata $PKG/usr/bin/fontdata
cd ..

install -Dm644 docs/english/man/splat.1 $PKG/usr/share/man/man1/splat.1
install -Dm644 docs/spanish/man/splat.1 $PKG/usr/share/man/es/man1/splat.1
