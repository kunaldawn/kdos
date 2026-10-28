## About
[VLC]{.idx} is VideoLAN's media player: files, DVDs, Blu-ray discs, audio CDs, capture devices and
network streams, with transcoding and streaming out (`sout`) as well. This port builds VLC 3 with
its Qt 5 interface, `libvlc` and `libvlccore` for programs that embed it, the `ncurses` text
interface, and the modules its configure line names.

The Qt interface is built with its X11 helpers ([qt5-qtx11extras](#port-qt5-qtx11extras)), and
VLC 3 then starts Qt on its xcb platform whatever `QT_QPA_PLATFORM` says, and does not open the
interface at all when no X display answers. So VLC runs under Xwayland, embeds the video in its
window by X window id, and draws through the xcb, XVideo, GLX and EGL-on-X11 video outputs.
`--enable-wayland` builds the Wayland outputs as well, with an `xdg-shell` video window that VLC
opens when it runs without the Qt interface and with no X display, as `cvlc` does with `DISPLAY`
unset.

## Flags
- `LUA_CFLAGS` — with `LUA_LIBS` and `LUAC`: build the Lua modules against
  [lua54](#port-lua54), through its versioned names `lua5.4` and `luac5.4`. configure asks
  `pkg-config` for `lua5.2`, then `lua5.1`, then `lua`; the tree's `lua` is 5.5, whose API VLC's
  Lua modules do not build against. The two variables answer configure's check outright, and
  `LUAC` compiles the playlist, extension and interface scripts for the same virtual machine.
- `BUILDCC` — the compiler for the helper programs the build runs on the build machine. configure
  looks for one by the POSIX names `c11-gcc`, `c11`, `c99-gcc` and `c99`, which nothing in the
  tree installs, and stops when it finds none and `BUILDCC` is unset.
- `--enable-avcodec` — and every other `--enable-` of a module with an outside library, from
  `--enable-archive` to `--enable-secret`, `--enable-srt` and `--enable-librist`: each module
  named, so that for most of them a missing port stops configure instead of producing a VLC that
  cannot open a format. Each library is a port in `depends`.
- `--enable-lirc` — infrared remote controls through [lirc](#port-lirc). Its check never stops
  configure: a missing library leaves the module out without a word, whatever the switch says.
  The checks behind `--enable-png` and `--enable-jpeg` do the same, and those behind
  `--enable-libass`, `--enable-taglib`, `--enable-fontconfig`, `--enable-fribidi` and
  `--enable-harfbuzz` print only a warning. For these the port in `depends` is what keeps the
  module or feature.
- `--enable-merge-ffmpeg` — build the `avformat` demuxer and muxer, the `avio` access and the
  `avparser` packetizer into the `avcodec` plugin instead of plugins of their own; `swscale` stays
  separate. Off by upstream's default.
- `--enable-wma-fixed` — the fixed-point WMA decoder module, off by upstream's default.
- `--enable-telx` — VLC's own Teletext decoder, the one built when zvbi is absent; zvbi is not a
  port.
- `--disable-live555` — and `dc1394`, `dv1394`, `linsys`, `dsm`, `smb2`, `shout`, `mpc`, `shine`,
  `dca`, `tremor`, `daala`, `openapv`, `bpg`, `x262`, `vpl`, `zvbi`, `aribsub`,
  `aribb25`, `kate`, `tiger`, `vdpau`, `caca`, `goom`, `projectm`, `vsxu`, `upnp`, `microdns`,
  `libcddb`, `aom` and `fluidlite`: modules whose library is not a port.
- `--disable-x26410b` — no separate 10-bit H.264 encoder module. Its library is
  [x264](#port-x264), which the `x264` module uses.
- `--disable-libplacebo` — VLC 3 accepts only libplacebo older than 6, and the
  [libplacebo](#port-libplacebo) port is newer.
- `--disable-postproc` — the `postproc` filter needs FFmpeg's `libpostproc`, which the
  [ffmpeg](#port-ffmpeg) port does not install.
- `--disable-opencv` — VLC 3 asks `pkg-config` for `opencv`; OpenCV 4, the
  [opencv](#port-opencv) port, installs `opencv4.pc`.
- `--disable-sdl-image` — VLC 3 takes SDL_image 1.2, not [sdl2-image](#port-sdl2-image).
- `--disable-sid` — VLC 3 takes libsidplay2, not [libsidplayfp](#port-libsidplayfp).
- `--disable-freerdp` — VLC 3's RDP access module asks `pkg-config` for `freerdp`, FreeRDP 1's
  name; the [freerdp](#port-freerdp) port installs its file under a name carrying the major
  version, such as `freerdp3.pc`.
- `--disable-vnc` — no VNC access module.
- `--disable-fdkaac` — the FDK AAC encoder's licence cannot be combined with GPL code.
- `--disable-gst-decode` — with `--disable-oss` and `--disable-sndio`: modules that duplicate
  ones the build has. avcodec decodes what the GStreamer decoder would, and PulseAudio and ALSA
  are the audio outputs, both carried to PipeWire.
- `--disable-aa` — no ASCII-art video output through [aalib](#port-aalib).
- `--disable-jack` — no JACK audio output module.
- `--disable-kwallet` — no KWallet keystore; passwords go to the Secret Service through
  `--enable-secret` and [libsecret](#port-libsecret).
- `--disable-skins2` — with `--disable-notify`: no skinnable interface and no libnotify popups;
  the Qt interface covers both.
- `--disable-update-check` — with `--disable-addonmanagermodules` and `--disable-chromecast`: no
  check against videolan.org for new versions, no add-on manager for videolan.org's catalogue,
  and no Chromecast output. Each needs the network to do anything.
- `--disable-decklink` — with `--disable-asdcp`, `--disable-realrtsp`, `--disable-omxil`,
  `--disable-crystalhd`, `--disable-mmal` and `--disable-evas`: no Blackmagic DeckLink SDI input,
  no Digital Cinema Package reader (asdcplib), no RealNetworks RTSP access, no OpenMAX IL or
  Broadcom Crystal HD decoders, no Raspberry Pi MMAL output and no EFL Evas output.
- `--disable-rpath` — hardcode no run-time library search path into the installed binaries.

## Patches
- `fribidi_allow_deprecated.patch` — removes `#define FRIBIDI_NO_DEPRECATED` before `#include <fribidi.h>` in the FreeType text renderer. With the define, `fribidi.h` leaves out FriBidi's compatibility declarations, which the renderer still calls.
- `musl-ioctl.patch` — in the V4L2 access module, declares the `v4l2_ioctl` function pointer as `int (*)(int, int, ...)`, the type of musl's `ioctl()`, instead of glibc's `unsigned long int` request. With glibc's type, assigning `ioctl` to it does not compile.

## Notes
The install hook writes `plugins.dat`, a cache of each plugin's size and modification time.
Packaging resets every modification time, so the installed files would not match the cache, and the
cache would differ between two builds; `build.sh` deletes it. kpkg writes it instead, once the
files are in place, by running `vlc-cache-gen` whenever a package touches `/usr/lib/vlc/plugins`
(see [Shared indexes](../../../../docs/kdos/05-developer/writing-ports.md#shared-indexes)). Without
the cache libvlc loads every plugin at start to learn what it does, the Qt 5 interface included, and
Elisa, a Qt 6 program that links libvlc, crashes inside it.

The browser-plugin directory `/usr/lib/mozilla` and the KDE 4 service menus in
`/usr/share/kde4` name programs KDOS does not have, and are removed.

`build.sh` replaces upstream's desktop entry. The Qt interface sets the window class `vlc`, which
`StartupWMClass` names. The entry has no `MimeType` line: other players' entries claim the
audio and video types, and the default for each type is chosen in the desktop's mimeapps table,
`/etc/xdg/kdos-mimeapps.list`. The icons are
upstream's, from `make install`.
