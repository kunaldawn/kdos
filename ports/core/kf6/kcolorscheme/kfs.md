## About

KColorScheme loads a KDE colour scheme, the `[Colors:*]` groups of a `kdeglobals` file, and gives a
program the colour for each role: view and window backgrounds, normal, inactive and link text,
selection, and the positive, neutral and negative states. The `kde` platform theme of
[plasma-integration](#port-plasma-integration) reads the colours through it, which is how a Qt 6
application follows the colour scheme applied to `~/.config/kdeglobals`, by System Settings or by
`kdos theme` (see
[A graphical application](../../../../docs/kdos/05-developer/writing-ports.md#a-graphical-application)).
[kconfigwidgets](#port-kconfigwidgets), [kiconthemes](#port-kiconthemes) and [kio](#port-kio)
link it.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Build none of the gettext catalogues under `po/`, which
  `ki18n_install()` would compile into `.mo` files. The strings in the source are English, and the
  package-wide English-only rule (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
  would remove every catalogue in another language.
