## About
Ghostscript interprets PostScript and PDF and renders them to a printer's language, a raster or
another document format. In KDOS it is where print jobs and OCR pipelines end: the CUPS filters
turn a PDF into the raster or page description a printer takes through it, and
[ocrmypdf](#port-ocrmypdf) and other document tools call `gs`. This build installs Ghostscript
as a shared library, `libgs`, with `gs` as a small program that loads it.

## Flags
- `--with-system-libtiff` — Link the [libtiff](#port-libtiff) port. With the bundled `brotli`,
  `freetype`, `jbig2dec`, `jpeg`, `libpng`, `tiff`, `zlib` and `openjpeg` directories deleted
  first (the `rm -rf`), configure finds the system copy of each. Ghostscript decides between its
  copy and the system's by whether the directory exists, and refuses to mix them ("Mixing local
  libtiff with shared libjpeg not supported"), so the directories are removed rather than
  switched off. The machine then carries one copy of each library, the one its port updates.
- `--disable-gtk` — Build `gsx`, the loader that would show pages in a GTK window, from the same
  text-only `dxmainc.c` as `gsc`, so it links no GTK.
- `--with-drivers=ALL` — Compile every output device into `libgs`, printers included.
- `--without-x` — Build no X11 display device.
- `--enable-fontconfig` — Find system fonts through [fontconfig](#port-fontconfig) for fonts a
  document names but does not embed.
- `--enable-cups` — Build the `cups` raster output device, which writes CUPS raster for the
  printer drivers, and with `--with-pdftoraster` CUPS' `pdftoraster` filter.
- `--enable-openjpeg` — Decode JPEG 2000 images in PDFs with [openjpeg](#port-openjpeg).
- `--with-jbig2dec` — Decode JBIG2 images with [jbig2dec](#port-jbig2dec).
- `--with-libidn` — Prepare Unicode PDF passwords with [libidn](#port-libidn) (SASLprep).
- `--with-libpaper` — Take the default paper size from [libpaper](#port-libpaper).
- `--disable-dbus` — Do not use D-Bus to talk to other services.
- `CC="gcc -std=gnu11"` — Ghostscript's build adds `CFLAGS` to its C++ compiles too, so the C
  standard is moved out of `CFLAGS` (`CFLAGS="${CFLAGS/-std=gnu11/}"`) into `CC`, which only the
  C compiler runs as; see
  [C flags handed to the C++ compiler](../../../../docs/kdos/05-developer/build-troubleshooting.md#c-flags-handed-to-the-c-compiler).

## Patches
- `sprintf-guard.patch` — Add `#undef sprintf` after the includes of `base/mkromfs.c` and `cups/gdevcups.c`. Both `#define sprintf sprintf` before their includes so that Ghostscript's `stdio_.h` leaves `sprintf` alone instead of turning it into the poison name `DO_NOT_USE_SPRINTF`; the fortified `<stdio.h>` this tree compiles against, which `stdio_.h` includes before that check, `#undef`s `sprintf`, so the guard is gone and the files' own `sprintf()` calls would reach the poison name.

## Notes
`make so` and `make soinstall` build and install the shared-library form: `libgs.so` and the
loaders `gsc` and `gsx`. `build.sh` links `gs` to `gsc`, because the shared install names its text loader `gsc`
and every caller runs `gs`. With every driver compiled in, `--enable-dynamic` (loadable drivers)
would add nothing; upstream deprecates it and refuses it beside its default hidden visibility.

Not every bundled directory goes. `lcms2mt` stays because Ghostscript uses its own thread-safe
fork of Little CMS, which the system `lcms2` is not; `leptonica` and `tesseract` stay because they
are the OCR device, locked to Ghostscript's version of them, and configure builds that device only
from its own source directories. `extract` (the Office document writers) and `ijs` stay as well;
no port in the tree builds either. These copies are compiled into `libgs`.
