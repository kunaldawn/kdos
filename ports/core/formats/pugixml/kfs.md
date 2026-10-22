## About
pugixml is a small, fast C++ XML parser with a DOM-style tree and XPath 1.0 queries. Desktop
applications use it to read their XML formats and project files: [libkiwix](#port-libkiwix) reads
its OPDS catalogues with it, and [blender](#port-blender), [darktable](#port-darktable),
[audacity](#port-audacity), [mkvtoolnix](#port-mkvtoolnix), [musescore](#port-musescore) and
[filezilla](#port-filezilla) link it.

## Flags
- `-DBUILD_SHARED_LIBS=ON` — Build `libpugixml.so`. Upstream's default is a static archive, which
  would put a private copy of the parser into every program that links it.
