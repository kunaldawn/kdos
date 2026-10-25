## About

KNotifications sends desktop notifications from a KDE program through the freedesktop
`org.freedesktop.Notifications` D-Bus interface, whose name `kdos-notifyd` owns in a KDOS session
(see [Notifications](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/04-programs/kdos-shell.md#notifications)), and plays a notification's event sound through [libcanberra](#port-libcanberra). A program
describes its events in a `.notifyrc` file; the QML module `org.kde.notification` exposes the same
API to Qt Quick programs.

## Flags

- `-D BUILD_PYTHON_BINDINGS=OFF` — Build no Python bindings. Upstream's default is on, and the
  configure then requires Shiboken6 and PySide6, which this port's `depends` does not name.

## Notes

The QML module is built only when the configure finds Qt Qml, which upstream looks for quietly and
as optional (`find_package(Qt6 QUIET OPTIONAL_COMPONENTS Qml)`), so a missing
[qt6-qtdeclarative](#port-qt6-qtdeclarative) leaves it out with no message. `build.sh` therefore
checks after the install that `org/kde/notification/qmldir` is in the package, and fails the build
if it is not; a Qt Quick program that imports the module would otherwise fail only when it
starts.
