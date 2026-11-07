## About

libkdepim holds the widgets KDE's PIM applications share: the progress manager and the status-bar
progress widget that show every running mail and sync job, the SSL indicator, the
multiplying-line editor the composer's recipient list is built from, and a few line edits and
combo boxes. It also installs the D-Bus interface descriptions of the address book and
mail-transport services, for the PIM modules that call them.
[libksieve](#port-libksieve), [mailcommon](#port-mailcommon), [messagelib](#port-messagelib) and
Merkuro require it.

## Flags

- `-D BUILD_DESIGNERPLUGIN=OFF` — Build no Qt Designer plugin for the widgets. Upstream builds one
  by default, and the configure then requires Qt's `UiPlugin` module from
  [qt6-qttools](#port-qt6-qttools), which this port's `depends` does not name.
- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the translation catalogues under `po/`, which
  the package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

Qt D-Bus is looked up as optional and then marked required unless it is named in upstream's
`FORCE_NOT_REQUIRED_DEPENDENCIES`, which this recipe leaves empty; [qt6-qtbase](#port-qt6-qtbase)
provides it, and the two interface files under `/usr/share/dbus-1/interfaces` are installed with
it.
