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

# The MTP filesystem: `aft-mtp-mount ~/Phone` puts the phone's storage under a
# directory any file manager reads, through fusermount3, and
# `fusermount3 -u ~/Phone` takes it away. It speaks MTP itself over the
# kernel's usbdevfs rather than through libmtp; libmtp is in depends for
# 69-libmtp.rules, which is what gives the dialout group the phone's USB node.
#
# BUILD_QT_UI=OFF is the hard rule. BUILD_MTPZ=OFF: MTPZ is the Zune handshake
# and needs keys that are not distributable. BUILD_PYTHON and BUILD_TAGLIB are
# off because nothing here imports the module or wants the tags written on
# upload. libmagic, from `file`, gives an uploaded file its MIME type. The
# library is linked statically into both programs and its archive is not
# shipped.
mkdir -p build && cd build
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DBUILD_QT_UI=OFF \
	-DBUILD_FUSE=ON \
	-DBUILD_MTPZ=OFF \
	-DBUILD_PYTHON=OFF \
	-DBUILD_TAGLIB=OFF \
	-DBUILD_SHARED_LIB=OFF
make
make DESTDIR=$PKG install
rm "$PKG/usr/lib/libmtp-ng-static.a"
