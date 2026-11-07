## About

Rocs is KDE Education's graph theory IDE for teaching and experimenting with graph algorithms: a
visual editor for graphs with typed nodes and edges, generators for common graph families, and a
JavaScript engine in which an algorithm runs over the graph on screen. Graphs are saved in Rocs's
own `.graph2` format and import and export DOT, GML, TGF and TikZ; a project, `.rocs`, bundles the
graphs and scripts.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — Stop the configure if
  [kdoctools](#port-kdoctools) is missing. Upstream looks for it as an optional package, and
  without it leaves the handbook out.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The entry claims `application/x-rocs`, a type no MIME package defines, so a project would never
be offered to Rocs. `build.sh` installs `kdos-rocs.xml`, which defines it for `*.rocs` as a
subclass of `application/x-compressed-tar`: a project is a gzip-compressed tar archive, written
with `KTar`. `kpkg` rebuilds the MIME database on install (see
[Shared indexes](../../../../docs/kdos/05-developer/writing-ports.md#shared-indexes)).

[boost](#port-boost) is used for headers only: Boost.Graph for the generators and layouts,
Boost.Spirit for the DOT and GML parsers, and Boost.Random; nothing of it is linked. The script
engine is Qt's `QJSEngine`, from [qt6-qtdeclarative](#port-qt6-qtdeclarative), which also gives
the editor its Quick Widgets view. The graph scene, `Scene.qml`, drives its editing modes with
`QtQml.StateMachine`, a QML module of [qt6-qtscxml](#port-qt6-qtscxml); CMake does not check for
it, so that port is in `depends` for run time, and without it the editor opens no graph.
`KAboutData` gives the window the app_id `org.kde.rocs`, the name of upstream's entry.
