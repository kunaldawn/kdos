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
# pkg-config finds none. libgit2 is the bundled copy because no port provides
# it, though libgit2-sys asks pkg-config first; libssh2 is always the bundled
# copy, since libssh2-sys asks pkg-config only when LIBSSH2_SYS_USE_PKG_CONFIG
# is set.
cargo install --frozen --offline --path . --root=$PKG/usr
rm -f $PKG/usr/.crates.toml
rm -f $PKG/usr/.crates2.json
