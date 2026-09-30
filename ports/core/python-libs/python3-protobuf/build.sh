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

# The sdist carries upb and utf8_range and compiles them into
# google._upb._message; it links no system library. Its version is the
# protobuf port's with a leading 7, and code protoc generates checks the
# runtime against that, so the two ports move together.
# --no-build-isolation reaches setuptools, a port.
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
