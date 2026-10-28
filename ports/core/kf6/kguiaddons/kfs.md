## About

KGuiAddons is the KDE Frameworks library of small helpers for Qt GUI programs: colour utilities,
font helpers, `KModifierKeyInfo` for the state of the modifier keys, `KSystemClipboard`, the key
sequence recorder that shortcut editors use, and the query for whether the desktop prefers a light
or dark colour scheme. [kcolorscheme](#port-kcolorscheme), [kxmlgui](#port-kxmlgui),
[kio](#port-kio) and many KDE applications in the tree link it.

## Flags

- `-D WITH_WAYLAND=ON` — Build the Wayland code of `KModifierKeyInfo`, `KSystemClipboard` and
  `KKeySequenceRecorder`, which inhibits the compositor's shortcuts while a shortcut is being
  recorded. It is upstream's default on Linux; on, the configure requires
  [qt6-qtwayland](#port-qt6-qtwayland), [wayland](#port-wayland),
  [wayland-protocols](#port-wayland-protocols) and
  [plasma-wayland-protocols](#port-plasma-wayland-protocols), so a missing one stops it.
- `-D WITH_X11=ON` — Build the X11 code of `KModifierKeyInfo`, which a KDE program uses when it
  runs under Xwayland. On, the configure requires [libX11](#port-libX11), [libxcb](#port-libxcb)
  and a [qt6-qtbase](#port-qt6-qtbase) built with its xcb feature. Every toolkit in the tree is
  built with both backends (see
  [Toolkits, Wayland and X11](../../../../docs/kdos/02-user-guide/applications.md#toolkits-wayland-and-x11)).
- `-D BUILD_GEO_SCHEME_HANDLER=OFF` — Build no `kde-geo-uri-handler`, the program that opens a
  `geo:` link in a web map, and none of the desktop entries that register it for OpenStreetMap,
  Google Maps, Wheelmap and Cartes.
- `-D BUILD_PYTHON_BINDINGS=OFF` — Build no Python bindings. Upstream's default is on, and the
  configure then requires Shiboken6 and PySide6, which this port's `depends` does not name.
