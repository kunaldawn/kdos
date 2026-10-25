## About
[OBS Studio]{.idx} records the screen and streams it live, composing scenes from screen and
window captures, cameras, images, text and media, with an audio mixer and filters. On Wayland its
screen capture is the `linux-pipewire` plugin, which asks the ScreenCast portal for a PipeWire
stream (see [Screen capture and the clipboard](../../../../docs/kdos/03-architecture/session.md#screen-capture-and-the-clipboard)).
The `linux-capture` plugin, XComposite and XSHM, sees only Xwayland windows; libobs links libX11
and xcb on Linux unconditionally, and that plugin is built with it.

## Flags
- `OBS_VERSION_OVERRIDE` — The version OBS reports. Without it, `cmake/common/versionconfig.cmake`
  asks `git describe` only when the source has a `.git`, which the release tarball does not, and
  otherwise reports its placeholder version.
- `CMAKE_SKIP_INSTALL_RPATH=ON` — Install the programs and plugins with no `RPATH`; libobs and
  the frontend library are in `/usr/lib`, on the loader's default path.
- `CMAKE_COMPILE_WARNING_AS_ERROR=OFF` — OBS turns every warning into an error unless this is
  set; with it, a warning from a newer compiler or library does not stop the build.
  `OBS_COMPILE_DEPRECATION_AS_WARNING=ON` likewise keeps deprecated-declaration warnings from
  being errors.
- `-Wno-incompatible-pointer-types` — Added to `CFLAGS`: GCC treats an incompatible pointer
  conversion in C as an error by default, and this keeps it a warning.
- `OpenGL_GL_PREFERENCE=GLVND` — Have CMake's `FindOpenGL` link the vendor-neutral `libOpenGL`
  from [libglvnd](#port-libglvnd) rather than the legacy `libGL`.
- `ENABLE_JACK=ON` — The JACK input, off upstream. The JACK library is PipeWire's
  (`pipewire-jack`, built by the [pipewire](#port-pipewire) port), so a JACK source in OBS reads
  from the PipeWire graph.
- `ENABLE_VLC=ON` — The VLC video source, which plays a playlist through [vlc](#port-vlc)'s
  libvlc.
- `ENABLE_NEW_MPEGTS_OUTPUT=ON` — OBS's own MPEG-TS output for SRT and RIST, through
  [libsrt](#port-libsrt) and [librist](#port-librist).
- `ENABLE_RNNOISE=ON` — The RNNoise noise-suppression filter. With no system library found, OBS
  compiles the copy in its tree instead and says so only in a warning; the `readelf` check at
  the end of `build.sh` makes sure the filter links the [rnnoise](#port-rnnoise) port.
- `ENABLE_BROWSER=OFF` — With `ENABLE_WHATSNEW=OFF`: the browser source and the What's New panel
  need the Chromium Embedded Framework, which is not a port.
- `ENABLE_WEBSOCKET=OFF` — The obs-websocket remote-control server needs WebSocket++ and
  qrcodegencpp, which are not ports.
- `ENABLE_SERVICE_UPDATES=OFF` — Do not download the list of streaming services from
  obsproject.com at every start; the list shipped in the package is used.
- `ENABLE_WEBRTC=OFF` — The WHIP/WebRTC output needs libdatachannel, which is not a port.
- `ENABLE_LIBFDK=OFF` — No FDK AAC encoder: its licence cannot be combined with the GPL of the
  OBS binary that ships. FFmpeg's AAC encoder is used instead.
- `ENABLE_SNDIO=OFF` — With `ENABLE_AJA=OFF`, `ENABLE_DECKLINK=OFF`, `ENABLE_NVENC=OFF`,
  `ENABLE_FFMPEG_NVENC=OFF` and `ENABLE_QSV11=OFF`: sndio output, AJA and DeckLink capture cards,
  NVIDIA NVENC and Intel QSV are left out. sndio, AJA's libajantv2, NVENC's FFnvcodec headers and
  QSV's libvpl are not ports. DeckLink builds from the SDK headers in OBS's own tree and loads
  Blackmagic's `libDeckLinkAPI.so` at run time.
- `ENABLE_TEST_INPUT=OFF` — Build no test sources.

## Notes
- **The checks at the end.** `build.sh` stops unless the `vlc-video`, `linux-jack` and
  `obs-ffmpeg` plugins were built, and unless `obs-filters.so` links librnnoise and
  `obs-ffmpeg.so` links libsrt. A missing libvlc header, JACK library, libsrt or librist already
  stops the configure; the `readelf` line on `obs-filters.so` is the one that catches OBS
  compiling its own RNNoise, which it reports only as a warning.
- **English only.** Every module's `locale/*.ini` but `en-US.ini` is removed; each module falls
  back to its English strings.
- **The menu entry is replaced** so that `StartupWMClass` is `com.obsproject.Studio`, the desktop
  file name OBS sets and its Wayland app_id, rather than `obs`.
