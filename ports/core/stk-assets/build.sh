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

# The release tarball merges the code's data with the art repository's. This
# port packages the art directories only; supertuxkart packages the rest.
install -d "$PKG/usr/share/supertuxkart/data"
for d in karts library models music sfx textures tracks; do
	cp -R "data/$d" "$PKG/usr/share/supertuxkart/data/$d"
done
