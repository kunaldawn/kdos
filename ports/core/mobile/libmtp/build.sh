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

# --disable-mtpz keeps libgcrypt off the link. MTPZ is Microsoft's Zune
# handshake and its key material is not distributable, so the feature is
# unusable here; configure defaults it on and silently degrades to off when
# libgcrypt is missing, which would make the link depend on what else the
# chroot happened to have installed.
#
# --disable-doxygen for the same reason: the default is `auto`, and this tree
# ships doxygen, so the API manual would appear or not depending on build
# order.
#
# The udev group is what makes the device reachable. Without --with-udev-group
# the generated rules carry no GROUP= and every phone node is root-only, which
# is a library that finds a device and cannot open it. `dialout` is the group
# fs/etc/udev/rules.d/70-kdos-camera.rules already grants for PTP cameras and
# the one the desktop user is in.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib --disable-static \
	--disable-mtpz \
	--disable-doxygen \
	--disable-rpath \
	--with-udev=/usr/lib/udev \
	--with-udev-rules=69-libmtp.rules \
	--with-udev-group=dialout \
	--with-udev-mode=0660

make
make DESTDIR=$PKG install
