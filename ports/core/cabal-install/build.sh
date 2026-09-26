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

# bootstrap/bootstrap.py builds cabal-install and its dependencies with
# Setup.hs alone, which is the only way to build it on a system with no cabal.
# The plan it follows names the exact GHC it was solved against and checks
# every GHC-shipped package in it by version, so `_ghc` moves with the ghc
# port. The vendor bundle is laid out where the script reads a prefetched set,
# <pkg>-<ver>.tar.gz and the revised <pkg>.cabal, and each is checked against
# the plan's hashes before anything is built.
tar xf "$PORT_SRC/$name-vendor-$version.tar.xz" -C "$SRC_ROOT"
mkdir -p _build/tarballs
for f in "$SRC_ROOT"/vendor/*.tar.gz; do
	cp "$f" _build/tarballs/
done
for f in "$SRC_ROOT"/vendor/*.cabal; do
	b=${f##*/}
	cp "$f" "_build/tarballs/${b%-*}.cabal"
done
python3 bootstrap/bootstrap.py -w /usr/bin/ghc -d "$hsplan" --no-archive

install -Dm755 _build/bin/cabal "$PKG/usr/bin/cabal"
install -d "$PKG/usr/share/man/man1"
_build/bin/cabal man --raw > "$PKG/usr/share/man/man1/cabal.1"
install -Dm644 cabal-install/bash-completion/cabal \
	"$PKG/usr/share/bash-completion/completions/cabal"
