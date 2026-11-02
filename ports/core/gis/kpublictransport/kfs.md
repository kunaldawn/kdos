## About

KPublicTransport asks public-transport operators for journeys, departures, arrivals and vehicle
layouts, through the backends Europe's operators run: HAFAS, EFA, Navitia, OpenTripPlanner,
MOTIS and the rest, plus GBFS feeds for rental bikes and scooters. It merges what several
backends answer, chooses backends by the area a query covers, and offers it all to C++ and to QML
(`org.kde.kpublictransport` and its `ui` module). A second library, `KPublicTransportOnboard`,
reads the on-board Wi-Fi portals of trains and buses for position and next stops.
[ktrip](#port-ktrip), Itinerary and [kosmindoormap](#port-kosmindoormap) are built on it.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6NetworkManagerQt=ON` — Requires
  [networkmanager-qt](#port-networkmanager-qt). It is the only way the onboard library learns which
  Wi-Fi network the machine is on, outside Android; without it upstream builds a stand-in that
  never reports one, so a train's Wi-Fi is never recognised.
- `-D CMAKE_DISABLE_FIND_PACKAGE_Protobuf=ON` — No OSM PBF reader. With Protobuf found, upstream
  builds it as a static plugin of the OSM library that nothing installed links, so it only adds
  build time and a dependency.
- `-D CMAKE_DISABLE_FIND_PACKAGE_PolyClipping=ON` — No coverage-area processing. It serves only
  `transport-api-sync`, the maintainer tool that refreshes the backend list from the Transport API
  Repository, which is not installed.
- `-D CMAKE_DISABLE_FIND_PACKAGE_OsmTools=ON` — No `osmconvert` or `osmfilter` search. They
  regenerate the line-metadata tables, which the tarball ships already generated.
- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the translation catalogues under `po/`, which
  the package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

Every query is a request to the operators' public servers; nothing is fetched until an
application asks. The QML pages import Kirigami, Kirigami Addons' form cards and delegates,
`org.kde.kitemmodels`, `org.kde.coreaddons`, `org.kde.i18n.localeData`, Qt Location and Qt
Positioning at run time, which is why [kirigami](#port-kirigami),
[kitemmodels](#port-kitemmodels), [kcoreaddons](#port-kcoreaddons),
[qt6-qtlocation](#port-qt6-qtlocation) and [qt6-qtpositioning](#port-qt6-qtpositioning) are in
`depends` beside the build's own requirements. The configure requires
[kirigami-addons](#port-kirigami-addons) 1.6.0 or newer.
