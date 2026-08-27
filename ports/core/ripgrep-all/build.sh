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
# on a corpus of PDFs is the whole corpus.
#
# NO pandoc, AND THE COST IS STATED: pandoc is Haskell, and a GHC bootstrap is
# a price this distro has already declined once (for shellcheck). Without it
# rga does not search .docx, .odt or .epub — it searches PDFs through
# pdftotext, media through ffprobe, and every archive and compressed file,
# which is most of a corpus. The adapter reports the type it skipped rather
# than pretending it found nothing.
#
# It is the fast complement to recoll rather than a replacement: recoll builds
# an index and answers instantly; rga needs no index and searches what is in
# front of you right now.
cargo build --release --frozen --offline
install -Dm755 target/release/rga             $PKG/usr/bin/rga
install -Dm755 target/release/rga-preproc     $PKG/usr/bin/rga-preproc
