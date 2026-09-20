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
# pinyin, on a more sensitive database. `--no-default-features` drops every
# feature and the two named back are the local half: `client` is the SQLite
# store, the search UI and the shell hooks, and `clipboard` is a yank out of
# that UI. `sync`, `check-update` and `daemon` stay off, and they are the three
# that reach the network.
#
# THE CLIPBOARD IS arboard AND IT REACHES THE DESKTOP'S. atuin asks it for
# `wayland-data-control` on Linux, which is wl-clipboard-rs speaking
# `zwlr_data_control_v1` — and that path is pure Rust: wayland-backend's
# `client_system` feature is the only thing that would link libwayland, it is
# opt-in, and nothing in the chain turns it on. The binary stays static-pie
# with no NEEDED at all, which is what every Rust port here must be. kdos-comp
# creates both data-control managers, so a yank reaches kdos-clip there like
# any other program's; under the console session there is no Wayland socket
# and arboard falls back to X11, which is not on this image either.
cargo build --release --frozen --offline \
	--package atuin --no-default-features --features client,clipboard

install -Dm755 target/release/atuin $PKG/usr/bin/atuin
