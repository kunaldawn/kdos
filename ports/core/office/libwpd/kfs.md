## About
libwpd reads WordPerfect documents and replays them through the
[librevenge](#port-librevenge) document interface. [libreoffice](#port-libreoffice) and
[abiword](#port-abiword) use it to import `.wpd` files, and [libwpg](#port-libwpg), the
WordPerfect graphics importer, builds on it. The package also installs `wpd2text`, `wpd2html` and
`wpd2raw`.

## Flags
- `--enable-tools` — Build and install the three command-line converters under `src/conv`. It is
  configure's default.
- `--disable-fuzzers` — Build no fuzzing programs; also the default.
- `--disable-werror` — Do not treat compiler warnings as errors; also the default.
- `--without-docs` — Generate no Doxygen API documentation.

## Patches
- `libwpd-gcc11.patch` — Include `<cstddef>` at the top of `src/lib/WPXContentListener.cpp` and `src/lib/WPXTable.cpp`. Both compile code that uses `size_t` unqualified (`WPXTable.h` declares `getCell(size_t, size_t)` after including only `<vector>`), and libstdc++'s `<vector>` does not declare it, so the two files fail to compile without it.
