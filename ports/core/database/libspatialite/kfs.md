## About
SpatiaLite adds spatial SQL to SQLite: geometry types, spatial indexes, coordinate
transformations and the OGC Simple Features functions, so an SQLite file can hold and query maps.
It comes as a library, `libspatialite`, and as `mod_spatialite`, the module any SQLite program
loads with `SELECT load_extension('mod_spatialite')`. [gdal](#port-gdal) and [qgis](#port-qgis)
are built on it.

## Flags
- `--enable-mathsql`, `--enable-proj`, `--enable-iconv`, `--enable-freexl=yes`, `--enable-epsg`,
  `--enable-geos`, `--enable-gcp`, `--enable-geosadvanced`, `--enable-geosreentrant`,
  `--enable-rttopo`, `--enable-libxml2`, `--enable-minizip`, `--enable-geopackage` — Every
  feature, each upstream's default written out: SQL math functions, coordinate transformations with
  [proj](#port-proj), character set conversion, reading spreadsheets with [freexl](#port-freexl),
  the full EPSG dataset of coordinate systems, geometry operations with [geos](#port-geos),
  with its advanced functions and its thread-safe interface, ground control points from GRASS GIS, topology with
  [librttopo](#port-librttopo), XML documents with [libxml2](#port-libxml2), zip archives with
  [minizip](#port-minizip), and GeoPackage support. Each library is required once its feature is
  on: configure stops when it is missing.
- `--enable-examples=no` — Build none of the example programs.

## Patches
- `libxml2-no-http.patch` — Compiles the call to `xmlNanoHTTPCleanup()` in `reset_wfs_http_connection()` only when libxml2 defines `LIBXML_HTTP_STUBS_ENABLED`. The tree's [libxml2](#port-libxml2) does not define `LIBXML_HTTP_STUBS_ENABLED`, and its `nanohttp.h` then declares none of the `xmlNanoHTTP*` functions, so the unguarded call does not compile; with the patch the function does nothing there.

## Notes
`build.sh` ends by checking that `mod_spatialite.so` is in the package, and fails the build when
it is not, so the port never ships the library without the loadable module.
