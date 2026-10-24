## About

Zint encodes barcodes in a wide range of symbologies, from Code 128, EAN and UPC to QR Code, Data
Matrix, Aztec and the postal codes. The port installs the `libzint` library, the `zint` command,
the `libQZint` Qt widget library and Zint Barcode Studio, the Qt 6 GUI built on it.

## Flags

- `-DZINT_QT6=ON` — Build the Qt backend and the GUI against Qt 6; `-DZINT_USE_QT=ON` builds them
  at all. Without `ZINT_QT6` the project looks for Qt 5.
- `-DZINT_USE_GS1SE=OFF` — Do not look for libgs1encoders, the GS1 Syntax Engine, which is not a
  port. Zint then checks GS1 data with its own rules.
- `-DZINT_UNINSTALL=OFF` — Add no `uninstall` target; the package manager removes the files.
- `-DZINT_SHARED=ON` — With `-DZINT_STATIC=OFF`, `-DZINT_FRONTEND=ON` and `-DZINT_USE_PNG=ON`,
  these are upstream's defaults, written out: the shared library only, the `zint` command, and PNG
  output through [libpng](#port-libpng).

## Patches

- `shared-libqzint.patch` — In `backend_qt/CMakeLists.txt`, build libQZint as a shared library instead of a static one, so the GUI and any other Qt program that uses it link one copy.

## Notes

Upstream installs no desktop entry or icon, so `build.sh` installs `zint-qt.png` and writes the
entry. No desktop file name is set, so Qt's Wayland app id is the organisation domain the
program sets, `zint.org.uk`, reversed, before the program name: `uk.org.zint.zint-qt`.
