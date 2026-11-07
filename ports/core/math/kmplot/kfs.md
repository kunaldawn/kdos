## About

KmPlot is KDE Education's function plotter: Cartesian, parametric, polar, implicit and
differential-equation plots, with derivatives and integrals drawn alongside, sliders for
parameters, and tools that find a function's roots, extrema and the area under it. It installs a
KPart as well, `kmplotpart`, through which other KDE programs embed a plot.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The toolbar's own icons (`newfunction`, `editplots`, `newparametric` and the rest) are not in this
package: [kdeedu-data](#port-kdeedu-data) installs them in hicolor, so that port is in `depends`
for run time. The handbook is built under upstream's `BUILD_DOC` option, on by default, which makes
[kdoctools](#port-kdoctools) a requirement.

The entry claims `application/x-kmplot`, which [kcoreaddons](#port-kcoreaddons) defines in its
`kde6.xml` MIME package. `KAboutData` gives the window the app_id `org.kde.kmplot`, the name of
upstream's entry.
