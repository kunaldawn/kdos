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

# Linked against the zeromq port: PYZMQ_NO_BUNDLE turns a missing libzmq into
# a configure error, where pyzmq would otherwise download and build a private
# copy. No rpath: libzmq is in the default library path. The draft API stays
# off, matching the library.
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr . \
	--config-settings=cmake.define.ZMQ_PREFIX=/usr \
	--config-settings=cmake.define.PYZMQ_NO_BUNDLE=ON \
	--config-settings=cmake.define.PYZMQ_LIBZMQ_RPATH=OFF \
	--config-settings=cmake.define.ZMQ_DRAFT_API=OFF
site=$(find "$PKG"/usr/lib -maxdepth 2 -type d -name site-packages)
test -n "$(find "$site/zmq/backend/cython" -maxdepth 1 -name '_zmq*.so')"
