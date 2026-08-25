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

# The default feature set pulls `unrar` and `bzip3`, each of which compiles a
# vendored C library through cc. They vendor as source, so the build stays
# offline, and dropping them would make ouch answer "unsupported format" for
# two of the formats it advertises.
cargo build --release --frozen --offline

install -Dm755 target/release/ouch $PKG/usr/bin/ouch
