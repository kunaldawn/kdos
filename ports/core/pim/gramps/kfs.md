## About

Gramps is a genealogy program: people, families, events, places and sources in a family tree
database, with charts, reports and GEDCOM import and export. It is a Python program on GTK 3
through [python3-gobject](#port-python3-gobject).

## Notes

The Geography views (places, events and people on a map) load the OsmGpsMap widget through GObject
introspection, from [osm-gps-map](#port-osm-gps-map). Without its typelib Gramps disables those
views and shows an "OsmGpsMap module not loaded" dialog at every start, addressed to the package
names of other distributions. Map tiles are downloaded from OpenStreetMap when a Geography view is
opened.

Gramps sorts, groups and compares names through [python3-pyicu](#port-python3-pyicu), the
ICU bindings, so that names in the locale's own alphabet order and accented letters sort beside
their base letters. Without the `icu` module it falls back to Python's locale collation and prints
"ICU not loaded … Localization will be impaired" at every start.
