## About

osm-gps-map is a GTK 3 widget that draws a slippy map from OpenStreetMap and other tile servers,
caches the tiles on disk, and overlays tracks, images, polygons and markers. Its GObject
introspection typelib, `OsmGpsMap-1.0`, is how [gramps](#port-gramps)'s Geography views reach it
from Python. Tiles are fetched through [libsoup3](#port-libsoup3).

## Flags

- `--enable-introspection=yes` — Builds the typelib and fails configure when
  [gobject-introspection](#port-gobject-introspection) is missing; upstream builds it only when
  it is found.
- `--disable-gtk-doc` — No API reference; the port installs no developer documentation.

## Notes

The `test` line at the end stops the build if the typelib is missing, which would leave Gramps
showing its "OsmGpsMap module not loaded" warning on every start.
