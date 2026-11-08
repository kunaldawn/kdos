## About

KGraphViewer displays Graphviz DOT files: it lays a graph out with [graphviz](#port-graphviz)'s
engines, draws it with zoom, a bird's-eye view and a choice of layout command, reloads it when the
file changes and prints it. The drawing is a KPart, installed with its CMake package
(`KGraphViewerPart`), so another program can embed it; [massif-visualizer](#port-massif-visualizer)
builds against it, which places this port with the libraries in `43_toolkits`.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — The handbook, through
  [kdoctools](#port-kdoctools), which upstream looks for as optional; required, a missing one
  stops the configure instead of leaving the handbook out.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Patches

`app-id.patch` changes `StartupWMClass=kgraphviewer` to `StartupWMClass=org.kde.kgraphviewer` in
upstream's desktop entry. KGraphViewer takes its Wayland app_id from KAboutData, which derives
`org.kde.kgraphviewer`, the desktop file name; upstream's shorter class matches no window it
opens.

## Notes

Graphviz is `REQUIRED` at 2.30 or later through upstream's `FindGraphviz`, over pkg-config's
`libgvc`, `libcgraph` and `libcdt`; the `dot` program, which upstream reports as a `TYPE RUNTIME`
package, comes from the same port. [boost](#port-boost) is used for its headers.

Upstream's `BUILD_GRAPHEDITOR` stays at its default, off: the editor, `kgrapheditor`, is marked
unfinished, so neither the program nor its desktop entry is installed.
