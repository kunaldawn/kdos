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
	--prefix=/usr --sysconfdir=/etc --libdir=lib --libexecdir=/usr/lib --localstatedir=/var \
	-Dintrospection=false \
	-Ddocs=false \
	-Dman=false \
	-Dvapi=false \
	-Dtests=no \
	-Dsystemd_journal=false \
	-Dsystemdsystemunitdir=no \
	-Dsession_tracking=no \
	-Dsession_tracking_consolekit=false \
	-Dpolkit=true \
	-Dmodem_manager=false \
	-Dofono=false \
	-Dteamdctl=false \
	-Dovs=false \
	-Dppp=false \
	-Dnmcli=true \
	-Dnmtui=true \
	-Dwifi=true \
	-Diptables= \
	-Dip6tables= \
	-Dnft=/usr/sbin/nft \
	-Ddhclient=no \
	-Ddhcpcd= \
	-Ddnsmasq=/usr/sbin/dnsmasq \
	-Diwd=false \
	-Dlibpsl=true \
	-Dnbft=false \
	-Dconfig_dhcp_default=internal \
	-Dconfig_logging_backend_default=syslog \
	-Dcrypto=gnutls \
	-Dqt=false \
	-Dselinux=false \
	-Dlibaudit=no
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
