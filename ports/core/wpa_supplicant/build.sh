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

cd wpa_supplicant
cat > .config <<'EOF'
CONFIG_DRIVER_NL80211=y
CONFIG_DRIVER_WIRED=y
CONFIG_DRIVER_MACSEC_LINUX=y
CONFIG_MACSEC=y
CONFIG_LIBNL32=y
CONFIG_BACKEND=file
CONFIG_CTRL_IFACE=y
CONFIG_CTRL_IFACE_DBUS_NEW=y
CONFIG_CTRL_IFACE_DBUS_INTRO=y
CONFIG_DEBUG_FILE=y
CONFIG_DEBUG_SYSLOG=y
CONFIG_IEEE80211R=y
CONFIG_IEEE8021X_EAPOL=y
CONFIG_EAP_MD5=y
CONFIG_EAP_MSCHAPV2=y
CONFIG_EAP_TLS=y
CONFIG_EAP_PEAP=y
CONFIG_EAP_TTLS=y
CONFIG_EAP_GTC=y
CONFIG_EAP_OTP=y
CONFIG_EAP_LEAP=y
CONFIG_EAP_SIM=y
CONFIG_EAP_PSK=y
CONFIG_EAP_AKA=y
CONFIG_EAP_FAST=y
CONFIG_EAP_PWD=y
CONFIG_PKCS12=y
CONFIG_SMARTCARD=y
CONFIG_AP=y
CONFIG_P2P=y
CONFIG_HS20=y
CONFIG_INTERWORKING=y
CONFIG_TLS=openssl
CONFIG_BGSCAN_SIMPLE=y
CONFIG_BGSCAN_LEARN=y
CONFIG_SAE=y
CONFIG_OWE=y
CONFIG_SUITEB192=y
CONFIG_FILS=y
CONFIG_OCV=y
CONFIG_DPP=y
CONFIG_WEP=y
CONFIG_DPP2=y
CONFIG_WPS=y
CONFIG_IEEE80211AC=y
CONFIG_IEEE80211AX=y
CONFIG_IEEE80211BE=y
CONFIG_TDLS=y
CONFIG_IBSS_RSN=y
CONFIG_SAE_PK=y
CONFIG_WNM=y
CONFIG_MBO=y
CONFIG_PASN=y
CONFIG_MESH=y
CONFIG_EAP_TEAP=y
CONFIG_EAP_IKEV2=y
CONFIG_EAP_SAKE=y
CONFIG_EAP_GPSK=y
CONFIG_EAP_GPSK_SHA256=y
CONFIG_EAP_PAX=y
CONFIG_EAP_EKE=y
CONFIG_EAP_AKA_PRIME=y
CONFIG_EAP_TNC=y
CONFIG_PCSC=y
CONFIG_WPA_CLI_EDIT=y
CONFIG_ELOOP_EPOLL=y
EOF

make BINDIR=/usr/sbin LIBDIR=/usr/lib
install -Dm755 wpa_supplicant "$PKG/usr/sbin/wpa_supplicant"
install -Dm755 wpa_cli       "$PKG/usr/sbin/wpa_cli"
install -Dm755 wpa_passphrase "$PKG/usr/sbin/wpa_passphrase"
install -Dm644 dbus/fi.w1.wpa_supplicant1.service \
	"$PKG/usr/share/dbus-1/system-services/fi.w1.wpa_supplicant1.service"
install -Dm644 dbus/dbus-wpa_supplicant.conf \
	"$PKG/etc/dbus-1/system.d/wpa_supplicant.conf"
install -Dm644 doc/docbook/wpa_supplicant.8 doc/docbook/wpa_cli.8 \
	doc/docbook/wpa_passphrase.8 doc/docbook/wpa_background.8 \
	-t "$PKG/usr/share/man/man8"
install -Dm644 doc/docbook/wpa_supplicant.conf.5 -t "$PKG/usr/share/man/man5"
