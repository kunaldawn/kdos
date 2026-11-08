## About

Analitza is KDE's mathematical expression library: the MathML-based parser and evaluator, the
symbolic operations, and the 2D and 3D function plotter with its QML module.
[kalgebra](#port-kalgebra) and [cantor](#port-cantor) are built on it.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`; the image is
  English only (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Eigen3=ON` — Requires [eigen](#port-eigen), on which Analitza's
  `eigenvalues` and `eigenvectors` functions are built. Upstream looks for it quietly and leaves
  both functions out without it.

## Notes

The 3D plotter draws with OpenGL through [libglvnd](#port-libglvnd).
