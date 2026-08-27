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
mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_wireshark=OFF \
	-DBUILD_qtshark=OFF \
	-DBUILD_logray=OFF \
	-DBUILD_androiddump=OFF \
	-DBUILD_sshdump=OFF \
	-DBUILD_ciscodump=OFF \
	-DBUILD_wifidump=OFF \
	-DENABLE_LUA=OFF \
	-DENABLE_SMI=OFF \
	-DENABLE_KERBEROS=OFF \
	-DENABLE_NGHTTP2=OFF \
	-DENABLE_NGHTTP3=OFF \
	-DENABLE_BROTLI=OFF \
	-DENABLE_ILBC=OFF \
	-DENABLE_SBC=OFF \
	-DENABLE_SPANDSP=OFF \
	-DENABLE_BCG729=OFF \
	-DENABLE_AMRNB=OFF \
	-DENABLE_OPUS=OFF \
	-DENABLE_PLUGINS=ON \
	-DENABLE_WERROR=OFF
ninja
DESTDIR=$PKG ninja install
