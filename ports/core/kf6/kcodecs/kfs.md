## About

KCodecs is the KDE Frameworks library of string encodings: base64, base45, quoted-printable,
uuencode and RFC 2047 header encoding, an HTML entity table, e-mail address parsing
(`KEmailAddress`), and charset detection for text of unknown encoding.
[kcompletion](#port-kcompletion), [kconfigwidgets](#port-kconfigwidgets) and the mail and
contacts libraries link it.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Build none of the Qt translation catalogues under `poqm/`,
  which `ecm_install_po_files_as_qm()` would compile into `.qm` files. The strings in the source
  are English, and the package-wide English-only rule (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
  would remove every catalogue in another language.

## Notes

The entity table is a sorted array compiled into the library, so the build runs no generator and
needs nothing beyond Qt Core.
