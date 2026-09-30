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

# The fish completion directory is named: left empty, meson asks fish.pc for it
# when fish happens to be installed and falls back to this path when it is not,
# so where the two files land would follow the chroot's contents.
meson setup build --prefix=/usr --libdir=lib \
	-Dfishcompletiondir=/usr/share/fish/vendor_completions.d
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
