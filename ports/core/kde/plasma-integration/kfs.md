## About

Plasma Integration is the Qt platform theme `kde`, the plugin Qt loads when
`QT_QPA_PLATFORMTHEME=kde`. Through it every Qt 6 program, KDE's or not, takes the colours, fonts,
icon theme, widget style and dialogs from `kdeglobals`: KDE's file dialog in place of Qt's own
(or the portal's, with `PLASMA_INTEGRATION_USE_PORTAL=1`), KDE's colour and message dialogs,
the notification-area icon, the global menu and the Qt Quick Controls style. Over Wayland it
tells KWin which colour scheme a window's title bar is to be drawn in.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the translations of the dialogs, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D BUILD_QT5=OFF` — No Qt 5 platform theme. Upstream builds one by default, and the Qt 5 build
  needs KDE Frameworks 5, which are not ports; Qt 5 programs are themed
  through [qt5ct](#port-qt5ct).
- `-D BUILD_QT6=ON` — The Qt 6 platform theme, upstream's default.
- `-D WITH_X11=ON` — Build the X11 side of the platform theme, so a Qt program running on the xcb
  platform under Xwayland gets its style properties and the cursor theme too. With it the
  configure requires [libxcb](#port-libxcb), [libX11](#port-libX11) and
  [libXcursor](#port-libXcursor). It is upstream's default, named so those requirements stay in
  `depends`.
- `-D CMAKE_DISABLE_FIND_PACKAGE_FontHack=ON` — Skip the run-time check for Hack, the monospace
  face the theme falls back to when `kdeglobals` names none. Hack is not a port: KDOS sets its
  monospace face, JetBrains Mono, as the `fixed` font in its `kdeglobals` defaults, so the
  fallback is never reached.

## Notes

The theme picks the Qt Quick Controls style for a program at start-up: `org.kde.union` when the
widget style is Union and [union](#port-union) is installed, `org.kde.desktop` from
[qqc2-desktop-style](#port-qqc2-desktop-style) for a program with widgets, and `org.kde.breeze`
from [qqc2-breeze-style](#port-qqc2-breeze-style) for a Qt Quick program with no widgets. Both
styles are in `depends`; without the second, such a program is left on Qt's own default style
and does not look like the rest of the desktop. The emoji font is also what country-flag icons
are drawn with.

The configure looks for three things the theme needs only at run time and reports them in its
summary, without changing what is built: Noto Sans, Noto Color Emoji and the KDE desktop portal.
The two font checks look for a file at another distribution's path
(`share/fonts/google-noto/NotoSans-Regular.ttf` and
`share/fonts/google-noto-color-emoji-fonts/Noto-COLRv1.ttf`), so they report both fonts missing;
the fonts themselves come from [noto-fonts](#port-noto-fonts) and [noto-emoji](#port-noto-emoji),
both in `depends`. The portal is [xdg-desktop-portal-kde](#port-xdg-desktop-portal-kde), a session
port in `50_desktop` that this `43_toolkits` port cannot name in `depends`; the `kde-plasma` meta
port installs it. The theme's "Open With" dialog is drawn by that portal's backend,
`org.freedesktop.impl.portal.desktop.kde`, so without it a program that asks which application to
open a file with gets no dialog.
