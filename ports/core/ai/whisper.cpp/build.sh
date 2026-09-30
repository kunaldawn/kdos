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

# WHISPER_SDL2 IS WHAT MAKES TRANSCRIPTION LIVE. `whisper-cli` reads a closed
# file; the streaming one opens a microphone through SDL's audio device and
# transcribes a rolling window, and upstream builds it only behind this flag.
# `SDL_Init(SDL_INIT_AUDIO)` opens no window, so it runs in a terminal on the
# console with no compositor above it.
#
# AND THE FLAG BUILDS FOUR PROGRAMS BESIDE THE ONE IT IS HERE FOR, one of
# which cannot be built at all: talk-llama fetches llama.cpp from github AT
# CONFIGURE TIME, and this distribution builds with no network by rule, so an
# unpatched WHISPER_SDL2 ends the whole configure with `Could not resolve
# host`. There is no flag — see the patch, which is also what stops the other
# three from being compiled and then deleted.
patch -p1 -i "$PORT_SRC/sdl2-stream-only.patch"

mkdir -p build && cd build
# WHISPER_USE_SYSTEM_GGML links the libggml port, so whisper.cpp and llama.cpp
# load one set of CPU, OpenBLAS and Vulkan backend modules from /usr/lib/ggml,
# and the copy of ggml inside this tarball is not built; a second copy would
# install the same libraries and modules over the port's. How the modules are
# chosen for the running CPU and GPU is in the libggml recipe.
#
# WHISPER_COMMON_FFMPEG lets whisper-cli read mp3, ogg, opus and the audio
# track of a video directly; without it the input must be 16 kHz WAV.
# WHISPER_CURL is left alone: 1.9.4 declares it and nothing reads it.
#
# base.en ships in the whisper-model-base-en port at
# /usr/share/whisper.cpp/models, where kdos-rec looks last; every other model
# comes from `kdos speech get`.
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
