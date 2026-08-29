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

# xh EXPLORES AND THIS ASSERTS, which is why both are here and a third HTTP
# client is not. A .hurl file is a request plus what the answer must contain,
# in plain text — so "is the local kiwix still serving" or "does this API still
# behave" is a file in git that diffs, rather than a command somebody remembers
# and a result somebody eyeballs. On a machine with no CI that file IS the CI.
#
# It links the SYSTEM libcurl rather than vendoring one, so it speaks exactly
# the protocols and uses exactly the CA store the rest of this machine does —
# a test client that disagreed with curl about TLS would be testing itself.
# hurl's libxml2 binding GENERATES ITSELF, so this build runs bindgen, which
# dlopens libclang — and a static-musl rust binary cannot dlopen anything at
# all. The failure arrives from inside bindgen rather than from the linker
# ("Dynamic loading not supported"), so it reads as a missing library on a
# machine where libclang is right there. Build-time only: the shipped binary
# links musl dynamically like everything else here.
export RUSTFLAGS="-C target-feature=-crt-static"
export LIBCLANG_PATH=/usr/lib

cargo build --release --frozen --offline --bin hurl --bin hurlfmt
install -Dm755 target/release/hurl    $PKG/usr/bin/hurl
install -Dm755 target/release/hurlfmt $PKG/usr/bin/hurlfmt
