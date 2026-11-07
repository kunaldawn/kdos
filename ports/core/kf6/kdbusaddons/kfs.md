## About

KDBusAddons is the KDE Frameworks library of conveniences over Qt D-Bus: `KDBusService`, which
registers an application's unique bus name so a second launch hands its arguments to the running
instance; `KDEDModule`; and helpers for D-Bus activation. Its `kquitapp6` command asks a running
application to quit over the bus. [kio](#port-kio) and many KDE applications link it.

## Flags

- `-D WITH_X11=ON` — Build the X11 code, which passes the launch's startup identifier to the running
  instance for a program under Xwayland. It is upstream's default on Linux.
- `-D KF_SKIP_PO_PROCESSING=ON` — Build none of the Qt translation catalogues under `poqm/`,
  which `ecm_install_po_files_as_qm()` would compile into `.qm` files. The strings in the source
  are English, and the package-wide English-only rule (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
  would remove every catalogue in another language.
