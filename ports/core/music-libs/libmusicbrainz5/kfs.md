## About

libmusicbrainz5 is the client library for the [MusicBrainz]{.idx} web service: given a disc ID or a
search, it returns the artist, release and track metadata the MusicBrainz database holds. It talks
HTTP through neon and parses the replies with libxml2. libkcddb, KDE's CD database library, uses it,
and through libkcddb k3b looks up an audio CD's track names.

## Patches

- `custom-command-inc-depends.patch` — names the seven `c-int-*.inc` files in the `DEPENDS` of the custom command that generates the C interface (`mb5_c.cc`, `mb5_c.h`), instead of `*.inc`. CMake does not expand a wildcard there, so the generated build waits on a file literally called `*.inc`, which Ninja refuses as missing with no rule to make it.
- `libxml2-const-error.patch` — includes `<libxml/parser.h>`, which declares `xmlParseFile()` and `xmlParseMemory()`, in `src/xmlParser.cc`, and holds `xmlGetLastError()`'s result as `const xmlError *`, the type libxml2 returns; C++ refuses to convert it to the non-const `xmlErrorPtr` the code declared.

## Notes

`src/xmlParser.cc` is compiled twice: into the library and into `make-c-interface`, the helper
program the build runs to generate the C interface from `cinterface.xml`. Both patches are needed
before the library itself is reached. `build.sh` checks that `libmusicbrainz5.so.1`,
`libmusicbrainz5.pc` and the generated `mb5_c.h` were installed.
