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
# build machine that falls back to a guess: resolvconf is openresolv, which
# rc-manager=auto then picks as the one writer of /etc/resolv.conf that wg-quick
# and dhcpcd share; netconfig is SUSE's and off; dhcpcd is an optional client
# beside the internal one; iptables is not a port and nft is the firewall, so
# its path is only a name.
#
# WWAN drives ModemManager, which 42_modemmanager starts ahead of this daemon,
# and takes a carrier's APN from mobile-broadband-provider-info. The Bluetooth
# device plugin (PAN, and DUN through bluez5_dun) is compiled only inside the
# WWAN switch, so turning WWAN off also takes phone tethering away. pppd carries
# PPPoE, DUN and the AT modems that have no raw-IP bearer; its plugin directory
# comes from ppp's pppd.pc.
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

# DNS THROUGH A LOCAL dnsmasq, which NetworkManager spawns on 127.0.0.1 and ::1
# and feeds over D-Bus: a cache, and split DNS, so a VPN's servers answer for
# the VPN's domains instead of replacing every server the machine has. It is
# written under /usr/lib, which NetworkManager reads before
# /etc/NetworkManager, so a file of the same name in /etc/NetworkManager/conf.d
# overrides it and removing this package takes the setting with it.
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
