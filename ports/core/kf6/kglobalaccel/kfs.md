## About

KGlobalAccel is the client side of KDE's global shortcuts: a program registers its actions and
their default key sequences with the shortcut daemon over D-Bus, and the daemon triggers the
action when the keys are pressed in any window. The daemon itself is
[kglobalacceld](#port-kglobalacceld), a Plasma module, to which [kwin](#port-kwin) passes the key
presses under Wayland. [kxmlgui](#port-kxmlgui)'s
shortcut editor, [kdeclarative](#port-kdeclarative)'s key sequence picker and
[konsole](#port-konsole) use it.

## Flags

- `-D WITH_X11=ON` — Build the X11 code, which hands a shortcut's X timestamp on to a program that
  runs under Xwayland, so the window the action raises may take focus. It is upstream's default
  and needs only Qt's private X11 header, from [qt6-qtbase](#port-qt6-qtbase); every toolkit in the
  tree keeps both backends (see
  [Toolkits, Wayland and X11](../../../../docs/kdos/02-user-guide/applications.md#toolkits-wayland-and-x11)).
- `-D KF_SKIP_PO_PROCESSING=ON` — Build none of the Qt translation catalogues under `poqm/`,
  which `ecm_install_po_files_as_qm()` would compile into `.qm` files. The strings in the source
  are English, and the package-wide English-only rule (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
  would remove every catalogue in another language.
