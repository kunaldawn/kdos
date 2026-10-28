## About

Gqrx is a software-defined radio receiver: it tunes and demodulates AM, FM and SSB with a live
spectrum and waterfall. It is a Qt 6 program
built on [GNU Radio](#port-gnuradio), and it reaches the radio hardware through
[gr-osmosdr](#port-gr-osmosdr), which in turn drives RTL-SDR, Airspy, HackRF, bladeRF, USRP and
anything [SoapySDR](#port-soapysdr) supports.

## Flags

- `-DCMAKE_POLICY_DEFAULT_CMP0167=NEW` — makes every `find_package(Boost)` in the configure,
  including the ones GNU Radio's installed CMake package makes, read Boost's own config package
  instead of CMake's old FindBoost module. The module looks for a `libboost_system` library file,
  and Boost.System is header-only, so the search through it fails. [gnuradio](#port-gnuradio) sets
  the same policy for the same reason.
- `-DFORCE_QT6=ON` — require Qt 6. Without it, configure tries Qt 6 quietly and falls back to Qt 5
  when Qt 6 is not found, which would build against a different toolkit than `depends` names.
- `-DLINUX_AUDIO_BACKEND=Pulseaudio` — play demodulated audio through the PulseAudio API
  ([libpulse](#port-libpulse)), which PipeWire serves on KDOS (see
  [Audio](../../../../docs/kdos/03-architecture/session.md#audio)). The other choices are
  PortAudio and GNU Radio's own audio sink. Pulseaudio is also upstream's default.
- `-DCUSTOM_AIRSPY_KERNELS=OFF` — leave out what upstream calls "non-standard Airspy
  optimizations". With it on (upstream's default), gqrx sets the source's bandwidth to its
  decimation rate whenever the input device string contains `airspy`, and
  [gr-osmosdr](#port-gr-osmosdr)'s Airspy source answers by loading one of its built-in IQ
  conversion filter kernels into the radio. Off, the radio keeps its own filter.
- `-DCCACHE=OFF` — gqrx's configure looks for a `ccache` program and, when it finds one, sets it as
  the compiler launcher itself. Presetting the cache variable to `OFF` stops that search, so whether
  a compile goes through [ccache](#port-ccache) is left to the `CMAKE_C_COMPILER_LAUNCHER` and
  `CMAKE_CXX_COMPILER_LAUNCHER` the build environment sets.

## Notes

Upstream's manual page `resources/gqrx.1` is not installed by its build, so `build.sh` installs
it. Upstream installs a `dk.gqrx.gqrx.desktop`, and the recipe writes its own over it. gqrx sets
no desktop file name, so Qt's Wayland plugin makes the app id from the reversed organisation
domain (`gqrx.dk`) and the executable's name: `dk.gqrx.gqrx`, which the entry repeats in
`StartupWMClass`. The panel reads only PNG icons from hicolor, never an SVG (see
[Desktop entries belong to the port](../../../../docs/kdos/05-developer/writing-ports.md#desktop-entries-belong-to-the-port)),
so the SVG icon is rasterised to PNGs from 32 to 256 pixels.
