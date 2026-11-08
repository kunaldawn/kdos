## About

Layer Shell Qt lets a Qt window be a layer surface of the `wlr-layer-shell` protocol: a surface
the compositor anchors to an edge of an output and keeps above or below the ordinary windows,
with no title bar and no place in the task list. Plasma's panels and desktop, the lock screen,
the splash screen, KRunner, the on-screen displays and Spectacle's region selector are drawn that
way. It installs a library, a Qt Wayland shell-integration plugin named `layer-shell` and a QML
module, `org.kde.layershell`.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Passed as by every KDE port, so a later release that adds
  translations stays English-only with no change to the recipe (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
  This release carries none, and CMake reports the variable as unused.

## Notes

The plugin talks to the compositor through Qt Wayland's private client API, which carries no
compatibility promise between Qt releases, so this port is rebuilt with every
[qt6-qtwayland](#port-qt6-qtwayland) bump. The `tests` directory is configured whatever
`BUILD_TESTING` says and builds `layershellqt-test`, which the install leaves out.
