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

# AND THE FLAG BUILDS FOUR PROGRAMS BESIDE THE ONE IT IS HERE FOR, one of
# which cannot be built at all: talk-llama fetches llama.cpp from github AT
# CONFIGURE TIME, and this distribution builds with no network by rule, so an
# unpatched WHISPER_SDL2 ends the whole configure with `Could not resolve
# host`. There is no flag — see the patch, which is also what stops the other
# three from being compiled and then deleted.
patch -p1 -i "$PORT_SRC/sdl2-stream-only.patch"

mkdir -p build && cd build
# No CUDA, no BLAS, no Vulkan: the CPU backend is what a machine with no
# accelerator has, and each of the others is a dependency this host lacks.
# The MODELS ARE NOT SHIPPED — they are hundreds of megabytes each and picking
# one is picking a language and an accuracy; `models/download-ggml-model.sh`
# is the fetch and it needs a network. kdos-rec searches
# /usr/share/whisper.cpp/models, which is where an install here would land, so
# a later change that packages one has a single answer rather than two.
#
# WHISPER_SDL2 IS WHAT MAKES TRANSCRIPTION LIVE. `whisper-cli` reads a closed
# file; the streaming one opens a microphone through SDL's audio device and
# transcribes a rolling window, and upstream builds it only behind this flag.
# `SDL_Init(SDL_INIT_AUDIO)` opens no window, so it runs in a terminal on the
# console with no compositor above it.
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON -DWHISPER_BUILD_TESTS=OFF \
	-DWHISPER_BUILD_EXAMPLES=ON -DWHISPER_SDL2=ON -DGGML_NATIVE=OFF
make
make DESTDIR=$PKG install

