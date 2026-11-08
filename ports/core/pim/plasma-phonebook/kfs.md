## About

Plasma Phonebook is the Plasma Mobile contact book: it lists, searches, adds and edits contacts,
and from a contact starts a call, a message or a chat. It reads contacts through
[kpeople](#port-kpeople) and edits vCards through [kcontacts](#port-kcontacts). The port also
installs a KPeople action plugin, so every KPeople client offers the same call and message
actions on a contact.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

KPeople shows what its data-source plugins provide. The contacts here come from the vCard plugin
[kpeople](#port-kpeople) itself installs, reading and writing vCards under
`~/.local/share/kpeoplevcard`.

A number opens as a `tel:` or `sms:` link; KDOS gives `tel:` to KDE Connect, which places the
call on a paired phone. `qml/Header.qml` and `qml/RoundImage.qml` import the Qt 5 module
`QtGraphicalEffects`, which does not exist in Qt 6; no other file loads either of them, so the
pages that are shown never ask for it. [qt6-qt5compat](#port-qt6-qt5compat) provides the
`Qt5Compat.GraphicalEffects` module the contact and add-contact pages import.

`KAboutData` sets the desktop file name `org.kde.phonebook`, which is the name of upstream's
entry, so the Wayland app_id matches it.
