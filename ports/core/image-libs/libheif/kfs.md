## About

libheif reads and writes [HEIF]{.idx}, the container a phone camera writes its photos in (HEIC) and
the one AVIF uses, with `heif-dec`, `heif-enc`, `heif-info`, `heif-thumbnailer` and a gdk-pixbuf
loader. It does no coding itself: each compression format is a codec library behind a plugin
interface. gimp, darktable, digikam, krita, imagemagick, graphicsmagick, libvips, chafa and the KDE
image-format plugins open HEIC files through it.

## Flags

Every codec is named, on or off, so the codec set is written in the recipe rather than inherited
from upstream's defaults. One decoder and one encoder are built per format:

| Format | Decoder | Encoder |
|---|---|---|
| HEIC (H.265) | `libde265` | `x265` |
| AVIF (AV1) | `dav1d` | `svt-av1` |
| AVC (H.264) | `openh264` | none |
| JPEG in HEIF | `libjpeg-turbo` | `libjpeg-turbo` |
| JPEG 2000 in HEIF | `openjpeg` | `openjpeg` |

- `-DWITH_LIBDE265=ON` — With `WITH_X265`, `WITH_DAV1D`, `WITH_SvtEnc`, `WITH_OpenH264_DECODER`,
  `WITH_JPEG_DECODER`, `WITH_JPEG_ENCODER`, `WITH_OpenJPEG_DECODER` and `WITH_OpenJPEG_ENCODER`,
  build the codecs in the table. Each is a `find_package` that, when its library is missing, prints
  `not found` and builds without the codec, so every one of them is in `depends`. dav1d, svt-av1 and
  the two OpenJPEG codecs are built as plugins in `/usr/lib/libheif`, upstream's default for them;
  the rest are linked into the library.
- `-DWITH_AOM_DECODER=OFF` — With `-DWITH_AOM_ENCODER=OFF`, build no codec on libaom, a second AV1
  decoder and encoder; libaom is not a port.
- `-DWITH_X264=OFF` — Build no H.264 encoder. [x264](#port-x264) is a port, so this is a choice the
  recipe makes: libheif here decodes AVC-coded images through OpenH264 and cannot write them.
- `-DWITH_RAV1E=OFF` — With `WITH_KVAZAAR`, `WITH_UVG266`, `WITH_VVDEC`, `WITH_VVENC` and
  `WITH_OPENJPH_ENCODER` off, build none of the codecs on rav1e, kvazaar, uvg266, vvdec, vvenc and
  OpenJPH; none of those libraries is a port.
- `-DWITH_FFMPEG_DECODER=OFF` — Build no decoder on ffmpeg's libavcodec. It would be a second HEVC
  decoder beside libde265, and [ffmpeg](#port-ffmpeg) is built in a later phase, `42_graphics`.
- `-DWITH_WEBCODECS=OFF` — Build no HEVC decoder on a web browser's WebCodecs API, an experimental
  path upstream leaves off by default.
- `-DWITH_UNCOMPRESSED_CODEC=ON` — Support the ISO/IEC 23001-17 uncompressed codec, which libheif
  implements itself; its deflate and brotli variants use zlib and brotli. Upstream marks it
  experimental and builds it only when asked.
- `-DWITH_HEADER_COMPRESSION=ON` — Support compressed metadata boxes, through zlib.
- `-DWITH_LIBSHARPYUV=ON` — Convert between RGB and YCbCr with libwebp's `libsharpyuv`, upstream's
  higher-quality conversion, from the [libwebp](#port-libwebp) port.
- `-DWITH_EXAMPLES=ON` — Build the `heif-*` command-line programs.
- `-DWITH_EXAMPLE_HEIF_THUMB=ON` — Build `heif-thumbnailer`, the thumbnailer file managers call.
- `-DWITH_EXAMPLE_HEIF_VIEW=OFF` — Build no `heif-view`, an SDL2 viewer that is otherwise built
  whenever SDL2 happens to be installed.
- `-DWITH_GDK_PIXBUF=ON` — Build the gdk-pixbuf loader.
- `-DBUILD_DOCUMENTATION=OFF` — Build no Doxygen API reference.
