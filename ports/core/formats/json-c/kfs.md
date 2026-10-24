## About
json-c is a small C library for reading and writing JSON as a tree of reference-counted objects.
Storage and system tools are its main users here: [cryptsetup](#port-cryptsetup) keeps LUKS2
metadata in JSON through it, and [nvme-cli](#port-nvme-cli), [libnvme](#port-libnvme),
[ndctl](#port-ndctl), [crun](#port-crun) and [libvirt](#port-libvirt) link it, as do
[gdal](#port-gdal), [freerdp](#port-freerdp) and [clamav](#port-clamav).

## Flags
- `-DCMAKE_C_FLAGS_RELEASE="$CFLAGS -DNDEBUG"` — Replaces CMake's Release flags, `-O3 -DNDEBUG`,
  with the tree's `CFLAGS` and `-DNDEBUG`, so the library builds at the tree's `-O2` (json-c's own
  `CMakeLists.txt` then appends a further `-O2`). `-DNDEBUG` is kept, so assertions stay off as in
  any Release build.
- `-DCMAKE_INSTALL_LIBEXECDIR=lib` — Where helper programs would be installed; json-c installs none.
- `-DBUILD_STATIC_LIBS=OFF` — Build no `libjson-c.a`. Upstream builds both the static and the shared
  library by default.
- `-DDISABLE_EXTRA_LIBS=ON` — Never link an extra library for `arc4random()`. musl has no
  `arc4random`, so without this switch configure looks for libbsd and links it whenever its header
  is installed, and the library's dependencies would then change with what happened to be
  installed. With it, the hash seed comes from `getrandom()`.
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

