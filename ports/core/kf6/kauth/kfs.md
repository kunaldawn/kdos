## About

KAuth lets a KDE program run one action as root without running as root itself: the program
asks for a named action, polkit decides whether the user may perform it, and a small helper
program, started on the system bus, performs it and reports back. `kauth-policy-gen` writes the
polkit policy file for a program's actions at build time. [ktexteditor](#port-ktexteditor) saves
files the user does not own through it, and so do the system settings pages that change machine
configuration.

## Flags

- `-D KAUTH_BACKEND_NAME=PolkitQt6-1` — Build the polkit backend, through
  [polkit-qt6](#port-polkit-qt6). Left unset, upstream picks polkit only when the configure finds
  it and otherwise builds the `FAKE` backend, which refuses every action, with no error; named, a
  missing polkit-qt6 still falls back to `FAKE` with a warning, which is why `build.sh` checks
  after the install that `kauth_backend_plugin.so` is in the package and fails the build if it is
  not.
- `-D KAUTH_HELPER_BACKEND_NAME=DBus` — Build the helper side over D-Bus: the helper is a
  system-bus service started by D-Bus activation for each action, the only helper backend upstream
  has on Linux.
- `-D KF_SKIP_PO_PROCESSING=ON` — Build none of the Qt translation catalogues under `poqm/`,
  which `ecm_install_po_files_as_qm()` would compile into `.qm` files. The strings in the source
  are English, and the package-wide English-only rule (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
  would remove every catalogue in another language.

## Notes

The polkit backend requires [kwindowsystem](#port-kwindowsystem), which passes the asking window
to the polkit agent so its dialog opens over it.
