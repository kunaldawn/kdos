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

# THE THIN AND CACHE TOOLS ARE A DEPENDENCY, NOT A PROBE. Their paths are
# named rather than searched for, and configure still runs thin_check -V and
# cache_check -V to decide on --needs-check, dropping it when the tool is
# absent — so without thin-provisioning-tools installed first the result
# would change with build order. Thin and cache LVs refuse to activate
# without these tools.
# dmeventd is what makes lvm.conf's monitoring work: thin-pool autoextend and
# RAID repair. systemd and selinux have no port and are pinned off.
# EVENT ACTIVATION IS OFF, because its udev rule activates a completed volume
# group by running systemd-run, which does not exist here: left on, every
# hotplugged PV fails that RUN and activates nothing. Off, the rule only
# records the PV, and /etc/init.d/03_lvm.sh activates the groups at boot.
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
