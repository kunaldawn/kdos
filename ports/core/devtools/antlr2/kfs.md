## About
[ANTLR 2]{.idx} is the second generation of the ANTLR parser generator: the `antlr` tool, a Java
program that turns a grammar into a lexer and parser, and `libantlr`, the C++ runtime that a
generated C++ parser links. ANTLR 2 grammars are not compatible with later ANTLR releases, so a
program whose grammar is written for it needs this version. [NCO](#port-nco) is the port that
names it in `depends`.

## Flags
- `--enable-java`, `--enable-cxx` — Build the tool, `antlr.jar`, with [openjdk](#port-openjdk)'s
  `javac`, and the C++ runtime library.
- `--disable-python` — Install no Python runtime. The bundled `lib/python` module is Python 2
  code (`print` statements), which the tree's [python3](#port-python3) cannot run.
- `--disable-csharp` — Build no C# runtime; the tree carries no .NET or Mono compiler.
- `--disable-examples` — Build none of the example grammars.
- `prefix=$PKG/usr`, `libdir=$PKG/usr/lib` — The install directories, given to `make install`
  directly. ANTLR's makefiles do not use `DESTDIR`, so the staging root goes into the prefix at
  install time, while `configure` keeps `/usr` for the paths written into the installed `antlr`
  script.

## Patches
- `gcc-headers.patch` — Adds `#include <cstdio>` and `#include <cstring>` to `lib/cpp/antlr/CharScanner.hpp`, which uses `EOF` (for `EOF_CHAR`) and C string functions without including either header, and so compiles only where another header happens to pull them in.
- `reproducible-timestamp.patch` — Makes `configure` take its `TIMESTAMP`, the build date compiled into `antlr.Version` and printed in the tool's version string, from `SOURCE_DATE_EPOCH` instead of today's date, in `configure` and `configure.in` alike.

## Notes
- **The jar is rebuilt.** The `jar-tree` block after `make` unpacks `antlr/antlr.jar`, lists its
  files in a fixed order, and packs them again with the original manifest and every entry dated
  `SOURCE_DATE_EPOCH`. A jar records each entry's modification time, so the one `make` writes
  carries the time of the build; the repacked one is the same from build to build.
- **One copy of the jar.** The install puts `antlr.jar` both in `/usr/lib` and in
  `/usr/share/antlr-<version>`. The installed `antlr` script reads `/usr/lib/antlr.jar`, so
  `build.sh` removes the second copy. It also removes `/usr/share/doc`, where the install puts
  `LICENSE.txt`, `README.txt` and `INSTALL.txt`.
