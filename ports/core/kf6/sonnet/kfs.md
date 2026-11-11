## About

Sonnet is KDE's spell checker: one API over several spelling engines, loaded as plugins, with
automatic detection of the language a text is written in, the spell check dialog and highlighter
of Qt Widgets text fields, and a QML module, `org.kde.sonnet`, for Qt Quick ones.
[ktextwidgets](#port-ktextwidgets), [ktexteditor](#port-ktexteditor),
[qqc2-desktop-style](#port-qqc2-desktop-style), [kmail](#port-kmail) and
[kdevelop](#port-kdevelop) check spelling through it.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_HUNSPELL=ON` — With
  `-D CMAKE_REQUIRE_FIND_PACKAGE_ASPELL=ON`: upstream builds a plugin for each engine it finds and
  stops only when it finds none, so a missing [hunspell](#port-hunspell) or [aspell](#port-aspell)
  would leave its plugin out with only a line in the configure summary. Required, a missing one
  stops the configure. The English dictionaries of both engines,
  [hunspell-en](#port-hunspell-en) and [aspell-en](#port-aspell-en), are in `depends` for run time.
- `-D BUILD_DESIGNERPLUGIN=OFF` — Build no Qt Designer plugin for the widgets. Upstream builds one
  by default, and the configure then requires Qt's `UiPlugin` module from
  [qt6-qttools](#port-qt6-qttools), which this port's `depends` does not name.
- `-D KF_SKIP_PO_PROCESSING=ON` — Build none of the Qt translation catalogues under `poqm/`,
  which `ecm_install_po_files_as_qm()` would compile into `.qm` files. The strings in the source
  are English, and the package-wide English-only rule (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
  would remove every catalogue in another language.

## Notes

The Hebrew engine, Hspell, and the Finnish one, Voikko, are looked for as well and are not ports,
so neither plugin is built.

Sonnet's default language is the locale's name unless `defaultLanguage` is set, and the C locale
names no dictionary: every program would then check against whichever dictionary sorts first, and
the Spell Check page of System Settings would open with unsaved changes. KDOS ships
`/usr/share/kdos/xdg/KDE/Sonnet.conf` with `defaultLanguage=en_US`, which Sonnet reads through
`XDG_CONFIG_DIRS` (see
[Configuration](../../../../docs/kdos/06-reference/configuration.md)). The aspell English
dictionary's spelling variants (`american-variant_0`, `-w_accents` and the like) are listed among
the languages on that page; they are aspell-en's own aliases and are kept.
