## About

FreeRDP is an implementation of Microsoft's Remote Desktop Protocol. The port builds its
libraries (`libfreerdp3`, `libwinpr3` and the client library with its virtual-channel plugins)
and one client, `sdl-freerdp`, which runs on SDL 3 and so opens as a Wayland window, and the server
library `libfreerdp-server3`. The client libraries are the RDP engine of [remmina](#port-remmina);
the server library is the one [krdp](#port-krdp) shares a Plasma desktop through. No server
program is built.

## Flags

- `-DCMAKE_SKIP_INSTALL_RPATH=ON` — Install the binaries without the `$ORIGIN/../lib:$ORIGIN/..`
  run path FreeRDP's `cmake/ConfigureRPATH.cmake` sets; the libraries are in `/usr/lib`, where
  the loader looks anyway.
- `-DWITH_SAMPLE=OFF` — No sample client or sample server.
- `-DWITH_SERVER=ON` — Build `libfreerdp-server3`, its headers and `freerdp-server3.pc`, and the
  server side of the channels: the protocol half of an RDP server, which [krdp](#port-krdp) links
  and feeds with KWin's screencast. It is upstream's default, stated beside the three switches
  below that leave out the server programs it would otherwise bring.
- `-DWITH_SHADOW=OFF` — No `freerdp-shadow-cli`. The shadow server mirrors a desktop through its
  X11 subsystem, which a build with no X11 cannot have; krdp is the server that shares a Plasma
  session.
- `-DWITH_PROXY=OFF` — No `freerdp-proxy`, the RDP proxy server with its module plugins; nothing
  in KDOS relays RDP connections.
- `-DWITH_PLATFORM_SERVER=OFF` — No platform server. `server/CMakeLists.txt` builds one only for
  Windows, and reports the macOS one as broken, so on Linux the option adds nothing; upstream's
  default is on, and it is stated off so the configure says what is built.
- `-DWITH_CLIENT=ON`, `-DWITH_CLIENT_SDL=ON`, `-DWITH_CLIENT_SDL3=ON` — Build the SDL client, on
  [sdl3](#port-sdl3).
- `-DWITH_CLIENT_SDL2=OFF` — The SDL 2 client, which upstream marks deprecated and experimental;
  it would otherwise be built whenever SDL 2 is found ([sdl2-compat](#port-sdl2-compat) provides
  it).
- `-DWITH_SDL_IMAGE_DIALOGS=ON` — Show images in the client's dialogs through
  [sdl3-image](#port-sdl3-image); upstream calls it recommended and leaves it off by default.
- `-DWITH_X11=OFF`, `-DCMAKE_DISABLE_FIND_PACKAGE_X11=ON` — No X11 client, and no search for the X
  libraries at all, so nothing in the build links them even where a component would take them
  when found.
- `-DWITH_WAYLAND=OFF` — Skips `uwac` and `wlfreerdp`, the native Wayland client, which upstream
  builds only while its deprecated 3.x interfaces are built (`WITHOUT_FREERDP_3x_DEPRECATED`
  off). The SDL client is the Wayland client here.
- `-DWITH_CHANNELS=ON` — Build the virtual-channel plugins: clipboard, sound, drive, printer,
  smart-card and USB redirection, and the graphics pipeline.
- `-DWITH_MANPAGES=ON` — Generate the manual pages.
- `-DWITH_FFMPEG=ON`, `-DWITH_VIDEO_FFMPEG=ON` — H.264 decoding for the graphics pipeline through
  [ffmpeg](#port-ffmpeg).
- `-DWITH_DSP_FFMPEG=OFF` — Keep FFmpeg out of the audio path; sound is encoded and decoded by
  FreeRDP's own DSP code with the codec libraries named below.
- `-DWITH_SWSCALE=ON`, `-DWITH_CAIRO=OFF` — Scale the remote screen with FFmpeg's libswscale
  rather than cairo.
- `-DWITH_JPEG=ON` — JPEG decoding through [libjpeg-turbo](#port-libjpeg-turbo).
- `-DWINPR_UTILS_IMAGE_JPEG=ON`, `-DWINPR_UTILS_IMAGE_PNG=ON`, `-DWINPR_UTILS_IMAGE_WEBP=ON` —
  Convert clipboard images between Windows bitmaps and JPEG, PNG and WebP, so a picture copied on
  either side pastes on the other. Upstream leaves all three off.
- `-DWITH_OPENH264=OFF` — Cisco's OpenH264 decoder, a second H.264 path beside FFmpeg's.
- `-DWITH_GSM=OFF`, `-DWITH_LAME=OFF`, `-DWITH_FAAD2=OFF`, `-DWITH_FAAC=OFF` — The GSM, MP3 and two
  further AAC codecs for audio redirection; AAC is covered by FDK-AAC below.
- `-DWITH_AOM=OFF`, `-DWITH_DAV1D=OFF`, `-DWITH_YUV=OFF` — AV1 decoding through libaom or dav1d,
  and libyuv's colour conversion.
- `-DWITH_FDK_AAC=ON`, `-DWITH_OPUS=ON`, `-DWITH_SOXR=ON` — AAC through [fdk-aac](#port-fdk-aac),
  Opus, and resampling through [soxr](#port-soxr) for audio redirection.
- `-DWITH_ALSA=ON`, `-DWITH_PULSE=ON` — The ALSA and PulseAudio sound backends; the PulseAudio one
  is built against [libpulse](#port-libpulse) and reaches PipeWire's PulseAudio server.
- `-DWITH_OSS=OFF`, `-DWITH_SNDIO=OFF` — The OSS and sndio sound backends, which upstream builds
  when it finds them. sndio is not a port.
- `-DWITH_CUPS=ON` — Printer redirection through [cups](#port-cups).
- `-DWITH_PCSC=ON` — Smart-card redirection through [pcsc-lite](#port-pcsc-lite).
- `-DWITH_PKCS11=ON` — WinPR's NCrypt layer over PKCS #11 modules, which smart-card logon in
  NLA uses to reach the keys and certificates on a card.
- `-DWITH_KRB5=ON` — Kerberos authentication through [krb5](#port-krb5), for NLA against a domain.
- `-DWITH_FUSE=ON` — Copy files through the clipboard, exposed as a [fuse](#port-fuse) file
  system.
- `-DWITH_JSONC_REQUIRED=ON` — Use [json-c](#port-json-c) for JSON and fail if it is not found;
  without it the build takes the first it finds of Jansson, json-c and cJSON, in that order.
- `-DWITH_TIMEZONE_ICU=ON` — Map time zones between their Windows and IANA names through
  [icu](#port-icu).
- `-DWITH_URIPARSER=ON` — Validate, escape and unescape URLs in WinPR's string helpers
  with [uriparser](#port-uriparser) rather than WinPR's own fallback code.
- `-DWITH_SYSTEMD=OFF` — No export of FreeRDP's log to the systemd journal; KDOS has no systemd
  ([principles](../../../../docs/kdos/01-philosophy/principles.md#no-systemd-init-units-or-service-manager)).
- `-DWITH_INSTALL_CLIENT_DESKTOP_FILES=OFF` — Install no `.desktop` file for `sdl-freerdp`, which
  takes the server and its options on the command line; it gets no menu entry. This is
  upstream's default.
- `-DWITH_BINARY_VERSIONING=OFF` — Install the programs (`sdl-freerdp`, `winpr-hash`,
  `winpr-makecert`), the manual pages and the SDL client's resource directory without the
  major version in the name (`sdl-freerdp`, not `sdl-freerdp3`); the library names do not depend
  on it. This is upstream's default.
- `-DWITH_CCACHE=OFF`, `-DWITH_CLANG_FORMAT=OFF` — Do not use ccache, and do not probe for
  `clang-format`, both of which upstream turns on when found.
- `-DUSE_VERSION_FROM_GIT_TAG=OFF` — Take the version from the release's own `CMakeLists.txt`,
  not from `git describe`; the source is a tarball with no repository.
