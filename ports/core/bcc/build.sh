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

# THE RELEASE ASSET IS USED, NOT THE TAG ARCHIVE, and that is the whole reason
# this recipe works. bcc carries libbpf and blazesym as git SUBMODULES, which a
# GitHub tag archive omits entirely — leaving empty directories, a configure
# that succeeds and a build that fails deep in a header. Upstream publishes
# `bcc-src-with-submodule-<ver>.tar.gz` for exactly this, and that is what
# `source =` names.
cd bcc

# IT IS HERE FOR bpftrace AND NOTHING ELSE. bpftrace's
# `find_package(LibBcc REQUIRED)` is unconditional — it uses bcc's USDT probe
# resolution — so the library is the deliverable and bcc's own hundred python
# tools are not: they are a second tracing UI with a python dependency at
# runtime, on a machine where bpftrace's one-liners are the interface.
# ENABLE_CLANG_JIT stays ON because that IS the library bpftrace links.
mkdir -p build && cd build
# ENABLE_LLVM_SHARED=OFF because THERE IS NO libLLVM.so HERE. ports/core/llvm
# builds with BUILD_SHARED_LIBS=ON and LLVM_BUILD_LLVM_DYLIB=OFF — 413
# per-component shared objects, which is a different thing from the single
# aggregate dylib. With it ON, bcc's clang_libs.cmake sets llvm_libs to the
# literal "LLVM" and the link ends at `cannot find -lLLVM`; with it OFF it maps
# the component names, which are exactly what this LLVM installs.
#
# REVISION IS PASSED BECAUSE THERE IS NO .git. bcc derives it from
# git_describe(), which in a tarball yields -NOTFOUND — and that string reaches
# the SOVERSION, so the link target is literally
# `libbcc.so.EAD-HASH-NOTFOUND`. The CMakeLists guards the lookup with
# `if(NOT REVISION)` for this case. Same shape as prjtrellis's version.cpp.
cmake .. -G Ninja \
	-DREVISION=$version \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DENABLE_LLVM_SHARED=OFF \
	-DENABLE_CLANG_JIT=ON \
	-DENABLE_MAN=OFF \
	-DENABLE_EXAMPLES=OFF \
	-DENABLE_TESTS=OFF \
	-DENABLE_USDT=ON \
	-DPYTHON_CMD=python3
ninja
DESTDIR=$PKG ninja install
