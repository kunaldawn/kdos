## About

KContacts is the KDE Frameworks data model of an address book entry: names, e-mail and postal
addresses, phone numbers, pictures and the rest of vCard 2.1, 3.0 and 4.0, with parsers and
writers for the format and a QML plugin for Qt Quick programs. [kpeople](#port-kpeople) and the
contacts parts of KDE PIM ([kaddressbook](#port-kaddressbook), [merkuro](#port-merkuro)) are built
on it.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Build none of the gettext catalogues under `po/`, which
  `ki18n_install()` would compile into `.mo` files. The strings in the source are English, and the
  package-wide English-only rule (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
  would remove every catalogue in another language.

## Notes

The QML plugin, on by default (`USE_QML`), requires Qt Qml from
[qt6-qtdeclarative](#port-qt6-qtdeclarative).
