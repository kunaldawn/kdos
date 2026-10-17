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
install -Dm755 target/release/tldr $PKG/usr/bin/tldr
install -Dm644 completion/bash_tealdeer $PKG/usr/share/bash-completion/completions/tldr
install -Dm644 completion/zsh_tealdeer $PKG/usr/share/zsh/site-functions/_tldr

install -dm755 $PKG/usr/share/tldr/tldr-pages
cp -a $SRC_ROOT/tldr-2.3/pages $PKG/usr/share/tldr/tldr-pages/pages.en

install -Dm644 /dev/stdin $PKG/etc/skel/.config/tealdeer/config.toml <<'TOML'
# The pages are shipped under /usr/share/tldr and are the only ones this
# machine will have. auto_update off is what stops `tldr` reaching for a cache
# it cannot download, and warn_cache_age off stops it calling the shipped set
# stale by its packaged date. `tldr --update` writes into cache_dir, so with
# a network, point cache_dir at a directory of your own first.
[directories]
cache_dir = "/usr/share/tldr"

[updates]
auto_update = false
warn_cache_age = "never"
TOML
