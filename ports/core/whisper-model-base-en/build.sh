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


# English only, base size: 142 MB that transcribes faster than real time on
# the CPU backends. The multilingual and larger models are hundreds of
# megabytes each and belong on the library medium, not the image.
# /usr/share/whisper.cpp/models is where kdos-rec looks, and upstream's own
# file name is the one whisper-cli's -m and every model list use.
install -Dm644 ggml-base.en-$version.bin "$PKG/usr/share/whisper.cpp/models/ggml-base.en.bin"
