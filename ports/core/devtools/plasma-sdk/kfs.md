## About

The Plasma SDK is the set of tools for writing Plasma themes, widgets and global themes:
`plasmoidviewer` runs one widget in a window at a chosen form factor and location,
`lookandfeelexplorer` edits a global theme's metadata and previews, `plasmathemeexplorer` shows
every element of a Plasma style, `iconexplorer` searches the installed icon themes,
`plasmaengineexplorer` browses Plasma 5 Support's data engines, and `kqml` runs a QML file with
KDE's QML engine set up. KDOS uses them for its own theme work; nothing depends on the package.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — Make [kdoctools](#port-kdoctools) a
  requirement. Upstream looks for it as optional and, without it, drops the manual pages of
  `plasmoidviewer`, `kqml` and `plasmaengineexplorer`, the only documentation the tools have (see
  [Manual pages](../../../../docs/kdos/05-developer/writing-ports.md#manual-pages)).
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6Core5Compat=ON` — Make [qt6-qt5compat](#port-qt6-qt5compat)
  a requirement. Upstream looks for it quietly and links nothing from it, but the theme explorer's
  line-edit preview imports `Qt5Compat.GraphicalEffects` at run time, and without the module
  `plasmathemeexplorer` cannot draw that element.

## Notes

The package is an application, so it sits in the applications phase and its `depends` names only
the Plasma libraries ([libplasma](#port-libplasma), [plasma5support](#port-plasma5support)) and
frameworks it links. `plasmoidviewer`'s shell package and `lookandfeelexplorer` load QML modules of
the Plasma session at run time (`org.kde.plasma.shell.panel`, `org.kde.plasma.clock`). A session
port cannot be named in an application's `depends`, so the tools find those modules only on a
machine with the Plasma session installed.
