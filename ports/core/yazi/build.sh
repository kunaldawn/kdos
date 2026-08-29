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

# DO NOT DISABLE THE IMAGE PREVIEW. foot supports sixel, which is the shipped
# terminal on this desktop, so yazi's previewer draws real thumbnails in a
# window that is otherwise a character grid — the one place on this system
# where a picture beats a filename, and the reason to have this beside mc and
# kdos-pick rather than instead of them.
# VERGEN_GIT_SHA IS SUPPLIED BECAUSE A TARBALL IS NOT A REPOSITORY. yazi's
# build script uses vergen to stamp the binary with the commit it came from,
# and with no .git present the crate emits nothing while the source still reads
# `env!("VERGEN_GIT_SHA")` — a compile error naming an environment variable
# rather than a missing tool. The release tag is the honest answer to "which
# commit is this": it is exactly what the tarball was cut from.
export VERGEN_GIT_SHA="$version"
export VERGEN_IDEMPOTENT=1

export YAZI_GEN_COMPLETIONS=1
cargo build --release --frozen --offline
install -Dm755 target/release/yazi $PKG/usr/bin/yazi
install -Dm755 target/release/ya   $PKG/usr/bin/ya
