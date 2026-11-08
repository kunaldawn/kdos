## About

svgpart is a KParts component that displays SVG images with Qt SVG. It has no window or launcher
of its own: Konqueror, Ark's preview pane and any other program that embeds parts by MIME type load
it to show an `image/svg+xml` file in place.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`, which kpkg's
  English-only rule would remove from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Notes

The part installs as `kf6/parts/svgpart.so` under Qt's plugin directory, with its metainfo file; the
part's JSON metadata, compiled into the plugin, carries the MIME type it serves.
[qt6-qtsvg](#port-qt6-qtsvg) supplies both `Qt6::Svg` and `Qt6::SvgWidgets`, which the configure
requires.
