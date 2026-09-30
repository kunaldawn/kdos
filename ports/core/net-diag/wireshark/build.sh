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

# THE WINDOW AND THE COMMAND LINE ARE ONE PACKAGE. -DBUILD_wireshark=ON is the
# Qt 6 GUI; tshark, dumpcap and the file tools come with it. Both sit on the
# DISSECTION ENGINE, which is the half nothing else on this machine has —
# tcpdump prints packets and tshark understands about three thousand
# protocols, which is the difference between seeing a TLS handshake and
# reading why it failed. The Qt find_package is REQUIRED, so a missing Qt
# stops configure; Multimedia is optional to it and is checked below, because
# without it the RTP player has no sound.
#
# THE DISSECTORS ARE ABOUT 250 MB INSTALLED, most of the package. That is the
# price of the feature and it is stated rather than trimmed: a curated
# protocol set is a capture that decodes until it meets the one protocol you
# needed.
#
# dumpcap is the only part that touches an interface, and it ships WITHOUT the
# setuid bit — this tree has exactly two setuid binaries and a packet capture
# tool is not becoming the third. Capture is root's, or a deliberate
# `setcap cap_net_raw,cap_net_admin+eip`. Until then the window started from
# the menu lists no interface to capture on and opens capture files only.
#
# EVERY OPTIONAL LIBRARY IS NAMED, ON OR OFF. Upstream's ws_find_package() is
# a probe that quietly drops a feature whose library is missing, so an ON here
# is a request, not a guarantee; the config.h check after configure is
# what turns a missing library into a failed build. sbc and opus decode
# captured RTP into sound for the GUI's RTP player; minizip is the GUI's
# profile import and export, through zlib's minizip rather than minizip-ng.
# The OFF ones are libraries that are not ports (libsmi, libmaxminddb,
# snappy, cpuinfo, opencore-amr, spandsp, bcg729, ilbc), and zlib-ng, which
# would stand in for the zlib already linked. libssh is probed only for the
# sshdump, ciscodump and wifidump remote-capture extcaps, which are not built.
# sdjournal reads the systemd journal, which does not exist here.
mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_wireshark=ON \
	-DBUILD_androiddump=OFF \
	-DBUILD_sshdump=OFF \
	-DBUILD_ciscodump=OFF \
	-DBUILD_wifidump=OFF \
	-DBUILD_sdjournal=OFF \
	-DBUILD_mmdbresolve=OFF \
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
	-DENABLE_SMI=OFF \
	-DENABLE_ZLIBNG=OFF \
	-DENABLE_MINIZIP=ON \
	-DENABLE_MINIZIPNG=OFF \
	-DENABLE_SNAPPY=OFF \
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

# The Lua probe searches /usr/include before any versioned directory, so it
# takes the lua port's 5.5 header and then liblua.so, never lua54's.
for have in HAVE_LIBPCAP HAVE_LUA HAVE_KERBEROS HAVE_LIBGNUTLS \
	HAVE_GNUTLS_PKCS11 HAVE_NGHTTP2 HAVE_NGHTTP3 HAVE_BROTLI HAVE_ZLIB \
	HAVE_ZSTD HAVE_LZ4 HAVE_XXHASH HAVE_LIBNL HAVE_LIBCAP HAVE_MINIZIP \
	HAVE_SBC HAVE_OPUS; do
	grep -q "^#define $have 1" config.h || {
		echo "wireshark: $have not found at configure" >&2
		exit 1
	}
done
grep -q '^Qt6Multimedia_DIR:PATH=/' CMakeCache.txt || {
	echo "wireshark: Qt Multimedia not found at configure" >&2
	exit 1
}
# THE MANUAL PAGES SHIP TWICE, as roff under /usr/share/man and as HTML under
# /usr/share/doc/wireshark, because the GUI's Help menu opens the HTML ones.
# With asciidoctor found, the install rule lists every page in both forms plus
# both sets of release notes, and `docs` is the target that renders all of
# them; `ninja install` stops on the first one missing. The user and
# developer guides are not in the install.
ninja
ninja docs
DESTDIR=$PKG ninja install
rm -f "$PKG/usr/share/doc/wireshark/Stratoshark Release Notes.html"

# The page list is upstream's whole family, not what was built: stratoshark
# and the extcap tools switched off above each get a page in both forms. A
# page stays when a program of its name is in the package — in /usr/bin, or an
# extcap under the libexec directory — and section 4 describes formats, not
# programs.
for page in "$PKG"/usr/share/man/man1/*.1; do
	prog=$(basename "$page" .1)
	if ! find "$PKG" -path "$PKG/usr/share" -prune -o -type f -name "$prog" -print | grep -q .; then
		rm -f "$page" "$PKG/usr/share/doc/wireshark/$prog.html"
	fi
done

# Upstream's entry has no StartupWMClass. The GUI sets its desktop file name,
# which Qt makes the Wayland app_id, and the entry repeats it. The icons are
# upstream's hicolor PNGs; the capture MIME types are defined by the XML the
# install puts under /usr/share/mime/packages, and nothing else claims them.
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
