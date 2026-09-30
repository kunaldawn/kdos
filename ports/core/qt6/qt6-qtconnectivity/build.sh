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

# The install paths come from qt6-qtbase's installed build internals.
# QtBluetooth talks to bluetoothd over D-Bus and builds against bluez's
# headers; QtNfc reads readers through pcscd. Each feature is forced on, so a
# missing library stops the configure instead of leaving an empty module.
mkdir -p build && cd build
cmake .. -G Ninja -Wno-dev \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DFEATURE_bluez=ON \
	-DFEATURE_bluez_le=ON \
	-DFEATURE_pcsclite=ON \
	-DQT_BUILD_EXAMPLES=OFF \
	-DQT_BUILD_TESTS=OFF
ninja
DESTDIR=$PKG ninja install
