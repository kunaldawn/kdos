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

patch -p1 -i "$PORT_SRC/sdl2-stream-only.patch"

mkdir -p build && cd build
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON -DWHISPER_BUILD_TESTS=OFF \
	-DWHISPER_BUILD_EXAMPLES=ON -DWHISPER_SDL2=ON \
	-DWHISPER_COMMON_FFMPEG=ON \
	-DWHISPER_USE_SYSTEM_GGML=ON
make
make DESTDIR=$PKG install
[ ! -e "$PKG/usr/lib/libggml.so" ] && [ ! -d "$PKG/usr/lib/ggml" ] ||
	{ echo "whisper.cpp: installed its own ggml over the libggml port" >&2; exit 1; }
