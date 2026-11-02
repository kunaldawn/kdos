## About

KOSMIndoorMap draws the inside of a building from OpenStreetMap data: the floors of a railway
station or an airport, platforms, shops, toilets, lifts and the paths between them. It fetches
and caches OSM tiles, styles them with MapCSS, renders them with QPainter, and offers the result
to QML as `org.kde.kosmindoormap`, with an element-information view and opening hours from
[kopeninghours](#port-kopeninghours). Itinerary and Kongress show station and venue maps through
it.

## Flags

- `-D USE_KPUBLICTRANSPORT=ON` — Makes [kpublictransport](#port-kpublictransport) a requirement
  instead of a recommendation. With it the QML module `org.kde.kosmindoormap.kpublictransport` is
  built, which overlays real-time departures, platforms and rental vehicles on the map, and which
  Itinerary imports; without it a missing kpublictransport would drop the module with only a
  warning.
- `-D BUILD_STANDALONE_APP=OFF` — No `kosmindoormap-app`, the demonstration and test viewer.
  Upstream's default is the same; the switch is pinned so the package never ships it.
- `-D CMAKE_DISABLE_FIND_PACKAGE_recastnavigation=ON` — No indoor routing. Recast Navigation is
  not a port; it is one of the optional enrichments KDOS defers, and pinning it off keeps a later
  port of it from changing this build without notice. Without it `KOSMIndoorRouting` is
  built with `HAVE_RECAST=0`, has no navigation mesh, and the map offers no route between two
  points.
- `-D CMAKE_DISABLE_FIND_PACKAGE_Protobuf=ON` — No OSM PBF reader. Upstream builds it as a static
  plugin that only the uninstalled command-line tools and the demonstration app link.
- `-D CMAKE_DISABLE_FIND_PACKAGE_OsmTools=ON` — No `osmconvert` or `osmfilter` search. They only
  regenerate data tables the tarball ships already generated.
- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the translation catalogues under `po/`, which
  the package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

[flex](#port-flex) and [bison](#port-bison) generate the MapCSS parser at build time; the
configure requires both. It also requires [kopeninghours](#port-kopeninghours),
[kirigami-addons](#port-kirigami-addons) 1.6.0 or newer and Qt's private GUI headers, which
[qt6-qtbase](#port-qt6-qtbase) installs.

The "edit in OSM editor" action looks up JOSM's desktop entry through KService. Upstream finds
KService and KCoreAddons with `OPTIONAL_COMPONENTS`, which no switch can turn into a requirement,
so [kservice](#port-kservice) and [kcoreaddons](#port-kcoreaddons) are in `depends` and the
configure output is checked for both.

Map data comes from `maps.kde.org` tile servers at run time, fetched only when an application
shows a map. The QML views import Kirigami, Kirigami Addons, `org.kde.kpublictransport` and
`org.kde.contacts` (an address in the element information), which is why
[kirigami](#port-kirigami) and [kcontacts](#port-kcontacts) are in `depends`.
