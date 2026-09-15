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

# THE SYNC CLIENT IS NOT BUILT AND THE SERVER IS NOT EITHER. atuin's history is
# every command anybody typed on this machine; shipping it able to post that to
# a remote by configuration is the argument that turned off fcitx5's cloud
# pinyin, on a more sensitive database. `--no-default-features --features
# client` is the local half: the SQLite store, the search UI and the shell
# hooks, with no network code linked in at all. It also drops `check-update`
# (a second reason to reach the network), `daemon`, and `clipboard`. That last
# one is arboard, which is pure Rust — it speaks the X11 protocol through
# x11rb and links no C library, measured on iamb, which keeps it. The reason
# to drop it here is simpler: there is no X server on this image and no
# compiled Wayland path in arboard, so the clipboard it offers cannot work,
# and `kdos-clip` is the desktop's.
cargo build --release --frozen --offline \
	--package atuin --no-default-features --features client

install -Dm755 target/release/atuin $PKG/usr/bin/atuin
