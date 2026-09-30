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

# Pervasives is gone from OCaml 5; Stdlib is the same module under its
# current name.
patch -p1 -i "$PORT_SRC/ocaml5-stdlib.patch"

# The release's jbuild files are for a dune that no longer reads them, so the
# library is built with the plain Makefile in lib/, which compiles the same
# modules with the same flags. It is installed where FindLibfacile looks:
# facile.a and facile.cmi under $(ocamlc -where)/facile.
make -C lib facile.cma facile.cmxa
_dest="$PKG$(ocamlc -where)/facile"
install -d "$_dest"
install -m644 lib/facile.cma lib/facile.cmxa lib/facile.a \
	lib/*.cmi lib/*.cmx lib/*.mli "$_dest"/
cat > "$_dest/META" <<META
version = "$version"
description = "Constraint programming library"
archive(byte) = "facile.cma"
archive(native) = "facile.cmxa"
META
