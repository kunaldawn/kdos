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

mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_wireshark=ON \
	-DBUILD_androiddump=OFF \
	-DBUILD_sshdump=ON \
	-DBUILD_ciscodump=ON \
	-DBUILD_wifidump=ON \
	-DBUILD_sdjournal=OFF \
	-DBUILD_mmdbresolve=ON \
	-DENABLE_PCAP=ON \
	-DENABLE_LUA=ON \
	-DENABLE_KERBEROS=ON \
	-DENABLE_GNUTLS=ON \
	-DENABLE_PKCS11=ON \
	-DENABLE_NGHTTP2=ON \
	-DENABLE_NGHTTP3=ON \
	-DENABLE_BROTLI=ON \
	-DENABLE_ZLIB=ON \
	-DENABLE_ZSTD=ON \
	-DENABLE_LZ4=ON \
	-DENABLE_XXHASH=ON \
	-DENABLE_NETLINK=ON \
	-DENABLE_CAP=ON \
	-DENABLE_SMI=ON \
	-DENABLE_ZLIBNG=OFF \
	-DENABLE_MINIZIP=ON \
	-DENABLE_MINIZIPNG=OFF \
	-DENABLE_SNAPPY=ON \
	-DENABLE_CPUINFO=OFF \
	-DENABLE_ILBC=OFF \
	-DENABLE_SBC=ON \
	-DENABLE_SPANDSP=OFF \
	-DENABLE_BCG729=OFF \
	-DENABLE_AMRNB=OFF \
	-DENABLE_AMRWB=OFF \
	-DENABLE_OPUS=ON \
	-DENABLE_PLUGINS=ON \
	-DENABLE_WERROR=OFF

for have in HAVE_LIBPCAP HAVE_LUA HAVE_KERBEROS HAVE_LIBGNUTLS \
	HAVE_GNUTLS_PKCS11 HAVE_NGHTTP2 HAVE_NGHTTP3 HAVE_BROTLI HAVE_ZLIB \
	HAVE_ZSTD HAVE_LZ4 HAVE_XXHASH HAVE_LIBNL HAVE_LIBCAP HAVE_MINIZIP \
	HAVE_SBC HAVE_OPUS HAVE_LIBSSH HAVE_MAXMINDDB HAVE_LIBSMI HAVE_SNAPPY; do
	grep -q "^#define $have 1" config.h || {
		echo "wireshark: $have not found at configure" >&2
		exit 1
	}
done
grep -q '^Qt6Multimedia_DIR:PATH=/' CMakeCache.txt || {
	echo "wireshark: Qt Multimedia not found at configure" >&2
	exit 1
}
ninja
ninja docs
DESTDIR=$PKG ninja install
rm -f "$PKG/usr/share/doc/wireshark/Stratoshark Release Notes.html"

for page in "$PKG"/usr/share/man/man1/*.1; do
	prog=$(basename "$page" .1)
	if ! find "$PKG" -path "$PKG/usr/share" -prune -o -type f -name "$prog" -print | grep -q .; then
		rm -f "$page" "$PKG/usr/share/doc/wireshark/$prog.html"
	fi
done

cat > "$PKG/usr/share/applications/org.wireshark.Wireshark.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Wireshark
GenericName=Network Analyzer
Comment=Capture network traffic and dissect every protocol in it
Exec=wireshark %f
TryExec=wireshark
Icon=org.wireshark.Wireshark
Terminal=false
StartupWMClass=org.wireshark.Wireshark
MimeType=application/vnd.tcpdump.pcap;application/x-pcapng;application/x-snoop;application/x-iptrace;application/x-lanalyzer;application/x-nettl;application/x-radcom;application/x-etherpeek;application/x-visualnetworks;application/x-netinstobserver;application/x-5view;application/x-tektronix-rf5;application/x-micropross-mplog;application/x-apple-packetlogger;application/x-endace-erf;application/ipfix;application/x-ixia-vwr;
Categories=Qt;Network;Monitor;
Keywords=packet;capture;pcap;sniffer;protocol;tshark;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.wireshark.Wireshark.desktop"
