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
install -Dm755 target/release/tldr $PKG/usr/bin/tldr
install -Dm644 completion/bash_tealdeer $PKG/usr/share/bash-completion/completions/tldr

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
install -dm755 $PKG/usr/share/tldr
cp -a $SRC_ROOT/tldr-2.3/pages $PKG/usr/share/tldr/pages

install -Dm644 /dev/stdin $PKG/etc/skel/.config/tealdeer/config.toml <<'TOML'
# The pages are shipped at /usr/share/tldr/pages and are the only ones this
# machine will have. auto_update off is what stops `tldr` reaching for a cache
# it cannot download; `tldr --update` still works where there IS a network.
[directories]
custom_pages_dir = "/usr/share/tldr/pages"

[updates]
auto_update = false
TOML
