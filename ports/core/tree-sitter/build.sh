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

# nvim-treesitter AND HELIX DOWNLOAD AND COMPILE A GRAMMAR ON FIRST USE, which
# offline is a permanently broken feature that looks like a config bug —
# highlighting simply never appears and the editor says nothing useful. The CLI
# is what compiles a grammar; shipping it plus the library is the half of the
# fix that is a package, and the grammar set built at ISO time into
# /usr/lib/tree-sitter is the other half.
# The CLI's own C-library binding GENERATES itself, so this build runs
# bindgen, which dlopens libclang — and a static-musl rust binary cannot dlopen
# anything at all. Build-time only; the shipped binary links musl dynamically
# like everything else here.
export RUSTFLAGS="-C target-feature=-crt-static"
export LIBCLANG_PATH=/usr/lib

cargo build --release --frozen --offline
install -Dm755 target/release/tree-sitter $PKG/usr/bin/tree-sitter

# The C library and its header, so a grammar built later links against the
# same runtime the CLI generated it for. A grammar compiled against a
# different ABI version loads and then crashes the editor.
# THE MAKEFILE IS AT THE TOP LEVEL, not in lib/ — `make -C lib` is
# "No targets specified and no makefile found" AFTER the two-and-a-half-minute
# cargo build has already succeeded. lib/ still holds the sources; only the
# build file moved.
make PREFIX=/usr LIBDIR=/usr/lib
make PREFIX=/usr LIBDIR=/usr/lib DESTDIR=$PKG install
