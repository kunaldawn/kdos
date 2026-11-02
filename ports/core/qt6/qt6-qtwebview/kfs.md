## About
Qt WebView for Qt 6 shows web content in a Qt Quick item through the platform's own web engine.
On Linux that engine is Qt WebEngine, so the module is a thin layer over
[qt6-qtwebengine](#port-qt6-qtwebengine): the `WebView` QML type and a plugin that renders it
with a `WebEngineView`. [neochat](#port-neochat) requires it, and [tokodon](#port-tokodon) uses it.

## Flags
- `-DFEATURE_webview_webengine_plugin=ON` — build the Qt WebEngine plugin, the only backend Qt
  WebView has on Linux. Upstream builds it only when it finds Qt WebEngine Core and Quick, and
  without them the module installs with no backend to load; named, a missing Qt WebEngine stops
  the configure.

## Notes
The port's version follows [qt6-qtwebengine](#port-qt6-qtwebengine), not
[qt6-qtbase](#port-qt6-qtbase). The plugin links Qt WebEngine's private libraries
(`WebEngineCorePrivate`, `WebEngineQuickPrivate`), which carry no compatibility promise from one
release to the next, so a Qt WebView of another release than the Qt WebEngine beside it may fail
to compile or break at run time. The two move together, and both are built against whichever
`qt6-qtbase` the set is at (see
[How Qt 6 is built](../../../../docs/kdos/05-developer/writing-ports.md#how-qt-6-is-built)).
Like every Qt 6 module but `qt6-qtbase`, it passes no install paths of its own.
