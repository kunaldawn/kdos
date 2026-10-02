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

# EVERY KERNEL PREREQUISITE IS ALREADY SET IN kdos.config — DEBUG_INFO_BTF,
# BPF_SYSCALL, BPF_JIT, KPROBES, UPROBES — which is what makes this a port
# rather than a kernel change plus a port. Without BTF in particular bpftrace
# can attach to a probe and cannot name a single struct field, which is most of
# what people use it for.
#
# USE_SYSTEM_LIBBPF=ON IS REQUIRED, NOT PREFERRED. bpftrace vendors libbpf as a
# git submodule and the default is to build it; a GitHub tag archive contains
# an EMPTY libbpf/ directory, so the default configuration fails in a way that
# looks like a broken checkout. The `libbpf` port is what fills that in.
#
# No BLAZESYM (rust symbolisation for a case this does not need; its search is
# disabled so a stray install cannot switch it on) and no static link — LLVM
# here is shared and a static bpftrace would want the whole of it.
#
# THE OPTIONAL LIBRARIES ARE REQUIRED HERE. bpftrace find_package()s libbfd and
# libopcodes (the disassembler behind -d), libdw (uprobe arguments and struct
# types from DWARF) and libpcap (skb_output) and quietly builds without any it
# misses; CMAKE_REQUIRE_FIND_PACKAGE turns a missing one into a configure
# error, and binutils, elfutils and libpcap are in `depends`.
#
# -DASCIIDOCTOR NAMES THE PROGRAM rather than letting cmake search for it: a
# search that fails only warns and ships no bpftrace(8), where a named path that
# is missing fails the man target.
#
# EVERY LLVM COMPONENT IS NAMED AT THE END OF EVERY C++ LINK. This LLVM is
# BUILD_SHARED_LIBS=ON, one library per component, and cmake puts on a link
# line only the few components bpftrace asked for. bpftrace-aotrt calls none of
# them, so --as-needed drops each, and the libraries they need, LLVMSupport,
# LLVMObject and LLVMDebugInfoDWARF among them, are never searched: the link
# fails on `llvm::raw_ostream` and `llvm::object::createBinary`. `llvm-config
# --libs` names all of them, and --as-needed records only the ones a binary
# uses.

patch -p1 -i $PORT_SRC/llvm-definitions.patch

mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DUSE_SYSTEM_LIBBPF=ON \
	-DBUILD_TESTING=OFF \
	-DENABLE_MAN=ON \
	-DASCIIDOCTOR=/usr/bin/asciidoctor \
	-DENABLE_SKB_OUTPUT=ON \
	-DENABLE_SYSTEMD=OFF \
	-DCMAKE_REQUIRE_FIND_PACKAGE_LibBfd=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_LibOpcodes=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_LibDw=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_LibPcap=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_LibBlazesym=ON \
	-DSTATIC_LINKING=OFF \
	-DCMAKE_CXX_STANDARD_LIBRARIES="$(llvm-config --libs)"
ninja
DESTDIR=$PKG ninja install
