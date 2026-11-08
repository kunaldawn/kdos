## About

KCompletion supplies text completion for Qt programs: the `KCompletion` matcher, which offers
completions from a list of strings, and the line edit and combo box widgets (`KLineEdit`,
`KComboBox`, `KHistoryComboBox`) that use it. [kio](#port-kio)'s file dialogs and
[ktextwidgets](#port-ktextwidgets) build on it.

## Flags

- `-D BUILD_DESIGNERPLUGIN=OFF` — Build no Qt Designer plugin for the widgets. Upstream builds one
  by default, and the configure then requires Qt's `UiPlugin` module from
  [qt6-qttools](#port-qt6-qttools), which this port's `depends` does not name.
- `-D KF_SKIP_PO_PROCESSING=ON` — Build none of the Qt translation catalogues under `poqm/`,
  which `ecm_install_po_files_as_qm()` would compile into `.qm` files. The strings in the source
  are English, and the package-wide English-only rule (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
  would remove every catalogue in another language.
