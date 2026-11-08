## About

libkleo is the KDE PIM library for key management over GnuPG, through
[gpgmepp](#port-gpgmepp) and [qgpgme](#port-qgpgme): the key cache, the key selection and
certificate widgets, the formatting of keys and validity for display, and the audit-log viewer.
[kleopatra](#port-kleopatra), [mimetreeparser](#port-mimetreeparser) and the KDE mail programs
link it.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`; the image is
  English only (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D BUILD_QCH=OFF` — No API documentation in Qt Help format. Upstream's default, named.
- `-D USE_UNITY_CMAKE_SUPPORT=OFF` — Compiles each source file on its own instead of in CMake
  unity batches. Upstream's default, named.

## Notes

The KDE PIM libraries require one another at the same version: `messagelib` and `merkuro` find
libkleo, [kmbox](#port-kmbox) and [mimetreeparser](#port-mimetreeparser) at exactly their own
release, so the four are bumped together.
