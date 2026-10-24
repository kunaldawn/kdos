## About

SDL 3 is the Simple DirectMedia Layer: one C library that opens a window, draws into it through
OpenGL, OpenGL ES or Vulkan, plays and records sound, and reads keyboards, mice, game controllers
and touch. In KDOS it is the SDL that carries the drivers. SDL 2 programs, among them baresip's
video display and whisper.cpp's streaming transcription, link [sdl2-compat](#port-sdl2-compat),
which answers every SDL 2 call through this library at run time.

The build keeps one video driver, Wayland, and the four audio APIs a KDOS session can answer: ALSA,
PipeWire, PulseAudio and JACK, the last three all served by [pipewire](#port-pipewire).

## Flags

- `-DSDL_TEST_LIBRARY=OFF`, `-DSDL_TESTS=OFF`, `-DSDL_EXAMPLES=OFF` — Builds neither the
  `SDL3_test` helper library, which upstream builds by default, nor the test and example programs.
- `-DSDL_RPATH=OFF` — Upstream's default on Linux is on, and it writes `-Wl,-rpath,${libdir}` into
  the `Libs:` line of `sdl3.pc`, so every program linked through pkg-config would carry a run path
  of `/usr/lib`. That is the default search path, where `libSDL3.so.0` already is, and the package
  lint warns on such an entry (see [What the package lint
  refuses](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#what-the-package-lint-refuses)).
- `-DSDL_X11=OFF` — Builds no X11 video driver. Every SDL program has the Wayland path, so SDL
  never needs Xwayland, which serves only programs with no Wayland path at all (see [No Xorg
  server, and one
  carve-out](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/01-philosophy/principles.md#no-xorg-server-and-one-carve-out)).
  A program that forces `SDL_VIDEODRIVER=x11` fails to open a window. With the driver off, SDL's
  Fribidi and libthai support, which upstream ties to the X11 driver, is off too.
- `-DSDL_KMSDRM=OFF` — Builds no KMS/DRM video driver, the one that draws straight to the display
  with no compositor. Such a program would take the card and the input devices from the running
  compositor; without the driver, an SDL program started outside `kdos-comp` exits with "No
  available video device".
- `-DSDL_WAYLAND=ON` — The Wayland video driver, the only one built.
- `-DSDL_WAYLAND_LIBDECOR=ON` — Draws a window's title bar and borders itself, through
  [libdecor](#port-libdecor), when the compositor offers no `xdg-decoration` manager to draw them.
- `-DSDL_ALSA=ON`, `-DSDL_PIPEWIRE=ON`, `-DSDL_PULSEAUDIO=ON` — The ALSA, native PipeWire and
  PulseAudio audio drivers. SDL tries PipeWire first, then PulseAudio, then ALSA. In a KDOS session
  the PulseAudio server is `pipewire-pulse`, so the PulseAudio driver, for a program that asks for
  `SDL_AUDIO_DRIVER=pulseaudio`, reaches the same graph (see
  [Audio](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/session.md#audio));
  ALSA is the floor, for a machine where the session's sound server is not running.
- `-DSDL_JACK=ON` — The JACK audio driver. There is no JACK server: [pipewire](#port-pipewire)
  installs its own `libjack.so.0` and `jack.pc`, and once `jack.pc` exists a port that leaves JACK
  on auto-detection gains it or not by build order, so every port that can link JACK names it (see
  [JACK programs on the
  host](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/session.md#jack-programs-on-the-host)).
  `pipewire` is in `depends` for this reason.
- `-DSDL_SNDIO=OFF` — Builds no sndio driver; sndio, OpenBSD's sound server, is not a port.
- `-DSDL_OSS=OFF` — Builds no OSS driver. Upstream's default is already off on Linux; named here.
- `-DSDL_DBUS=ON` — D-Bus support, which SDL uses for screensaver inhibition, the desktop's
  settings portal and the fcitx input-method frontend; fcitx needs nothing but D-Bus.
- `-DSDL_IBUS=OFF` — Builds no IBus frontend. IBus is not a port; the input method in KDOS is
  [fcitx5](#port-fcitx5) (see [Input
  methods](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/session.md#input-methods)).
- `-DSDL_LIBUDEV=ON` — Finds joysticks and other input devices, and their hot-plugging, through
  libudev from [eudev](#port-eudev).
- `-DSDL_HIDAPI_LIBUSB=ON` — Lets SDL's HIDAPI controller drivers talk to devices over libusb as
  well as over hidraw. libusb reaches the pads that have no usable hidraw node, the GameCube
  adapter and the Switch 2 controllers; every other pad's HIDAPI driver opens `/dev/hidraw*`, which
  `fs/etc/udev/rules.d/70-kdos-gamepad.rules` grants to the `input` group. The switch only permits
  a probe, and configure carries on without the feature when libusb is missing, so
  [libusb](#port-libusb) is named in `depends`.
- `-DSDL_LIBURING=ON` — Builds SDL's asynchronous file I/O on io_uring, through `liburing-ffi`;
  without it the same API runs on SDL's generic thread pool. Like the libusb switch it only permits
  a probe, so [liburing](#port-liburing) is in `depends`.
- `-DSDL_OPENGL=ON`, `-DSDL_OPENGLES=ON`, `-DSDL_VULKAN=ON` — OpenGL, OpenGL ES and Vulkan
  surfaces, and the render drivers built on them.
- `-DSDL_INSTALL_DOCS=ON` — Generates and installs a manual page for every public function.
  Upstream's default is off. The pages are written from the headers by
  `build-scripts/wikiheaders.pl`, which is why [perl](#port-perl) is in `depends`.

## Notes

SDL loads most of what it can use with `dlopen` at run time, by soname, rather than linking it:
upstream's `SDL_DEPS_SHARED` is left on, so ALSA, PipeWire, PulseAudio, JACK, Wayland, libdecor and
libusb are each looked up when SDL starts. The libraries must be present when the port builds, for
their headers and for the probes, and each is in `depends` for that; a library missing at run time
removes only that one driver.

`SDL_Init(SDL_INIT_AUDIO)` opens no window and needs no video device, so a program that uses SDL
for sound alone, such as whisper.cpp's streaming transcription, runs in a terminal with no
compositor above it.

SDL 2 programs reach this library through [sdl2-compat](#port-sdl2-compat), and the two are built
to match: `SDL2COMPAT_X11` stays off for as long as this port is built with `-DSDL_X11=OFF`.
