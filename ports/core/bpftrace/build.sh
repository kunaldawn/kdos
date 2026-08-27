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
# No BLAZESYM (rust symbolisation for a case this does not need) and no static
# link — LLVM here is shared and a static bpftrace would want the whole of it.
# --copy-dt-needed-entries, AND THE DT_NEEDED CHAIN IS ALREADY CORRECT. This
# LLVM is BUILD_SHARED_LIBS=ON, so `libLLVMBPFCodeGen.so` records a NEEDED on
# `libLLVMBPFDesc.so` and the symbol IS reachable at run time. binutils has
# defaulted to --no-copy-dt-needed-entries since 2.22, which refuses a symbol
# an object references DIRECTLY unless the providing library is on the LINK
# LINE — and cmake puts only the component bpftrace named. The failure is
# `undefined reference to symbol 'LLVMInitializeBPFTargetMC'` beside a library
# that defines it. Restoring the older policy is narrower than second-guessing
# which of LLVM's 413 components each target transitively needs.
export LDFLAGS="$LDFLAGS -Wl,--copy-dt-needed-entries"

mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DUSE_SYSTEM_LIBBPF=ON \
	-DBUILD_TESTING=OFF \
	-DENABLE_MAN=OFF \
	-DBUILD_FUZZ=OFF \
	-DUSE_BLAZESYM=OFF \
	-DSTATIC_LINKING=OFF
ninja
DESTDIR=$PKG ninja install

# The tools ARE the documentation for a language nobody remembers the syntax
# of, so they are installed even with ENABLE_MAN off.
install -dm755 $PKG/usr/share/bpftrace/tools
cp -a $SRC/man/adoc $PKG/usr/share/bpftrace/ 2>/dev/null || true
cp -a $SRC/tools/*.bt $PKG/usr/share/bpftrace/tools/ 2>/dev/null || true
