## About

GEGL (Generic Graphics Library) is the graph-based image processing engine under
[gimp](#port-gimp): an image operation is a node, and GIMP's filters and tools are graphs of
GEGL operations, working in high bit depth and in the pixel formats of [babl](#port-babl). The
port also installs the `gegl` command, which runs a graph from the command line.

## Flags

- `--wrap-mode=nodownload` — Never download a meson subproject. GEGL's own source carries
  poly2tri-c and libnsgif under `subprojects/`; nodownload still allows those copies.
  [libnsgif](#port-libnsgif) is a port and is found first, and poly2tri-c, which is not, is built
  from its copy.
- `-Dintrospection=true`, `-Dvapigen=enabled` — Generate `Gegl-0.4.gir` and its typelib, and the
  Vala bindings, for which [vala](#port-vala) is in `depends`.
- `-Drelocatable-bundle=no` — Look for operations and data at the paths compiled in, not relative
  to the running program.
- `-Dgdk-pixbuf=enabled`, `-Dgexiv2=enabled`, `-Dlcms=enabled`, `-Dlensfun=enabled`,
  `-Dlibraw=enabled`, `-Dlibrsvg=enabled`, `-Dlibtiff=enabled`, `-Dopenexr=enabled`,
  `-Dcairo=enabled`, `-Dpango=enabled`, `-Dpangocairo=enabled`, `-Dpoppler=enabled`,
  `-Dwebp=enabled` — Each optional library this build uses, named `enabled` instead of
  upstream's `auto`, so a missing one stops the configure. They give the image loaders and savers (TIFF,
  OpenEXR, camera RAW, SVG, PDF, WebP), metadata through [gexiv2](#port-gexiv2), colour
  management through [lcms2](#port-lcms2), lens correction through [lensfun](#port-lensfun),
  and text rendering.
- `-Dlibspiro=enabled` — Spiro curves in the `gegl` command's path drawing, through
  [libspiro](#port-libspiro).
- `-Dumfpack=enabled` — Build the `matting-levin` operation, which solves a sparse linear system
  through UMFPACK from [suitesparse](#port-suitesparse). GIMP's foreground select offers it as one
  of its matting engines.
- `-Dopenmp=enabled` — Use OpenMP where GEGL's code has it.
- `-Dlibav=disabled` — Build no `ff-load` and `ff-save` operations, which read and write video
  through FFmpeg's libraries. It is upstream's default; with them the library would follow every
  [ffmpeg](#port-ffmpeg) release.
- `-Djasper=disabled` — No JPEG 2000 loader through JasPer.
- `-Dlibv4l=disabled`, `-Dlibv4l2=disabled` — No video-capture source operations.
- `-Dlua=disabled`, `-Dmrg=disabled`, `-Dsdl1=disabled`, `-Dsdl2=disabled`, `-Dsdl3=disabled` —
  No interactive viewer for the `gegl` command and no SDL display operations. mrg is not a port.
- `-Dmaxflow=disabled` — No operations built on the maxflow graph-cut library, which is not a
  port.
- `-Dpygobject=disabled` — No Python tests through PyGObject.
- `-Dgraphviz=disabled` — The option is declared in `meson_options.txt` and read by no
  `meson.build`; Graphviz's `dot` is looked up only for the documentation, which is off.
- `-Dworkshop=false` — Build none of the experimental operations under `operations/workshop`,
  upstream's default.
- `-Dparallel-tests=false`, `-Doperation-test=false` — Settings for the test suite, which is not
  run here.

## Notes

After the install, `build.sh` checks that `matting-levin.so` is in the package and that the
`gegl` program links libspiro, and fails the build when either is missing.
