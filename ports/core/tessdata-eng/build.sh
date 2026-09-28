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


# fast rather than best: the integer models are a quarter of the size for a few
# points of accuracy on clean scans. osd is what `--psm 0` and `--psm 1` load to
# find a page's orientation and script; without it both stop with "Failed
# loading language 'osd'". Tesseract searches /usr/share/tessdata, so a
# language added later is one more file beside these.
install -Dm644 tessdata-eng-$version.traineddata "$PKG/usr/share/tessdata/eng.traineddata"
install -Dm644 tessdata-osd-$version.traineddata "$PKG/usr/share/tessdata/osd.traineddata"
