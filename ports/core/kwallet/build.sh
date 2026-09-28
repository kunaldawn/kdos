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

# KF_SKIP_PO_PROCESSING leaves every translation out: bundled data is English
# only, and without it each language's catalogue is compiled and installed.
# kwalletd6 is the only daemon built: it answers org.kde.kwalletd6, the one
# name the client library calls, and keeps every wallet in whatever serves
# org.freedesktop.secrets. ksecretd, KDE's own store, is off; so is
# kwallet-query. Gpgmepp is disabled because GPG wallets belong to ksecretd's
# file backend, which nothing here reaches.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D BUILD_QCH=OFF \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D BUILD_KSECRETD=OFF \
	-D BUILD_KWALLETD=ON \
	-D BUILD_KWALLET_QUERY=OFF \
	-D CMAKE_DISABLE_FIND_PACKAGE_Gpgmepp=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

# kwalletd6 sends to org.kde.secretservicecompat, ksecretd's name, unless
# [KSecretD] Enabled is false; with ksecretd not built that name has no owner
# and every wallet fails to open. The system default turns it off, so the
# wallet lands in the Secret Service provider; a user's own kwalletrc still
# overrides it.
install -d "$PKG/etc/xdg"
cat > "$PKG/etc/xdg/kwalletrc" <<'CONF'
[KSecretD]
Enabled=false
CONF
chmod 644 "$PKG/etc/xdg/kwalletrc"
