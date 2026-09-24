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
cargo build --release --frozen --offline
install -Dm755 target/release/zoxide $PKG/usr/bin/zoxide
install -Dm644 man/man1/*.1 -t "$PKG/usr/share/man/man1"
install -Dm644 contrib/completions/zoxide.bash $PKG/usr/share/bash-completion/completions/zoxide
install -Dm644 contrib/completions/_zoxide     $PKG/usr/share/zsh/site-functions/_zoxide
install -Dm644 contrib/completions/zoxide.fish $PKG/usr/share/fish/vendor_completions.d/zoxide.fish
