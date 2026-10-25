## About

c-ares is a C library for asynchronous DNS queries: a program sends lookups and reads the answers
from its own event loop, without a thread blocked in `getaddrinfo`. It reads `/etc/resolv.conf` and
`/etc/hosts` itself. The package also installs two small query programs, `adig` and `ahost`.

Its consumers in the tree are [nodejs](#port-nodejs), built `--shared-cares` against this library in
place of the copy in Node's own source tree, and [wireshark](#port-wireshark), which uses it to
resolve the addresses in a capture to names. [curl](#port-curl) can use c-ares as its resolver and
is not built that way: it uses its threaded resolver.

## Flags

- `-DCMAKE_C_FLAGS_RELEASE` — set to `"$CFLAGS -DNDEBUG"`, it replaces CMake's Release flags, `-O3
  -DNDEBUG`, which come after the tree's `CFLAGS` on the compile line and would raise the library to
  `-O3`. With the replacement it is compiled at the tree's own level with assertions still off; a
  recipe that sets the Release flags keeps `-DNDEBUG` in them, as
  [cmake](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#cmake)
  in Writing ports says.
- `-DCMAKE_INSTALL_LIBEXECDIR=lib` — the `GNUInstallDirs` location for helper programs. c-ares's
  CMake build installs nothing there, so this switch does not change the package.
