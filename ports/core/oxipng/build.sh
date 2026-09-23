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

# LOSSLESS, WHICH IS THE ONLY REASON IT IS SAFE TO RUN OVER AN ARCHIVE. oxipng
# re-encodes the same pixels with better filters and a better deflate; the
# output is bit-identical when decoded, so it can be pointed at a directory of
# screenshots or scanned pages without anybody having to decide whether the
# quality loss is acceptable. Typically 15-30% on material a camera or a
# screenshot tool produced.
cargo build --release --frozen --offline --bin oxipng
install -Dm755 target/release/oxipng $PKG/usr/bin/oxipng

# The manual page is the xtask's: it renders the same clap definition the
# binary parses, so the page and the options cannot disagree. The xtask stamps
# the page with a date; the patch makes it SOURCE_DATE_EPOCH's rather than the
# build's, or the package differs on every rebuild.
patch -p1 -i "$PORT_SRC/source-date-epoch.patch"
cargo run --frozen --offline --manifest-path xtask/Cargo.toml -- mangen
install -Dm644 target/xtask/mangen/manpages/oxipng.1 -t "$PKG/usr/share/man/man1"
