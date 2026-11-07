## About

Milou draws search results. Its QML module, `org.kde.milou`, is a results list over
[krunner](#port-krunner)'s results model, grouped by the runner that found each match, with the
favourite runners first; the KRunner window of the Plasma shell and the search field of
[kwin](#port-kwin)'s Overview both show their matches through it. The port also installs the
Search applet (`org.kde.milou`), a search field with the same list that can be put on a panel.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Compiles none of the translation catalogues. kpkg removes
  every non-English catalogue from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so this only saves build time.
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Notes

Milou searches nothing itself: what a query finds is whatever the installed KRunner plugins
return, so the list grows with each port that installs a runner. Its QML imports
`org.kde.plasma.components` and `org.kde.plasma.extras` from [libplasma](#port-libplasma),
`org.kde.ksvg` from [ksvg](#port-ksvg) and `org.kde.kirigami` from [kirigami](#port-kirigami) at
run time. The build links the first two libraries; [kirigami](#port-kirigami) is in `depends` for
its QML module alone, which the build does not look for.
