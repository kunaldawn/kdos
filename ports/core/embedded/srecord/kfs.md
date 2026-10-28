## About

SRecord converts and manipulates the files that carry firmware images: Intel HEX, Motorola S-record,
TI-TXT, raw binary and several dozen others. `srec_cat` reads any of them, crops, fills gaps, moves
data to another offset, adds CRCs, checksums and message digests, and writes the result in any
format; `srec_cmp` compares two images and `srec_info` describes one. These are the steps between a
linker's output and what a programmer or bootloader accepts.

## Flags

- `-DBUILD_SHARED_LIBS=ON` — Has no effect on this build: the one library, `lib_srecord`, is
  declared `STATIC` by upstream, is linked into the three programs, and is not installed.
- `-DHAVE_LIBGCRYPT=ON`, `-DHAVE_GCRY_MD_HD_T=ON` — Upstream turns these on, with the SHA-224 and
  Whirlpool ones beside them, only when it finds `gcrypt.h`, and has no switch that requires
  [libgcrypt](#port-libgcrypt). `srecord/config.h` needs both before it compiles any of the
  libgcrypt digest filters (MD5, RIPEMD-160, the SHA family and Whirlpool); set outright, they turn
  a missing libgcrypt into a compile error instead of an `srec_cat` without those filters.
- `make srec_cat srec_cmp srec_info srecord` — The three programs by name instead of `all`. The
  `doc` directory is part of every build, and its targets, which `all` includes, run groff and
  ps2pdf to make HTML pages and PDF manuals; neither groff nor Ghostscript is in this port's
  `depends`. `all` also holds the library's `doxygen` target, which runs Doxygen over its headers.
  `srecord` matches no target: make takes it for the build tree's `srecord` directory and does
  nothing for it.

## Patches

- `build-fixes.patch` — Two changes. In `etc/configure.cmake`, `find_package(Doxygen REQUIRED doxygen dot)` loses its `dot` component, so graphviz is not required; it would only draw call and collaboration graphs into the API documentation, which upstream installs under its `website` component and `build.sh` neither builds nor installs. (Doxygen itself stays required by configure, which is why [doxygen](#port-doxygen) is in `depends`.) In `srecord/input/file/hp64k.h`, it adds `#include <cstdint>`, because the header uses `uint16_t` without including anything that declares it, which the tree's GCC rejects.

## Notes

The manual pages are installed by `build.sh` from `doc/man1` and `doc/man5`, not by the `doc`
targets. Upstream's pages pull shared sections in with `.so` requests that name other files of the
source; the `srec_man` function in `build.sh` follows each `.so` and writes the included text in its
place, so every installed page stands alone.
