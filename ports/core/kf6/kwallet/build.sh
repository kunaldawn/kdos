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

cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D KDE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D BUILD_KSECRETD=OFF \
	-D BUILD_KWALLETD=ON \
	-D BUILD_KWALLET_QUERY=OFF \
	-D CMAKE_DISABLE_FIND_PACKAGE_Gpgmepp=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

install -d "$PKG/etc/xdg"
cat > "$PKG/etc/xdg/kwalletrc" <<'CONF'
[KSecretD]
Enabled=false
CONF
chmod 644 "$PKG/etc/xdg/kwalletrc"
