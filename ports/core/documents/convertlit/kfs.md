## About

ConvertLIT's `clit` unpacks Microsoft Reader `.lit` ebooks into their OEB form, an `.opf` package
with its HTML, images and metadata, which other ebook tools can read or convert further. It also
opens DRM-protected files when given the private keys of a Reader activation in a `keys.txt`. The
source is a zip of two directories, `lib/`, the LIT-reading library, and `clit18/`, the program, and
the port compiles both by hand with the tree's flags. [ebook-tools](#port-ebook-tools) depends on
it: its `lit2epub` script runs `clit`.

## Flags

- `-funsigned-char` — Makes plain `char` unsigned, as upstream's `clit18/Makefile` compiles the
  program; the recipe uses it for every file, the library's too.
- `-Ilzx` — `litsections.c` includes `lzx.h` for the decompressor's interface, `LZXinit()`,
  `LZXreset()` and `LZXdecompress()`. That header is in `lib/lzx/`; `lib/newlzx/`, whose decoder and
  `lzxglue.c` wrapper are the ones the library is built with, has a different `lzx.h`, from
  libmspack, that declares none of them. So `litsections.c` is compiled on its own with `-Ilzx` and
  the rest with `-Inewlzx`.
- `-include string.h` — Includes the header in front of `newlzx/lzxglue.c` and every file of the
  program; `lzxglue.c` and several of the program's files call `memcpy`, `strlen` and the other
  string functions without including it.
- `-include ctype.h`, `-include sys/stat.h` — Likewise for the program, whose files call `isalnum`,
  `mkdir` and the other functions of those headers without including them. GCC treats a call to an
  undeclared function as an error.
- `-ltommath` — Links the system [libtommath](#port-libtommath), which the DRM code in `drm5.c` uses
  for its big-number arithmetic. Upstream's makefile links a `libtommath.a` from a `libtommath-0.30`
  directory beside the source, which the zip does not include.

## Patches

- `bounds.patch` — Corrects two writes one element past the end of an array. In `clit18/clit.c`, the copy of `argv[0]` into `dir_program[MAX_PATH]` is terminated at index `MAX_PATH`, which is outside the array; the patch terminates it at `MAX_PATH-1`. In `lib/newlzx/lzxd.c`, `lzxd_static_init()` fills `extra_bits[]` two entries at a time up to index 51, one more than the 51-entry array holds; the patch makes the array 52 entries long.

## Notes

The recipe does not run upstream's makefiles: they name `gcc`, set their own `CFLAGS` and link
against the `../libtommath-0.30` tree. `build.sh` compiles the same files with `$CC`, `$CFLAGS` and
`$LDFLAGS` instead, and links the program against the library's objects directly rather than through
upstream's `openclit.a` archive. `bounds.patch` has its own `sha256 =` line in the recipe, like a
source.
