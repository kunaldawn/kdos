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

patch -p1 -i $PORT_SRC/musl-stdio-freopen.patch

# THE THIN AND CACHE TOOLS ARE NAMED, NOT PROBED. thin-provisioning-tools
# links this port's libdevmapper, so it is built after lvm2 and cannot be
# asked for its version here. Every path is given and --needs-check is
# enabled outright: thin_check takes the flag from 0.3 and cache_check from
# 0.5.4, and the port is 1.3. THIN_CONFIGURE_WARN and CACHE_CONFIGURE_WARN,
# set, make configure skip its `thin_check -V` and `cache_check -V` probes,
# which would find no tool and turn --needs-check off. Its closing warning
# that the tools are missing then describes the build machine, not the image.
# Thin and cache LVs refuse to activate without the tools; the phase list
# installs them.
# dmeventd is what makes lvm.conf's monitoring work: thin-pool autoextend and
# RAID repair. systemd and selinux have no port and are pinned off.
# EVENT ACTIVATION IS OFF, and its udev rule, 69-dm-lvm.rules, is not
# installed: the rule activates a completed volume group by running
# /usr/bin/systemd-run, which does not exist here, and with activation off it
# does nothing but run `lvm pvscan` on every block device event.
# /etc/init.d/03_lvm.sh activates the groups at boot, and the initramfs before
# it on a disk boot.
THIN_CONFIGURE_WARN=y CACHE_CONFIGURE_WARN=y \
CONFIG_SHELL=/bin/bash  \
./configure --prefix=/usr \
	--libdir=/usr/lib \
	--libexecdir=/usr/lib \
	--enable-cmdlib \
	--enable-pkgconfig \
	--enable-udev_sync \
	--enable-dmeventd \
	--enable-readline \
	--with-blkid \
	--enable-blkid_wiping \
	--with-libnvme \
	--enable-nvme-wwid \
	--with-thin-check=/usr/sbin/thin_check \
	--with-thin-dump=/usr/sbin/thin_dump \
	--with-thin-repair=/usr/sbin/thin_repair \
	--with-thin-restore=/usr/sbin/thin_restore \
	--with-cache-check=/usr/sbin/cache_check \
	--with-cache-dump=/usr/sbin/cache_dump \
	--with-cache-repair=/usr/sbin/cache_repair \
	--with-cache-restore=/usr/sbin/cache_restore \
	--enable-thin_check_needs_check \
	--enable-cache_check_needs_check \
	--without-systemd \
	--with-default-event-activation=0 \
	--disable-selinux
make 
make DESTDIR=$PKG install_lvm2 install_device-mapper
find "$PKG" -name 69-dm-lvm.rules -delete
