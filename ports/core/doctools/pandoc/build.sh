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

cabal v2-install -j"$KDOS_JOBS" \
	--flags="+lua +server" \
	--constraint="pandoc +embed_data_files" \
	--enable-split-sections \
	--enable-executable-stripping \
	--installdir="$PKG/usr/bin" --install-method=copy \
	exe:pandoc

ln -s pandoc "$PKG/usr/bin/pandoc-server"
ln -s pandoc "$PKG/usr/bin/pandoc-lua"
install -Dm644 -t "$PKG/usr/share/man/man1" \
	man/pandoc.1 man/pandoc-server.1 man/pandoc-lua.1
