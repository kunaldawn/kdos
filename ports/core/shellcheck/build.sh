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

# The source is Hackage's ShellCheck: the library, the `shellcheck`
# executable and the manual page's Markdown. Its dependencies come from the
# vendor bundle, and the freeze in the bundle holds the offline solve to the
# set that was downloaded.
tar xf "$PORT_SRC/$name-vendor-$version.tar.xz" -C "$SRC_ROOT"
cp "$SRC_ROOT/vendor/cabal.project.freeze" .
export CABAL_DIR="$SRC_ROOT/cabal"
mkdir -p "$CABAL_DIR"
printf 'repository hackage.haskell.org\n  url: file+noindex://%s\n' "$SRC_ROOT/vendor" \
	> "$CABAL_DIR/config"

cabal v2-install -j \
	--enable-split-sections \
	--enable-executable-stripping \
	--installdir="$PKG/usr/bin" --install-method=copy \
	exe:shellcheck

# The manual page is pandoc Markdown with a `%` title block, rendered by
# lowdown so that pandoc is not under this port. --out-no-smarty keeps each
# option's leading `--` from becoming an en dash.
patch -p1 -i "$PORT_SRC/shellcheck-1-md-spaces.patch"
install -d "$PKG/usr/share/man/man1"
lowdown -s -Tman --out-no-smarty -o "$PKG/usr/share/man/man1/shellcheck.1" shellcheck.1.md
