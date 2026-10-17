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

meson setup build --buildtype=release \
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
	-Dmodem_manager=true \
	-Dofono=false \
	-Dbluez5_dun=true \
	-Dteamdctl=false \
	-Dovs=false \
	-Dclat=true \
	-Dbpf-compiler=clang \
	-Dppp=true \
	-Dpppd=/usr/sbin/pppd \
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
	-Dresolvconf=/usr/sbin/resolvconf \
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

install -d "$PKG/usr/lib/NetworkManager/conf.d"
cat > "$PKG/usr/lib/NetworkManager/conf.d/10-kdos-dns.conf" <<'EOF'
[main]
dns=dnsmasq
EOF
chmod 644 "$PKG/usr/lib/NetworkManager/conf.d/10-kdos-dns.conf"

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
