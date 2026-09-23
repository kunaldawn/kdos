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

# -DBUILD_wireshark=OFF IS WHAT MAKES THIS LEGAL HERE. The GUI is Qt and there
# is no Qt on the host by rule; what is wanted is the DISSECTION ENGINE, which
# is the half nothing else on this machine has — tcpdump prints packets and
# tshark understands about three thousand protocols, which is the difference
# between seeing a TLS handshake and reading why it failed.
#
# ~250 MB INSTALLED, and almost all of it is the dissectors. That is the price
# of the feature and it is stated rather than trimmed: a curated protocol set
# is a capture that decodes until it meets the one protocol you needed.
#
# dumpcap is the only part that touches an interface, and it ships WITHOUT the
# setuid bit — this tree has exactly two setuid binaries and a packet capture
# tool is not becoming the third. Capture is root's, or a deliberate
# `setcap cap_net_raw,cap_net_admin+eip`.
#
# EVERY OPTIONAL LIBRARY IS NAMED, ON OR OFF. Upstream's ws_find_package() is
# a probe that quietly drops a feature whose library is missing, so an ON here
# is a request, not a guarantee; the cmakeconfig.h check after configure is
# what turns a missing library into a failed build. The OFF ones are either
# libraries that are not ports (libsmi, libmaxminddb, libssh, zlib-ng,
# minizip, snappy, cpuinfo, opencore-amr, spandsp, bcg729, ilbc), or RTP
# audio codecs (sbc, opus), which only turn captured RTP into sound for the Qt
# player and sharkd's audio export. sdjournal reads the systemd journal, which
# does not exist here.
mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_wireshark=OFF \
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
	-DENABLE_MINIZIP=OFF \
	-DENABLE_MINIZIPNG=OFF \
	-DENABLE_SNAPPY=OFF \
	-DENABLE_CPUINFO=OFF \
	-DENABLE_ILBC=OFF \
	-DENABLE_SBC=OFF \
	-DENABLE_SPANDSP=OFF \
	-DENABLE_BCG729=OFF \
	-DENABLE_AMRNB=OFF \
	-DENABLE_AMRWB=OFF \
	-DENABLE_OPUS=OFF \
	-DENABLE_PLUGINS=ON \
	-DENABLE_WERROR=OFF

# The Lua probe searches /usr/include before any versioned directory, so it
# takes the lua port's 5.5 header and then liblua.so, never lua54's.
for have in HAVE_LIBPCAP HAVE_LUA HAVE_KERBEROS HAVE_LIBGNUTLS \
	HAVE_GNUTLS_PKCS11 HAVE_NGHTTP2 HAVE_NGHTTP3 HAVE_BROTLI HAVE_ZLIB \
	HAVE_ZSTD HAVE_LZ4 HAVE_XXHASH HAVE_LIBNL HAVE_LIBCAP; do
	grep -q "^#define $have 1" cmakeconfig.h || {
		echo "wireshark: $have not found at configure" >&2
		exit 1
	}
done
ninja
DESTDIR=$PKG ninja install

# THE MANUAL PAGES ARE THE ONLY DOCUMENTATION KEPT, and there is no switch that
# builds only them. With asciidoctor present the default target renders every
# page twice, as roff and as HTML, plus both sets of release notes, and the
# install puts the roff under /usr/share/man and the HTML under
# /usr/share/doc/wireshark — where nothing else this package installs is HTML.
# The user and developer guides are not in the default install.
rm -f "$PKG"/usr/share/doc/wireshark/*.html

# The page list is upstream's whole family, not what was built: wireshark,
# stratoshark and the extcap tools switched off above each get a page. A page
# stays when a program of its name is in the package — in /usr/bin, or an
# extcap under the libexec directory — and section 4 describes formats, not
# programs.
for page in "$PKG"/usr/share/man/man1/*.1; do
	prog=$(basename "$page" .1)
	if ! find "$PKG" -path "$PKG/usr/share" -prune -o -type f -name "$prog" -print | grep -q .; then
		rm -f "$page"
	fi
done
