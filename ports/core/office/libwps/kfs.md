## About

libwps reads Microsoft Works documents (word processor, spreadsheet and database, from the DOS
versions to Works 9) and the Lotus 1-2-3, Quattro Pro and other old spreadsheet formats it shares
code with, through [librevenge](#port-librevenge)'s interfaces. Calligra's Works import filter is
its consumer ([calligra](#port-calligra)). The package also carries the command-line converters
`wps2html`, `wps2text`, `wps2raw`, `wks2csv`, `wks2text` and `wks2raw`.

## Flags

- `--disable-static` — Shared library only; nothing links the archive.
- `--disable-werror` — Upstream's default, stated: warnings stay warnings.
- `--disable-debug` — Upstream's default, stated. It is what puts `-DNDEBUG` on the compile line.
- `--enable-tools` — The conversion tools, upstream's default, stated. They need the
  `librevenge-generators-0.0` and `librevenge-stream-0.0` modules, which
  [librevenge](#port-librevenge) installs.
- `--disable-fuzzers` — No fuzzing harnesses; upstream's default, stated.
- `--without-docs` — No Doxygen API reference.
