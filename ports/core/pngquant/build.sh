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


# --with-libimagequant with no directory links the system libimagequant
# rather than the copy of its C sources under lib/. libimagequant 4 keeps
# the C interface pngquant 2 was written against; pngquant 3 is a Rust
# program that vendors the library as a crate, which is why the port stays
# on the 2 line.
./configure --prefix=/usr --with-libimagequant --with-lcms2
make
make DESTDIR=$PKG install
