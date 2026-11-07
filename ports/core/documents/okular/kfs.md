## About

Okular is KDE's document viewer: PDF through [poppler](#port-poppler), PostScript through
[libspectre](#port-libspectre), DjVu, EPUB through [ebook-tools](#port-ebook-tools), XPS, comic
book archives, Mobipocket, Markdown through [discount](#port-discount), plain text and images, with
annotations, forms, text-to-speech and presentation mode. Its KPart is the document view other
KDE programs embed.

## Flags

- `-D_GNU_SOURCE` — added to `CFLAGS`. The SyncTeX parser, the one C source in the tree, calls
  `asprintf` and `vasprintf`, which musl declares only under `_GNU_SOURCE`.
- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`; the image is
  English only (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D OKULAR_UI=desktop` — Builds the desktop interface only, not the Kirigami mobile one.
  Upstream's default off Android, named.
- `-D USE_DBUS=ON` — Builds the D-Bus parts: opening a document in the instance already running,
  and the presentation-mode screen saver inhibition. Upstream's default on Linux, named.

## Notes

The loop at the end of `build.sh` removes every directory under `/usr/share/man` that is not a
`man<section>` one, where `kdoctools_install()` puts the translated manual pages; kpkg's
language rule removes the translated handbooks.
