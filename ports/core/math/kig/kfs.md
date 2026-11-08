## About

Kig is KDE Education's interactive geometry program: points, lines, circles, conics and curves
are constructed from one another, and dragging a point moves everything built on it. It draws
loci, measures lengths and angles, records constructions as macros, and exports to SVG, LaTeX,
Asymptote and XFig. The drawing area is a KPart, `kigpart`, so other KParts hosts can open Kig
files as well; `kig` is the window around it.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_BoostPython=ON` — Make Boost.Python a requirement. Upstream
  looks for it as an optional package and, without it, builds a Kig with no Python script
  objects and no `kig` module, with only a line in the configure summary to say so. The library
  is `libboost_python` from [boost](#port-boost), built against [python3](#port-python3).
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — Stop the configure if
  [kdoctools](#port-kdoctools) is missing. Upstream looks for it as an optional package and,
  without it, skips the `doc/` directory and the handbook with it.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

A Python script object runs in an interpreter embedded in `kigpart`. Before starting it, the part
loads the Python library by the file name the configure found (`LIB_PYTHON`, the real name of
`libpython3.*.so`), so a [python3](#port-python3) bump to a new minor version needs this port
rebuilt, or scripting fails to start. `pykig.py`, installed to `/usr/bin`, turns a Python
description of a figure into a Kig file and runs Kig on it.

The GeoGebra import is built only against Qt 5's XmlPatterns, which Qt 6 does not have, so this
Kig opens its own files and KGeo, KSeg, Cabri and Dr. Geo documents, but not GeoGebra's.
`application/x-kig` and `application/x-kgeo`, the types the entry claims, are defined in the
`kde6.xml` MIME package of [kcoreaddons](#port-kcoreaddons).

`KAboutData` gives the window the app_id `org.kde.kig`, the name of upstream's entry, from the
`edu.kde.org` homepage it is constructed with.
