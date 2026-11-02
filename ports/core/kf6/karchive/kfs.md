## About

KArchive is the KDE Frameworks library for reading and writing archives and compressed files:
tar, zip, 7z and ar archives, and gzip, bzip2, xz and zstd streams through one `QIODevice`
interface. [kiconthemes](#port-kiconthemes), [kdoctools](#port-kdoctools),
[knewstuff](#port-knewstuff), [kio](#port-kio) and the archive-handling applications link it.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Build none of the Qt translation catalogues under `poqm/`,
  which `ecm_install_po_files_as_qm()` would compile into `.qm` files. The strings in the source
  are English, and the package-wide English-only rule (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
  would remove every catalogue in another language.

## Notes

Upstream's own options `WITH_BZIP2`, `WITH_LIBLZMA`, `WITH_OPENSSL` and `WITH_LIBZSTD` are on by
default and make [bzip2](#port-bzip2), [xz](#port-xz), [openssl](#port-openssl) (for encrypted 7z
archives) and [zstd](#port-zstd) requirements of the configure; [zlib](#port-zlib) is required
with no option. A missing one stops the configure rather than leaving its format out.
