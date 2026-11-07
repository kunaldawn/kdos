## About

MBoxImporter copies the messages of one mbox file into an Akonadi mail folder the user picks,
through the mbox filter of [mailimporter](#port-mailimporter) and the folder chooser of
[mailcommon](#port-mailcommon). It is reached from a file manager's Open With menu on an mbox
file; [kmail](#port-kmail)'s own import menu runs
[akonadi-import-wizard](#port-akonadi-import-wizard) instead.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — compile none of the translation catalogues in `po/`. kpkg
  removes every non-English catalogue from the package anyway (see
  [size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so this only saves build time.

## Notes

Upstream finds [kiconthemes](#port-kiconthemes) with `QUIET` and no `REQUIRED`, then links
`KF6::IconThemes` unconditionally for `KIconTheme::initTheme()`, so a missing kiconthemes fails
the generate step; it is in `depends`.

The one entry, `org.kde.mboximporter.desktop`, is `NoDisplay=true` and claims `application/mbox`,
which [kmail](#port-kmail)'s `kmail_view.desktop` claims too; which of the two opens an mbox file
is chosen in `mimeapps.list`. It names the icon `kmail`, which [breeze-icons](#port-breeze-icons),
in `depends` for run time only, draws. The program sets `org.kde.mboximporter` as its desktop
file name, so its window matches the entry.
