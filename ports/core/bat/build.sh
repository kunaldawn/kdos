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

# vendored-libgit2: libgit2-sys otherwise links a system libgit2 whenever
# pkg-config finds one in its version range, so the bundled copy is named.
# libz-sys takes the system zlib whenever one is installed, which is why zlib
# is in `depends`: the link is then the same on every build.
cargo build --release --frozen --offline --features vendored-libgit2
install -Dm755 target/release/bat $PKG/usr/bin/bat
install -Dm644 gen/assets/manual/bat.1 -t "$PKG/usr/share/man/man1"
