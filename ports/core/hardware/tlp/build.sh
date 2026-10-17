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

make TLP_WITH_SYSTEMD=0 TLP_WITH_ELOGIND=0 TLP_SBIN=/usr/sbin TLP_BIN=/usr/bin \
     TLP_TLIB=/usr/share/tlp TLP_ULIB=/usr/lib/udev TLP_CONFDIR=/etc/tlp.d \
     TLP_CONFDEF=/usr/share/tlp/defaults.conf

make TLP_WITH_SYSTEMD=0 TLP_WITH_ELOGIND=0 TLP_SBIN=/usr/sbin TLP_BIN=/usr/bin \
     TLP_TLIB=/usr/share/tlp TLP_ULIB=/usr/lib/udev TLP_CONFDIR=/etc/tlp.d \
     TLP_CONFDEF=/usr/share/tlp/defaults.conf TLP_NO_INIT=1 \
     DESTDIR=$PKG install-tlp install-man-tlp install-rdw install-man-rdw
