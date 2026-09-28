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

# offline-default starts the game with internet access refused rather than
# asking on first start: news, add-ons and online play have nothing to reach
# offline, and a player can allow them under Options, User Interface.
patch -p1 -i "$PORT_SRC/offline-default.patch"

# The karts, tracks, music, sounds, models, textures and library are the
# stk-assets port, built from this same tarball. The in-game video recorder
# needs libopenglrecorder and the Wiimote driver needs BlueZ, and neither is
# here. ENet stays bundled because STK's IPv6 support lives in its own copy;
# mcpp, squish, angelscript and SheenBidi have no port and stay bundled.
cmake -S . -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DSERVER_ONLY=OFF \
	-DCHECK_ASSETS=OFF \
	-DBUILD_RECORDER=OFF \
	-DUSE_WIIUSE=OFF \
	-DUSE_SYSTEM_WIIUSE=OFF \
	-DUSE_IPV6=ON \
	-DUSE_SQLITE3=ON \
	-DUSE_CRYPTO_OPENSSL=ON \
	-DUSE_MOJOAL=OFF \
	-DUSE_DNS_C=OFF \
	-DUSE_GLES2=OFF \
	-DNO_SHADERC=OFF \
	-DUSE_LIBBFD=OFF
cmake --build build
DESTDIR=$PKG cmake --install build

# English only, and the assets belong to stk-assets.
find "$PKG/usr/share/supertuxkart/data/po" -name '*.po' -delete
for d in karts library models music sfx textures tracks; do
	rm -rf "$PKG/usr/share/supertuxkart/data/$d"
done
