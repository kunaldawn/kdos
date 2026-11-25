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

install -Dm644 tessdata-eng-$version.traineddata "$PKG/usr/share/tessdata/eng.traineddata"
install -Dm644 tessdata-osd-$version.traineddata "$PKG/usr/share/tessdata/osd.traineddata"

install -Dm644 /usr/share/licenses/openssl/LICENSE.txt "$PKG/usr/share/licenses/$name/LICENSE"
