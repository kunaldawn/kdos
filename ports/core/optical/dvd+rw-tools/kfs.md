## About
dvd+rw-tools writes, formats and inspects DVD and Blu-ray discs. `growisofs` burns a disc, either
from an image or by running `mkisofs` and writing its output as it is produced;
`dvd+rw-format`, `dvd+rw-booktype`, `dvd+rw-mediainfo` and `dvd-ram-control` format media, set
its book type, report what is in the drive and control DVD-RAM drives. [k3b](#port-k3b) and
[brasero](#port-brasero) run `growisofs` to write DVDs. The `mkisofs` it runs is the link to
`xorriso` that [libisoburn](#port-libisoburn) installs.

## Patches
- `fix-sysmacros-header.patch` — Include `<sys/sysmacros.h>` in `growisofs.c`, which uses `major()` and `minor()` on device numbers. musl declares them only there, not through `<sys/types.h>`.
- `transport.hxx.patch` — Size the buffer `plusminus_locale()` gives to `wctomb()` as `MB_LEN_MAX`, the longest multibyte character the C library can produce, instead of a fixed four bytes, and include `<limits.h>` to define it.

## Notes
There is no configure step. The top-level `Makefile` pipes `Makefile.m4` through
[m4](#port-m4), with the operating system's name from `uname -s`, into a second `make` that
builds the programs, so `m4` is in `depends`. [perl](#port-perl) is the interpreter of
`genasctable.pl`, the rule that regenerates `asctable.h` from `keys.txt`. The tarball's
`asctable.h` is a second newer than `keys.txt`, so the rule runs only when `keys.txt` changes.
The Linux section of `Makefile.m4` sets `CC = gcc` and `CXX = g++` outright and appends its flags to `CFLAGS` and `CXXFLAGS`, so the build takes the tree's
compiler flags but not a different compiler named in `$CC`.

The upstream `install` target has no `DESTDIR` and copies straight into `$(prefix)`, so
`build.sh` installs the five programs and `growisofs.1` into `$PKG` itself.
