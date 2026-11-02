## About

Prison draws and reads barcodes. Its `KF6Prison` library generates QR codes, Data Matrix, Aztec,
PDF417, Code 39, Code 93, Code 128 and EAN-13 barcodes as images, with a QML module,
`org.kde.prison`, for Qt Quick programs; `KF6PrisonScanner` reads barcodes from a camera's video
frames, with its own CMake package. Spectacle's QR code scanning, the clipboard applet's QR code
and [merkuro](#port-merkuro) use it.

## Flags

- `-D WITH_DMTX=ON` — Generate Data Matrix barcodes, through [libdmtx](#port-libdmtx). It is
  upstream's default; on, the configure requires libdmtx.
- `-D WITH_ZXING=ON` — Read barcodes, and generate PDF417 ones, through
  [zxing-cpp](#port-zxing-cpp). It is upstream's default; on, the configure requires ZXing.
- `-D WITH_QUICK=ON` — Build the QML module. It is upstream's default; on, the configure requires
  Qt Quick from [qt6-qtdeclarative](#port-qt6-qtdeclarative).
- `-D WITH_MULTIMEDIA=ON` — Build `KF6PrisonScanner`, which takes its frames from Qt Multimedia's
  video sink. It is upstream's default; on, the configure requires
  [qt6-qtmultimedia](#port-qt6-qtmultimedia).

## Notes

QR codes come from [qrencode](#port-qrencode), which the configure requires with no switch. The
scanner has a CMake package of its own, `KF6PrisonScanner`; `KF6PrisonConfig.cmake` loads it as
well for a program that asks for `KF6Prison` below version 6.31, so such a program finds the
`KF6::PrisonScanner` target where it looks for it. The source carries no translations, so the recipe has no
`KF_SKIP_PO_PROCESSING`.
