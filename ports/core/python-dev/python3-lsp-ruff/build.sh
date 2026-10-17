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

_vendor="$SRC_ROOT/vendor"
mkdir -p "$_vendor"
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C "$_vendor"

export SETUPTOOLS_SCM_PRETEND_VERSION_FOR_CATTRS=26.2.1
pip3 install --ignore-installed --no-deps --no-index --find-links="$_vendor" --no-build-isolation \
	--root=$PKG --prefix=/usr \
	lsprotocol cattrs .
test -e "$PKG"/usr/lib/python3*/site-packages/pylsp_ruff/plugin.py
