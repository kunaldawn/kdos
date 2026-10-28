## About
DCMTK is the OFFIS DICOM toolkit: the libraries and command-line tools that read, write and
convert DICOM, the file format and network protocol of medical imaging. `dcmdump` prints a file's
data set, `dcm2img` turns a DICOM image into PNM, BMP, PNG, TIFF, JPEG or JPEG-LS (the deprecated
`dcm2pnm` and `dcmj2pnm` it supersedes are still installed), `img2dcm` wraps an ordinary image as
DICOM, `dcm2json` and `dcm2xml` export the header, and `storescu`, `storescp` and `findscu` send,
receive and query images on a PACS over the DICOM network protocol, with TLS from
[openssl](#port-openssl). The port installs the shared libraries and DCMTK's CMake package, so a
program can be built against it.

## Flags
- `-DDCMTK_ENABLE_LFS=lfs` — Use large-file support through the plain `off_t` interfaces. Left to
  detect it, DCMTK's CMake code first tries the explicit `*64` interfaces with
  `-D_LARGEFILE64_SOURCE` and takes them when they compile. musl's `off_t` is 64 bits already, and
  its headers declare `stat64`, `off64_t` and the rest only as aliases under
  `_LARGEFILE64_SOURCE`, so the `*64` path gains nothing and leans on names musl keeps for
  compatibility alone.
- `-DDCMTK_ENABLE_STL=ON` — Use the C++ standard library's containers, strings and algorithms
  instead of DCMTK's own replacements for them. Upstream's default is off.
- `-DDCMTK_ENABLE_PRIVATE_TAGS=ON` — Build in the dictionary of vendors' private DICOM tags that
  DCMTK ships. Upstream's default is off.
- `-DDCMTK_ENABLE_MANPAGES=ON`, `-DDCMTK_WITH_DOXYGEN=OFF` — Install the manual page of every
  tool, and generate no API reference with Doxygen.
- `-DDCMTK_ENABLE_CHARSET_CONVERSION=oficonv`, `-DDCMTK_WITH_ICONV=OFF` — Convert between DICOM's
  character sets with oficonv, the module built into DCMTK from FreeBSD's Citrus iconv code, which
  DCMTK's `INSTALL` recommends over a C library's iconv, and do not look for libiconv or the C
  library's iconv at all.
- `-DDCMTK_WITH_OPENSSL=ON`, `-DDCMTK_WITH_OPENJPEG=ON`, `-DDCMTK_WITH_PNG=ON`,
  `-DDCMTK_WITH_TIFF=ON`, `-DDCMTK_WITH_XML=ON`, `-DDCMTK_WITH_ZLIB=ON` — TLS and the signature
  module from [openssl](#port-openssl), JPEG 2000 from [openjpeg](#port-openjpeg), PNG and TIFF
  output from [libpng](#port-libpng) and [libtiff](#port-libtiff), XML from
  [libxml2](#port-libxml2) and deflated transfer syntaxes from [zlib](#port-zlib).
- `-DDCMTK_WITH_THREADS=ON` — Build DCMTK's multi-threading support, with its code compiled
  reentrant.
- `-DDCMTK_WITH_SNDFILE=OFF` — No libsndfile, upstream's default written out. No source file
  includes `sndfile.h` or reads the `WITH_SNDFILE` define the switch sets; on, it would only add
  `sndfile` to the `Requires.private` of DCMTK's pkg-config file.
- `-DDCMTK_WITH_WRAP=OFF` — No TCP wrappers access control for the network servers. libwrap is not
  in the tree.

## Patches
- `dont-export-executables.patch` — Debian's patch: it takes the tools out of DCMTK's exported CMake targets, so `DCMTKTargets.cmake` names only libraries and a consumer's `find_package(DCMTK)` does not check for tool files.
- `openssl4.patch` — Builds the signature module (`dcmsign`) against OpenSSL 4, where `ASN1_STRING` is opaque: it reads strings through `ASN1_STRING_type()`, `ASN1_STRING_length()` and `ASN1_STRING_get0_data()`, sets the time-stamp nonce with `ASN1_STRING_set()`, checks a certificate's validity period with `X509_check_certificate_times()` when CMake finds that function, and casts away the `const` on the extension pointer `TS_TST_INFO_get_ext()` returns.

## Notes
Every `-DDCMTK_WITH_*=ON` above only asks: when its library is missing, DCMTK's CMake code prints
a warning and builds without the feature. The `depends` line is what makes each one present.
