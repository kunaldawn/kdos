## About

KTorrent is KDE's BitTorrent client. The protocol, the DHT, the trackers and the disk work are
[libktorrent](#port-libktorrent); KTorrent is the window around it and a set of plugins: torrent
search and RSS feeds in a web view, an IP filter, transfer statistics, a bandwidth scheduler, UPnP
port mapping, local peer discovery over DNS-SD, a scanner that adds torrents dropped in a folder,
a media player for files still downloading, and shutdown when all transfers finish.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — The handbook, through
  [kdoctools](#port-kdoctools), which upstream asks for as an optional component.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6TextWidgets=ON` — Each of the plugins below is built only when
  its library is found, and upstream looks for every one as optional, so a missing library drops
  a plugin with no error; required, it stops the configure. This one is the IP filter plugin,
  through [ktextwidgets](#port-ktextwidgets).
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Plotting=ON` — The statistics plugin's graphs, through
  [kplotting](#port-kplotting).
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Syndication=ON` — With
  `-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6Core5Compat=ON`: the RSS feed plugin, through
  [syndication](#port-syndication), which also links Qt 5 compatibility classes from
  [qt6-qt5compat](#port-qt6-qt5compat).
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6WebEngineWidgets=ON` — The search plugin and the feed
  plugin's article view, through [qt6-qtwebengine](#port-qt6-qtwebengine).
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DNSSD=ON` — The Zeroconf plugin, which finds peers for the
  same torrent on the local network through [kdnssd](#port-kdnssd).
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Completion=ON` — The UPnP plugin, which asks the router to
  forward the listening ports; its dialog uses [kcompletion](#port-kcompletion).
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6Multimedia=ON` — With
  `-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6MultimediaWidgets=ON` and
  `-D CMAKE_REQUIRE_FIND_PACKAGE_Taglib=ON`: the media player plugin, which plays audio and video
  files of a torrent through [qt6-qtmultimedia](#port-qt6-qtmultimedia) and reads their tags with
  [taglib](#port-taglib).
- `-D BUILD_WITH_GEOIP=OFF` — No peer country column. With it on, the torrent details plugin
  links [libmaxminddb](#port-libmaxminddb) and, the first time it starts without a database,
  downloads DB-IP's country database from download.db-ip.com, a request to a third party that
  nobody asked for; off, the plugin builds without the lookup.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The torrent details panel (status, files, peers, trackers, web seeds) is the infowidget plugin,
which builds on musl as it is. The shutdown plugin asks the session over D-Bus to log out, suspend
or power off (`org.kde.LogoutPrompt`, `org.freedesktop.PowerManagement`), which only the Plasma
session answers. The configure also looks for KF6ItemViews, for a scripting plugin this release
does not have; nothing reads the result, so the lookup is left unpinned.

KTorrent listens for peers on TCP and UDP ports of its own; incoming connections need them opened
in the firewall, and UPnP opens them on a router only.
