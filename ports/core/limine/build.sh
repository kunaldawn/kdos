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

# FOUR PORTS AND A CD IMAGE, WHICH IS EVERY WAY AN x86-64 MACHINE STARTS.
#
# `uefi-ia32` IS NOT AN ARCHITECTURE, IT IS A FIRMWARE. A 64-bit CPU whose
# firmware is 32 bit — the early Atom tablets and a few netbooks — runs the
# same kernel and the same userland as any other x86-64 machine and can load
# only a 32-bit EFI binary, so without this port there is no first stage on
# it at all and the medium is not bootable. It costs one more freestanding
# build of the same source and nothing at run time: firmware picks the
# binary it can execute out of `EFI/BOOT` and never sees the other.
#
# `--enable-uefi-cd` gathers whichever EFI binaries were built into
# `limine-uefi-cd.bin`, so the El Torito record follows this list with no
# second switch.
./configure --prefix=/usr \
	--enable-bios \
	--enable-bios-cd \
	--enable-uefi-x86-64 \
	--enable-uefi-ia32 \
	--enable-uefi-cd

make
make install DESTDIR="$PKG"
