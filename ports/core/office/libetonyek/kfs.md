## About

libetonyek reads Apple's iWork formats, Keynote presentations above all and Pages and Numbers
documents too, through [librevenge](#port-librevenge)'s interfaces. Calligra's Keynote import
filter is its consumer, writing ODF through [libodfgen](#port-libodfgen)
([calligra](#port-calligra)). The package also carries the command-line converters `key2*`,
`numbers2*` and `pages2*`, which write raw calls, text, CSV, HTML or XHTML.

## Flags

- `--disable-static` — Shared library only; nothing links the archive.
- `--disable-werror` — Upstream's default, stated: warnings stay warnings.
- `--disable-debug` — Upstream's default, stated. It is what puts `-DNDEBUG` on the compile line.
- `--disable-tests` — No unit tests. Upstream's default builds them, and `configure` then requires
  CppUnit.
- `--disable-fuzzers` — No fuzzing harnesses; upstream's default, stated.
- `--with-mdds=3.0` — The mdds API version to build against. `configure` looks for the pkg-config
  module `mdds-<version>` and defaults to `mdds-2.1`; [mdds](#port-mdds) 3 installs `mdds-3.0`.
- `--with-liblangtag` — Language tags through [liblangtag](#port-liblangtag), upstream's default,
  stated. It maps the language codes in a document's styles to proper BCP 47 tags; without it
  every text is left without a language.
- `--with-tools` — The conversion tools.
- `--without-docs` — No Doxygen API reference.

## Patches

The patch carries a `Subject:` line. It is Alpine's `mdds3.patch`.

## Notes

The headers of [boost](#port-boost) and [glm](#port-glm), and the `mdds-3.0`, `libxml-2.0`,
`zlib` and `librevenge-0.0` modules are required by `configure`. The token tables are generated
by `gperf` from the `.gperf` files, and the tarball ships the generated `.inc` files too, so
[gperf](#port-gperf) is in `depends` for the case where a timestamp makes `make` regenerate one.
