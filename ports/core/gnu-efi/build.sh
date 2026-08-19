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

export CFLAGS="$CFLAGS -D__DEFINED_wchar_t -Dwchar_t=uint16_t"

# Make.rules links with $(LD) $(LDFLAGS) — the raw linker, not the compiler
# driver — so a -Wl, prefix arrives at ld verbatim and is rejected outright.
# Nothing is given up by stripping it: Make.defaults already carries
# --build-id=sha1 in ld's own spelling, so the package stays reproducible.
export LDFLAGS="${LDFLAGS//-Wl,/}"

make
make INSTALLROOT=$PKG PREFIX=/usr install
