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


# Several plugins compile with -Werror; a warning a newer compiler adds must
# not stop the build.
export CFLAGS="$CFLAGS -Wno-error"

# Every plugin udisks2 requires is built: part, fs, loop, swap, mdraid,
# crypto (with escrow through volume_key and NSS), nvme and smart, plus btrfs,
# dm, lvm, mpath and nvdimm (through ndctl's libndctl; deprecated upstream).
# The btrfs, lvm, mdraid, mpath and fs plugins drive the command-line tools of
# their ports at run time, which is why those ports are dependencies.
# lvm_dbus talks to lvmdbusd, which lvm2 does not ship here. The smart plugin
# reads smartmontools' drive database for its attribute names.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib --disable-static \
	--enable-introspection=yes \
	--disable-tests \
	--without-python3 \
	--without-gtk-doc \
	--without-tools \
	--without-s390 \
	--with-nvdimm \
	--without-lvm_dbus \
	--with-escrow \
	--with-nvme \
	--with-smart \
	--with-smartmontools \
	--with-drivedb=/usr/share/smartmontools
# The plugin API sources are regenerated from their .api files when make
# thinks them stale, by $(PYTHON), which --without-python3 sets to a bare
# `python` this system does not have.
make PYTHON=python3
make PYTHON=python3 DESTDIR=$PKG install
