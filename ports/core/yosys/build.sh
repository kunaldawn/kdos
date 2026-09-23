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
# THAT TARBALL IS FLAT — no wrapping directory — and kpkg
# passes --strip-components=1 to the first source unconditionally, so every
# top-level file is DISCARDED and each subdirectory's contents are promoted
# into its place. Same shape as tzdata's, and the same answer: unpack it here,
# unstripped, and build from that.
mkdir -p unpacked
tar xf "$PORT_SRC/$name-$version.tar.gz" -C unpacked
cd unpacked

# CMAKE, NOT `make config-gcc`. The source carries no Makefile; every switch
# is a YOSYS_* cache entry, and an ENABLE_* variable is ignored.
#
# YOSYS_USE_BUNDLED_LIBS stays OFF so the readline, zlib, libffi and tcl in
# this tree are the ones linked — abc is the one vendored thing kept, because
# it is the revision yosys was tested against and abc publishes no releases to
# pin to independently. Python is off: pyosys is a binding nothing here uses.
#
# THE LIBRARIES CANNOT BE FORCED ON FROM THE COMMAND LINE. yosys has only
# YOSYS_WITHOUT_* switches, and a library pkg-config does not find is silently
# a feature compiled out. The generated yosys_config.h is checked instead, so a
# missing readline, zlib, libffi or tcl stops the build here rather than
# shipping a yosys without gzip input, DPI-C, line editing or SDC parsing.
# The `show` pass renders with graphviz's `dot` (`-format svg`) or opens xdot
# (no -format); graphviz is not a port and xdot is a GTK viewer, so both fail
# and `show -format dot` — the .dot file alone — is what works here.
mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DYOSYS_USE_BUNDLED_LIBS=OFF \
	-DYOSYS_WITH_PYTHON=OFF \
	-DYOSYS_WITHOUT_ZLIB=OFF \
	-DYOSYS_WITHOUT_LIBFFI=OFF \
	-DYOSYS_WITHOUT_READLINE=OFF \
	-DYOSYS_WITHOUT_TCL=OFF \
	-DYOSYS_WITHOUT_EDITLINE=ON \
	-DYOSYS_ENABLE_UNIT_TESTS=OFF \
	-DYOSYS_ENABLE_FUNCTIONAL_TESTS=OFF \
	-DYOSYS_INSTALL_DRIVER=ON
for f in ZLIB LIBFFI READLINE TCL; do
	grep -q "^#define YOSYS_ENABLE_$f\$" kernel/yosys_config.h || {
		echo "yosys: YOSYS_ENABLE_$f is off — its library was not found" >&2
		exit 1
	}
done
ninja
DESTDIR=$PKG ninja install
