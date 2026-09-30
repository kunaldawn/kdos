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

meson setup build --prefix=/usr --libdir=lib --buildtype=release
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

# rst2man is an optional find_program with no switch behind it, so the manual
# page is the one output that can go missing without an error. Its absence
# means python3-docutils did not reach the chroot, and that stops the build.
test -f $PKG/usr/share/man/man1/sshfs.1
