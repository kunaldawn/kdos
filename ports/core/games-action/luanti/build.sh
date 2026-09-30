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

# The engine ships no game, and ContentDB, where players fetch one, is on the
# network: Minetest Game is installed beside the engine so a world can be
# started offline. cURL is left out with everything that needs it (ContentDB,
# the public server list, remote media), as is the update check. English only:
# no gettext catalogues, and the game's translation files are dropped. The
# database backends other than SQLite need servers or libraries not ported.
cmake -S . -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_CLIENT=ON \
	-DBUILD_SERVER=OFF \
	-DBUILD_UNITTESTS=OFF \
	-DBUILD_BENCHMARKS=OFF \
	-DBUILD_DOCUMENTATION=OFF \
	-DBUILD_WITH_TRACY=OFF \
	-DRUN_IN_PLACE=OFF \
	-DENABLE_LTO=OFF \
	-DUSE_SDL3=ON \
	-DENABLE_UPDATE_CHECKER=OFF \
	-DENABLE_CURL=OFF \
	-DENABLE_GETTEXT=OFF \
	-DENABLE_SOUND=ON \
	-DENABLE_OPENSSL=ON \
	-DREQUIRE_LUAJIT=ON \
	-DENABLE_POSTGRESQL=OFF \
	-DENABLE_LEVELDB=OFF \
	-DENABLE_REDIS=OFF \
	-DENABLE_SPATIAL=OFF \
	-DENABLE_PROMETHEUS=OFF \
	-DENABLE_CURSES=OFF \
	-DINSTALL_DEVTEST=OFF
cmake --build build
DESTDIR=$PKG cmake --install build

install -d "$PKG/usr/share/luanti/games"
cp -R "$SRC_ROOT/minetest_game-$_mtg" "$PKG/usr/share/luanti/games/minetest_game"
find "$PKG/usr/share/luanti/games/minetest_game" -path '*/locale/*.tr' -delete
