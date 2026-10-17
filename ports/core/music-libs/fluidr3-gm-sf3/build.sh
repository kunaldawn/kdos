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

install -Dm644 FluidR3Mono_GM-$version.sf3 "$PKG/usr/share/soundfonts/FluidR3Mono_GM.sf3"
ln -s FluidR3Mono_GM.sf3 "$PKG/usr/share/soundfonts/default.sf2"
install -d "$PKG/usr/share/sounds/sf2"
ln -s ../../soundfonts/FluidR3Mono_GM.sf3 "$PKG/usr/share/sounds/sf2/FluidR3_GM.sf2"
install -Dm644 FluidR3Mono_License-$version.md "$PKG/usr/share/licenses/$name/LICENSE.md"
