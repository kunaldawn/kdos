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
# The cargo library inside links the curl, zlib, openssl and libssh2 ports
# through pkg-config, and curl-sys, libz-sys and libssh2-sys compile bundled
# copies in silence when pkg-config finds none; libssh2-sys asks pkg-config
# only when LIBSSH2_SYS_USE_PKG_CONFIG is set. libgit2 is the bundled copy
# because no port provides it, though libgit2-sys asks pkg-config first.
#
# -crt-static: the musl target links statically by default, and then takes
# libcurl.a without the libraries it needs.
export LIBSSH2_SYS_USE_PKG_CONFIG=1
export RUSTFLAGS="-C target-feature=-crt-static"
cargo install --frozen --offline --path . --root=$PKG/usr
rm -f $PKG/usr/.crates.toml
rm -f $PKG/usr/.crates2.json
