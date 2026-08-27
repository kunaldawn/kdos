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

# -crt-static, the same as every other rust port here. Without it the target is
# fully static and openssl-sys links libcrypto.A, whose compression BIO calls
# inflate/deflate — a static archive carries no dependency of its own, so the
# link fails on zlib symbols nothing on the command line provides. Dynamically,
# libcrypto.so names libz itself and the question does not arise.
export CARGO_HOME="$SRC_ROOT/.cargo"
export RUSTFLAGS="-C target-feature=-crt-static"
export CARGO_NET_OFFLINE=true

# THE CASE AGAINST TeX HERE IS THE ERROR MESSAGES AND THE INSTALL SIZE. A
# TeX Live distribution is gigabytes and answers a mistake with forty lines of
# macro expansion; typst is one binary that says which line is wrong. On a
# machine where nobody can search for the error text, that difference is
# whether a document gets written.
#
# --no-default-features drops BOTH `embed-fonts`, which bakes a handful of
# faces into the binary, and `self-update`, which reaches the network — the
# second is the one that matters here, since an updater on an offline machine
# is a command that can only ever fail. Fonts come from fontconfig, so typst
# sees exactly what the rest of this machine does.
cargo build --release --frozen --offline --bin typst --no-default-features
install -Dm755 target/release/typst $PKG/usr/bin/typst
