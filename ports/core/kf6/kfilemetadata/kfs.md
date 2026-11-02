## About

KFileMetaData reads metadata and text out of files, and writes some of it back: title, author,
duration, resolution, camera settings, page counts and the rest, through one extractor plugin per
family of formats. [baloo](#port-baloo) indexes what it extracts, and [dolphin](#port-dolphin)'s
information panel and tooltips, [elisa](#port-elisa) and [kfind](#port-kfind) read it directly.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_QMobipocket6=ON` — Make QMobipocket, the Mobipocket reader of
  [kdegraphics-mobipocket](#port-kdegraphics-mobipocket), a requirement. Upstream looks for it as
  optional and, when it finds it, builds the extractor for `.mobi` e-books; without it the format
  is left out with only a line in the configure summary. Text extraction from the books
  (`ENABLE_MOBIPOCKET_TEXT_EXTRACTION`) stays at upstream's experimental default, off, so only the
  metadata is read.
- `-D CMAKE_DISABLE_FIND_PACKAGE_libappimage=ON` — Never look for libappimage, which the extractor
  for AppImage bundles needs. libappimage is not a port, and the switch keeps the result from
  depending on what is installed in the build root.
- `-D BUILD_FUZZERS=OFF` — Build none of the fuzzing harnesses. It is upstream's default, written
  out so the recipe states it.
- `-D KF_SKIP_PO_PROCESSING=ON` — Build none of the gettext catalogues under `po/`, which
  `ki18n_install()` would compile into `.mo` files. The strings in the source are English, and the
  package-wide English-only rule (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
  would remove every catalogue in another language.

## Notes

Every other extractor's library is optional upstream and in `depends`, so each is built:
[poppler](#port-poppler) for PDF, [taglib](#port-taglib) for audio tags, [exiv2](#port-exiv2) for
EXIF and XMP in images, [ffmpeg](#port-ffmpeg) for video, and [ebook-tools](#port-ebook-tools) for
EPUB, whose find module also needs [libzip](#port-libzip) and [libxml2](#port-libxml2). The office
formats of ODF and Office Open XML are read through [karchive](#port-karchive). Extended attributes,
where tags, ratings and comments are stored, come from [attr](#port-attr), which the configure
requires on Linux. [catdoc](#port-catdoc) is a run-time dependency: the extractor for Word,
Excel and PowerPoint 97 files runs its `catdoc`, `xls2csv` and `catppt` commands, and without them
those formats give no text.
