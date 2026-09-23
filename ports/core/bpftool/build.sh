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


# The release archive carries libbpf in libbpf/, at the commit this bpftool was
# synced against, and the Makefile always links that copy statically; it has
# no switch for a system libbpf.
cd src

# Every probe in Makefile.feature is overridden on the command line, so a
# missing dependency fails the build instead of quietly narrowing the binary.
# llvm is the JIT disassembler and libbfd is its fallback, never linked here.
# clang-bpf-co-re builds the profiler and pid-iterator skeletons that
# `prog profile` and the PID columns need.
#
# The skeletons are generated against the BTF of the running kernel, which is
# what the chroot's /sys holds. Naming VMLINUX_BTF makes a kernel without BTF a
# build failure; left to the default search, the skeletons are dropped and the
# build still succeeds.
_mk=(
	prefix=/usr
	VMLINUX_BTF=/sys/kernel/btf/vmlinux
	feature-llvm=1
	feature-libcap=1
	feature-clang-bpf-co-re=1
	feature-libelf-zstd=1
	feature-libbfd=0
	feature-libbfd-liberty=0
	feature-libbfd-liberty-z=0
	feature-disassembler-four-args=0
	feature-disassembler-init-styled=0
)

make "${_mk[@]}"
make "${_mk[@]}" DESTDIR=$PKG install
make -C ../docs prefix=/usr mandir=/usr/share/man DESTDIR=$PKG install
