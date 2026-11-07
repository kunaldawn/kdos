## About

KItemViews adds to Qt's item views: a search line that filters a list or tree as one types
(`KTreeWidgetSearchLine`, `KListWidgetSearchLine`), a categorised list view and its drawer.
[kio](#port-kio), [kxmlgui](#port-kxmlgui) and many KDE applications link it.

## Flags

- `-D BUILD_DESIGNERPLUGIN=OFF` — Build no Qt Designer plugin for the widgets, which upstream
  builds by default.
- `-D KF_SKIP_PO_PROCESSING=ON` — Build none of the Qt translation catalogues under `poqm/`,
  which `ecm_install_po_files_as_qm()` would compile into `.qm` files. The strings in the source
  are English, and the package-wide English-only rule (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
  would remove every catalogue in another language.
