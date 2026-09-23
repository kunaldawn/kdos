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

cargo build --release --frozen --offline --no-default-features \
	--features rustls
install -Dm755 target/release/xh $PKG/usr/bin/xh

ln -s xh $PKG/usr/bin/xhs
install -Dm644 doc/xh.1 $PKG/usr/share/man/man1/xh.1
ln -s xh.1 $PKG/usr/share/man/man1/xhs.1
