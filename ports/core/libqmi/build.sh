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


meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dcollection=full \
	-Dfirmware_update=true \
	-Dmbim_qmux=true \
	-Dmm_runtime_check=true \
	-Dqrtr=true \
	-Drmnet=true \
	-Dudev=true \
	-Dqmi_username= \
	-Dqmi_groupname= \
	-Dintrospection=true \
	-Dgtk_doc=false \
	-Dman=true \
	-Dbash_completion=true \
	-Dfuzzer=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
