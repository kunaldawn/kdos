## About

KBookmarks reads and writes bookmarks in the XBEL format and builds the bookmark menus and the
"Add Bookmark" actions of KDE programs from them. [kio](#port-kio) keeps the Places panel of the
file dialogs and of [dolphin](#port-dolphin) in `~/.local/share/user-places.xbel` through it, and
[konsole](#port-konsole) and [kate](#port-kate) use its menus.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Build none of the Qt translation catalogues under `poqm/`,
  which `ecm_install_po_files_as_qm()` would compile into `.qm` files. The strings in the source
  are English, and the package-wide English-only rule (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
  would remove every catalogue in another language.
