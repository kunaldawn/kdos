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

_back="$SRC_ROOT/backends"
pip3 install --no-deps --no-index --find-links="$_vendor" --no-build-isolation \
	--target "$_back" coherent-licensed
export PYTHONPATH="$_back${PYTHONPATH:+:$PYTHONPATH}"

pip3 install --ignore-installed --no-deps --no-index --find-links="$_vendor" --no-build-isolation \
	--root=$PKG --prefix=/usr \
	jaraco-classes jaraco-context jaraco-functools more-itertools jeepney \
	secretstorage .
test -x "$PKG/usr/bin/keyring"
