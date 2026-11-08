## About

libkexiv2 is a Qt 6 wrapper around [exiv2](#port-exiv2): it reads and writes the Exif, IPTC and
XMP metadata of pictures and the previews embedded in them. The thumbnailers in
[kio-extras](#port-kio-extras) and [kdegraphics-thumbnailers](#port-kdegraphics-thumbnailers), and
[okular](#port-okular), read pictures' orientation and previews through it.

## Notes

The tarball has no translations, so `build.sh` passes only the release flags.
