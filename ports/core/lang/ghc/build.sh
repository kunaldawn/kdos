#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

_seedghc="$SRC_ROOT/seed/bin/ghc"
(
	cd "$SRC_ROOT/ghc-$_seed-x86_64-unknown-linux"
	./configure --prefix="$SRC_ROOT/seed"
	make install
)

tar xf "$PORT_SRC/$name-vendor-$version.tar.xz" -C "$SRC_ROOT"
mkdir -p hadrian/bootstrap/_build/tarballs
for f in "$SRC_ROOT"/vendor/*.tar.gz; do
	cp "$f" hadrian/bootstrap/_build/tarballs/
done
for f in "$SRC_ROOT"/vendor/*.cabal; do
	b=${f##*/}
	cp "$f" "hadrian/bootstrap/_build/tarballs/${b%-*}.cabal"
done
(
	cd hadrian/bootstrap
	python3 bootstrap.py -w "$_seedghc" -d "$hsplan" --no-archive
)
_hadrian="$SRC/hadrian/bootstrap/_build/bin/hadrian"

./configure \
	--prefix=/usr \
	--build=x86_64-unknown-linux \
	--host=x86_64-unknown-linux \
	--target=x86_64-unknown-linux \
	--with-system-libffi \
	--disable-ld-override \
	--disable-numa \
	CONF_GCC_LINKER_OPTS_STAGE2="-Wl,-z,now" \
	GHC="$_seedghc"

"$_hadrian" -j"$KDOS_JOBS" \
	--flavour=release+no_profiled_libs \
	--docs=no-haddocks \
	--docs=no-sphinx-pdfs \
	--docs=no-sphinx-html \
	binary-dist-dir

cd _build/bindist/ghc-$version-x86_64-unknown-linux
./configure --prefix=/usr --with-system-libffi --disable-ld-override \
	CONF_GCC_LINKER_OPTS_STAGE2="-Wl,-z,now"
make install DESTDIR="$PKG"
rmdir "$PKG/usr/share/doc/ghc-$version"
