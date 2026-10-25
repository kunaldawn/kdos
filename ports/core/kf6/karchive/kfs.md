## About

KArchive is the KDE Frameworks library for reading and writing archives and compressed files:
tar, zip, 7z and ar archives, and gzip, bzip2, xz and zstd streams through one `QIODevice`
interface. [kiconthemes](#port-kiconthemes), [kdoctools](#port-kdoctools),
[knewstuff](#port-knewstuff), [kio](#port-kio) and the archive-handling applications link it.

## Notes

Upstream's own options `WITH_BZIP2`, `WITH_LIBLZMA`, `WITH_OPENSSL` and `WITH_LIBZSTD` are on by
default and make [bzip2](#port-bzip2), [xz](#port-xz), [openssl](#port-openssl) (for encrypted 7z
archives) and [zstd](#port-zstd) requirements of the configure; [zlib](#port-zlib) is required
with no option. A missing one stops the configure rather than leaving its format out.
