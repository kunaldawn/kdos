## About

[ImageMagick]{.idx} converts, edits and composes raster images from the command line (`magick`,
with `convert`, `mogrify`, `identify` and the other classic names) and through its MagickCore and
MagickWand libraries. Many ports depend on it, among them
[darktable](#port-darktable), [digikam](#port-digikam), [libvips](#port-libvips) and
[zbar](#port-zbar).

## Flags

- `ac_cv_path_PSDelegate=gs` — Name the Ghostscript delegate by its bare name. Left to itself,
  configure looks for `gs` on the build machine and, finding it, writes its absolute path into
  `delegates.xml` and adds a `type-ghostscript.xml` font table naming
  `/usr/share/ghostscript/fonts/`, which the [ghostscript](#port-ghostscript) port does not
  install. Pinned, the delegates run `gs` from `PATH`, and no font table names files that are not
  there, whatever was built first.
- `--without-gcc-arch` — Compile for the baseline the rest of the tree uses. Otherwise configure
  guesses the build machine's processor and adds `-march`/`-mtune` for it.
- `--without-modules` — Build every coder into the libraries instead of as loadable modules,
  upstream's default.
- `--with-threads` — POSIX threads, so operations run on several cores.
- `--with-png`, `--with-jpeg`, `--with-tiff`, `--with-webp`, `--with-zlib`, `--with-bzlib`,
  `--with-lzma`, `--with-zstd`, `--with-zip`, `--with-xml`, `--with-freetype`,
  `--with-fontconfig`, `--with-pango` — Each delegate named. configure answers a missing library
  by disabling the feature rather than failing, so a delegate left to detection gives a build that
  succeeds and cannot read the format, or one that has it only because its port happened to be
  built first.
- `--with-lcms` — The ICC colour engine, [lcms2](#port-lcms2). Without it `-profile` is accepted
  and ignored.
- `--with-heic` — HEIC and AVIF, through [libheif](#port-libheif); what a phone camera writes.
- `--with-jxl` — JPEG XL, through [libjxl](#port-libjxl).
- `--with-raw` — Camera RAW files (CR2, NEF, ARW, DNG) through [libraw](#port-libraw).
- `--with-openjp2` — JPEG 2000, through [openjpeg](#port-openjpeg).
- `--with-openexr` — OpenEXR high-dynamic-range images.
- `--with-rsvg` — SVG through [librsvg](#port-librsvg) instead of ImageMagick's internal MSVG
  renderer.
- `--with-raqm` — Shaped, bidirectional text for `label:`, `caption:` and `-annotate`, through
  [libraqm](#port-libraqm); without it Arabic and Indic scripts come out as unjoined glyphs.
- `--with-fftw` — The `-fft` and `-ift` operators, through [fftw](#port-fftw).
- `--with-djvu` — DjVu documents, through [djvulibre](#port-djvulibre).
- `--with-jbig` — JBIG bi-level images, through [jbigkit](#port-jbigkit).
- `--without-x` — No X11 display (`display`, `animate`, `import`) and no X11 delegates.
- `--without-perl` — No PerlMagick.
- `--without-lqr`, `--without-uhdr`, `--without-dmr`, `--without-fpx`, `--without-wmf` — No
  liquid rescale, no Ultra HDR, no MagickCache digital media repository, no FlashPIX and no
  Windows Metafile support. liblqr, libultrahdr, MagickCache, libfpx and libwmf are not ports.
- `--without-gslib`, `--without-gvc` — Do not link the Ghostscript library or Graphviz's `gvc`.
  PostScript and PDF still go through the `gs` delegate.
- `--disable-docs` — Install no HTML documentation; `build.sh` checks afterwards that
  `/usr/share/doc/ImageMagick-7/www` is absent.

## Notes

`/etc/ImageMagick-7/type.xml` is the one font table ImageMagick reads, and it includes no other,
so every font ImageMagick names comes from fontconfig, which `--with-fontconfig` builds in:
`magick -list font` lists every face the system has, and `-font DejaVu-Sans` or a file path
selects one. configure writes an include for each upstream table whose font directory it finds or
is given, and `make install` installs every table, `type-apple.xml`, `type-dejavu.xml`,
`type-ghostscript.xml`, `type-urw-base35.xml`, `type-urw-base35-type1.xml` and
`type-windows.xml`, whether `type.xml` includes it or not. `build.sh` deletes them: with no
include they are read by nothing, and each names font files that no port installs, so a reader
of `/etc/ImageMagick-7` would take them for fonts the system has. It first checks that
`type.xml` holds no `<include`, and the build fails if it does: configure finds a font directory
by the files in it, and a tree that gained one would otherwise ship a `type.xml` naming a table
the package no longer has. `type-dejavu.xml` is not a table
of DejaVu faces: it maps the PostScript base names (`Helvetica`, `Times-Roman`, `AvantGarde-Book` and
the rest) to URW base-35 files such as `NimbusSans-Regular.ttf`, which no port installs, so the
recipe passes no `--with-dejavu-font-dir`, and none of the directories configure searches for it
exists here. A PostScript base name is therefore no font: `-font Helvetica` stops with `unable to
read font`, and `Liberation-Sans`, `Liberation-Serif` and `Liberation-Mono`, metric-compatible with
Helvetica, Times and Courier, are the names to use instead.

Upstream's release numbers carry a hyphen before the last field, which a KDOS version may not (see
[kpkgbuild](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#kpkgbuild)). The recipe's
`version` is the dotted form, and the `_imver` helper rebuilds the hyphenated one for the
`source` URL.
