## About

Umbrello is a UML modeller: class, sequence, collaboration, use case, state, activity, component,
deployment and entity-relationship diagrams in an XMI document. It imports classes from C++,
Java, Python, IDL, Pascal, Ada and other sources, generates code from a model in about twenty
languages, and lays diagrams out automatically through Graphviz.

## Flags

- `-D BUILD_WITH_QT6=ON` — Build against Qt 6 and KDE Frameworks 6. Umbrello carries its own copy
  of `QtVersionOption.cmake`, whose default, when no Qt has been found yet, is Qt 5.
- `-D BUILD_PHP_IMPORT=OFF` — No PHP code import. The importer needs a KDevelop platform of the
  5.x series, which is Qt 5 and KDE Frameworks 5, and the bundled `lib/kdev5-php` it falls back to
  asks for Qt 5 and stops the configure. Off, the result does not depend on whether kdevelop or
  kdevelop-pg-qt happen to be installed.
- `-D BUILD_APIDOC=OFF` — With `-D BUILD_QCH=OFF`: no Doxygen API reference and no Qt Help file.
  Both default on and run whenever doxygen is found, and the package would then carry an HTML tree
  of Umbrello's internals that no user reads.
- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`, which kpkg's
  English-only rule would remove from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Notes

`BUILD_DOC` stays at its default, on, which makes [kdoctools](#port-kdoctools) a requirement and
builds the handbook; `kdoctools_install(po)` builds it in every translated language too, and kpkg's
language rule removes `/usr/share/doc/HTML/<lang>/`. [libxslt](#port-libxslt) and
[libxml2](#port-libxml2) are requirements: the XMI export to DocBook and XHTML runs XSLT.

Automatic layout runs `dot` from [graphviz](#port-graphviz), in `depends` for run time. The
`po2xmi` and `xmi2pot` tools that translate a model's text are installed with the program.

The window's Wayland app_id is `org.kde.umbrello`, the desktop file name `KAboutData` builds from
the `apps.kde.org` home page and the component name. It is the desktop entry's file name too, and
upstream's entry sets no `StartupWMClass`, so the task manager pairs the window with its entry
without a patch.
