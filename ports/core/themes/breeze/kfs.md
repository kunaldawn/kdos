## About

Breeze is the default look of KDE software, and the base of KDOS's own. The port installs the
Breeze widget style for Qt 6 applications and its settings page, the Breeze window decoration
KWin draws title bars with, the Breeze Classic, Breeze Light and Breeze Dark colour schemes, the
two cursor themes, `breeze_cursors` and `Breeze_Light`, with `kcursorgen`, and the `Next`
wallpaper. KDOS's window decoration is this one, squared and outlined in the accent through two
options its patch adds and KDOS's configuration turns on, rather than replaced; its cursor is `breeze_cursors` for every accent.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the translations of the style's and the
  decoration's settings, which the package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D BUILD_QT5=OFF` — No Qt 5 widget style. Upstream builds one by default, and the Qt 5 build
  needs KDE Frameworks 5, which are not ports; Qt 5 applications are themed
  through [qt5ct](#port-qt5ct) instead.
- `-D BUILD_QT6=ON` — The Qt 6 style, the decoration and the wallpaper, upstream's default.
- `-D BUILD_WITH_QTQUICK=ON` — Lets the style's window manager start a window move from an empty
  area of a Qt Quick window, as it does for widgets; it needs
  [qt6-qtdeclarative](#port-qt6-qtdeclarative).
- `-D BUILD_CURSOR=ON` — Install the two cursor themes and build `kcursorgen`. The Breeze global
  theme names `breeze_cursors` as its cursor, and kde-gtk-config hands the same name to GTK
  programs, so without it both fall back to whatever cursor theme the image has.
- `-D WITH_DECORATIONS=ON` — Build the Breeze window decoration plugin and its settings page,
  against [kdecoration](#port-kdecoration). KWin uses Breeze when no decoration is configured.
- `-D WITH_WALLPAPERS=ON` — Install the `Next` wallpaper. It is the last fallback of Plasma's
  default-wallpaper lookup, so a wallpaper setting that names a package which is not installed
  shows `Next` rather than a blank desktop.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6FrameworkIntegration=ON` — Make
  [frameworkintegration](#port-frameworkintegration) a requirement. Upstream looks for it
  quietly, and without it the style derives from `QCommonStyle` instead of KStyle and loses the
  KDE style extensions, such as the capacity bar Dolphin draws for free disk space.

## Patches

`sandbox-badge.patch` draws a small box before the caption of a window whose client is sandboxed,
in the caption's own colour, with its width taken out of the caption's so the two never overlap
whatever the title alignment. The engine's state picks the drawing: a solid box for a confined KDOS
box (`+private`) or any other sandbox, a dashed one for a box that could reach past its tag
(`+tagged`), and a warning triangle for an engine that reports none of its boundary held
(`+unconfined`). It reads `isSandboxed()` and `sandboxEngine()` from the
[kdecoration](#port-kdecoration) patch of the same name, without which it does not compile, and is
proposed upstream with it.

`square-corners.patch` adds two `[Common]` keys to `breezerc`, both off unless set, so Breeze
without them draws as upstream does. `SquareCorners=true` sets the decoration's corner radius to
zero, which squares the title bar, the frame, the outline and the shadow together: upstream's
`RoundedCorners` only rounds or squares the bottom corners of a borderless window, and the title
bar's top corners keep their radius whatever it says. `OutlineHighlight=true` draws the active
window's one-pixel outline in the link colour of the window's palette, which is the accent of
every KDOS scheme and of a box's forced scheme. It is not the selection colour: a dark accent's
selection is the accent darkened until text reads on it, and QPalette carries no focus colour, so
the link is the one palette role a scheme holds at its accent. Upstream's outline is a mix of the frame and text
colours, which no colour-scheme key can make the accent, and an inactive window keeps that mix.
KDOS's `/usr/share/kdos/xdg/breezerc` turns both on. It is applied after `sandbox-badge.patch`,
whose hunks it is cut against, and is to be offered upstream to
[Breeze](https://invent.kde.org/plasma/breeze) as two options of the decoration.

## Notes

The cursors ship in the source as finished XCursor files, with the SVGs they were drawn from in
`cursors_scalable`, and the install copies both; nothing renders a cursor at build time, so the
cursor themes need no tool beyond the build. `kcursorgen` is installed beside them: it turns an
SVG cursor theme into XCursor files at the sizes and scales it is given, for programs that read
only XCursor.

The decoration plugin and the style both link `libbreezecommon6`, and the decoration is a
KDecoration3 plugin, so it is rebuilt with every Plasma release together with
[kdecoration](#port-kdecoration) and kwin.

The QtCurve preset `Breeze.qtcurve` is installed under `/usr/share/QtCurve`, where only the QtCurve
style reads it; QtCurve is not a port.
