## About

KIconThemes implements the freedesktop icon theme lookup for KDE programs: `KIconLoader`, the
`KIconEngine` that recolours symbolic icons to the colour scheme, the icon chooser dialog, and the
`kiconfinder6` command that prints the file an icon name resolves to. [kio](#port-kio),
[kxmlgui](#port-kxmlgui) and many KDE applications link it.

## Flags

- `-D USE_BreezeIcons=ON` — Link the library of [breeze-icons](#port-breeze-icons), so that
  `KIconTheme::initTheme()` makes Breeze the fallback theme: an icon missing from the theme in use
  is taken from Breeze, which carries the icons KDE programs name. It is upstream's default; on, the
  configure requires `KF6BreezeIcons`, which is why that port is in `depends`.
- `-D KICONTHEMES_USE_QTQUICK=ON` — Build the QML plugin, `org.kde.iconthemes`, for Qt Quick
  programs. It is upstream's default and needs [qt6-qtdeclarative](#port-qt6-qtdeclarative).
- `-D BUILD_DESIGNERPLUGIN=OFF` — Build no Qt Designer plugin for the icon widgets, which upstream
  builds by default.
