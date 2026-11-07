## About

markdownpart is a KParts component that renders Markdown documents. Kate's Markdown preview
plugin and Konqueror embed it to show a `text/markdown` file as formatted text, and follow links
between documents inside it.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`, which kpkg's
  English-only rule would remove from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Notes

Rendering is Qt's own: the part hands the text to `QTextDocument::setMarkdown()`, which parses it
with the md4c copy bundled in [qt6-qtbase](#port-qt6-qtbase). No Markdown library is a dependency,
and the part follows whatever Markdown dialect that Qt release implements.
