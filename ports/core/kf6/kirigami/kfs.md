## About

Kirigami is KDE's set of Qt Quick components for applications that run on a desktop and on a
phone: pages, drawers, cards, the page row and the toolbar. It is a QML module, `org.kde.kirigami`,
which a Kirigami application draws its whole window with;
[knewstuff](#port-knewstuff)'s Qt Quick dialogs need it at run time, and
[kcmutils](#port-kcmutils) requires it at build time.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Build none of the Qt translation catalogues under `poqm/`,
  which `ecm_install_po_files_as_qm()` would compile into `.qm` files. The strings in the source
  are English, and the package-wide English-only rule (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
  would remove every catalogue in another language.

## Notes

The components' shadows and rounded corners are drawn by shaders, which the build compiles with
`qsb` from [qt6-qtshadertools](#port-qt6-qtshadertools); without it the configure stops.

The configure looks for OpenMP, optional upstream, and finds GCC's `libgomp` from
[gcc](#port-gcc); `Kirigami.ImageColors` then spreads the palette it computes from an image over
several threads.
