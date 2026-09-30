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


# --no-build-isolation because setuptools and wheel are installed ports; pip's
# isolated environment would fetch them and fail with no network. setup.py
# lists its data files before its build step has rendered xdg/hicolor, so the
# icons are copied from there after the install, or the Start menu row has no
# picture. Every translation is compiled whatever the build is told, so the
# catalogues are removed from the package: bundled data is English only.
# The spell-check plugin loads gspell through GObject introspection and is
# unavailable without its typelib.
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .

install -d "$PKG/usr/share/icons"
cp -r xdg/hicolor "$PKG/usr/share/icons/"
rm -rf "$PKG/usr/share/locale"
