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

# THIS IS NOT A PROGRAM, IT IS AN EFI PAYLOAD, and that is the whole reason it
# earns a place: bad RAM is the one fault a tool running under an OS cannot
# honestly diagnose, because the OS is itself in the memory being tested and
# the pages it would like to check are the ones it is running from. memtest
# boots INSTEAD of the kernel, owns the machine, and tests everything.
#
# It is GPL-2.0 since the v6 rewrite, which is what makes shipping it possible
# at all — the pre-v6 lineage was not redistributable on these terms.
#
# The build is a freestanding 64-bit EFI image with its own linker script, and
# the Makefile assigns CFLAGS with `=` rather than `+=` — so it already
# discards the hosted-target flags this tree exports, and NOTHING may be passed
# on the command line. `make CFLAGS=` would beat that assignment and take
# -DARCH_BITS=64 with it, which compiles the 32-bit word paths against a
# 64-bit testword_t and fails on an incompatible pointer four files in.
#
# One build directory per architecture since v7; the target is `mt86plus`,
# which is the image linked against ldscripts/memtest_efi.lds. `make all` is
# not the default `iso`, which additionally wants xorriso and a floppy image.
cd build/x86_64
make -j1 all

# 06_packaging places it beside refind on the ESP and kinstall copies it to the
# installed one; the package's job is only to put the payload somewhere both
# can find it, under the name they already look for.
install -Dm644 mt86plus $PKG/usr/share/kdos/memtest86plus/memtest.efi
