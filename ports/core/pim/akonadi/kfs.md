## About

Akonadi is the storage service under the KDE PIM applications. `akonadiserver` keeps every mail
folder, address book and calendar in one database and serves it to clients over a private
socket; `akonadi_control` starts the server and the agents (the resources that talk to IMAP,
CalDAV or a local file, and the indexers) and restarts them when they fail. The port also carries
the client libraries every PIM program links (`KPim6AkonadiCore`, `…Widgets`, `…AgentBase`,
`…Xml`, the QML module `org.kde.akonadi`) and `akonadictl`, which starts, stops and checks the
server from a shell.

## Flags

- `-Wl,-z,stack-size=1048576` — Give every thread of the programs this port links a 1 MiB stack.
  The server walks mail threads recursively, and musl's default thread stack, 128 KiB, overflows
  on a long thread and crashes `akonadiserver`. musl takes the size from the executable's
  `PT_GNU_STACK` header, which this linker flag writes; Alpine builds the same port with it.
- `-D DATABASE_BACKEND=SQLITE` — Store the data in SQLite. It is upstream's default, and the one
  backend KDOS can run: the server reaches SQLite through Qt's `QSQLITE` driver, which
  [qt6-qtbase](#port-qt6-qtbase) builds against the system [sqlite](#port-sqlite), and there is no
  MariaDB or MySQL port. Passing it keeps a change of upstream's default from switching the
  backend without notice.
- `-D INSTALL_APPARMOR=OFF` — Install no AppArmor profiles. Upstream's default writes four under
  `/etc/apparmor.d`, for a security module KDOS does not use.
- `-D BUILD_PYTHON_BINDINGS=OFF` — Build no Python bindings. Upstream's default is on, and the
  configure then requires Shiboken6 and PySide6, which are not ports.
- `-D BUILD_DESIGNERPLUGIN=OFF` — No Qt Designer plugin for the Akonadi widgets. It is a tool for
  people writing PIM programs, and Qt Designer is not in `depends`.
- `-D BUILD_TOOLS=OFF` — Build none of upstream's test tools: `akonaditest`, the `knut` test
  resource and `akonadi_test_searchplugin`. On, they install with the rest, and the test search
  plugin lands in the directory the server loads its search plugins from.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_LibXml2=ON` — Make [libxml2](#port-libxml2) a requirement.
  Upstream finds it as optional and builds `KPim6AkonadiXml`, the library that reads and writes
  Akonadi's XML collection format, without schema validation otherwise.
- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations, which kpkg's
  English-only rule would remove from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The install puts a systemd user unit, `akonadi_control.service`, under `/usr/lib/systemd`;
`build.sh` removes the directory, since KDOS has no systemd (see
[No systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd)). The server is started
without it: the D-Bus service file `org.freedesktop.Akonadi.Control.service` names
`akonadi_control` in its `Exec=` line, so the session bus starts it the first time a PIM program
asks for it. Nothing in the default session asks, because Thunderbird is the default mail,
calendar and contacts program, so no Akonadi server runs until somebody opens KMail, KOrganizer,
KAddressBook or Merkuro.

The two files the install puts under `/etc/xdg/akonadi`, `mysql-global.conf` and
`mysql-global-mobile.conf`, are the server's MySQL settings. They ship with every backend and
are read only when the backend is MySQL, so on KDOS nothing reads them.

`xsltproc` from [libxslt](#port-libxslt) generates the server's database schema code and D-Bus
interfaces at build time, and the configure stops without it. The MIME types the server stores
are registered through [shared-mime-info](#port-shared-mime-info).

The two desktop entries, `org.kde.akonadi.desktop` and `org.kde.akonadi.configdialog.desktop`,
are both `NoDisplay=true`: the first names the PIM suite for notifications, the second is the
agent configuration dialog the applications open. The QML module imports
[kirigami](#port-kirigami) and [kirigami-addons](#port-kirigami-addons) at run time, which is why
both are in `depends`.

musl has no `malloc_trim()`; the configure checks for it with `check_symbol_exists` and the
server leaves out the call that returns freed memory to the system after a large sync.
