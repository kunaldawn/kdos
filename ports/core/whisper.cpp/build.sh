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
# ONE PACKAGE, EVERY x86-64. GGML_NATIVE would tune to the build machine and
# SIGILL elsewhere; turned off with nothing in its place, ggml builds only the
# baseline CPU backend, which has no AVX2 or FMA and is several times slower on
# every machine made in the last decade. GGML_CPU_ALL_VARIANTS builds a CPU
# backend per microarchitecture (sse42 through zen4 and sapphirerapids) as
# modules under /usr/lib/ggml, and libggml picks the best one the running CPU
# supports when a model is loaded. It requires GGML_BACKEND_DL, which requires
# BUILD_SHARED_LIBS; GGML_BACKEND_DIR is where libggml looks, and a module
# missing from it is a transcription that fails with no CPU backend.
#
# OpenMP is libgomp from the gcc port and drives the CPU backend's threads.
# BLAS is OpenBLAS, a second backend module the scheduler hands the encoder's
# large matrix products to. Both are probes that disable themselves when the
# library is missing, so each is checked below rather than trusted.
#
# VULKAN IS THE GPU BACKEND, as one more module under /usr/lib/ggml: mesa's
# radv, anv and nvk drivers answer it. The backend lists no device where the
# only Vulkan driver is a CPU one such as lavapipe, so a machine with no GPU
# transcribes on the CPU backends. Its shaders are compiled at build time by
# glslc from the shaderc port, and a missing glslc stops configure. No CUDA,
# HIP or SYCL: none has a runtime here. No OpenCL: ggml's OpenCL backend drives
# only Adreno and Intel GPUs, and Vulkan already reaches Intel through anv.
#
# WHISPER_COMMON_FFMPEG lets whisper-cli read mp3, ogg, opus and the audio
# track of a video directly; without it the input must be 16 kHz WAV.
# WHISPER_CURL is left alone: 1.9.4 declares it and nothing reads it. A model
# is fetched by `kdos speech get`.
#
# The MODELS ARE NOT SHIPPED — they are hundreds of megabytes each and picking
# one is picking a language and an accuracy. kdos-rec searches
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
	-DWHISPER_BUILD_EXAMPLES=ON -DWHISPER_SDL2=ON \
	-DWHISPER_COMMON_FFMPEG=ON \
	-DGGML_NATIVE=OFF -DGGML_CCACHE=OFF \
	-DGGML_BACKEND_DL=ON -DGGML_CPU_ALL_VARIANTS=ON \
	-DGGML_BACKEND_DIR=/usr/lib/ggml \
	-DGGML_OPENMP=ON \
	-DGGML_BLAS=ON -DGGML_BLAS_VENDOR=OpenBLAS \
	-DGGML_VULKAN=ON -DGGML_CUDA=OFF -DGGML_HIP=OFF \
	-DGGML_SYCL=OFF -DGGML_OPENCL=OFF -DGGML_RPC=OFF
grep -q '^GGML_OPENMP_ENABLED:INTERNAL=ON$' CMakeCache.txt ||
	{ echo "whisper.cpp: OpenMP not found at configure" >&2; exit 1; }
make
make DESTDIR=$PKG install
for mod in libggml-blas.so libggml-cpu-x64.so libggml-cpu-haswell.so \
	libggml-vulkan.so; do
	[ -f "$PKG/usr/lib/ggml/$mod" ] ||
		{ echo "whisper.cpp: $mod was not built" >&2; exit 1; }
done
