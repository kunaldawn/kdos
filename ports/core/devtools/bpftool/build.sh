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

cd src

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
