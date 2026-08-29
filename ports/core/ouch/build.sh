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
#
# bzip3 GENERATES ITS BINDINGS, so this build runs bindgen, which dlopens
# libclang — and a static-musl rust binary cannot dlopen anything at all
# ("Dynamic loading not supported", from inside bindgen rather than from the
# linker, which is what makes it read like a missing library). Turning
# crt-static off is what gives the build script a dynamic loader; LIBCLANG_PATH
# saves it a search. Both are build-time only: the shipped binary links musl
# dynamically like everything else here.
export RUSTFLAGS="-C target-feature=-crt-static"
export LIBCLANG_PATH=/usr/lib

cargo build --release --frozen --offline

install -Dm755 target/release/ouch $PKG/usr/bin/ouch
