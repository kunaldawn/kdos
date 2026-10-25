## About

Converseen is a batch image converter: it converts, resizes, rotates, flips and renames many
images in one pass, through [imagemagick](#port-imagemagick)'s Magick++ interface. It is built
against Qt 6.

## Flags

- `-D USE_QT6=ON` — Build against Qt 6. Upstream's default is Qt 5.
- `CXXFLAGS="$CXXFLAGS $(pkg-config --cflags-only-other Magick++)"` — Take Magick++'s preprocessor
  definitions, among them `MAGICKCORE_HDRI_ENABLE` and `MAGICKCORE_QUANTUM_DEPTH`, from the
  installed `Magick++.pc`. Magick++'s headers size every `Quantum` from those two, so they must
  match the library: a mismatch compiles and then reads every pixel as the wrong type.
  `imagemagick-flags.patch` deletes the values upstream hard-codes, so these are the only ones.

## Patches

- `imagemagick-flags.patch` — Delete the `-DMAGICKCORE_HDRI_ENABLE=0` and `-DMAGICKCORE_QUANTUM_DEPTH=16` definitions from `CMakeLists.txt`. They describe a non-HDRI, 16-bit build whatever ImageMagick was configured as, and against an HDRI build every `Quantum` is the wrong type.
- `no-update-check.patch` — In `src/mainwindowimpl.cpp`, replace the start-up call to `checkForUpdates()` with hiding the Check for Updates menu entry. Converseen otherwise asks its server for the latest version at every start, and no setting stops the request.

## Notes

`build.sh` removes `/usr/share/converseen/loc`, the program's own translation directory. The
program shows its English source strings.

The desktop entry is written over upstream's with `StartupWMClass=net.fasterland.converseen`, the
Wayland app id `setDesktopFileName` sets, so the panel can tie the window to its launcher.
