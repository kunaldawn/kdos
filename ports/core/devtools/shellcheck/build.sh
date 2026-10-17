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
cp "$SRC_ROOT/vendor/cabal.project.freeze" .
export CABAL_DIR="$SRC_ROOT/cabal"
mkdir -p "$CABAL_DIR"
printf 'repository hackage.haskell.org\n  url: file+noindex://%s\n' "$SRC_ROOT/vendor" \
	> "$CABAL_DIR/config"

cabal v2-install -j$KDOS_JOBS \
	--enable-split-sections \
	--enable-executable-stripping \
	--installdir="$PKG/usr/bin" --install-method=copy \
	exe:shellcheck

patch -p1 -i "$PORT_SRC/shellcheck-1-md-spaces.patch"
install -d "$PKG/usr/share/man/man1"
lowdown -s -Tman --out-no-smarty -o "$PKG/usr/share/man/man1/shellcheck.1" shellcheck.1.md
