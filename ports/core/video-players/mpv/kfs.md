## About

mpv is a media player with no toolkit: it draws video with its own GPU renderer straight onto a
Wayland surface, and its controls, statistics overlay and console are Lua scripts built into the
binary. The port also builds `libmpv`, the playback engine that front ends such as
[haruna](#port-haruna) draw their windows around ([smplayer](#port-smplayer) runs the `mpv`
program itself inside its window instead), and loads C plugins such as [mpv-mpris](#port-mpv-mpris), which puts mpv on the session bus for the media keys
and the panel.

Nearly every optional part is named on or off. A meson feature left at `auto` is built when its
library happens to be in the build root, so naming each one keeps the player the same on every
build and turns a missing library into a failed configure.

## Flags

- `-Dlibmpv=true`, `-Dcplayer=true` — Build both the shared library and the `mpv` program.
- `-Dbuild-date=false` — Leave the compile time out of the binary, which otherwise makes two
  builds of the same source differ
  ([Reproducible packages](../../../../docs/kdos/03-architecture/packaging.md#reproducible-packages)).
- `-Dcplugins=enabled` — Load C plugins from `~/.config/mpv/scripts` and the system script
  directory; [mpv-mpris](#port-mpv-mpris) is one.
- `-Dwayland=enabled`, `-Dgl=enabled`, `-Degl=enabled`, `-Degl-wayland=enabled` — The Wayland
  output and OpenGL through Mesa's EGL on it, the default path on the desktop.
- `-Degl-drm=enabled`, `-Dgbm=enabled`, `-Ddrm=enabled` — Video straight to the display through
  KMS, with no compositor running (`--vo=gpu --gpu-context=drm`, or `--vo=drm`).
- `-Ddmabuf-wayland=enabled` — `--vo=dmabuf-wayland`, which hands decoded frames to the compositor
  as DMA-BUFs instead of drawing them.
- `-Dvulkan=enabled` — The Vulkan rendering context. The renderer itself is
  [libplacebo](#port-libplacebo), a required dependency built with its Vulkan and OpenGL backends,
  so `--vo=gpu-next` runs on Vulkan where a device answers and on OpenGL through EGL where none
  does.
- `-Dshaderc=disabled`, `-Dspirv-cross=disabled` — A second GLSL compiler beside the
  [glslang](#port-glslang) libplacebo uses, and SPIRV-Cross, which is not a port.
- `-Dx11=enabled`, `-Degl-x11=enabled`, `-Dx11-clipboard=enabled` — A second output for a front
  end that embeds mpv by X11 window id, as SMPlayer does with `--wid`; without an X11 output such a
  front end plays the sound with no picture. The window is drawn with OpenGL through EGL, and the
  clipboard is the X11 one. The libraries are the X client libraries, served by Xwayland; no X
  server is involved on the login path
  ([principles](../../../../docs/kdos/01-philosophy/principles.md#no-xorg-server-and-one-carve-out)).
- `-Dgl-x11=disabled`, `-Dxv=disabled`, `-Dvdpau=disabled`, `-Dvdpau-gl-x11=disabled` — GLX, the
  XVideo output and VDPAU decode, leaving EGL as the only OpenGL route on X11 and VA-API as the
  only hardware decoder.
- `-Dvaapi-x11=disabled` — VA-API on an X11 window, which needs a libva built with its X11
  backend; [libva](#port-libva) is built with `-Dwith_x11=no`.
- `-Dvaapi=enabled`, `-Dvaapi-drm=enabled`, `-Dvaapi-wayland=enabled` — Hardware decoding through
  [libva](#port-libva), presented on DRM and on Wayland.
- `-Dcuda-hwaccel=disabled`, `-Dcuda-interop=disabled` — NVIDIA CUDA decoding, whose toolkit is
  not a port.
- `-Dsixel=enabled` — `--vo=sixel`, video drawn as sixel pictures in a terminal that supports them
  ([Picture capabilities](../../../../docs/kdos/04-programs/kdos-term.md#picture-capabilities)).
  It reaches a terminal where no compositor runs: a serial line, an ssh login or a console
  terminal. mpv builds its other terminal outputs, `--vo=tct` and `--vo=kitty`, with no switch.
- `-Dcaca=disabled` — The libcaca text-art output, which is not a port.
- `-Dpipewire=enabled`, `-Dalsa=enabled`, `-Djack=enabled` — The PipeWire and ALSA outputs, and
  JACK, which builds against the `libjack` that [pipewire](#port-pipewire) provides.
- `-Dpulse=disabled`, `-Dopenal=disabled`, `-Dsdl2-audio=disabled` — Further routes to the sound
  server the PipeWire output already reaches; the PulseAudio protocol is served by PipeWire
  itself.
- `-Dsndio=disabled`, `-Doss-audio=disabled` — The sndio and OSS outputs. sndio is not a port.
- `-Dsdl2-video=disabled`, `-Dsdl2-gamepad=disabled` — SDL's video output and gamepad input.
- `-Dlua=luajit` — The scripting layer, and with it the on-screen controller, the stats overlay
  and the console. mpv accepts Lua 5.1 or 5.2; [luajit](#port-luajit) is 5.1 with 5.2
  extensions.
- `-Djavascript=disabled` — JavaScript scripting, which needs MuJS, not a port.
- `-Drubberband=enabled` — The `rubberband` pitch- and tempo-shifting audio filter.
- `-Dzimg=enabled` — The [zimg](#port-zimg) scaler, used for software conversions and screenshots
  in preference to libswscale.
- `-Djpeg=enabled` — JPEG screenshots.
- `-Duchardet=enabled` — Detect the character set of an external subtitle file, so
  `--sub-codepage=auto`, the default, converts a CP1251, GBK or Shift-JIS subtitle instead of
  showing it garbled.
- `-Diconv=enabled`, `-Dzlib=enabled`, `-Dlcms2=enabled`, `-Dlibarchive=enabled`,
  `-Dlibavdevice=enabled` — Character-set conversion, zlib-compressed Matroska tracks, ICC
  colour management, playing files from inside archives (`archive://`), and FFmpeg's capture
  devices as inputs.
- `-Dlibbluray=enabled`, `-Ddvdnav=enabled`, `-Dcdda=enabled` — `bd://`, `dvd://` and `cdda://`
  input, through [libbluray](#port-libbluray), [libdvdnav](#port-libdvdnav) and
  [libcdio-paranoia](#port-libcdio-paranoia). mpv builds `dvdnav`, `cdda` and `dvbin`, like
  `jack` and `x11`, only as a GPL build, which this is: `-Dgpl` keeps its default of `true`.
- `-Ddvbin=enabled` — `dvb://` television input, which needs only the kernel's DVB headers.
- `-Dvapoursynth=disabled` — The VapourSynth filter, which is not a port.
- `-Dmanpage-build=enabled` — Build `mpv.1` with rst2man from
  [python3-docutils](#port-python3-docutils).

## Notes

[yt-dlp](#port-yt-dlp) is in `depends` but not used by the build: mpv's built-in `ytdl_hook`
script runs it at play time when given a web address.
