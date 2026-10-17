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

export RUSTFLAGS="-C target-feature=-crt-static"

cargo build --release --frozen --offline --no-default-features \
	--features native-tls,http-message-signatures
readelf -d target/release/xh | grep -q 'NEEDED.*\[libssl\.so'
readelf -d target/release/xh | grep -q 'NEEDED.*\[libonig\.so\.5\]'
install -Dm755 target/release/xh $PKG/usr/bin/xh

ln -s xh $PKG/usr/bin/xhs
install -Dm644 doc/xh.1 $PKG/usr/share/man/man1/xh.1
ln -s xh.1 $PKG/usr/share/man/man1/xhs.1

install -Dm644 completions/xh.bash $PKG/usr/share/bash-completion/completions/xh
install -Dm644 completions/_xh $PKG/usr/share/zsh/site-functions/_xh
install -Dm644 completions/xh.fish $PKG/usr/share/fish/vendor_completions.d/xh.fish
