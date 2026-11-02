## About

PIM Data Exporter backs up and restores KDE PIM data in one zip archive: KMail's mail, accounts,
identities, transports, filters and settings, and the address books, calendars, alarms and feeds
of KAddressBook, KOrganizer, KAlarm and Akregator. The archive can be restored on another
machine, where the exporter recreates the Akonadi resources and settings. The port installs the
window, `pimdataexporter`, which [kmail](#port-kmail) opens from its Tools menu, and
`pimdataexporterconsole` for scripted backups.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — compile none of the translation catalogues in `po/`. kpkg
  removes every non-English catalogue from the package anyway (see
  [size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so this only saves build time.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6IconThemes=ON` — `KIconTheme::initTheme()` at start-up,
  which makes the window's icons follow the desktop's icon theme and colour scheme. Upstream finds
  [kiconthemes](#port-kiconthemes) quietly and links it only when it is found.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — the handbook is built only when
  [kdoctools](#port-kdoctools) is found; required, a missing kdoctools stops the configure instead
  of quietly dropping the handbook.
- `-D CMAKE_DISABLE_FIND_PACKAGE_KF6UserFeedback=ON` — no KUserFeedback, through which the
  exporter would submit usage statistics. It is not a port, and refusing the search keeps a stray
  copy in the build root from turning telemetry on.

## Notes

ktextaddons' `TextAddonsWidgets` is found without `REQUIRED` but marked `TYPE REQUIRED`, and the
feature summary stops the configure without it.

The exporter reads and writes what the Akonadi resources of
[kdepim-runtime](#port-kdepim-runtime) hold, and restoring an archive creates such resources, but
the port does not name kdepim-runtime: [kmail](#port-kmail), which opens it, already does, and
the `kde-gear-pim` meta installs both.

The entry names the icon `kontact`, which [breeze-icons](#port-breeze-icons), in `depends` for run
time only, draws. The program sets `org.kde.pimdataexporter` as its desktop file name, the name of
the entry, which carries no `StartupWMClass`, so the task manager matches the window to it with no
change. While a backup or restore runs, the window shows an icon in the system tray through
[kstatusnotifieritem](#port-kstatusnotifieritem).

`KF_SKIP_PO_PROCESSING` stops only `ki18n_install()`: `kdoctools_install()` still builds the
handbook in every translated language, and kpkg's language rule removes
`/usr/share/doc/HTML/<lang>/` from the package.
