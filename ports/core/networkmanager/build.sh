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

# Every tool path is named, because an empty one is a find_program() on the
# build machine that falls back to a guess: resolvconf and netconfig are not
# ports, so both are off; dhcpcd is an optional client beside the internal one;
# iptables is not a port and nft is the firewall, so its path is only a name.
#
# WWAN stays off: modem_manager takes a carrier's APN from the
# mobile-broadband-provider-info database, and that is not a port; the
# Bluetooth plugin, DUN included, is built only alongside WWAN. PPP needs pppd, which is not a port.
# CLAT (464XLAT on IPv6-only networks) compiles a BPF program with clang and
# generates its skeleton with bpftool.
#
# suspend_resume is spelled out because 'auto' would switch to elogind the day
# a libelogind appears; consolekit is the backend that listens for nothing here.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib --libexecdir=/usr/lib --localstatedir=/var \
	-Dintrospection=false \
	-Ddocs=false \
	-Dman=true \
	-Dvapi=false \
	-Dtests=no \
	-Dsystemd_journal=false \
	-Dsystemdsystemunitdir=no \
	-Dsystemdsystemgeneratordir=no \
	-Dsession_tracking=no \
	-Dsession_tracking_consolekit=false \
	-Dpolkit=true \
	-Dpolkit_agent_helper_1=/usr/lib/polkit-1/polkit-agent-helper-1 \
	-Dsuspend_resume=consolekit \
	-Dmodem_manager=false \
	-Dofono=false \
	-Dbluez5_dun=false \
	-Dteamdctl=false \
	-Dovs=false \
	-Dclat=true \
	-Dbpf-compiler=clang \
	-Dppp=false \
	-Dconcheck=true \
	-Dnm_cloud_setup=false \
	-Dfirewalld_zone=false \
	-Difupdown=false \
	-Dnmcli=true \
	-Dnmtui=true \
	-Dreadline=libreadline \
	-Dwifi=true \
	-Diptables=/usr/sbin/iptables \
	-Dip6tables=/usr/sbin/ip6tables \
	-Dnft=/usr/sbin/nft \
	-Ddhcpcd=/usr/sbin/dhcpcd \
	-Ddnsmasq=/usr/sbin/dnsmasq \
	-Dmodprobe=/usr/sbin/modprobe \
	-Dresolvconf=no \
	-Dnetconfig=no \
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

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/nmtui.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Network
GenericName=Network Settings
Comment=Wi-Fi, Ethernet and VPN connections
Exec=nmtui
Icon=gtk-network
Terminal=true
Categories=Settings;Network;
Keywords=network;wifi;wireless;ethernet;vpn;nmtui;
EOF
chmod 644 "$PKG/usr/share/applications/nmtui.desktop"
