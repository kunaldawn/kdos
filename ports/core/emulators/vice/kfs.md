## About

VICE emulates the 8-bit Commodore machines: the C64 (`x64sc`), the C64 with a SuperCPU and the
C64 DTV, the C128, the VIC-20, the Plus/4, the PET and the CBM-II, each as its own program, plus
`vsid`, a player for C64 SID music files. The port uses the SDL 2 front end, drawn here by
[sdl2-compat](#port-sdl2-compat) on SDL 3's Wayland driver, whose menus are drawn inside the
emulated screen. The machines' ROMs are installed with the emulators; they are one of the
exemptions in
[What is built from source](../../../../docs/kdos/05-developer/writing-ports.md#what-is-built-from-source).

## Flags

- `DOS2UNIX=true` — Answer configure's check for `dos2unix`, which it refuses to run without.
  The tool is used only by the targets that make distribution archives, never by a build, so
  `true` stands in for it. The other tool configure requires, [xa](#port-xa), is real: it
  assembles the PSID player that `vsid` loads into the emulated machine.
- `--enable-sdl2ui` — The SDL 2 front end, one of the four configure offers. The GTK 3 front end
  would put an X11 child window under its OpenGL view.
- `--with-sdlsound` — This switch and `--with-alsa`, `--with-pulse`, `--with-flac`, `--with-vorbis`,
  `--with-mpg123`, `--with-lame`, `--with-png`, `--with-gif`, `--with-resid` and `--with-fastsid`
  name every sound path and codec. Some, FLAC among them, are off unless named, and a named
  library configure cannot find stops it with an error. Between them they are sound through SDL,
  ALSA or PulseAudio (served by pipewire-pulse), FLAC and Ogg Vorbis for recording sound and, with
  mpg123, for the audio files the sampler reads, LAME for MP3 recording, PNG and GIF screenshots,
  and the reSID and FastSID sound chip engines.
- `--without-oss` — No OSS sound. OSS is not on this system.
- `--without-libcurl` — libcurl serves only the WIC64, a userport Wi-Fi module that reaches the
  internet. configure's default is on, and stops with an error when libcurl is missing.
- `--without-portaudio` — No PortAudio, the sampler's only live input from the host's capture
  device; the sampler reads audio files only. VICE's ALSA driver is sound output alone.
- `--enable-midi` — MIDI through the ALSA sequencer.
- `--disable-realdevice` — No access to real Commodore peripherals through OpenCBM, which needs a
  device this image does not carry.
- `--disable-ethernet` — No emulation of the Ethernet cartridges, which needs root access to a
  network device.
- `--disable-html-docs`, `--disable-pdf-docs` — Build neither the HTML nor the PDF manual.
- `-g0` — Appended to `CFLAGS` and `CXXFLAGS` on both `make` lines. configure puts its own
  `-g -O3` into `AM_CFLAGS`, ahead of `CFLAGS` on the compile line, so `-g0` cancels the `-g`;
  `${CFLAGS/-O2/-O3}` keeps upstream's `-O3`, which the tree's `-O2` would otherwise override.

## Notes

The SDL front end writes its log to `vice.log` in `~/.local/state/vice` and does not survive
failing to open it, so an emulator started on an account without that directory crashes before
its window. The patch creates the directory at start-up beside the cache and configuration
directories, as the GTK 3 front end does.

configure refuses any `--enable-` or `--with-` switch it does not list itself, so the usual
`--disable-static` is not passed.

The SDL front end installs no icons or menu entries. `build.sh` installs the PNG icons from
`data/common` and writes one entry per machine; `x64sc` and `xcbm5x0` have no icon of their own
and share their family's. Each emulator is its own program, and SDL's Wayland app_id is the
executable's name.
