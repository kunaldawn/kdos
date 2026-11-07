## About

kdegraphics-mobipocket is the library Okular and the thumbnailers read Mobipocket e-books with:
`.mobi`, `.prc` and unencrypted `.azw` files, their text and their cover pictures.
[okular](#port-okular)'s Mobipocket generator and the e-book thumbnailer of
[kdegraphics-thumbnailers](#port-kdegraphics-thumbnailers) link it.

## Notes

The tarball has no translations, so `build.sh` passes only the release flags. The two `test`
lines after the install fail the build when the library or its CMake package file is missing, the
two things its consumers find it by.
