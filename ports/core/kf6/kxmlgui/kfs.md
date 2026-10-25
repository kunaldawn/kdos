## About

KXMLGUI builds a KDE application's menus and toolbars from an XML description of its actions, so a
user can rearrange them, and it supplies `KMainWindow` and `KXmlGuiWindow`, the shortcut editor,
the "About" and bug report dialogs and the toolbar editor. Many KDE applications with a main
window link it, and [kparts](#port-kparts) is built on it.

## Flags

- `-D FORCE_DISABLE_KGLOBALACCEL=OFF` — Keep global shortcut support, through
  [kglobalaccel](#port-kglobalaccel). It is upstream's default; on, the configure requires
  `KF6GlobalAccel`, so a missing kglobalaccel is an error.
- `-D BUILD_PYTHON_BINDINGS=OFF` — Build no Python bindings. Upstream's default is on, and the
  configure then requires Shiboken6 and PySide6, which this port's `depends` does not name.
- `-D BUILD_DESIGNERPLUGIN=OFF` — Build no Qt Designer plugin for the widgets, which upstream
  builds by default.
