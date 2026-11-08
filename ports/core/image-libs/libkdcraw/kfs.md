## About

libkdcraw is a Qt 6 wrapper around [libraw](#port-libraw): it decodes camera raw files and reads
the preview pictures and metadata they carry. [kdegraphics-thumbnailers](#port-kdegraphics-thumbnailers)
draws raw thumbnails through it and [gwenview](#port-gwenview) opens raw files with it.

## Notes

The tarball has no translations, so `build.sh` passes only the release flags.
