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

# THE CRATES GO BESIDE THE TREE, NOT INTO ITS vendor/. Upstream's vendor/ holds
# axoasset as a `[patch.crates-io]` path crate with no `.cargo-checksum.json`;
# a directory source over that vendor/ refuses the whole registry on it. The
# bundle's own copy of axoasset is left out for the same reason, and the patch
# still resolves to upstream's.
mkdir .kdos-vendor
tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz -C .kdos-vendor \
	--exclude=vendor/axoasset

# NOTHING IN THE BINARY CALLS THE SYNC CLIENT, AND THE SERVER IS NOT BUILT.
# atuin's history is every command anybody typed on this machine; shipping it
# able to post that to a remote by configuration is the argument that turned
# off fcitx5's cloud pinyin, on a more sensitive database.
# `--no-default-features` drops every feature and the three named back are the
# local half: `client` is the SQLite store, the search UI and the shell hooks,
# `clipboard` is a yank out of that UI, and `pty-proxy` is `atuin hex` — the
# shell re-executed inside a local pseudo-terminal so the search UI can draw
# inline; its one socket is a Unix socket in a per-user temporary directory,
# which the shell inside uses to find it. `sync`, `check-update`, `daemon` and
# `ai` stay off, and they are the four that reach the network: every command,
# hook and auto-sync call into the network code is gated on them in the
# `atuin` crate.
# The `atuin-client` library underneath is still compiled with its default
# `sync`, `hub` and `daemon` features, because the workspace crates beside it
# (kv, scripts, dotfiles, history, pty-proxy) take it with defaults on and
# Cargo unions features; that code is linked in and never called. Taking it
# out of the binary needs a patch to the workspace manifests.
#
# THE CLIPBOARD IS arboard AND IT REACHES THE DESKTOP'S. atuin asks it for
# `wayland-data-control` on Linux, which is wl-clipboard-rs speaking
# `zwlr_data_control_v1` — and that path is pure Rust: wayland-backend's
# `client_system` feature is the only thing that would link libwayland, it is
# opt-in, and nothing in the chain turns it on. The binary stays static-pie
# with no NEEDED at all, which is what every Rust port here must be. kdos-comp
# creates both data-control managers, so a yank reaches kdos-clip like any
# other program's. A login with no Wayland socket falls back to arboard's X11
# path, and there is no X server on this image either.
cargo build --release --frozen --offline --config .kdos-vendor/.cargo/config.toml \
	--package atuin --no-default-features --features client,clipboard,pty-proxy

install -Dm755 target/release/atuin $PKG/usr/bin/atuin
