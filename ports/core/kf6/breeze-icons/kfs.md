## About

Breeze is KDE's icon theme, installed twice from one set of SVGs: `breeze` for light colour
schemes and `breeze-dark`, whose symbolic icons are recoloured for dark ones. The port also builds
`KF6BreezeIcons`, a small library that carries the whole light theme as a compiled Qt resource, so
a program that links it, as every program linking [kiconthemes](#port-kiconthemes) does, has
Breeze to fall back on whatever theme is installed. [plasma-integration](#port-plasma-integration)
and the KDE applications in the tree name it in `depends`.

## Flags

- `-D WITH_ICON_GENERATION=ON` — Generate the 24×24 icons from the 22×22 ones, by padding each
  SVG with `tools/generate-24px-versions.py`, and install them in both themes. It is upstream's
  default; on, the configure requires a Python 3 interpreter, and the script imports `lxml`, which
  is why [python3](#port-python3) and [python3-lxml](#port-python3-lxml) are in `depends`.
- `-D BINARY_ICONS_RESOURCE=OFF` — Install no `breeze-icons.rcc`, a binary Qt resource of the
  whole theme that upstream keeps for programs that load it at run time. Nothing in the tree reads
  it, and it would add the theme a second time to the package.
- `-D WITH_ICONS_LIBRARY=ON` — Build `KF6BreezeIcons` and its CMake package. It is upstream's
  default, and [kiconthemes](#port-kiconthemes), built with `-D USE_BreezeIcons=ON`, requires the
  package.
