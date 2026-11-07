## About

Alligator is KDE's feed reader: it subscribes to RSS and Atom feeds, keeps their entries for
reading offline and shows them in a Kirigami window that fits a phone as well as a desktop. It
parses feeds with the KF6 [syndication](#port-syndication) framework and keeps subscriptions and
entries in an SQLite database through the SQLite driver of [qt6-qtbase](#port-qt6-qtbase).

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

[qqc2-desktop-style](#port-qqc2-desktop-style) is in `depends` for run time only. `main.cpp`
selects its `org.kde.desktop` Qt Quick Controls style whenever `QT_QUICK_CONTROLS_STYLE` is
unset, and without it the window falls back to Qt's plain controls. The form and delegate QML
modules the interface imports come from [kirigami-addons](#port-kirigami-addons); CMake checks
for the package but not for the modules.

Upstream's desktop entry is installed as it is. `KAboutData` gives the window the Wayland app_id
`org.kde.alligator`, the entry's own file name, so the task manager matches the window to its
entry without a `StartupWMClass` line.

`autotests/` is added whatever `BUILD_TESTING` says, and it needs nothing beyond Qt Test, which
is part of [qt6-qtbase](#port-qt6-qtbase); no test runs during the build.
