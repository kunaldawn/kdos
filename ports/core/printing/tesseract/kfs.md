## Flags

- `-DTESSDATA_PREFIX=/usr/share` — The directory whose `tessdata` holds the language files,
  compiled into the library through `config_auto.h`. The autotools build defines it from
  `datadir`; the CMake build defines it only when this is passed, and the library otherwise looks
  for its languages in `./`, so with `TESSDATA_PREFIX` unset in the environment
  `tesseract --list-langs` lists none and gImageReader has no language to recognise. `build.sh`
  fails unless `config_auto.h` defines it, beside the libtiff, libarchive and curl defines it checks.
- `-DGRAPHICS_DISABLED=ON` — Compile out ScrollView, the debugging display that opens a TCP
  connection to a separate Java viewer, `ScrollView.jar`, to draw the page analysis. The CMake
  build does not build that viewer and nothing in the tree ships it.
