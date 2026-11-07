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

make TLP_WITH_SYSTEMD=0 TLP_WITH_ELOGIND=1 TLP_SBIN=/usr/sbin TLP_BIN=/usr/bin \
     TLP_TLIB=/usr/share/tlp TLP_ULIB=/usr/lib/udev TLP_CONFDIR=/etc/tlp.d \
     TLP_CONFDEF=/usr/share/tlp/defaults.conf TLP_ELOD=/usr/lib/elogind/system-sleep

make TLP_WITH_SYSTEMD=0 TLP_WITH_ELOGIND=1 TLP_SBIN=/usr/sbin TLP_BIN=/usr/bin \
     TLP_TLIB=/usr/share/tlp TLP_ULIB=/usr/lib/udev TLP_CONFDIR=/etc/tlp.d \
     TLP_CONFDEF=/usr/share/tlp/defaults.conf TLP_ELOD=/usr/lib/elogind/system-sleep \
     DESTDIR=$PKG install-pd install-man-pd

rm -rf "$PKG/usr/lib/systemd" "$PKG/usr/share/dbus-1/system-services"
rm -f "$PKG/usr/share/man/man8/tlp-pd.service.8"
rmdir "$PKG/usr/lib"
