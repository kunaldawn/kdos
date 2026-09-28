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


# The daemon is D-Bus activated: org.freedesktop.UDisks2.service names
# /usr/lib/udisks2/udisksd, and no init script starts it. With no libsystemd
# and no libelogind found, it tracks no seats, so every request is decided by
# polkit's allow_any; wheel's grants are in /etc/polkit-1/rules.d.
#
# The LVM2, Btrfs and LSM modules are built. The LSM module (RAID volume
# data and the drive identify and fault LEDs) connects to lsmd over
# /var/run/lsm when it loads; with no lsmd answering it fails to load, logs
# the error and leaves the other modules loaded. iSCSI needs the libiscsi of
# a patched open-iscsi, not the libiscsi port, and is off. Mounts go under
# /run/media/<user>.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--libexecdir=/usr/lib --localstatedir=/var --disable-static \
	--enable-introspection=yes \
	--enable-daemon \
	--enable-man \
	--disable-gtk-doc \
	--disable-nls \
	--enable-acl \
	--enable-lvm2 \
	--enable-btrfs \
	--disable-iscsi \
	--enable-lsm \
	--enable-smart \
	--disable-fhs-media \
	--with-udevdir=/usr/lib/udev \
	--with-systemdsystemunitdir=no \
	--with-tmpfilesdir=no
make
make DESTDIR=$PKG install

# Neither directory is read here: there is no systemd-modules-load and no
# systemd-tmpfiles.
rm -rf "$PKG/usr/lib/modules-load.d" "$PKG/usr/lib/tmpfiles.d"
