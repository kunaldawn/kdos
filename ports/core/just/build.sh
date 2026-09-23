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

cargo build --release --frozen --offline
install -Dm755 target/release/just $PKG/usr/bin/just
install -d "$PKG/usr/share/man/man1"
target/release/just --man > "$PKG/usr/share/man/man1/just.1"
for _sh in bash zsh fish; do
	target/release/just --completions $_sh > just.$_sh
done
install -Dm644 just.bash $PKG/usr/share/bash-completion/completions/just
install -Dm644 just.zsh  $PKG/usr/share/zsh/site-functions/_just
install -Dm644 just.fish $PKG/usr/share/fish/vendor_completions.d/just.fish
