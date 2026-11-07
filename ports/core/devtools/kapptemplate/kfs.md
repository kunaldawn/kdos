## About

KAppTemplate starts a new KDE or Qt project from a template: a KDE Frameworks widgets
application, a Kirigami application, a plasmoid, a KParts component and others. It asks for the
project's name and location and writes the source tree, with its CMake files, ready to build.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`, which kpkg's
  English-only rule would remove from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Patches

The desktop entry's `StartupWMClass` is upstream's `kapptemplate`, but the window's Wayland
app_id is `org.kde.kapptemplate`, the desktop file name that KAboutData derives from the
`apps.kde.org` home page and the component name; `app-id.patch` sets the class to the app_id so
the task manager matches the window to its entry.

## Notes

The window is Kirigami. It imports `org.kde.kirigamiaddons.formcard`, `.treeview` and
`.delegates` from [kirigami-addons](#port-kirigami-addons) and `org.kde.kitemmodels` from
[kitemmodels](#port-kitemmodels), and asks for the `org.kde.desktop` Qt Quick Controls style that
[qqc2-desktop-style](#port-qqc2-desktop-style) provides, falling back to Fusion; all three are
needed only at run time. A generated project is built with cmake, extra-cmake-modules and a
compiler, and a plasmoid template needs libplasma, none of which the program itself runs.

The templates are packed into archives at build time by ECM's `KDEPackageAppTemplates`.
`kdoctools_install(po)` still builds the handbook in every translated language; kpkg's language
rule removes `/usr/share/doc/HTML/<lang>/` from the package.
