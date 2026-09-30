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

# Pure shell over /sys — nothing is compiled, so there is no configure and no
# musl surface at all. TLP_WITH_SYSTEMD=0 is what keeps it from installing
# units and a systemd-sleep hook.
#
# tlp-rdw is the radio device wizard: a NetworkManager dispatcher script and a
# udev rule that switch Wi-Fi and WWAN when a LAN cable or a dock comes and
# goes, through nmcli. It does nothing until DEVICES_TO_*_ON_* is set in
# /etc/tlp.d. tlp-pd, the power-profiles D-Bus daemon, is not installed: it is
# Python over PyGObject, which is not a port.
make TLP_WITH_SYSTEMD=0 TLP_WITH_ELOGIND=0 TLP_SBIN=/usr/sbin TLP_BIN=/usr/bin \
     TLP_TLIB=/usr/share/tlp TLP_ULIB=/usr/lib/udev TLP_CONFDIR=/etc/tlp.d \
     TLP_CONFDEF=/usr/share/tlp/defaults.conf

make TLP_WITH_SYSTEMD=0 TLP_WITH_ELOGIND=0 TLP_SBIN=/usr/sbin TLP_BIN=/usr/bin \
     TLP_TLIB=/usr/share/tlp TLP_ULIB=/usr/lib/udev TLP_CONFDIR=/etc/tlp.d \
     TLP_CONFDEF=/usr/share/tlp/defaults.conf TLP_NO_INIT=1 \
     DESTDIR=$PKG install-tlp install-man-tlp install-rdw install-man-rdw
