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

# kpkg copies a .zip into $SRC rather than unpacking it, so the recipe does.
unzip -q -o $name-$version.zip

# One OpenType Collection carrying every weight for jp, kr, sc, tc and hk.
# The per-language downloads are the same glyphs partitioned, so this is the
# smallest complete set and the one a fallback chain wants: fontconfig picks a
# face out of the collection per script with nothing to configure.
install -dm755 $PKG/usr/share/fonts/noto-cjk
install -m644 NotoSansCJK.ttc $PKG/usr/share/fonts/noto-cjk/
install -dm755 $PKG/usr/share/licenses/$name
install -m644 LICENSE $PKG/usr/share/licenses/$name/
