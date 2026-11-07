## About

KDE Itinerary collects a trip's train and flight tickets, boarding passes, hotel and event
bookings into one timeline, read from PDFs, Apple Wallet passes, e-mails and barcodes by
[kitinerary](#port-kitinerary) and [kpkpass](#port-kpkpass). Around them it shows live departures
and alternative connections from [kpublictransport](#port-kpublictransport), station and airport
maps from [kosmindoormap](#port-kosmindoormap), opening hours from
[kopeninghours](#port-kopeninghours), the weather, and vaccination certificates from
[khealthcertificate](#port-khealthcertificate). It is a Kirigami application from KDE's
convergent set.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KHealthCertificate=ON` — Make the health-certificate manager's
  library a requirement. Upstream looks for it quietly and drops the manager without it.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_QuotientQt6=ON` — Make [libquotient](#port-libquotient) a
  requirement, for sharing a trip or a location into a Matrix room. Upstream treats it as
  optional.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Solid=ON` — With `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Crash=ON`,
  `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6FileMetaData=ON` and `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6KIO=ON`:
  make the four frameworks upstream asks for as optional components requirements. Solid drives
  the screen brightness while a barcode is shown, KCrash handles crashes, and
  [kfilemetadata](#port-kfilemetadata) and [kio](#port-kio) are what the port's metadata extractor
  and thumbnailer plugins for travel documents are built against; each is dropped without it.
- `-D CMAKE_DISABLE_FIND_PACKAGE_QMapLibre=ON` — Never look for QMapLibre, the vector-map
  renderer behind the trip map view. Its library, maplibre-native-qt, is not a port, and it is on
  the list of optional dependencies the KDE port defers, each pinned off so that porting it later
  changes this build on purpose rather than by being installed.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The program links `Qt6::CorePrivate`, Qt's private API, so it is rebuilt with every Qt release,
as every port that uses a private Qt header is.

Live departures, journey search, map tiles and the weather come from public network services when
the person opens those views; the timeline itself and every imported document stay on the
machine. LibIcal and the Selenium AT-SPI driver, which the configure also looks for, are used
only on Android and in the GUI tests, so neither changes this build.

The screen-brightness control talks to PowerDevil's `org.kde.Solid.PowerManagement`, which the
Plasma session provides; a Gear port names no session port in `depends`. Sending a ticket to a
phone goes through KDE Connect's D-Bus service when it is running.

The desktop entry claims PDF, HTML, JSON, calendar and `geo:` links among its types, as upstream
writes it. Which program opens each by default is set in the desktop's `mimeapps.list`, not here.
