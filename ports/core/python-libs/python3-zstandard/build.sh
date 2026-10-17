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

pip3 install --ignore-installed --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr . \
	--config-settings=--build-option=--system-zstd \
	--config-settings=--build-option=--no-cffi-backend
site=$(find "$PKG"/usr/lib -maxdepth 2 -type d -name site-packages)
test -n "$(find "$site/zstandard" -maxdepth 1 -name 'backend_c*.so')"
