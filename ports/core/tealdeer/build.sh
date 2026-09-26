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

# THE PAGE ARCHIVE IS SEEDED HERE AND AUTO-UPDATE IS OFF, and those are one
# decision rather than two. Out of the box `tldr` downloads the archive on
# first use: on this machine that is a tool whose whole value — an example when
# you are trying to recover data and cannot remember tar's flags — appears only
# when there is a network, which is the case it was chosen FOR.
#
# kpkg ALREADY EXTRACTED IT. An extra `source =` that is an archive is unpacked
# into $SRC_ROOT unstripped — only a NON-archive source stays a file, and then
# in $PORT_SRC rather than here. Untarring it again looks for a .tar.gz that
# was never copied and fails after the whole rust build has succeeded.
#
# THE PAGES GO IN THE LAYOUT OF A DOWNLOADED CACHE. tealdeer looks a page up
# only as <cache_dir>/tldr-pages/pages.<lang>/<platform>/<cmd>.md; its
# custom_pages_dir holds flat <cmd>.page.md overrides and nothing else, so a
# tldr tree there is never read. The archive's `pages` is English, and English
# is the language every lookup falls back to.
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
