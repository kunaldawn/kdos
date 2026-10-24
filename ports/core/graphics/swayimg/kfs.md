## About

swayimg is an image viewer that draws its own window straight on Wayland, or on the console
through DRM with no compositor at all. It shows single images, a gallery of
thumbnails and slideshows, and is configured and scripted in Lua. It is one of the
[applications](../../../../docs/kdos/02-user-guide/applications.md) here that need no toolkit. Its
desktop entry is hidden from the menu: it is a handler that pictures are opened in by type.

## Flags

- `-Dversion=$version` — The version the program reports. Left at its default, `0.0.0`, the
  build asks `git describe`, and a release archive is no repository, so the version would stay
  `0.0.0`.
- `-Dwayland=enabled`, `-Ddrm=enabled` — Both front ends: a Wayland window, and drawing straight
  on the console through [libdrm](#port-libdrm) when no compositor runs.
- `-Dcompositor=disabled` — Leaves out the integration with Sway's and Hyprland's IPC sockets,
  through which swayimg places its window over the focused one. Those are the only two
  compositors it speaks to, and `kdos-comp` is neither.
- `-Dliblua=luajit` — Embeds [luajit](#port-luajit) as the Lua engine; upstream's default, named.
- `-Dexr=enabled`, `-Dgif=enabled`, `-Dheif=enabled`, `-Davif=enabled`, `-Djpeg=enabled`,
  `-Djp2=enabled`, `-Djxl=enabled`, `-Dpng=enabled`, `-Dsvg=enabled`, `-Dtiff=enabled`,
  `-Dsixel=enabled`, `-Draw=enabled`, `-Dwebp=enabled`, `-Dvideo=enabled`, `-Dexif=enabled` —
  Every format whose reader needs a library (BMP, PNM, QOI, TGA and a few others are built in),
  each `auto` by default and named `enabled`, so a missing library
  stops `meson setup` instead of building a viewer that quietly cannot open that type. `raw` is
  camera raw files through [libraw](#port-libraw), `video` a storyboard of a clip's frames through
  [ffmpeg](#port-ffmpeg), `sixel` the Sixel image format through [libsixel](#port-libsixel), and
  `exif` the orientation and metadata of photographs through [exiv2](#port-exiv2).
- `-Dbash=enabled`, `-Dzsh=enabled` — Install the bash and zsh completions; both are `auto`
  upstream.
- `-Ddesktop=true`, `-Dman=true`, `-Dluameta=true` — Upstream's defaults, named: the desktop
  entry with its hicolor icons, the manual page, and the Lua definitions file with an example
  configuration under `/usr/share/swayimg`.
- `-Ddoc=false` — Leaves out the documentation in Markdown, which is installed by default.
- `-Dtests=disabled` — No unit tests; upstream's default, named.

## Notes

Upstream's desktop entry is replaced by one that adds `StartupWMClass=swayimg`, the app_id the
window carries unless its configuration names another. It stays `NoDisplay=true`, as upstream
ships it, and keeps upstream's `MimeType=` list.
