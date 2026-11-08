## About

Three editors for the templates KDE PIM draws with. The Header Theme Editor writes themes for the
headers above a message in [kmail](#port-kmail); the Contact Theme Editor and the Contact Print
Theme Editor write the themes [kaddressbook](#port-kaddressbook) uses to show and to print a
contact. Each theme is a KTextTemplate template with its desktop file, handled through
[grantleetheme](#port-grantleetheme), and each editor shows a live preview in a
[qt6-qtwebengine](#port-qt6-qtwebengine) view while the template is edited in
[ktextaddons](#port-ktextaddons)' editor with [syntax-highlighting](#port-syntax-highlighting).

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — compile none of the translation catalogues in `po/`. kpkg
  removes every non-English catalogue from the package anyway (see
  [size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so this only saves build time.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — the editors' handbooks are built only when
  [kdoctools](#port-kdoctools) is found; required, a missing kdoctools stops the configure instead
  of quietly dropping them.

## Notes

The three entries name the icons `kmail` and `kaddressbook`, which the port does not install;
[breeze-icons](#port-breeze-icons), in `depends` for run time only, draws both.

Each program sets its desktop file name in `main.cpp` (`org.kde.headerthemeeditor`,
`org.kde.contactthemeeditor`, `org.kde.contactprintthemeeditor`), which is the name of its entry;
the entries carry no `StartupWMClass`, so the task manager matches each window to its entry with
no change.

`KF_SKIP_PO_PROCESSING` stops only `ki18n_install()`: `kdoctools_install()` still builds the
handbooks in every translated language, and kpkg's language rule removes
`/usr/share/doc/HTML/<lang>/` from the package.
