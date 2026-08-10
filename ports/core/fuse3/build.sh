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

meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

# util/install_helper.sh mknods a /dev/fuse into DESTDIR. A device node is
# devtmpfs's to create when the module loads, never a package's to own, and
# shipping one means kpkg tries to rename a file onto /dev — a separate mount
# inside the chroot — which fails EXDEV and aborts the install half-written.
# `-Duseroot=false` would also suppress it, but it would take the `chmod u+s`
# on fusermount3 with it, and rootless fuse-overlayfs in the appbox needs that
# setuid bit. So the node is removed and the rest of useroot=true kept.
rm -rf $PKG/dev
