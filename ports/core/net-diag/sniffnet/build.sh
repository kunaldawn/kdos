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

# The startup check asks GitHub for the latest release, six times over three
# minutes when there is no network; the patch makes it report nothing.
patch -p1 -i "$PORT_SRC/sniffnet-no-update-check.patch"

# Capturing needs cap_net_raw and cap_net_admin, and the binary ships with
# neither, as dumpcap does: capture is root's, or a deliberate
# `setcap cap_net_raw,cap_net_admin+eip /usr/bin/sniffnet`. Reading a saved
# pcap file needs nothing.
# The window is winit's, which dlopens the Wayland, xkbcommon and Vulkan or
# EGL libraries at run time; libpcap and ALSA (notification sounds) are
# linked. reqwest's TLS is aws-lc, compiled from the vendored aws-lc-sys,
# whose fallback builder is cmake.
tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz
export CARGO_HOME="$SRC_ROOT/.cargo"
export RUSTFLAGS="-C target-feature=-crt-static"
export LIBCLANG_PATH=/usr/lib
export CARGO_NET_OFFLINE=true
cargo build --release --frozen --offline
install -Dm755 target/release/sniffnet "$PKG/usr/bin/sniffnet"

for s in 16 22 24 32 48 64 72 96 128 192 256 512; do
	install -Dm644 resources/packaging/linux/graphics/sniffnet_${s}x${s}.png \
		"$PKG/usr/share/icons/hicolor/${s}x${s}/apps/sniffnet.png"
done

# UPSTREAM'S ENTRY IS REPLACED: it names the binary by path. The app_id is
# sniffnet, set in main.rs.
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/sniffnet.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Sniffnet
GenericName=Network Monitor
Comment=Watch network traffic by host, service and program
TryExec=sniffnet
Exec=sniffnet
Icon=sniffnet
Terminal=false
StartupWMClass=sniffnet
Categories=Network;Monitor;
Keywords=traffic;network;packet;capture;pcap;monitor;sniffer;
DESKTOP
chmod 644 "$PKG/usr/share/applications/sniffnet.desktop"
install -Dm644 LICENSE-MIT "$PKG/usr/share/licenses/sniffnet/LICENSE-MIT"
install -Dm644 LICENSE-APACHE "$PKG/usr/share/licenses/sniffnet/LICENSE-APACHE"
