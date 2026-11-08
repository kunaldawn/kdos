## About

libkmahjongg holds the tile sets and backgrounds of the KDE Mahjongg games and the code that loads
and renders them, with the tile-set and background selectors. [kmahjongg](#port-kmahjongg) and
[kshisen](#port-kshisen) draw their boards through it.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`; the image is
  English only (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D BUILD_SVG_CHECKS=OFF` — No SVG rendering checks, which compare each tile set's rendering
  with ImageMagick's `compare`. Upstream's default, named.
