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

# IT SHELLS OUT, WHICH IS WHY ITS DEPENDENCIES ARE PROGRAMS AND NOT LIBRARIES.
# rga is an adapter layer over ripgrep: it recognises a file type and pipes it
# through pdftotext, pandoc, ffprobe or a decompressor before ripgrep ever sees
# it. Each missing helper is a file type it silently declines to search — which
# on a corpus of PDFs is the whole corpus. pandoc is the reader for .docx,
# .odt, .epub, .fb2, .ipynb and .html, and it claims HTML ahead of ripgrep,
# so without it an HTML file is not searched even as text.
#
# It is the fast complement to recoll rather than a replacement: recoll builds
# an index and answers instantly; rga needs no index and searches what is in
# front of you right now.
#
# Its compression and cache libraries come from the system, not from the
# copies the -sys crates carry: lzma-sys and bzip2-sys take the system library
# whenever pkg-config finds it, zstd-sys and libsqlite3-sys (which rusqlite's
# `bundled` feature otherwise compiles from source) only when told to, and a
# dynamically linked library needs a binary that is not crt-static.
export CARGO_HOME="$SRC_ROOT/.cargo"
export CARGO_NET_OFFLINE=true
export RUSTFLAGS="-C target-feature=-crt-static"
export ZSTD_SYS_USE_PKG_CONFIG=1
export LIBSQLITE3_SYS_USE_PKG_CONFIG=1
cargo build --release --frozen --offline
install -Dm755 target/release/rga             $PKG/usr/bin/rga
install -Dm755 target/release/rga-preproc     $PKG/usr/bin/rga-preproc

# `rga-fzf <query>` is the interactive mode: it runs fzf with rga as both the
# search and the preview, and hands the chosen file to rga-fzf-open, which
# opens it through xdg-open. It finds rga and rga-fzf-open beside its own
# binary and fzf on PATH, so fzf is in `depends`.
install -Dm755 target/release/rga-fzf         $PKG/usr/bin/rga-fzf
install -Dm755 target/release/rga-fzf-open    $PKG/usr/bin/rga-fzf-open
