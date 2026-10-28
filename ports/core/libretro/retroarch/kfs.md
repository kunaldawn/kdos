## About

RetroArch is the libretro frontend: one window, one menu, one set of controller bindings, save
states, shaders and netplay for every emulator and game engine built as a libretro core. In KDOS
the cores, assets, core information files, databases and controller profiles are ports of their
own, installed under `/usr/lib/libretro` and `/usr/share/libretro`, and RetroArch is pointed at
them.

## Flags

- `CFLAGS="${CFLAGS/-std=gnu11/}"` — Remove the tree's `-std=gnu11` from `CFLAGS`. The configure
  tests each C++ standard by compiling with `CFLAGS` and `-Werror`, and g++ refuses the C-only
  `-std=gnu11`, so every C++ standard test would fail and the configure would turn C++ off; with
  `--enable-vulkan` named, which needs C++, it then stops with an error. The project sets
  `-std=gnu99` for its C code itself.
- `--enable-wayland` — Every switch the build relies on is named, so a missing library fails the
  configure instead of being probed away: Wayland with `--enable-libdecor` for the window
  decorations, `--enable-egl`, `--enable-opengl`, `--enable-opengl_core` and `--enable-vulkan` for
  the video drivers, and `--enable-kms` for running on a bare console with no compositor.
- `--disable-x11` — No X11 video or input driver: the Wayland driver draws the window, so
  RetroArch needs no Xwayland.
- `--disable-qt` — No Qt companion window; the menu is RetroArch's own.
- `--enable-udev` — Gamepads through udev and evdev ([eudev](#port-eudev)).
- `--enable-hid` — Build RetroArch's low-level HID layer, off by default. `--enable-libusb`, the
  joypad driver that reads USB pads directly, is part of it, and the configure refuses libusb
  unless HID is enabled with it.
- `--enable-sdl2` — The SDL 2 video, input, joypad and audio drivers, through
  [sdl2-compat](#port-sdl2-compat); `--disable-sdl` leaves out SDL 1.2.
- `--enable-alsa` — Audio through ALSA, `--enable-pulse` and `--enable-pipewire`.
- `--disable-jack` — None of the other audio back ends: JACK, OSS (`--disable-oss`), the RSound
  and RoarAudio network sound servers (`--disable-rsound`, `--disable-roar`), TinyALSA
  (`--disable-tinyalsa`) and the BSD `audio(4)` driver (`--disable-audioio`).
- `--disable-systemd` — No systemd integration ([principles](../../../../docs/kdos/01-philosophy/principles.md#no-systemd)).
- `--enable-freetype` — Draw menu and on-screen text with [freetype2](#port-freetype2).
- `--disable-ffmpeg` — No FFmpeg: its recording path and the FFmpeg media core do not build
  against the [ffmpeg](#port-ffmpeg) this tree carries. `--disable-mpv` is upstream's default.
- `--enable-networking` — Keep netplay, which works on a local network with no TLS;
  `--disable-ssl` leaves out the TLS code.
- `--disable-online_updater` — Every online service is off: the online updater and its core,
  core-info and asset downloads (`--disable-update_cores`, `--disable-update_core_info`,
  `--disable-update_assets`), RetroAchievements (`--disable-cheevos`), Discord presence
  (`--disable-discord`) and the OCR translation service (`--disable-translate`). Cores and data
  come from ports.
- `--disable-langextra` — Build no menu translations; the menu is English
  ([language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `--disable-builtinzlib` — Link the system [zlib](#port-zlib) instead of the bundled copy.
- `--enable-builtinflac` — Compile the bundled FLAC. The CHD disc-image reader decodes through
  the bundled `dr_flac`, which only this path compiles in (`HAVE_DR_FLAC`).
- `--disable-cg` — No Nvidia Cg shader runtime. With `--disable-caca` and `--disable-sixel`
  (terminal video drivers), `--disable-vg` (OpenVG) and `--disable-videocore` (the Raspberry Pi's
  Broadcom VideoCore), these drivers are left out.

## Patches

- `config.patch` — Edit the skeleton `retroarch.cfg` that a first run copies: hide the core updater in the menu, and point the assets, core, core information and controller autoconfiguration directories at `/usr/share/libretro/assets`, `/usr/lib/libretro`, `/usr/share/libretro/info` and `/usr/share/libretro/autoconfig`, where the data and core ports install. Upstream's cores, which the updater downloads, are built for glibc.
- `database-path.patch` — In the same file, point the content database directory at `/usr/share/libretro/database/rdb`, the [libretro-database](#port-libretro-database) port's.

## Notes

The cheat directory is left at its per-user default: libretro-database installs only its
`rdb/` databases and `cursors/`, no cheat collection, so there is no system directory to point it
at.

`build.sh` removes `retroarch-cg2glsl` and its manual page: it converts Nvidia Cg shaders, which
this build cannot load, and it is a Python script nothing here runs.

The panel reads no SVG, so the menu icon is a PNG rasterised from upstream's. Upstream's desktop
entry is replaced: it claims every ROM type, among them the Game Boy, Game Boy Color and Game Boy
Advance types the [mgba](#port-mgba) entry claims, which would open those files in whichever entry
sorted first. RetroArch opens games from its own menu. The Wayland app id is `com.libretro.RetroArch`.
