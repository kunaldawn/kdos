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

# An empty udevrulesdir asks pkg-config for udev's directory, so whether
# 99-fuse3.rules ships would follow build order; eudev's is /lib/udev.
#
# enable-io-uring is off because fuse-over-io-uring links libnuma as well as
# liburing, and no port provides libnuma: left on, the feature would switch
# itself on for whichever build root happened to carry both.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib --libexecdir=/usr/lib \
	-Dinitscriptdir= \
	-Dudevrulesdir=/lib/udev/rules.d \
	-Denable-io-uring=false \
	-Dexamples=false \
	-Dtests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

# useroot=true is what chmods u+s onto /usr/bin/fusermount3, and libfuse's
# fallback helper is useless without it: a login session with no user namespace
# gets EPERM from mount(2), runs fusermount3, and it cannot mount either. sshfs,
# gocryptfs and the document portal all arrive by that path.
#
# The same branch also mknods a /dev/fuse into DESTDIR. That node is devtmpfs's
# to create when the module loads, and /dev in the build chroot is a bind of the
# container's own: a package owning the path makes kpkg write a character device
# outside the rootfs being built, and records in the package database a file no
# image ever carries.
rm -rf "$PKG/dev"
