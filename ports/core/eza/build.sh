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
install -Dm755 target/release/eza $PKG/usr/bin/eza
install -Dm644 completions/bash/eza     $PKG/usr/share/bash-completion/completions/eza
install -Dm644 completions/fish/eza.fish $PKG/usr/share/fish/vendor_completions.d/eza.fish
install -Dm644 completions/zsh/_eza      $PKG/usr/share/zsh/site-functions/_eza
