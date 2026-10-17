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

patch -p1 -i "$PORT_SRC/offline-default.patch"

cmake -S . -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DSERVER_ONLY=OFF \
	-DCHECK_ASSETS=OFF \
	-DBUILD_RECORDER=OFF \
	-DUSE_WIIUSE=ON \
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
rm "$PKG/usr/lib/libwiiuse.a" "$PKG/usr/include/wiiuse.h" \
	"$PKG/usr/lib/libangelscript.a" "$PKG/usr/include/angelscript.h"
rm -r "$PKG/usr/lib/cmake/Angelscript"
rmdir "$PKG/usr/include" "$PKG/usr/lib/cmake" "$PKG/usr/lib"

find "$PKG/usr/share/supertuxkart/data/po" -name '*.po' -delete
for d in karts library models music sfx textures tracks; do
	rm -rf "$PKG/usr/share/supertuxkart/data/$d"
done
