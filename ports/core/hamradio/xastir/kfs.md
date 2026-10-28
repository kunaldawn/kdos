## About

Xastir is an APRS client: it maps stations, objects, weather and messages received from a TNC,
from [direwolf](#port-direwolf) or from the APRS-IS network, and sends position reports and
messages. It is a Motif program, so it runs under Xwayland
([principles](../../../../docs/kdos/01-philosophy/principles.md#no-xorg-server-and-one-carve-out)).

## Flags

- `--with-libcurl` — Fetch online maps and data through [curl](#port-curl).
- `--with-nominatim` — Geocode addresses through the Nominatim service; it needs curl and
  [cjson](#port-cjson).
- `--with-libproj` — Map projections through [proj](#port-proj).
- `--with-shapelib` — Vector maps (Shapefiles and dbfawk) through [shapelib](#port-shapelib).
- `--without-ax25` — Leave out the kernel AX.25 socket interface. The [linux](#port-linux)
  source tree carries no AX.25 protocol (there is no `net/ax25/`), so there are no AX.25
  sockets. A TNC is reached through the KISS serial and network interfaces, which is also how
  direwolf connects.
- `--with-gpsman` — Download from a GPS receiver through [gpsman](#port-gpsman).
- `--without-festival` — No speech output: Festival is not a port.
- `--with-geotiff` — GeoTIFF maps through [libgeotiff](#port-libgeotiff) and libtiff.
- `--with-map-cache` — Cache downloaded map tiles in Berkeley DB ([db](#port-db)).
- `--with-graphicsmagick` — Draw image maps (tiled OpenStreetMap and georeferenced pictures)
  through [graphicsmagick](#port-graphicsmagick). `--without-imagemagick` keeps the configure
  from falling back to ImageMagick when GraphicsMagick is not found; it refuses ImageMagick 7 as
  too new in any case.
- `--without-postgis` — No PostgreSQL/PostGIS database back end; `--without-mysql` likewise
  leaves out MySQL.
- `--without-libgc` — Do not link the Boehm garbage collector (libgc); this is the configure's
  default.

## Notes

The tag archive carries no `configure` script, so `build.sh` starts with `autoreconf -fi`.

Most of the `--with-` switches above do not fail the configure when their library is missing:
they quietly build an Xastir without the feature. The `grep -q` lines after the configure read
`config.h` and stop the build unless shapelib, proj, curl, cJSON, GraphicsMagick, GPSMan, GeoTIFF
and the map cache were all found.

Xastir ships no desktop entry, so `build.sh` writes one and rasterises `symbols/icon.svg` into
hicolor PNGs. Its Xt application class is `Xastir`, which is the X11 window class Xwayland
reports and so the entry's `StartupWMClass`.
