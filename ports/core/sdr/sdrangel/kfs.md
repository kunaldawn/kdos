## About

[SDRangel]{.idx} is a Qt 6 software-defined radio receiver, transmitter and analyser. Every radio,
demodulator, modulator and tool is a plugin under `/usr/lib/sdrangel/plugins`: decoders for
ADS-B, AIS, APRS, FT8, DAB, DATV, DSD, FreeDV, M17 and many more, a satellite and star tracker,
and a map. `sdrangel` is the graphical program and `sdrangelsrv` the same engine as a headless
server driven over its REST API.

## Flags

- `CXXFLAGS="$CXXFLAGS -DFLATBUFFERS_LOCALE_INDEPENDENT=0"` — The headers of
  [libsigmf](#port-libsigmf) include FlatBuffers' own, which take `_XOPEN_VERSION 700` to mean
  that `strtoll_l` and `strtoull_l` exist; musl has neither. With the macro at `0`, FlatBuffers
  uses the plain `strtoll` and `strtoull`.
- `-DCMAKE_POLICY_DEFAULT_CMP0167=NEW` — Find Boost through the `BoostConfig.cmake` that
  [boost](#port-boost) installs, not CMake's own `FindBoost` module.
- `-DARCH_OPT=$_march` — `ARCH_OPT` is passed straight to `-march`, and upstream's default,
  `native`, ties the binary to the processor of the build machine. `build.sh` names the
  architecture's baseline instead: `x86-64`, or `armv8-a` on aarch64.
- `-DENABLE_QT6=ON` — Build against Qt 6; upstream's default is Qt 5.
- `-DENABLE_EXTERNAL_LIBRARIES=OFF` — Use installed libraries only. In its `ON` and `AUTO` modes
  the build clones and builds its helper libraries from git at configure time. Each optional
  library is then found from its port: [codec2](#port-codec2) (FreeDV, M17),
  [ffmpeg](#port-ffmpeg) (DATV receive), Opus, FLAC, zlib, [hidapi](#port-hidapi) (FUNcube),
  [cm256cc](#port-cm256cc) (the remote input, output, sink and source), [dsdcc](#port-dsdcc) and
  [mbelib](#port-mbelib) (DSD), [serialdv](#port-serialdv) (AMBE), [libdab](#port-libdab) and
  [faad2](#port-faad2) (DAB), [sgp4](#port-sgp4) (satellite tracker), [aptdec](#port-aptdec)
  (APT), [cspice](#port-cspice) (star tracker), [ggmorse](#port-ggmorse) (Morse decoder),
  [libinmarsatc](#port-libinmarsatc) (Inmarsat-C), [rnnoise](#port-rnnoise) (denoiser),
  [libsigmf](#port-libsigmf) (SigMF file input and sink) and [opencv](#port-opencv) (ATV and DATV
  transmit).
- `-DENABLE_LIBUNWIND=ON` — Stack traces in the exception handlers, through the
  [libunwind-nongnu](#port-libunwind-nongnu) port.
- `-DDEBUG_OUTPUT=OFF`, `-DENABLE_PROFILER=OFF`, `-DBUNDLE=OFF` — No debug messages, no runtime
  profiler, and no self-contained distribution bundle; all upstream's defaults.
- `-DBUILD_GUI=ON`, `-DBUILD_SERVER=ON`, `-DBUILD_BENCH=OFF` — Build `sdrangel` and
  `sdrangelsrv`, but not the benchmark program.
- `-DRX_SAMPLE_24BIT=ON` — 24-bit internal receive DSP, upstream's default. The Perseus input is
  built only with it.
- `-DVKFFT_BACKEND=1` — The CUDA backend for vkFFT, upstream's default, rather than Vulkan.
  CUDAToolkit is never looked up (below), so no GPU FFT engine is built.
- `-DENABLE_AIRSPY=ON` — and the other `ENABLE_<radio>=ON` switches: Airspy, Airspy HF+, bladeRF,
  HackRF, RTL-SDR, LimeSuite, PlutoSDR through [libiio](#port-libiio), MiriSDR (which also drives
  the old SDRplay radios as the `sdrplay` input), Perseus, USRP through [uhd](#port-uhd),
  SoapySDR and FUNcube, each from its port; Fobos, which loads [libfobos](#port-libfobos) at run
  time; and Metis and the Aaronia RTSA, which are reached over the network.
- `-DENABLE_SDRPLAY=OFF`, `-DENABLE_XTRX=OFF` — No SDRplay API 3 input and no XTRX input; their
  libraries are not ports.
- `-DENABLE_PACK_MIRSDRAPI=OFF` — Do not include the closed `mirsdr-api` library, which upstream
  marks for personal use only.
- `-DCSPICE_INCLUDE_DIR=/usr/include/cspice` — cspice keeps its headers in `/usr/include/cspice`,
  which the bundled find module does not search.
- `-DCM256CC_DIR=/usr` — Search that installation for cm256cc directly. Without it, the bundled
  find module first asks pkg-config for a minimum version of `libcm256cc`, and the
  `libcm256cc.pc` that the [cm256cc](#port-cm256cc) port installs states a lower version than its
  release, so the check fails and cm256cc counts as missing.
- `-DCMAKE_DISABLE_FIND_PACKAGE_CUDAToolkit=ON` — Never look for CUDA, so the CUDA vkFFT engine is
  never built.
- `-DCMAKE_DISABLE_FIND_PACKAGE_Qt6WebEngineCore=ON` — with the same for `Qt6WebEngineQuick` and
  `Qt6WebEngineWidgets`: do not use Qt WebEngine, although
  [qt6-qtwebengine](#port-qt6-qtwebengine) is a port. It serves only the sky map and the 3D view
  of the map feature; the map keeps its 2D view without it.
- `-DCCACHE=OFF` — Presets the variable upstream's `find_program(CCACHE ...)` would fill, so the
  build does not set ccache as its compiler launcher itself. The tree's ccache arrives through
  `CMAKE_C_COMPILER_LAUNCHER` and `CMAKE_CXX_COMPILER_LAUNCHER` in the build environment.

## Patches

- `sgp4-range-rate.patch` — the satellite tracker reads the topocentric range rate as `topo.range_rate`; the [sgp4](#port-sgp4) library the tree carries names that member `rangeRate`, and the patch reads it under that name.

## Notes

The remote input, remote sink, remote source and remote output plugins are built only when the
compiler has SSE3 or NEON. `ARCH_OPT`'s x86-64 baseline defines no SSE3, so those four are built
on aarch64 only.

A radio, decoder or tool whose library the configuration does not find is dropped with only a
status message. `build.sh` therefore ends by checking that `sdrangel` and `sdrangelsrv` are
executable and that a list of plugins is in the package (the RTL-SDR, Airspy HF+,
LimeSDR, PlutoSDR, old SDRplay, Perseus, USRP and Fobos inputs, the DSD, DAB, APT and Inmarsat
demodulators, the SigMF file sink, the AMBE, Morse decoder, denoiser, star tracker and LimeRFE
features, and the ATV modulator), so losing the library behind one of those fails the build
instead of dropping its plugin in silence.

Upstream's desktop entry has no `StartupWMClass`. With no organisation domain set, Qt's Wayland
app id is the executable's name, `sdrangel`, which the entry `build.sh` writes names. The panel
reads PNG icons only, so the SVG icon is rasterised from 48 to 512 pixels.
