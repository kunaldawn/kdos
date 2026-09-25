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

patch -p1 -i $PORT_SRC/llvm23-mccontext.patch

# THE LIBRARY IS HERE FOR bpftrace. bpftrace's `find_package(LibBcc REQUIRED)`
# is unconditional — it uses bcc's USDT probe resolution — so the library is
# the deliverable. The ready-made tracers are the compiled libbpf-tools below;
# bcc's own python tools are a second tracing UI with a python dependency at
# runtime, on a machine where libbpf-tools and bpftrace's one-liners are the
# interface. ENABLE_CLANG_JIT stays ON because that IS the library bpftrace
# links.
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
#
# CMAKE_USE_LIBBPF_PACKAGE LINKS THE libbpf PORT. Without it bcc compiles its
# bundled libbpf into libbcc_bpf, and bpftrace, which links both, then carries
# two copies of libbpf. The bundled snapshot is ahead of the port, and bcc
# calls nothing from the newer symbol version.
#
# liblzma (MiniDebugInfo symbols) is found by probing alone, so xz is in
# `depends`. debuginfod fetches symbols over the network and is off twice:
# ENABLE_LIBDEBUGINFOD only guards the headers, and the library is linked
# whenever it is found, so the search is disabled as well. The Lua front end
# builds whenever luajit is installed, and nothing uses it.
cmake .. -G Ninja \
	-DREVISION=$version \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_USE_LIBBPF_PACKAGE=ON \
	-DENABLE_LLVM_SHARED=OFF \
	-DENABLE_CLANG_JIT=ON \
	-DENABLE_LIBDEBUGINFOD=OFF \
	-DCMAKE_DISABLE_FIND_PACKAGE_LibDebuginfod=TRUE \
	-DCMAKE_DISABLE_FIND_PACKAGE_LuaJIT=TRUE \
	-DENABLE_MAN=ON \
	-DENABLE_EXAMPLES=OFF \
	-DENABLE_TESTS=OFF \
	-DENABLE_USDT=ON \
	-DPYTHON_CMD=python3
ninja
DESTDIR=$PKG ninja install

# libbpf-tools ARE THE PYTHON-FREE TRACERS — execsnoop, opensnoop, biolatency
# and the rest as compiled CO-RE programs, installed as bpf-<tool> so generic
# names like `profile` and `capable` stay free. They are their own Makefile,
# outside the cmake build, and carry vmlinux.h per architecture, so nothing
# is read from the build machine's kernel.
#
# BPFTOOL IS THE PORT'S. The Makefile would otherwise build a bootstrap bpftool
# from the release asset's libbpf-tools/bpftool, whose own libbpf submodule is
# empty. The tools link the bundled libbpf statically, which is what their
# Makefile builds, and a static copy inside each tool is not a second copy in
# any process that loads libbpf.
#
# USE_BLAZESYM=0 because blazesym is a Rust crate the Makefile builds with a
# networked cargo whenever cargo is on the PATH. argp is libc on glibc and a
# separate library on musl, hence argp-standalone and -largp.
export EXTRA_LDFLAGS=-largp
_mk=(USE_BLAZESYM=0 BPFTOOL=/usr/sbin/bpftool prefix=/usr APP_PREFIX=bpf-)
make -C ../libbpf-tools "${_mk[@]}"
make -C ../libbpf-tools "${_mk[@]}" DESTDIR=$PKG install
