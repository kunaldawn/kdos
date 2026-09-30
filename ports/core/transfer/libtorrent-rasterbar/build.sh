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

# -Dwebtorrent=OFF: WebTorrent pulls in a bundled libdatachannel, libjuice and
# usrsctp built statically into the library, for WebRTC peers no LAN swarm
# has. -Dgnutls=OFF keeps OpenSSL, the backend DHT needs: the GnuTLS build
# compiles DHT out.
#
# -Dpython-bindings=ON is Deluge's `import libtorrent`. It needs boost built
# with its Python component (Boost::python3NN); a boost without it fails
# here at configure, naming boost_python.
cmake -B build -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_CXX_STANDARD=17 \
	-DBUILD_SHARED_LIBS=ON \
	-Dwebtorrent=OFF \
	-Dgnutls=OFF \
	-Dencryption=ON \
	-Ddht=ON \
	-Dpython-bindings=ON \
	-Dpython-egg-info=ON \
	-Dpython-install-system-dir=ON \
	-Dbuild_tests=OFF \
	-Dbuild_examples=OFF \
	-Dbuild_tools=OFF
cmake --build build
DESTDIR=$PKG cmake --install build
