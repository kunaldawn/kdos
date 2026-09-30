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

# The C backend against the zstd port (--system-zstd), so a zstd update
# reaches Python too. The CFFI backend always compiles its own bundled copy of
# zstd, so it is off. The system library must be at least the release the
# bindings were cut against: the module refuses to import on an older one.
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr . \
	--config-settings=--build-option=--system-zstd \
	--config-settings=--build-option=--no-cffi-backend
site=$(find "$PKG"/usr/lib -maxdepth 2 -type d -name site-packages)
test -n "$(find "$site/zstandard" -maxdepth 1 -name 'backend_c*.so')"
