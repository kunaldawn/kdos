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
- `-D KF_SKIP_PO_PROCESSING=ON` — Build none of the Qt translation catalogues under `poqm/`,
  which `ecm_install_po_files_as_qm()` would compile into `.qm` files. The strings in the source
  are English, and the package-wide English-only rule (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
  would remove every catalogue in another language. The switch also skips upstream's `en`
  catalogue, which holds only English plural forms, so a count in one of the widgets reads as the
  source spells it, with the `(s)` left in.
