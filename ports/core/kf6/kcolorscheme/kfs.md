## About

KColorScheme loads a KDE colour scheme, the `[Colors:*]` groups of a `kdeglobals` file, and gives a
program the colour for each role: view and window backgrounds, normal, inactive and link text,
selection, and the positive, neutral and negative states. It is how a Qt 6 application follows the
colours that `kdos theme` writes to `~/.config/kdeglobals` (see
[A graphical application](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#a-graphical-application)).
[kconfigwidgets](#port-kconfigwidgets), [kiconthemes](#port-kiconthemes) and [kio](#port-kio)
link it.
