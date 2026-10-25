## About

CHMLib reads Microsoft Compiled HTML Help (`.chm`) files, the format of Windows help and of many
ebooks: it lists the files packed inside one and decompresses them. [calibre](#port-calibre) depends
on it for `pychm`, the Python binding it reads CHM books through, which calibre's recipe builds into
its bundle. The release tarball is taken from Debian's archive, and the port also builds the example
programs, among them `extract_chmLib`, which unpacks a `.chm` into a directory.

## Flags

- `--enable-pthread` — Builds the thread-safe library, which guards each open file's state with
  mutexes. It is the default; the recipe states it.
- `--enable-pread` — Reads with `pread()` instead of an `lseek()` and `read()` pair, which does not
  move a shared file position. Also the default.
- `--disable-io64` — Builds without the `pread64` and `lseek64` calls of the 64-bit path. musl's
  `off_t` is 64 bits wide, and musl exports no `pread64` or `lseek64` symbol for that path to link:
  under `_LARGEFILE64_SOURCE` its headers only define each name as the plain function. configure
  would otherwise probe for `lseek64`, fail to link it, and turn the path off with a warning.
- `--enable-examples` — Builds and installs `extract_chmLib`, `enum_chmLib`, `enumdir_chmLib`,
  `chm_http`, which serves a `.chm` over HTTP to a browser, and `test_chmLib`.

## Patches

- `0001-chmlib-implicit-declarations.patch` — Debian's patch, with a DEP-3 `Description:` header rather than a `Subject:`. It adds `#include <unistd.h>` and `#include <arpa/inet.h>` to `src/chm_http.c`, which calls functions from both without including them, and defines `_LARGEFILE64_SOURCE` at the top of `src/chm_lib.c` so the 64-bit file functions are declared when that path is built.

## Notes

The path built here passes each read offset to `pread()` cast to `unsigned int`, so the library
cannot read past the first 4 GiB of a file.
