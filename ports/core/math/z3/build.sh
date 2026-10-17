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

mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DPython3_EXECUTABLE=/usr/bin/python3 \
	-DZ3_BUILD_LIBZ3_SHARED=ON \
	-DZ3_BUILD_EXECUTABLE=ON \
	-DZ3_BUILD_TEST_EXECUTABLES=OFF \
	-DZ3_ENABLE_EXAMPLE_TARGETS=OFF \
	-DZ3_BUILD_DOCUMENTATION=OFF \
	-DZ3_USE_LIB_GMP=ON \
	-DZ3_BUILD_PYTHON_BINDINGS=ON \
	-DZ3_INSTALL_PYTHON_BINDINGS=ON \
	-DZ3_BUILD_JAVA_BINDINGS=OFF \
	-DZ3_BUILD_DOTNET_BINDINGS=OFF \
	-DZ3_BUILD_OCAML_BINDINGS=OFF \
	-DZ3_BUILD_JULIA_BINDINGS=OFF \
	-DZ3_BUILD_GO_BINDINGS=OFF \
	-DZ3_INCLUDE_GIT_HASH=OFF \
	-DZ3_INCLUDE_GIT_DESCRIBE=OFF
ninja
DESTDIR=$PKG ninja install

rm -f "$PKG"/usr/lib/python3*/site-packages/z3/libz3.so*

install -d "$PKG/usr/share/man/man1"
help2man -N --no-discard-stderr -n 'satisfiability modulo theories solver' \
	-o "$PKG/usr/share/man/man1/z3.1" ./z3
