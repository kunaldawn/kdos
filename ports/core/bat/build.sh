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

tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz
export BAT_ASSETS_GEN_DIR="$SRC/gen"

# THE HIGHLIGHTING SETS ARE BUILT HERE. assets/syntaxes.bin, themes.bin and
# acknowledgements.bin are serialized syntect sets that src/assets.rs embeds
# with include_bytes!. Their inputs are the 92 git submodules under
# assets/syntaxes and assets/themes, which the release archive carries empty;
# bat-assets-<version>.tar.xz is those submodules at the tag's pinned commits,
# holding every .sublime-syntax, .tmTheme and LICENSE/NOTICE file the asset
# build reads. It unpacks over the empty directories, upstream's own
# assets/patches apply as assets/create.sh applies them, and the first build's
# `bat cache --build --blank` compiles the sets without reading the ones it
# embeds. The second build recompiles only the bat crate, whose include_bytes!
# inputs changed.
#
# --source=assets is relative on purpose: the syntax set records each
# definition's path as given, so an absolute source puts this build's
# directory into the set and the bytes differ between two build trees.
tar xf $PORT_SRC/${name}-assets-${version}.tar.xz
for p in assets/patches/*.patch; do
	patch -d assets -p0 < "$p"
done

export RUSTFLAGS="-C target-feature=-crt-static"

# vendored-libgit2: libgit2-sys otherwise links a system libgit2 whenever
# pkg-config finds one in its version range, so the bundled copy is named.
# libz-sys takes the system zlib whenever one is installed, which is why zlib
# is in `depends`: the link is then the same on every build.
cargo build --release --frozen --offline --features vendored-libgit2
target/release/bat cache --build --blank --acknowledgements \
	--source=assets --target=assets
cargo build --release --frozen --offline --features vendored-libgit2

install -Dm755 target/release/bat $PKG/usr/bin/bat
install -Dm644 gen/assets/manual/bat.1 -t "$PKG/usr/share/man/man1"

# delta and presenterm embed the same sets and build-depend on this port for
# them, so the copies here are the only ones in the tree. acknowledgements.txt
# is bat's licence followed by the licences of the grammars and themes, the
# text presenterm prints for --acknowledgements.
install -Dm644 assets/syntaxes.bin assets/themes.bin assets/acknowledgements.bin \
	-t "$PKG/usr/share/bat/assets"
{
	cat LICENSE-MIT
	target/release/bat --no-config --acknowledgements
} > "$PKG/usr/share/bat/assets/acknowledgements.txt"
chmod 644 "$PKG/usr/share/bat/assets/acknowledgements.txt"
