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
make TLP_WITH_SYSTEMD=0 TLP_WITH_ELOGIND=0 TLP_SBIN=/usr/sbin TLP_BIN=/usr/bin \
     TLP_TLIB=/usr/share/tlp TLP_ULIB=/usr/lib/udev TLP_CONFDIR=/etc/tlp.d \
     TLP_CONFDEF=/usr/share/tlp/defaults.conf TLP_CONF=/etc/tlp.conf

make TLP_WITH_SYSTEMD=0 TLP_WITH_ELOGIND=0 TLP_SBIN=/usr/sbin TLP_BIN=/usr/bin \
     TLP_TLIB=/usr/share/tlp TLP_ULIB=/usr/lib/udev TLP_CONFDIR=/etc/tlp.d \
     TLP_CONFDEF=/usr/share/tlp/defaults.conf TLP_CONF=/etc/tlp.conf \
     DESTDIR=$PKG install-tlp
