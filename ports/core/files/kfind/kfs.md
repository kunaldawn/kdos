## About

KFind searches the filesystem: by name with wildcards, by type, by modification date, by size, by
owner and by the text inside the files, including the text inside archives and documents that
KFileMetaData can read. Results open, move, copy and delete like any file view, and Dolphin's
Tools menu starts it on the folder being shown.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — The handbook, through
  [kdoctools](#port-kdoctools), which upstream looks for as optional; required, a missing one
  stops the configure instead of leaving the handbook out.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

`find_package(Qt6Core5Compat REQUIRED)` makes [qt6-qt5compat](#port-qt6-qt5compat) a requirement
of the configure; the content search decodes text through its `QTextCodec`.
