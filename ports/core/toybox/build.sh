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

make defconfig -j1
sed -i 's/# CONFIG_EXPR is not set/CONFIG_EXPR=y/' .config
sed -i 's/# CONFIG_GETTY is not set/CONFIG_GETTY=y/' .config
sed -i 's/# CONFIG_INIT is not set/CONFIG_INIT=y/' .config
sed -i 's/# CONFIG_TR is not set/CONFIG_TR=y/' .config
sed -i 's/# CONFIG_AWK is not set/CONFIG_AWK=y/' .config
sed -i 's/# CONFIG_STTY is not set/CONFIG_STTY=y/' .config
sed -i 's/CONFIG_TAR=y/# CONFIG_TAR is not set/' .config
sed -i 's/CONFIG_GETOPT=y/# CONFIG_GETOPT is not set/' .config
sed -i 's/CONFIG_PATCH=y/# CONFIG_PATCH is not set/' .config
sed -i 's/CONFIG_FILE=y/# CONFIG_FILE is not set/' .config
sed -i 's/CONFIG_LOGIN=y/# CONFIG_LOGIN is not set/' .config
sed -i 's/CONFIG_SU=y/# CONFIG_SU is not set/' .config
# BLKID IS UTIL-LINUX'S, AND TWO OF THEM IS TWO ANSWERS TO "WHAT IS THIS
# DEVICE". The applet's prober knows ext, vfat, ntfs, btrfs, f2fs, squashfs
# and swap — not `crypto_LUKS` — and it implements no `-U` or `-L` LOOKUP at
# all, only reporting on devices it is handed. $PATH puts /usr/bin ahead of
# /usr/sbin, so leaving this on shadows the real one for every caller: the
# initramfs resolving a root, an ESP or a LUKS container by UUID, `kdos
# persist` finding its store by label, and anybody typing `blkid`.
sed -i 's/CONFIG_BLKID=y/# CONFIG_BLKID is not set/' .config
make PREFIX=$PKG install -j1
