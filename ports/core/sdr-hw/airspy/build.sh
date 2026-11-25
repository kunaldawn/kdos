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

export CFLAGS="$CFLAGS -std=gnu17"
mkdir -p build && cd build
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DINSTALL_UDEV_RULES=OFF
make
make DESTDIR=$PKG install
rm "$PKG/usr/lib/libairspy.a"
install -Dm644 ../libairspy/LICENSE.md "$PKG/usr/share/licenses/$name/libairspy-LICENSE.md"
install -Dm644 ../airspy-tools/LICENSE.md "$PKG/usr/share/licenses/$name/airspy-tools-LICENSE.md"
