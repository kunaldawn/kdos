## About

KPlotting is a small Qt Widgets library for drawing data plots: `KPlotWidget` with axes, labels
and a grid, `KPlotObject` for a set of points drawn as markers, lines or bars, and `KPlotPoint`.
[kalzium](#port-kalzium), [step](#port-step) and [knights](#port-knights) draw their charts with
it. It needs nothing beyond Qt Widgets.

## Flags

- `-D BUILD_DESIGNERPLUGIN=OFF` — Build no Qt Designer plugin for the widget. Upstream builds one
  by default, and the configure then requires Qt's `UiPlugin` module from
  [qt6-qttools](#port-qt6-qttools), which this port's `depends` does not name.

## Notes

The source carries no translations, so the recipe has no `KF_SKIP_PO_PROCESSING`.
