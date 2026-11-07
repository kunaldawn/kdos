## About

libodfgen implements [librevenge](#port-librevenge)'s drawing, presentation, spreadsheet and text
interfaces by writing OpenDocument: an import filter that reads a foreign format through
librevenge hands its calls to libodfgen and gets an ODF document back. Calligra's import filters
for Keynote, Microsoft Works and the other librevenge formats convert through it
([calligra](#port-calligra)).

## Flags

- `--disable-static` — Shared library only; nothing links the archive.
- `--disable-werror` — Upstream's default, stated: warnings stay warnings, so a newer compiler's
  new warning cannot stop the build.
- `--disable-debug` — Upstream's default, stated. It is what puts `-DNDEBUG` on the compile line;
  `--enable-debug` adds `-DDEBUG -g`.
- `--without-docs` — No Doxygen API reference, which the package would drop as an HTML manual.

## Patches

The patch carries a `Subject:` line. It is Alpine's `gcc-15.patch`.

## Notes

`libxml2` from [libxml2](#port-libxml2) and the `librevenge-0.0` and `librevenge-stream-0.0`
modules are required by `configure`.
