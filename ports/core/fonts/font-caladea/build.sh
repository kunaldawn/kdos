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

# The crosextrafonts release is the Caladea LibreOffice pins. The later
# huertatipografica Caladea changed its metrics, and a Cambria document set
# in it no longer paginates as it does in Word.
install -dm755 $PKG/usr/share/fonts/caladea
install -m644 Caladea-*.ttf $PKG/usr/share/fonts/caladea/
