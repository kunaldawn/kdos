## About

KDeclarative is a set of QML modules for KDE's Qt Quick programs: `org.kde.kquickcontrols`, with
the key sequence and colour pickers that settings pages use, `org.kde.kquickcontrolsaddons`,
`org.kde.draganddrop`, the shader effects of `org.kde.graphicaleffects`, and the calendar events
plugin interface that the Plasma clock's calendar loads its holiday and PIM event sources through.
Plasma's shell and System Settings pages import them, and so do [kwin](#port-kwin) and
[haruna](#port-haruna).

## Flags

- `-D BUILD_EXAMPLES=OFF` — Build and install none of the example programs. It is upstream's
  default, written out so the recipe states it.
- `-D KF_SKIP_PO_PROCESSING=ON` — Build none of the gettext catalogues under `po/`, which
  `ki18n_install()` would compile into `.mo` files. The strings in the source are English, and the
  package-wide English-only rule (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
  would remove every catalogue in another language.

## Notes

The key sequence picker checks a new shortcut against the global ones through KGlobalAccel, which
the configure requires on Linux, so [kglobalaccel](#port-kglobalaccel) is in `depends`. The
configure also looks up the QML module `org.kde.ki18n` with `ecm_find_qmlmodule(… REQUIRED)`, which
[ki18n](#port-ki18n) installs, and compiles the graphical effects' shaders with `qsb` from
[qt6-qtshadertools](#port-qt6-qtshadertools).
