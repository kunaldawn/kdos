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
	--config-settings=cmake.define.ZMQ_PREFIX=/usr \
	--config-settings=cmake.define.PYZMQ_NO_BUNDLE=ON \
	--config-settings=cmake.define.PYZMQ_LIBZMQ_RPATH=OFF \
	--config-settings=cmake.define.ZMQ_DRAFT_API=OFF
site=$(find "$PKG"/usr/lib -maxdepth 2 -type d -name site-packages)
test -n "$(find "$site/zmq/backend/cython" -maxdepth 1 -name '_zmq*.so')"
