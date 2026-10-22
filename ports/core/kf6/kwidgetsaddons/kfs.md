## About

KWidgetsAddons is the KDE Frameworks collection of extra Qt Widgets: message boxes and the
`KMessageWidget` banner, collapsible group boxes, page dialogs, the font and date choosers, ratings,
the action menus and toolbar helpers that KDE dialogs are built from. It has no dependency beyond
Qt Widgets, and many KDE libraries and applications with a window link it.

## Flags

- `-D BUILD_DESIGNERPLUGIN=OFF` — Build no Qt Designer plugin for the widgets, which upstream
  builds by default.
- `-D BUILD_PYTHON_BINDINGS=OFF` — Build no Python bindings. Upstream's default is on, and the
  configure then requires Shiboken6 and PySide6, which this port's `depends` does not name.
