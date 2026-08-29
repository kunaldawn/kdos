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
