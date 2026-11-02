## About

Kompare compares two files or folders, or reads a diff or patch, and shows the differences side by
side, with each change linked between the panes. Hunks can be applied to or removed from the
destination one at a time, and a patch can be saved. Its comparison view is also a KParts component
other programs embed.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`, which kpkg's
  English-only rule would remove from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Patches

The desktop entry's `StartupWMClass` is upstream's `kompare`, but the window's Wayland app_id is
`org.kde.kompare`, the desktop file name that KAboutData derives from the `apps.kde.org` home page
and the component name; `app-id.patch` sets the class to the app_id so the task manager matches the
window to its entry.

## Notes

Parsing diffs and comparing files is [libkomparediff2](#port-libkomparediff2), required at 6.0 or
later; the two move to a new release together. Kompare installs two shared libraries of its own,
`libkomparedialogpages` and `libkompareinterface`, which its part and its application share.

`kdoctools_install(po)` still builds the handbook in every translated language; kpkg's language
rule removes `/usr/share/doc/HTML/<lang>/` from the package.
