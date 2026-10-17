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

export XML_CATALOG_FILES=/etc/xml/catalog

meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--libexecdir=lib/gvfs \
	--buildtype=release \
	-Dsystemduserunitdir=no \
	-Dtmpfilesdir=no \
	-Dbusy_processes_command=lsof \
	-Dadmin=true \
	-Dafc=true \
	-Dafp=false \
	-Darchive=true \
	-Dcdda=true \
	-Ddnssd=true \
	-Dgoa=false \
	-Dgoogle=false \
	-Dgphoto2=true \
	-Dhttp=true \
	-Dmtp=true \
	-Dnfs=false \
	-Donedrive=false \
	-Dsftp=true \
	-Dsmb=true \
	-Dudisks2=true \
	-Dwsdd=false \
	-Dbluray=true \
	-Dfuse=true \
	-Dgcr=false \
	-Dgcrypt=true \
	-Dgudev=true \
	-Dkeyring=true \
	-Dlogind=false \
	-Dlibusb=true \
	-Ddevel_utils=false \
	-Dinstalled_tests=false \
	-Dunit_tests=false \
	-Dman=true
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
