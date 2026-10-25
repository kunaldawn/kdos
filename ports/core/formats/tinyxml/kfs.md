## About

TinyXML is a small C++ XML parser that reads a document into a tree of objects and writes one back
out. This is TinyXML 1, the original library, not its successor [tinyxml2](#port-tinyxml2), which
has a different API. KDOS carries it for the two programs that link it, [kodi](#port-kodi) and
[opencpn](#port-opencpn).

Upstream ships no way to build the library: its `Makefile` builds only the `xmltest` program, with
the four library sources compiled into it. `build.sh` therefore compiles the four sources itself,
links them into `libtinyxml.so.0`, installs the two headers and writes a `tinyxml.pc` for
pkg-config; the release has neither a library target nor a `.pc` file.

## Flags

- `-DTIXML_USE_STL` — Build the library with STL support: `TIXML_STRING` becomes `std::string`
  instead of TinyXML's own `TiXmlString`, and the `std::string` and stream overloads of the API are
  compiled in. The same macro is set in the installed header by `define-stl.patch`, because the
  choice changes the layout of the classes (their `value`, `name` and other members are
  `TIXML_STRING`), so a program and the library must agree on it.
- `-Wl,-soname,libtinyxml.so.0` — Give the library the shared-object name `libtinyxml.so.0`, which
  a program linked against it records as its dependency. The file itself is
  `libtinyxml.so.0.<version>`, with `libtinyxml.so.0` and `libtinyxml.so` as symbolic links to it.

## Patches

- `define-stl.patch` — Adds `#define TIXML_USE_STL 1` at the top of `tinyxml.h`, so every program that includes the header sees the STL build of the classes that the library was compiled as, without having to pass `-DTIXML_USE_STL` itself.
- `entity.patch` — Removes the special case in `TiXmlBase::EncodeString` that copied a hexadecimal character reference (`&#x…;`) through unchanged. Every `&` is then written as `&amp;`, so text that only looks like a character reference survives a parse and a save intact. It also adds an "Entity escaping" case for this to `xmltest.cpp`.
- `CVE-2021-42260.patch` — In `TiXmlParsingData::Stamp`, advances one byte past a `0xEF` lead byte when either of the two bytes after it is the terminating NUL. Without it the pointer does not move in that case, so a document ending in a truncated three-byte UTF-8 sequence makes the parser loop forever (CVE-2021-42260). The patch is Debian's and carries a `Description:` header rather than a `Subject:`.
