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

patch -p1 -i "$PORT_SRC/ocaml5-stdlib.patch"

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
