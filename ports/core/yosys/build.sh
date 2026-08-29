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

# THE RELEASE TARBALL IS USED, NOT THE TAG ARCHIVE, and it is the same reason
# as bcc's: yosys carries abc — the Berkeley logic synthesis tool it shells out
# to for technology mapping — as a submodule, which a tag archive omits. The
# published `yosys.tar.gz` has it. Without abc, synthesis runs and every
# `abc` pass fails, which is most of what makes the output small enough to fit.
#
# THAT TARBALL IS FLAT — 37 top-level entries, no wrapping directory — and kpkg
# passes --strip-components=1 to the first source unconditionally, so every
# top-level file is DISCARDED and each subdirectory's contents are promoted
# into its place. Same shape as tzdata's, and the same answer: unpack it here,
# unstripped, and build from that.
mkdir -p unpacked
tar xf "$PORT_SRC/$name-$version.tar.gz" -C unpacked
cd unpacked

# CMAKE, NOT `make config-gcc`. 0.68 has no Makefile at all; the hand-written
# build with its ENABLE_* variables is gone and every switch is a YOSYS_* cache
# entry now.
#
# YOSYS_USE_BUNDLED_LIBS stays OFF so the readline, zlib, libffi and tcl in
# this tree are the ones linked — abc is the one vendored thing kept, because
# it is the revision yosys was tested against and abc publishes no releases to
# pin to independently. Python is off: pyosys is a binding nothing here uses.
mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DYOSYS_USE_BUNDLED_LIBS=OFF \
	-DYOSYS_WITH_PYTHON=OFF \
	-DYOSYS_WITHOUT_EDITLINE=ON \
	-DYOSYS_ENABLE_FUNCTIONAL_TESTS=OFF \
	-DYOSYS_INSTALL_DRIVER=ON
ninja
DESTDIR=$PKG ninja install
