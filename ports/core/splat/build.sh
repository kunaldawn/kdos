# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# UPSTREAM'S `build` SCRIPT IS NOT USED, AND THAT IS THE POINT: it prints
# "Compilation failed!" and exits 0, so a broken build would package an empty
# directory and report success. It also hardcodes `-march=x86-64`, which is the
# one decision `kdos march` exists to make by measurement.
#
# TWO BINARIES FROM ONE SOURCE, AND THE HEADER IS THE ONLY DIFFERENCE. HD_MODE
# picks 1-arc-second over 3-arc-second terrain, and MAXPAGES is how many
# 1-degree tiles the program may hold at once — upstream's `configure` asks a
# human for both and neither header ships in the tarball. 9 pages standard and
# 4 HD are upstream's own middle answers; an HD page is nine times the memory,
# which is why the HD number is smaller for the same footprint.
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

# THE UTILITIES ARE HOW TERRAIN DATA GETS IN. srtm2sdf and usgs2sdf convert the
# elevation models SPLAT! reads, and without them the program has nothing to
# run against — which is the whole "needs DEM" note in the catalogue.
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
