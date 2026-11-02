## About

kpeoplevcard is a data source plugin for [kpeople](#port-kpeople): it presents every vCard file
under `~/.local/share/kpeoplevcard/` as a contact, so applications that list people through
KPeople show them without an address-book server. New contacts saved through it land in
`~/.local/share/kpeoplevcard/own/`. Plasma Phonebook keeps its contacts this way. The plugin
installs into `kpeople/datasource/` under Qt's plugin directory, where KPeople loads it.

The port builds a commit of upstream's `master`, named `0.1.git20241202` after its date. The only
release, 0.1, builds against Qt 5 and KDE Frameworks 5, which KDOS does not have; `master` builds
against either and carries the version 0.2 that has never been tagged. `_commit` holds the full
commit id.

## Flags

- `-D BUILD_WITH_QT6=ON` — Build against Qt 6 and KDE Frameworks 6. The project asks ECM for a
  version older than Frameworks 6, so ECM's `QtVersionOption` would otherwise pick Qt 5 and KF5.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

Each file is parsed with [kcontacts](#port-kcontacts). The plugin watches the directory, so a
vCard dropped there appears in a running application.
