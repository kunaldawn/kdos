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

# rustls, NOT native-tls — the alternative links openssl through a build script
# that probes for it, and a TLS stack chosen at build time by a probe is a
# thing that silently changes between two builds of the same recipe. rustls
# carries its own roots as well, so `xh` works against a private CA by being
# pointed at one rather than by the system store happening to be right.
cargo build --release --frozen --offline --no-default-features \
	--features rustls,online-tests
install -Dm755 target/release/xh $PKG/usr/bin/xh

# xhs is xh with https:// assumed — upstream ships it as a symlink and the
# binary dispatches on its own basename, the same trick kpkg and ksvc use.
ln -s xh $PKG/usr/bin/xhs
install -Dm644 doc/xh.1 $PKG/usr/share/man/man1/xh.1
