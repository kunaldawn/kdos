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

_pydevd=src/debugpy/_vendored/pydevd

# The C files beside pydevd's Cython sources are generated, so they are
# generated again here from the .pyx with the cython port, and the tracing
# accelerators are compiled from them in place. setup.py packs whatever
# compiled modules sit in the pydevd tree as data; REQUIRE_CYTHON_BUILD makes
# a failed compile stop the build instead of shipping the slow pure-Python
# tracer.
( cd "$_pydevd" && python3 setup_pydevd_cython.py build_ext --inplace --force-cython )
export REQUIRE_CYTHON_BUILD=1

# The attach library is what `debugpy --pid` injects into a running
# interpreter through gdb. Upstream ships no Linux build of it in the sdist;
# this is compile_linux.sh's command with the phase's flags.
g++ $CXXFLAGS $LDFLAGS -std=c++11 -shared -fPIC -nostartfiles \
	"$_pydevd/pydevd_attach_to_process/linux_and_mac/attach.cpp" \
	-o "$_pydevd/pydevd_attach_to_process/attach_linux_amd64.so"

pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
test -x "$PKG/usr/bin/debugpy"
ls "$PKG"/usr/lib/python3*/site-packages/debugpy/_vendored/pydevd/_pydevd_bundle/pydevd_cython*.so
test -e "$PKG"/usr/lib/python3*/site-packages/debugpy/_vendored/pydevd/pydevd_attach_to_process/attach_linux_amd64.so
