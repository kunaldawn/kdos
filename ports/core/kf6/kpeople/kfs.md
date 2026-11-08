## About

KPeople merges the contacts that several sources know about one person into a single entry: an
address book, a chat account and a phone's contact list each contribute, through data source
plugins, and the merged person is offered as an item model, a details widget and a QML module,
`org.kde.people`. It keeps the merges in an SQLite database through Qt Sql.
[kdeconnect](#port-kdeconnect) contributes the phone's contacts, and
[plasma-phonebook](#port-plasma-phonebook) and [calindori](#port-calindori) are built on it.

## Flags

- `-D ENABLE_EXAMPLES=OFF` — Build none of the example programs. Upstream's default is on, and the
  examples would be built for nothing, since none is installed.
- `-D KF_SKIP_PO_PROCESSING=ON` — Build none of the gettext catalogues under `po/`, which
  `ki18n_install()` would compile into `.mo` files. The strings in the source are English, and the
  package-wide English-only rule (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
  would remove every catalogue in another language.

## Notes

The package installs one data source of its own, `kpeople/datasource/KPeopleVCard.so`: every vCard
file under `~/.local/share/kpeoplevcard/` is a contact, and a new one is saved under `own/` there.
The plugin watches the directory, so a vCard dropped in appears in a running application. It is the
source the standalone kpeoplevcard project provides, so a port of that project would conflict with
this one on that path.

The configure requires Qt Qml and Qt Quick for the QML module, from
[qt6-qtdeclarative](#port-qt6-qtdeclarative), and Qt Sql, which [qt6-qtbase](#port-qt6-qtbase)
carries with its SQLite driver.
