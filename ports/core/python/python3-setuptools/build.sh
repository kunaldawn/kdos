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

python3 -c 'from setuptools import build_meta; build_meta.build_wheel("dist")'
_site=$(python3 -c 'import sysconfig; print(sysconfig.get_path("purelib"))')
python3 -m zipfile -e dist/setuptools-$version-*.whl "$PKG$_site"
python3 -m compileall -q -o 0 -o 1 -o 2 -d "$_site" "$PKG$_site"

find "$PKG" -path '*/setuptools/*.exe' -delete
