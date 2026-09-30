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

# The bundle unpacks beside the tree: a vendor/ directory at its top is a
# second top-level package to setuptools' discovery, which then refuses to
# guess which one is the plugin.
_vendor="$SRC_ROOT/vendor"
mkdir -p "$_vendor"
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C "$_vendor"

# lsprotocol and cattrs are the plugin's own: nothing else imports them, so
# they come from the bundle and install beside it. cattrs versions itself with
# hatch-vcs, which is given the version so it never looks for a git tree.
export SETUPTOOLS_SCM_PRETEND_VERSION_FOR_CATTRS=26.2.1
pip3 install --no-deps --no-index --find-links="$_vendor" --no-build-isolation \
	--root=$PKG --prefix=/usr \
	lsprotocol cattrs .
test -e "$PKG"/usr/lib/python3*/site-packages/pylsp_ruff/plugin.py
