## About

KNewStuff lets a KDE application download and install add-on content, such as themes, templates,
wallpapers or scripts, from the providers listed at `autoconfig.kde.org`, the KDE Store among
them, over the Open Collaboration Services API, which [attica](#port-attica) implements. It has a
Qt Widgets dialog and a Qt Quick one built on [kirigami](#port-kirigami), and the
`knewstuff-dialog6` command opens the dialog for a given `.knsrc` file.

## Flags

- `-D CMAKE_DISABLE_FIND_PACKAGE_KF6Syndication=ON` — Never look for KSyndication, the RSS and
  Atom parser that the OPDS (e-book catalogue) provider uses. Upstream makes it optional and uses it
  whenever it is found; Syndication is not a port, so the switch keeps the result from depending on
  what is installed.
- `-D BUILD_DESIGNERPLUGIN=OFF` — Build no Qt Designer plugin for the widgets, which upstream
  builds by default.
