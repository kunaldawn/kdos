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

# THE OPERATION LOG IS THE OFFLINE ARGUMENT. Every jj command is recorded and
# `jj op restore` puts the whole repository back to any earlier moment — which
# matters far more here than on a connected machine, because the usual recovery
# from a bad rebase is to re-clone from the remote and there is no remote. It
# reads and writes ordinary git repositories, so nothing has to be converted
# and git keeps working on the same tree.
#
# The default features stay on: `git` is the whole point, and `watchman` only
# acts when fsmonitor is configured. OPENSSL_NO_VENDOR makes any openssl-sys in
# the graph link the port rather than build a private copy.
export OPENSSL_NO_VENDOR=1
cargo build --release --frozen --offline --bin jj
install -Dm755 target/release/jj $PKG/usr/bin/jj
