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
# The cargo library inside links the curl, zlib and openssl ports through
# pkg-config, and curl-sys and libz-sys compile bundled copies in silence when
# pkg-config finds none. libgit2 and libssh2 are always the bundled copies:
# neither sys crate asks pkg-config unless told to.
cargo install --frozen --offline --path . --root=$PKG/usr
rm -f $PKG/usr/.crates.toml
rm -f $PKG/usr/.crates2.json
