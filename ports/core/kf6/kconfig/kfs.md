## About

KConfig is KDE's configuration system: `KConfig` and `KSharedConfig` read and write the INI-style
files under `~/.config` and the `$XDG_CONFIG_DIRS` directories, `/etc/xdg` among them, with
cascading from system to user files; `kconfig_compiler_kf6` generates typed settings classes from
an XML description at build time; and `kreadconfig6` and `kwriteconfig6` read and write a key from
the shell. `~/.config/kdeglobals`, the file KDE programs share their settings in and the one a
colour scheme is applied to, by System Settings or by `kdos theme` (see
[A graphical application](../../../../docs/kdos/05-developer/writing-ports.md#a-graphical-application)),
is read through it.
Nearly every KDE port in the tree depends on it.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Build none of the Qt translation catalogues under `poqm/`,
  which `ecm_install_po_files_as_qm()` would compile into `.qm` files. The strings in the source
  are English, and the package-wide English-only rule (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
  would remove every catalogue in another language.

## Notes

The QML module `org.kde.config` (`KCONFIG_USE_QML`, on by default) requires Qt Qml and Qt Quick,
which is why [qt6-qtdeclarative](#port-qt6-qtdeclarative) is in `depends`.
