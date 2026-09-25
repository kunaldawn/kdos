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

# The seed is an upstream GHC for musl, used to compile Hadrian and GHC's
# first stage and then discarded; nothing it built is in $PKG.
_seedghc="$SRC_ROOT/seed/bin/ghc"
(
	cd "$SRC_ROOT/ghc-$_seed-x86_64-unknown-linux"
	./configure --prefix="$SRC_ROOT/seed"
	make install
)

# Hadrian's own Hackage dependencies are the vendor bundle, laid out the way
# hadrian/bootstrap/bootstrap.py reads a prefetched set: <pkg>-<ver>.tar.gz
# and the revised <pkg>.cabal. The script checks both against the plan's
# hashes before building anything, and with every file present it never
# reaches for the network.
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

# --disable-ld-override keeps binutils' ld, the linker the rest of the tree
# links with, rather than whichever of gold or lld configure finds first.
./configure \
	--prefix=/usr \
	--build=x86_64-unknown-linux \
	--host=x86_64-unknown-linux \
	--target=x86_64-unknown-linux \
	--with-system-libffi \
	--disable-ld-override \
	GHC="$_seedghc"

# release: the optimised compiler with static and shared libraries, split
# sections so a static link drops unused code. no_profiled_libs: no profiling
# variant of any library, so `cabal --enable-profiling` has nothing to link.
# Only the manual page is built. The libraries' Haddock HTML, with its
# hyperlinked source, is 750 MB; without it no Haddock page links into the
# shipped libraries' documentation, and the User's Guide is not installed.
"$_hadrian" -j \
	--flavour=release+no_profiled_libs \
	--docs=no-haddocks \
	--docs=no-sphinx-pdfs \
	--docs=no-sphinx-html \
	binary-dist-dir

cd _build/bindist/ghc-$version-x86_64-unknown-linux
./configure --prefix=/usr --with-system-libffi --disable-ld-override
make install DESTDIR="$PKG"
rmdir "$PKG/usr/share/doc/ghc-$version"
