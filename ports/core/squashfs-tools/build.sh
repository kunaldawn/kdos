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

cd squashfs-tools
# Patch to bypass strict GNU sed check as we are using toybox sed
sed -i 's/if ! check_sed "${SED}"; then/if false; then/' generate-manpages/functions.sh

SED=/usr/bin/sed make \
	XZ_SUPPORT=1 \
	LZMA_XZ_SUPPORT=1 \
	ZLIB_SUPPORT=1 \
	LZ4_SUPPORT=1 \
	ZSTD_SUPPORT=1 \
	USE_PREBUILT_MANPAGES=y \
	INSTALL_DIR=$PKG/usr/bin \
	INSTALL_MANPAGES_DIR=$PKG/usr/share/man/man1 \
	install
