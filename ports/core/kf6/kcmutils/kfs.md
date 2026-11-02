## About

KCMUtils is how a KDE settings page is written and loaded. A configuration module (KCM) is a
plugin, a Qt Widgets one built on `KCModule` or a Qt Quick one built on `KQuickConfigModule`, and
this framework finds it by its plugin metadata, loads it and shows it, in System Settings, in a
program's own settings dialog (`KPluginWidget`, `KCMultiDialog`), or alone with `kcmshell6`.
`kcmdesktopfilegenerator` writes the desktop entry that lists a module in the application menu.
Plasma's System Settings, [dolphin](#port-dolphin)'s settings and the KCMs of every Plasma module
are built on it.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Build none of the gettext catalogues under `po/`, which
  `ki18n_install()` would compile into `.mo` files. The strings in the source are English, and the
  package-wide English-only rule (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
  would remove every catalogue in another language.

## Notes

The configure looks up the QML module `org.kde.kirigami` with `ecm_find_qmlmodule(… REQUIRED)`,
so [kirigami](#port-kirigami) must be installed before this port builds, not only at run time; the
Qt Quick modules draw their pages with it.
