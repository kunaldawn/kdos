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

# A STANDALONE LLDB IS THE ONLY SHAPE THIS TREE CAN BUILD: llvm and clang are
# already installed packages, and LLDBStandalone.cmake resolves them through
# their installed cmake packages. Pointing LLVM_DIR and Clang_DIR at those
# directories makes the search exact — the HINTS-driven find_package would
# otherwise be free to pick up any other LLVM on the builder.
#
# LLVM_TARGETS_TO_BUILD is deliberately NOT set: it arrives from LLVMConfig
# with the full target list llvm was built with, and lldb uses it to decide
# which ABI and disassembler plugins to compile. Narrowing it here would drop
# architectures whose backends are already on the image.
#
# LLDB_INCLUDE_TESTS MUST BE SET: a standalone configure pins LLVM_INCLUDE_TESTS
# ON as an internal cache entry and LLDB_INCLUDE_TESTS defaults to it, so the
# default build pulls in the test suite and the lit tooling it needs.
#
# LIBEDIT CARRIES THE INTERACTIVE PROMPT: IOHandlerEditline constructs an
# Editline only when LLDB_ENABLE_LIBEDIT is on, and without it `(lldb)` is a
# plain line read with no history, no arrow-key editing and no tab completion.
# lldb accepts no other line editor — readline is not a choice it offers.
# An explicit ON is a hard requirement rather than a hint, so a missing
# libedit stops configure instead of quietly producing the narrow prompt.
cmake -S lldb -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D CMAKE_C_FLAGS_RELEASE="$CFLAGS" \
	-D CMAKE_CXX_FLAGS_RELEASE="$CXXFLAGS" \
	-D LLVM_DIR=/usr/lib/cmake/llvm \
	-D Clang_DIR=/usr/lib/cmake/clang \
	-D LLVM_ENABLE_RTTI=ON \
	-D LLDB_ENABLE_SWIG=ON \
	-D LLDB_ENABLE_PYTHON=ON \
	-D LLDB_ENABLE_CURSES=ON \
	-D LLDB_ENABLE_LZMA=ON \
	-D LLDB_ENABLE_LIBXML2=ON \
	-D LLDB_ENABLE_LUA=OFF \
	-D LLDB_ENABLE_LIBEDIT=ON \
	-D LLDB_INCLUDE_TESTS=OFF \
	-D LLVM_INCLUDE_TESTS=OFF \
	-D LLVM_ENABLE_SPHINX=ON \
	-D SPHINX_OUTPUT_HTML=OFF \
	-D SPHINX_OUTPUT_MAN=ON \
	-D SPHINX_WARNINGS_AS_ERRORS=OFF \
	-D LLVM_MAIN_SRC_DIR="$SRC/llvm" \
	-Wno-dev

cmake --build build
DESTDIR=$PKG cmake --install build

# The manual pages are built by target and installed by hand, as in llvm.
# LLVM_MAIN_SRC_DIR must name this tree: the docs target puts
# $LLVM_MAIN_SRC_DIR/../utils/docs on PYTHONPATH for the llvm_sphinx module
# conf.py imports, and the value LLVMConfig carries is the llvm port's own
# unpacked source, which no longer exists.
cmake --build build --target docs-lldb-man
install -Dm644 build/docs/man/*.1 -t "$PKG/usr/share/man/man1"
