## About

qqc2-desktop-style is the `org.kde.desktop` style of Qt Quick Controls 2: buttons, fields, menus,
scroll bars and the rest drawn by the Qt Widgets style in use, Breeze for one, so a Qt Quick
program looks like a Qt Widgets one beside it. It also installs Kirigami's `org.kde.desktop`
platform plugin, through which [kirigami](#port-kirigami) takes its colours from the KDE colour
scheme and its icons from the icon theme. Plasma's System Settings pages and the Kirigami
applications in the tree, [elisa](#port-elisa), [haruna](#port-haruna) and
[kdeconnect](#port-kdeconnect) among them, draw through it.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Build none of the Qt translation catalogues under `poqm/`,
  which `ecm_install_po_files_as_qm()` would compile into `.qm` files. The strings in the source
  are English, and the package-wide English-only rule (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
  would remove every catalogue in another language.

## Notes

The configure looks for KIconThemes, KColorScheme and libX11 as optional: without
[kiconthemes](#port-kiconthemes) Kirigami's icons ignore the icon theme's colouring, and without
[kcolorscheme](#port-kcolorscheme) its palette ignores the colour scheme. All three are in
`depends`, so all three are built in. It also looks up the QML module `org.kde.sonnet`, the spell
checking of text fields, which [sonnet](#port-sonnet) installs.
