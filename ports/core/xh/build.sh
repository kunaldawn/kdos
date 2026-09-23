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

# network-interface is left out: on Linux --interface binds through
# SO_BINDTODEVICE and the crate is only the fallback for other systems.
cargo build --release --frozen --offline --no-default-features \
	--features rustls,http-message-signatures
install -Dm755 target/release/xh $PKG/usr/bin/xh

ln -s xh $PKG/usr/bin/xhs
install -Dm644 doc/xh.1 $PKG/usr/share/man/man1/xh.1
ln -s xh.1 $PKG/usr/share/man/man1/xhs.1

install -Dm644 completions/xh.bash $PKG/usr/share/bash-completion/completions/xh
install -Dm644 completions/_xh $PKG/usr/share/zsh/site-functions/_xh
install -Dm644 completions/xh.fish $PKG/usr/share/fish/vendor_completions.d/xh.fish
