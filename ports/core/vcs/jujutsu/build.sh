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

export OPENSSL_NO_VENDOR=1
export RUSTFLAGS="-C target-feature=-crt-static"
cargo build --release --frozen --offline --bin jj
install -Dm755 target/release/jj $PKG/usr/bin/jj
target/release/jj util install-man-pages "$PKG/usr/share/man"
for _sh in bash zsh fish; do
	target/release/jj util completion $_sh > jj.$_sh
done
install -Dm644 jj.bash $PKG/usr/share/bash-completion/completions/jj
install -Dm644 jj.zsh  $PKG/usr/share/zsh/site-functions/_jj
install -Dm644 jj.fish $PKG/usr/share/fish/vendor_completions.d/jj.fish
