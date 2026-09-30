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

# The libretro core alone, from the mgba tarball: no library, no front end
# and no external dependency, so the core links nothing RetroArch does not
# already have. The install rules for headers and licences are unconditional
# and would collide with the mgba package, so only the core is copied.
cmake -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_LIBRETRO=ON \
	-DSKIP_LIBRARY=ON \
	-DBUILD_SHARED=OFF \
	-DBUILD_STATIC=OFF \
	-DBUILD_SDL=OFF \
	-DBUILD_QT=OFF \
	-DBUILD_TEST=OFF \
	-DBUILD_SUITE=OFF \
	-DDISABLE_DEPS=ON \
	-DUSE_EPOXY=OFF \
	-DUSE_FFMPEG=OFF \
	-DUSE_LUA=OFF \
	-DENABLE_SCRIPTING=OFF \
	-DUSE_DISCORD_RPC=OFF \
	-DUSE_LIBZIP=OFF \
	-DUSE_MINIZIP=OFF \
	-DUSE_ELF=OFF \
	-DUSE_EDITLINE=OFF
ninja -C build mgba_libretro
install -Dm755 build/mgba_libretro.so "$PKG/usr/lib/libretro/mgba_libretro.so"
