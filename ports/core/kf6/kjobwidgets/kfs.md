## About

KJobWidgets shows the progress of a `KJob`, a copy, a download or a burn: as a widget or dialog in
the program's own window, or as an entry in the desktop's job tracker, which in a Plasma session is
the progress list of Plasma's notifications, reached over D-Bus. [kio](#port-kio)'s file
operations report through it, and so do [k3b](#port-k3b), [kdevelop](#port-kdevelop) and the PIM
applications.

## Flags

- `-D BUILD_PYTHON_BINDINGS=OFF` — Build no Python bindings. Upstream's default is on, and the
  configure then requires Shiboken6 and PySide6, which this port's `depends` does not name.
- `-D WITH_X11=ON` — Build the X11 code, which keeps the user's X timestamp on a progress dialog
  for a program that runs under Xwayland, so the dialog may take focus. It is upstream's default
  and needs only Qt's private X11 header, from [qt6-qtbase](#port-qt6-qtbase).
- `-D KF_SKIP_PO_PROCESSING=ON` — Build none of the Qt translation catalogues under `poqm/`,
  which `ecm_install_po_files_as_qm()` would compile into `.qm` files. The strings in the source
  are English, and the package-wide English-only rule (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
  would remove every catalogue in another language. The switch also skips upstream's `en`
  catalogue, which holds only English plural forms, so a remaining time reads as the source spells
  it, `2 hour(s)`.
