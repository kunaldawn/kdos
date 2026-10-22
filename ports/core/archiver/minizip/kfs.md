## About
[minizip]{.idx} is the small ZIP library in zlib's `contrib/minizip`: `libminizip`, with the
`<minizip/zip.h>`, `<minizip/unzip.h>` and `<minizip/ioapi.h>` headers that
[chromium](#port-chromium), [qt6-qtwebengine](#port-qt6-qtwebengine),
[keepassxc](#port-keepassxc), [wireshark](#port-wireshark), [vlc](#port-vlc) and the other ports
that name it in `depends` include. It is built from the zlib release archive itself, which is why
the recipe's `source` saves zlib's tarball under the port's own name, and the version is the zlib
release it comes from.

The zlib-ng fork of the library is the separate [minizip-ng](#port-minizip-ng) port, which
installs under its own names, so the two can be installed together.

## Notes
`build.sh` works in `contrib/minizip` alone; the rest of the zlib tree is not built, and the
library links the [zlib](#port-zlib) port. That directory carries a `configure.ac` and a
`Makefile.am` but no generated `configure`, so `autoreconf -fi` makes one, after
`install-ints-header.patch` has edited `Makefile.am`.

Both patches carry their own description. `install-ints-header.patch`, from zlib's pull request
1165, adds `ints.h` to the installed headers: `ioapi.h` includes it, and without it a consumer's
`#include <minizip/ioapi.h>` fails. `zlib-1.2.8-minizip-include.patch` makes `crypt.h` include
`zconf.h` for the `z_crc_t` type it uses.
