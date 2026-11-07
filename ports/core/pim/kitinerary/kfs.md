## About

KItinerary extracts travel bookings from the documents they arrive in: confirmation mails in HTML
or plain text, PDF tickets and boarding passes, Apple Wallet passes, iCalendar files, and the
barcodes on railway and airline tickets (UIC 918.3 and 918.9, the ERA FCB and SSB layouts, IATA
boarding passes, VDV e-tickets). It turns them into schema.org reservations with its own airport,
station and country data, and merges what several documents say about one trip. KMail's
itinerary formatter in kdepim-addons and KDE Itinerary are built on it; `kitinerary-extractor`,
under `/usr/lib/libexec/kf6`, runs an extraction from the command line.

## Flags

- `-D BUILD_TOOLS=OFF` — Build none of upstream's development tools under `src/tools`, which
  install nothing.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Mime=ON` — Take KMime from its Frameworks package. The
  configure asks for `KF6Mime` first and falls back to the PIM-era `KPim6Mime`; required, the
  fallback can never be taken. [kmime](#port-kmime) installs `KF6Mime`.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_libphonenumber=ON` — Make
  [libphonenumber](#port-libphonenumber) a requirement. Upstream finds it quietly, and without it
  a phone number in a booking is kept as text, neither normalised nor placed in a country.
- `-D CMAKE_DISABLE_FIND_PACKAGE_OsmTools=ON` — Never look for OsmTools, which only regenerates
  the airport database from OpenStreetMap; the tarball carries the generated data.
- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations, which kpkg's
  English-only rule would remove from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

PDF documents are read through [poppler](#port-poppler)'s private `OutputDev` headers, and the
configure stops with `Poppler was not build with ENABLE_UNSTABLE_API_ABI_HEADERS!` unless
poppler installs them; the poppler port builds with `ENABLE_UNSTABLE_API_ABI_HEADERS=ON`. Those
headers carry no stable ABI, so a poppler bump rebuilds this port. Barcodes are decoded by
[zxing-cpp](#port-zxing-cpp), Apple Wallet passes are opened through [kpkpass](#port-kpkpass),
HTML is parsed by [libxml2](#port-libxml2), and the signatures of European railway tickets are
checked with [openssl](#port-openssl); the configure requires all of them.

The bundled airport, station and country tables are generated from Wikidata and OpenStreetMap
and are licensed ODbL-1.0. Extraction reaches no network.
