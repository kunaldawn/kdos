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

cd hostapd

# THE ISLAND NETWORK IS COMPLETE WITH THIS. dnsmasq is already ported, so a
# machine with a wireless card can hand out addresses, resolve names, serve the
# corpus over kiwix and PXE-boot another machine off the same stick — with no
# router and no uplink anywhere in it.
#
# It is the same codebase as the ported wpa_supplicant, so the build shape is
# the same: no configure, a .config of feature lines, and CONFIG_LIBNL32 for
# the netlink library this tree carries.
cat > .config <<'CFG'
CONFIG_DRIVER_NL80211=y
CONFIG_LIBNL32=y
CONFIG_IEEE80211AC=y
CONFIG_IEEE80211AX=y
CONFIG_IEEE80211BE=y
CONFIG_ACS=y
CONFIG_WPS=y
CONFIG_RSN_PREAUTH=y
CONFIG_EAP=y
CONFIG_EAP_MD5=y
CONFIG_EAP_TLS=y
CONFIG_EAP_PEAP=y
CONFIG_EAP_MSCHAPV2=y
CONFIG_EAP_GTC=y
CONFIG_EAP_TTLS=y
CONFIG_SAE=y
CONFIG_OWE=y
CONFIG_INTERNAL_LIBTOMMATH=y
CONFIG_TLS=openssl
CONFIG_ELOOP=eloop
CFG

make V=1 BINDIR=/usr/sbin
install -Dm755 hostapd     $PKG/usr/sbin/hostapd
install -Dm755 hostapd_cli $PKG/usr/bin/hostapd_cli
install -Dm644 hostapd.conf $PKG/etc/hostapd/hostapd.conf.example
install -Dm644 hostapd.8     $PKG/usr/share/man/man8/hostapd.8
install -Dm644 hostapd_cli.1 $PKG/usr/share/man/man1/hostapd_cli.1
