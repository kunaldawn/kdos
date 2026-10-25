## About

FreeDV is digital voice for HF amateur radio over a sound card: it encodes speech into the RADE,
700D, 700E and 1600 modes, keys the transmitter through [hamlib](#port-hamlib), and decodes the
received signal back to speech. The GUI is built on [wxwidgets](#port-wxwidgets) with GTK 3. Its
modem and codec code is freedv-backend, which in turn builds RADE (the machine-learning voice
mode), its own Opus with the FARGAN vocoder, and RNNoise for noise suppression.

## Flags

- `-DFETCHCONTENT_FULLY_DISCONNECTED=ON` — Never let CMake's FetchContent go to the network.
  `-DFETCHCONTENT_SOURCE_DIR_FREEDV_BACKEND` points freedv-backend, which the configure always
  pulls in through FetchContent, at its unpacked later source. The other FetchContent
  dependencies (PortAudio, wxWidgets, libsamplerate, LibreSSL) are fallbacks, which the switches
  below and the libraries in `depends` keep from being declared at all.
- `-DRADE_C_URL="$_rade_src"` — The variables `backend-local-sources.patch` adds: the RADE, Opus
  and RNNoise ExternalProjects build from the unpacked later sources named here (`-DOPUS_URL`,
  `-DRNNOISE_URL`) instead of cloning a branch head or downloading an archive.
- `-DUSE_NATIVE_AUDIO=TRUE` — Use FreeDV's own PulseAudio engine instead of PortAudio; PipeWire
  answers it through its PulseAudio server (see [Audio](../../../../docs/kdos/03-architecture/session.md#audio)).
  It is upstream's default.
- `-DUSE_STATIC_DEPS=FALSE` — Link the system libraries instead of downloading and building
  static ones. With `-DUSE_STATIC_LIBRESSL=FALSE` and `-DDISABLE_TLS_SUPPORT=FALSE`, the TLS
  connection to FreeDV Reporter uses the [openssl](#port-openssl) port instead of a static
  LibreSSL, and with [libsamplerate](#port-libsamplerate) and
  [libebur128](#port-libebur128) in `depends` the backend builds none of its static fallbacks.
- `-DBOOTSTRAP_WXWIDGETS=FALSE` — Use the [wxwidgets](#port-wxwidgets) port instead of
  downloading and building a static wxWidgets.
- `-DUNITTEST=OFF` — No unit test programs; `-DBUILD_BACKEND_UNITTESTS=OFF` does the same for
  the backend.
- `X86_AVX2_CFLAGS="-mavx -mfma -mavx2 -Wno-error=incompatible-pointer-types"` — Opus's configure
  compiles its AVX2 sources with the flags in this variable. The value is Opus's default with
  `-Wno-error=incompatible-pointer-types` added, which turns that diagnostic, an error by default
  in GCC, into a warning in those files.
- `LDFLAGS="$LDFLAGS -Wl,--strip-debug"` — Have the linker drop debug sections from everything
  it links.

## Patches

- `backend-local-sources.patch` — In freedv-backend, turn the RADE and RNNoise ExternalProjects from `GIT_REPOSITORY … GIT_TAG main` into `URL ${RADE_C_URL}` and `URL ${RNNOISE_URL}`, make RADE's Opus location the variable `OPUS_URL`, each keeping upstream's address as the default when it is unset, and configure RNNoise with `autoreconf -isf` instead of `autogen.sh`, which downloads its model weights.
- `backend-musl.patch` — In freedv-backend, wait on semaphores with `sem_timedwait` against `CLOCK_REALTIME` instead of glibc's `sem_clockwait` on `CLOCK_MONOTONIC`; take error text from `strerror` in the real-time helper; and use the POSIX `strerror_r`, which returns an int, whenever the C library is not glibc.
- `musl.patch` — The same changes in FreeDV's own PulseAudio device and engine, and an explicit `#include <signal.h>` in `PulseAudioDevice.h`, which glibc's headers pull in by way of others and musl's do not.
- `rade-opus-no-model-download.patch` — In RADE, configure its Opus with `autoreconf -isf` instead of `autogen.sh`, which downloads the model weights from media.xiph.org.

## Notes

Nothing is fetched at build time. Every tree upstream would download is a later source, and the
two model weight archives (`opus_data-*`, `rnnoise_data-*`) unpack into `$SRC_ROOT`, from where
`build.sh` copies `dnn/` into the Opus tree and `src/rnnoise_data*` into the RNNoise tree before
the configure. Each commit and weight hash is a helper in the recipe.

The generator is CMake's default, Unix Makefiles, never Ninja: the Opus and RNNoise
ExternalProjects build with a literal `$(MAKE)`, which Ninja cannot run.

RADE is a shared library that the backend builds and upstream installs only in its macOS bundle.
`freedv` links it by soname, so `build.sh` copies `librade.so*` from the build tree into
`/usr/lib`, and the `test` lines at the end fail the build if it, the program or the icon is
missing.

The desktop entry replaces upstream's to carry `StartupWMClass=freedv`: wxGTK names the GTK
program after the command, so that is the Wayland app id.
