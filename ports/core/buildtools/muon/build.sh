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

./bootstrap.sh stage1
stage1/muon-bootstrap setup \
	-Dprefix=/usr \
	-Dbuildtype=release \
	-Dwrap_mode=nodownload \
	-Dlibcurl=disabled \
	-Dlibarchive=disabled \
	-Dlibpkgconf=disabled \
	-Dtracy=disabled \
	-Dmeson-docs=disabled \
	-Dmeson-tests=disabled \
	-Dwebsite=disabled \
	build
stage1/muon-bootstrap -C build samu
DESTDIR=$PKG build/muon -C build install
