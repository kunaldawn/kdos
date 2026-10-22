## About
[DjVuLibre]{.idx} is the reference implementation of DjVu, a document format for scanned pages
that stores text, line art and photographs as separately compressed layers. The port installs
`libdjvulibre`, which [Okular](#port-okular), [ImageMagick](#port-imagemagick),
[kio-extras](#port-kio-extras) and [gImageReader](#port-gimagereader) link to read DjVu
files, and the command-line tools: `ddjvu` renders pages to images, `djvused` edits a document's
structure, text and annotations, and `c44` and `cjb2` encode photographs and bilevel scans.

## Flags
- `--with-jpeg`, `--with-tiff` — Link [libjpeg-turbo](#port-libjpeg-turbo) and
  [libtiff](#port-libtiff). Both are link probes, on by default, that build without the library
  when the probe fails, and say so only in the configure summary: without JPEG, the library
  cannot decode a layer stored as JPEG and `c44` cannot read a JPEG file; without TIFF, `ddjvu` cannot
  write TIFF and `cjb2` cannot read it. Both libraries are in `depends`.
- `--enable-xmltools` — Build `djvutoxml` and `djvuxmlparser`, which export a document's text
  and annotations as XML and apply them back. It is upstream's default, stated.
- `--enable-desktopfiles` — Install the DjVu MIME type definition and its icons. It is
  upstream's default, stated. When configure finds `rsvg-convert` the icons are rasterised from
  `djvu.svg`; otherwise the prebuilt PNGs in the tarball are installed. [librsvg](#port-librsvg)
  is in `depends` for it.

## Patches
- `djvuport-stack-overflow.patch` — Makes `DjVuPortcaster::id_to_file()` remember the id it is opening and throw "recursive opening of the same file (corrupted file?)" when it is asked for the same id again before it returns. A corrupted multi-page document whose component refers to itself otherwise recurses until the stack overflows and the program crashes.
