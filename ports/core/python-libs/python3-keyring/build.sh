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

mkdir -p vendor
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C vendor

# coherent.licensed, the setuptools plugin keyring and the jaraco packages
# build with, is not a port. It is installed into a directory of its own on
# PYTHONPATH for this build only, so nothing outside the package is written.
_back="$SRC_ROOT/backends"
pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--target "$_back" coherent-licensed
export PYTHONPATH="$_back${PYTHONPATH:+:$PYTHONPATH}"

# The Secret Service backend (SecretStorage over jeepney's pure-Python D-Bus)
# is the one that works here: it stores into whichever program owns
# org.freedesktop.secrets on the session bus.
pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--root=$PKG --prefix=/usr \
	jaraco-classes jaraco-context jaraco-functools more-itertools jeepney \
	secretstorage .
test -x "$PKG/usr/bin/keyring"
