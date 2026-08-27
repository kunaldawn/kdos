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

# THE VERSION IS PINNED BY ITS CONSUMER AND MUST MATCH EXACTLY. corrosion's
# corrosion_add_cxxbridge() does `find_program(cxxbridge)` and accepts it only
# on `VERSION_EQUAL` with the cxx crate the project locks — taskwarrior 3.5.0
# locks 1.0.199. Anything else and corrosion falls back to
# `cargo install cxxbridge-cmd`, which is a download.
tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz
export CARGO_HOME="$SRC_ROOT/.cargo"
export RUSTFLAGS="-C target-feature=-crt-static"
export CARGO_NET_OFFLINE=true
cargo build --release --frozen --offline
install -Dm755 target/release/cxxbridge $PKG/usr/bin/cxxbridge
