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

# musl has no <execinfo.h>. Upstream guards the include in two of the three
# files that want it and forgets the third; the patch adds the guard configure
# already computes.
patch -p1 -i "$PORT_SRC/musl-execinfo.patch"

# And the 2.2.7 release does not compile on any libc: four writes to a display
# flag its own header records as removed outlived the removal, and no header
# declares the name.
patch -p1 -i "$PORT_SRC/dead-dpms-flag.patch"

# The tarball is a git archive, so there is no configure yet.
autoreconf -fi

# --disable-x11 is not a preference. It defaults to YES and its
# PKG_CHECK_MODULES for x11, xrandr and xext carry no action-if-not-found, so
# configure TERMINATES on a tree that has no xrandr.pc — which this one does
# not and will not. X11 buys ddcutil one thing, reading EDID through RandR, and
# --enable-drm reads the same EDID from the connector's own sysfs.
#
# --disable-systemd only avoids a warning: its check has a fallback that
# degrades to off. Saying it makes the absence deliberate rather than incidental.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--disable-x11 \
	--disable-systemd \
	--enable-drm \
	--enable-udev \
	--disable-usb

make
make DESTDIR=$PKG install

# UPSTREAM'S OWN UDEV RULE IS DELETED, AND THIS IS THE SECURITY LINE OF THE
# RECIPE. Its first rule is `SUBSYSTEM=="i2c-dev", KERNEL=="i2c-[0-9]*",
# GROUP="i2c", MODE="0660"` — every I2C bus on the machine, the chipset SMBus
# included, and every DIMM's SPD EEPROM hangs off that. It is inert today only
# because there is no `i2c` group, which makes it a hole that opens the day
# somebody adds one. fs/etc/udev/rules.d/70-kdos-i2c.rules is the scoped
# replacement and the only i2c rule this image carries.
rm -f "$PKG/usr/lib/udev/rules.d/60-ddcutil-i2c.rules"
rmdir --ignore-fail-on-non-empty "$PKG/usr/lib/udev/rules.d" 2>/dev/null || true

# And its modules-load drop-in, which says `i2c-dev` under /usr/lib where
# 02_modules.sh does not look. fs/etc/modules-load.d/kdos-i2c.conf is the copy
# that is read.
rm -f "$PKG/usr/lib/modules-load.d/ddcutil.conf"
rmdir --ignore-fail-on-non-empty "$PKG/usr/lib/modules-load.d" 2>/dev/null || true
