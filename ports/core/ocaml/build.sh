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

# The build starts from the bytecode compiler image the source carries in
# boot/, run by the ocamlrun it compiles first, and rebuilds every compiler
# from source with it; the image is a bootstrap seed and is not installed.
# ocamlopt emits assembly and links through gcc and as at run time, which is
# why binutils is a dependency. Libraries go under /usr/lib/ocaml, where
# 'ocamlc -where' and every consumer's find module look.
./configure \
	--prefix=/usr \
	--libdir=/usr/lib/ocaml \
	--mandir=/usr/share/man \
	--disable-ocamltest \
	--disable-debug-runtime \
	--disable-instrumented-runtime
make
make DESTDIR=$PKG install
