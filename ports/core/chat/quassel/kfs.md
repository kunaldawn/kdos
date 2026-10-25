## About
[Quassel IRC]{.idx} is an IRC client split in two: a core that stays connected to the networks
and keeps the backlog, and a client that attaches to the core from wherever the user is. The port
builds all three programs upstream offers: `quasselcore`, the core on its own; `quasselclient`,
which connects to a core; and `quassel`, the two in one process for a single machine. It is a Qt 5
application.

## Flags
- `-D USE_CCACHE=OFF` — do not wrap the compiler in ccache, which upstream does by default when it
  finds it.
- `-D WANT_CORE=ON` — build the core, and with `-D WANT_QTCLIENT=ON` and `-D WANT_MONO=ON` the
  client-only and the monolithic programs. All three are upstream's defaults, named.
- `-D WITH_KDE=OFF` — no integration with the KDE Frameworks runtime; upstream's default, named.
  Spell checking, which that integration would otherwise bring, comes from Sonnet on its own
  (below).
- `-D WITH_BUNDLED_ICONS=ON` — install the icons Quassel needs from the Breeze and Breeze Dark
  themes, which it carries under `3rdparty/icons`, into its own data directory,
  `/usr/share/quassel/icons`, where they cannot conflict with a system theme; without them upstream
  warns that a Breeze or Oxygen theme must be installed. It is upstream's default, and
  `-D WITH_OXYGEN_ICONS=OFF` leaves out the Oxygen set, also by default.
- `-D WITH_WEBENGINE=OFF` — no previews of links in the chat view, and with `-D WITH_WEBKIT=OFF`
  not through the legacy WebKit either. Previews need Qt 5's WebEngine or WebKit module, and the
  tree carries neither. WebEngine is on by default upstream and used when found; WebKit is off.
- `-D WITH_LDAP=OFF` — no LDAP sign-in for core users. Upstream's default is on, used if the
  library is present, and the tree's [openldap](#port-openldap) links OpenSSL 4 while Quassel's
  Qt 5 links OpenSSL 3; one process would load both.
- `-D EMBED_DATA=OFF` — install the icons and other data as files instead of compiling them into
  the binaries; upstream's default on Linux, named.
- `-D CMAKE_DISABLE_FIND_PACKAGE_LibsnoreQt5=ON` — skip the search for the snorenotify
  notification framework, which the tree does not carry; notifications go through D-Bus.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Qca-qt5=ON` — make a missing [qca-qt5](#port-qca-qt5) a configure
  error. Upstream finds it quietly and marks it "Required for encryption support": the message
  encryption keyed per channel or nick (`/setkey`, `/keyx`), which would otherwise drop out without a word.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Qt5Multimedia=ON` — likewise for Qt Multimedia, which plays the
  audio notifications.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Qt5DBus=ON` — likewise for Qt D-Bus, needed for D-Bus
  notifications and the tray icon.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF5Sonnet=ON` — likewise for [sonnet-qt5](#port-sonnet-qt5), the
  spell checker in the input line.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_dbusmenu-qt5=ON` — likewise for
  [libdbusmenu-qt5](#port-libdbusmenu-qt5), which gives the D-Bus tray icon its context menu.

## Notes
After the install, `build.sh` reads the dynamic section of every program and of the
`libquassel-*` libraries, and fails the build if any of them needs `libldap` or an OpenSSL 4
library (`libssl.so.4`, `libcrypto.so.4`). Qt 5 is built against the
[openssl3](#port-openssl3) slot, and musl's loader has one symbol namespace, so with both
`libcrypto` sonames in one process each would resolve the other's calls; see
[A second version beside the first](../../../../docs/kdos/05-developer/writing-ports.md#a-second-version-beside-the-first).

`build.sh` writes the two desktop entries upstream installs, `quassel.desktop` and
`quasselclient.desktop`, over upstream's, each with its own `StartupWMClass`.
