## About

KMime parses and writes MIME messages: e-mail and news articles with their headers, multipart
bodies, attachments, character sets and content transfer encodings, as RFC 2822, 2045-2049 and
their successors define them. It is the message model of the KDE PIM stack ([kmail](#port-kmail),
[mimetreeparser](#port-mimetreeparser), [kmbox](#port-kmbox)) and of
[kleopatra](#port-kleopatra)'s mail handling. It needs only Qt Core and
[kcodecs](#port-kcodecs).

## Flags

- `-D BUILD_FUZZERS=OFF` — Build none of the fuzzing harnesses. It is upstream's default, written
  out so the recipe states it.
- `-D KF_SKIP_PO_PROCESSING=ON` — Build none of the Qt translation catalogues under `poqm/`,
  which `ecm_install_po_files_as_qm()` would compile into `.qm` files. The strings in the source
  are English, and the package-wide English-only rule (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
  would remove every catalogue in another language.
