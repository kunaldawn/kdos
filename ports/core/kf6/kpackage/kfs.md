## About

KPackage installs, finds and loads packages of non-binary data: a directory of QML, images and
scripts with a `metadata.json` that names its type. Plasma's widgets, wallpapers, global themes,
look-and-feel and splash screens, KWin's scripts and effects in QML, and the Qt Quick settings
pages are such packages. `kpackagetool6` installs, upgrades, lists and removes them, system-wide
or under `~/.local/share`. [knewstuff](#port-knewstuff) installs store content of these kinds
through it, and Plasma's shell and [kwin](#port-kwin) load their packages with it.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Build none of the gettext catalogues under `po/`, which
  `ki18n_install()` would compile into `.mo` files. The strings in the source are English, and the
  package-wide English-only rule (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
  would remove every catalogue in another language. The switch does not reach
  `kdoctools_install()`, which still builds the translated manual pages of `kpackagetool6`; the
  same rule removes them from the package.

## Notes

[kdoctools](#port-kdoctools), optional upstream and in `depends`, builds the manual page of
`kpackagetool6`.
