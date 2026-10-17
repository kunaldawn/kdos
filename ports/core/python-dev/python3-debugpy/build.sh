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

patch -p1 -i "$PORT_SRC/cython-redeclared.patch"

_pydevd=src/debugpy/_vendored/pydevd

( cd "$_pydevd" && python3 setup_pydevd_cython.py build_ext --inplace --force-cython )
export REQUIRE_CYTHON_BUILD=1

g++ $CXXFLAGS $LDFLAGS -std=c++11 -shared -fPIC -nostartfiles \
	"$_pydevd/pydevd_attach_to_process/linux_and_mac/attach.cpp" \
	-o "$_pydevd/pydevd_attach_to_process/attach_linux_amd64.so"

pip3 install --ignore-installed --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
test -x "$PKG/usr/bin/debugpy"
ls "$PKG"/usr/lib/python3*/site-packages/debugpy/_vendored/pydevd/_pydevd_bundle/pydevd_cython*.so
test -e "$PKG"/usr/lib/python3*/site-packages/debugpy/_vendored/pydevd/pydevd_attach_to_process/attach_linux_amd64.so
