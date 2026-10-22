## About

libdvbpsi decodes and generates the PSI and SI tables of an MPEG transport stream: the program
association and program map tables, and DVB's service, event and time tables. [VLC](#port-vlc)'s
transport-stream demuxer is built on it, and without it VLC plays no `.ts` or `.m2ts` file and no
DVB stream.

## Flags

- `--enable-release` — Compile without `-DDVBPSI_DIST`, which configure adds to `CFLAGS` unless
  asked for a release build.
  The macro is read only by the example programs, to choose between the in-tree headers and the
  installed ones, so it changes nothing in the library.
- `--disable-debug` — No debug build. It is upstream's default.
- `--disable-gcc-sanitize` — No AddressSanitizer build. It is upstream's default.

## Patches

- `dr_7c-realloc.patch` — Corrects the AAC descriptor decoder in `src/descriptors/dr_7c.c` and the TOT generator in `src/tables/tot.c`. The decoder refuses a descriptor of length 0, and one shorter than three bytes whose type flag is set, before reading the bytes such a descriptor does not have; and it assigns the result of its `realloc` back to the pointer it then fills, which otherwise goes on pointing at the block `realloc` may have freed. The generator writes the high bits of `descriptors_loop_length` shifted right by 8 instead of left.
