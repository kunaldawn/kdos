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

# hatch-fancy-pypi-readme is a build backend plugin and not a port, so it goes
# from the bundle into the build root, not into $PKG. --break-system-packages
# because python3 marks its site-packages as kpkg's (PEP 668), and an install
# into the build root is what that marker refuses.
mkdir -p vendor
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C vendor
pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--break-system-packages hatch-fancy-pypi-readme
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
