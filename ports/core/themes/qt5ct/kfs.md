## About

qt5ct is the appearance configuration of Qt 5 programs: a Qt platform theme plugin that applies a
palette, widget style, fonts and icon theme from `~/.config/qt5ct/qt5ct.conf`. KDE's platform theme
is built for Qt 6 only, so every Qt 5 program on KDOS (VLC, KeePassXC, LMMS, Mumble and the rest)
draws through this one. A person with no `qt5ct.conf` of their own gets a copy of
`/usr/share/kdos/xdg/qt5ct/qt5ct.conf`, the bone accent, the first time a Qt 5 program starts, and
`kdos theme` merges each accent's palette into it from then on.

## Flags

- `-DDISABLE_DBUS=OFF` — keep D-Bus support, which upstream describes as the global menu and the
  D-Bus tray icon implementation; it links Qt 5's D-Bus and theme-support modules into the plugin.
  This is upstream's default, named so it stays on.

## Patches

- `kde-key.patch` — Makes the plugin answer the platform theme name `kde` as well as `qt5ct` and `qt6ct`: `Qt5CTPlatformThemePlugin::create()` in `src/qt5ct-qtplugin/main.cpp` accepts the key, and `qt5ct.json` lists it. The session exports `QT_QPA_PLATFORMTHEME=kde`, which loads KDE's platform theme in a Qt 6 program; KDE's platform theme is built for Qt 6 only, and each Qt major version searches only its own plugin directory, so with the patch a Qt 5 program loads qt5ct under the same value and the Qt 6 plugin is untouched.

## Notes

Upstream's desktop entry names the theme icon `preferences-desktop-theme` and no
`StartupWMClass`. The recipe rasterises Breeze's 32-pixel `preferences-desktop-theme.svg`, read
from the installed [breeze-icons](#port-breeze-icons) in the build root, into 32, 48, 64 and
128-pixel `hicolor` PNGs named `qt5ct`, and overwrites upstream's `qt5ct.desktop` with its own
entry, which names that icon. qt5ct sets its desktop file name to `qt5ct.desktop`, so its Wayland
app id is `qt5ct`, which the entry names in `StartupWMClass`.

The interface translations are compiled into the program by `lrelease` from
[qt5-qttools](#port-qt5-qttools), and the configure reads Qt 5's plugin directory from the
`qtpaths` beside it; it stops when it cannot find either.

The entry is `NoDisplay=true`: the settings window edits a file `kdos theme` rewrites on every
accent change, so offering it in the menu offers a change that does not last. `qt5ct` stays
installed and runs by name; Kickoff, KRunner's application search and Discover do not offer it.
