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

# qemu's aarch64 and riscv64 targets REQUIRE libfdt, and with --disable-download
# its own git-submodule copy of dtc is not fetched. Without this port the two
# targets fail meson setup rather than silently going missing.
meson setup build --prefix=/usr --libdir=lib --buildtype=release \
	-Dpython=disabled -Dtools=true -Dtests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
