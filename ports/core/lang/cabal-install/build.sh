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
