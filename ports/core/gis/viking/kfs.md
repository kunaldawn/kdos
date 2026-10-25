## About

Viking manages GPS data on maps: it reads, edits and writes tracks, waypoints and routes in GPX
and KML itself, draws them over OpenStreetMap and other tile layers, and follows a live position
from [gpsd](#port-gpsd). GeoJSON and the formats GPSBabel converts go through external programs
it runs from the path, `togeojson`, `togpx` and `gpsbabel`, and none of the three is a port. The port builds its GTK 3 interface.
Maps work offline from MBTiles files and from the tile cache it keeps of what it has downloaded.

## Flags

- `--disable-gtk2` — Builds the GTK 3 interface; upstream's default, named.
- `--disable-deprecations` — Upstream's default, named: no extra `-D…_DISABLE_DEPRECATED`
  definitions that make the build warn about deprecated GLib and GTK calls.
- `--disable-bing`, `--disable-google`, `--disable-terraserver`, `--disable-expedia`,
  `--disable-bluemarble`, `--disable-geonames`, `--disable-geocaches`, `--disable-dem24k` —
  Leave out map, search and download sources that are online services or need an account: Bing
  and Google maps, Terraserver and Expedia imagery, NASA's Blue Marble, GeoNames search, the
  geocache acquire tool, and the USGS 24k DEM download, which also needs a `dem24k.pl` script on
  the path. Bing, Google, Blue Marble and GeoNames are on by default upstream.
- `--enable-openstreetmap` — The OpenStreetMap tile layers and search, upstream's default.
- `--disable-osm-auth`, `--disable-oauth` — No authenticated OpenStreetMap access for uploading
  traces, and no OAuth 1.0a support, which needs liboauth, not a port. Both are off by default
  upstream.
- `--enable-geoclue` — Asks [geoclue](#port-geoclue) for the machine's position.
- `--enable-geotag` — Geotags photographs and reads their positions, through
  [gexiv2](#port-gexiv2).
- `--enable-realtime-gps-tracking` — Follows a live position from gpsd; configure stops if
  `gps.h` or libgps is missing.
- `--enable-bzip2`, `--enable-magic`, `--enable-mbtiles`, `--enable-zip`, `--enable-xz` — Opening
  bzip2-, zip- and xz-compressed files, file-type detection through libmagic from
  [file](#port-file), and MBTiles map files through SQLite.
- `--enable-nettle` — MD5 through [nettle](#port-nettle), which Viking uses only to name the
  thumbnail files of geotagged images; `nettle4.patch` is what lets it build.
- `--enable-nova` — Sunrise, sunset and moon-phase data through [libnova](#port-libnova).
- `--disable-mapnik` — No Mapnik rendering layer, which is on by default; Mapnik is not a port.
- `-g0` — Viking's `src/Makefile.am` and `src/icons/Makefile.am` put `-g` in `AM_CFLAGS`, which
  automake places before `CFLAGS`; `-g0` at the end of `CFLAGS` cancels it (see
  [What kpkg does around build.sh](../../../../docs/kdos/05-developer/writing-ports.md#what-kpkg-does-around-buildsh)).

Each `--enable-` above is also upstream's default, named. Each that needs a library stops
configure with an error when the library is missing, except zip support, which configure turns off
with only a warning when pkg-config finds no libzip of the version it asks for.

## Patches

- `nettle4.patch` — Moves Viking's MD5 from nettle's `md5-compat.h` to nettle's own interface, `nettle/md5.h`: `struct md5_ctx` with `md5_init()`, `md5_update()` and `md5_digest()` in `src/md5_hash.c`, and the same header in the check in the generated `configure`. The tree's nettle installs no `md5-compat.h`, and configure stops at that check without the patch.

## Notes

The manual page, `viking.1`, is generated from the DocBook help with `xsltproc --nonet` and the
DocBook stylesheets' `http://docbook.sourceforge.net/…` URL, which resolves offline through the
XML catalog of [docbook-xsl](#port-docbook-xsl). Both that and [libxslt](#port-libxslt) are in
`depends` for the page.

Upstream's install does not put `viking-128.png` or `org.viking.Viking.appdata.xml` in place;
they are only in its distribution archive, and `build.sh` installs them. The desktop entry is
written in `build.sh` with `StartupWMClass=viking`, the Wayland app_id GTK 3 takes from the
program's name.
