## About

Filelight shows where disk space goes: each folder is drawn as a ring of segments around its
parent, sized by what it holds, and a click descends into one. It is a KDE application on Qt 6,
with a QML interface on [kirigami](#port-kirigami), drawn through
[qqc2-desktop-style](#port-qqc2-desktop-style) so that it takes the Qt palette.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — The handbook, through
  [kdoctools](#port-kdoctools), which upstream looks for as optional; required, a missing one
  stops the configure instead of leaving the handbook out.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Patches

- `app-id.patch` — In upstream's desktop entry, replaces `MimeType=inode/directory;` with `StartupWMClass=org.kde.filelight`. The class names the app_id Filelight's window carries, its desktop file name. The MIME type is dropped because with it a folder could open in the disk-usage view instead of the file manager.

## Notes

Kirigami and qqc2-desktop-style are QML modules imported at run time: upstream's CMake only
reports them (`TYPE RUNTIME`, `ecm_find_qmlmodule`), so `depends` is what puts them on the system.
