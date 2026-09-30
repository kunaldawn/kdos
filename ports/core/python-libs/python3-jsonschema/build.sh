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

# hatch-fancy-pypi-readme is a build backend plugin and not a port. It goes
# into a directory of its own on PYTHONPATH for this build only, so nothing
# outside the package is written: installed into the build root it would be a
# file in the image that no package owns.
mkdir -p vendor
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C vendor
_back="$SRC_ROOT/backends"
pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--target "$_back" hatch-fancy-pypi-readme
export PYTHONPATH="$_back${PYTHONPATH:+:$PYTHONPATH}"
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
