## About
[UnZip]{.idx} is Info-ZIP's extractor: `unzip`, `unzipsfx`, `funzip`, `zipinfo` and the
`zipgrep` script. It is the command a user extracts a ZIP archive with, and many ports name it in
`depends`; [noto-fonts](#port-noto-fonts) and [josm](#port-josm), for example, unpack ZIP
archives with it in their `build.sh`.

Info-ZIP has made no release since this version, so every fix since exists only as a
distribution's patch. The numbered series beside the recipe is Alpine's, applied in Alpine's
order, which the numbers keep: each patch is a diff from the top of the tree and may depend on the
ones before it. It carries the CVE fixes for heap and buffer overflows and the check that refuses
a "zip bomb" built from overlapping entries.

## Flags
- `unzips` — the make target: build `unzip`, `funzip` and `unzipsfx` with the flags given on the
  command line, and never run `unix/configure`. Nothing is detected, so every feature the build
  has is one of the definitions below.
- `CFLAGS="$CFLAGS"` — the tree's flags, in place of the makefile's own `-O`.
- `LF2="$LDFLAGS"` — the tree's linker flags, in place of the makefile's `-s`, which would strip
  the programs at link time.
- `LD="$CC"` — link with the compiler. The makefile requires `CC` and `LD` to be the same program.
- `D_USE_BZ2=-DUSE_BZIP2` — with `L_BZ2=-lbz2`: support the bzip2 compression method, linked
  against the [bzip2](#port-bzip2) port's library. Both are commented out in the makefile, whose
  comment says they must be set to turn bzip2 support on.
- `LOCAL_UNZIP` — the definitions added to every compile line. `-DLARGE_FILE_SUPPORT` turns on
  64-bit file offsets and, with them, `ZIP64_SUPPORT`, so archives and members over 4 GB open.
  `-DUNICODE_SUPPORT`, `-DUNICODE_WCHAR` and `-DUTF8_MAYBE_NATIVE` read the UTF-8 names an
  archive records and write them as UTF-8, so a non-ASCII name is not mangled. `-DBSD`
  selects UnZip's own BSD code paths; on that path `unix/unxcfg.h` includes `<sys/time.h>` and
  `<sys/timeb.h>` and adds `<time.h>` only for glibc, so `-include time.h` puts `<time.h>` ahead of
  every source file for musl. `-D_BSD_SOURCE` and `-D_DEFAULT_SOURCE` select the BSD and default
  interfaces of the C library, and `-D_LARGEFILE64_SOURCE` and `-D_FILE_OFFSET_BITS=64` restate
  the 64-bit interface musl always has.
- `prefix=$PKG/usr` — with `MANDIR=$PKG/usr/share/man/man1`: where `make install` writes. The
  makefile takes no `DESTDIR`, so the staging directory goes into the prefix, and its manual
  directory is otherwise `$(prefix)/man/man1`.

## Patches
- `06-unzip-6.0-exec-shield.patch` — adds an empty `.note.GNU-stack` section to `crc_i386.S`, so an object assembled from it asks for no executable stack. The `unzips` target assembles that file only when `CRCA_O` names its object, and `build.sh` does not set it.
- `07-unzip-6.0-format-secure.patch` — passes each message `Info()` prints through a `"%s"` format in `extract.c`, `list.c` and `zipinfo.c`, instead of using the message itself as the format string.
- `10-CVE-2014-8139.patch` — checks that a PKVMS extra-field block is at least four bytes long before computing its CRC, and reports a block that is too short (CVE-2014-8139).
- `11-CVE-2014-8140.patch` — in `test_compr_eb()`, treats a compressed extra field that declares an uncompressed size of zero as bad data (CVE-2014-8140).
- `12-CVE-2014-8141.patch` — makes `getZip64Data()` in `process.c` check that enough bytes are left before reading each Zip64 field, and makes its caller in `fileio.c` warn that the extra field is corrupt when it fails (CVE-2014-8141).
- `16-CVE-2018-1000035.patch` — sizes the buffer for the password prompt from the lengths of the two file names it prints, instead of a fixed `2*FILNAMSIZ + 15` bytes the names can overrun (CVE-2018-1000035).
- `17-CVE-2018-18384.patch` — widens the buffer `list_files()` formats the compression factor into from 10 to 13 bytes (CVE-2018-18384).
- `30-gcc15.patch` — declares `gmtime()` and `localtime()` in `unix/unxcfg.h` with their `const time_t *` parameter, the prototype `<time.h>` gives them, instead of with an empty parameter list. That declaration is on the branch of `#ifdef BSD` that this build's `-DBSD` does not take.

## Notes
`29-zipbomb-switch.patch` makes the zip-bomb check switchable: with
`UNZIP_DISABLE_ZIPBOMB_DETECTION=TRUE` in the environment, `unzip` extracts an archive with
overlapping entries instead of refusing it.
