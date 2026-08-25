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
# (a second reason to reach the network), `daemon`, and `clipboard`, which is
# arboard and would put an X11 and Wayland client library under a command-line
# tool.
cargo build --release --frozen --offline \
	--package atuin --no-default-features --features client

install -Dm755 target/release/atuin $PKG/usr/bin/atuin
