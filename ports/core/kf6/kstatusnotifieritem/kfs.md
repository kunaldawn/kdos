## About

KStatusNotifierItem puts a program's icon in the system tray through the freedesktop
StatusNotifierItem D-Bus protocol: its icon, tooltip, attention state and context menu, and the
"minimise to tray" behaviour of an associated window. The Plasma system tray, from
[plasma-workspace](#port-plasma-workspace), hosts these items. [konversation](#port-konversation),
[kdeconnect](#port-kdeconnect), [kmix](#port-kmix), [spectacle](#port-spectacle) and the PIM
applications show their tray icons through it.

## Flags

- `-D WITHOUT_X11=OFF` — Look for libX11 and build the X11 code, which restores a window hidden to
  the tray onto the virtual desktop it was on and raises it in the stacking order, for a program
  that runs under Xwayland. It is upstream's default; with it, the configure finds
  [libX11](#port-libX11), which is in `depends`, so the code is always built. Every toolkit in the
  tree keeps both backends (see
  [Toolkits, Wayland and X11](../../../../docs/kdos/02-user-guide/applications.md#toolkits-wayland-and-x11)).
- `-D BUILD_PYTHON_BINDINGS=OFF` — Build no Python bindings. Upstream's default is on, and the
  configure then requires Shiboken6 and PySide6, which this port's `depends` does not name.
- `-D KF_SKIP_PO_PROCESSING=ON` — Build none of the Qt translation catalogues under `poqm/`,
  which `ecm_install_po_files_as_qm()` would compile into `.qm` files. The strings in the source
  are English, and the package-wide English-only rule (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
  would remove every catalogue in another language.
