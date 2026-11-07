## About
Qt HTTP Server for Qt 6 is a small HTTP server library: a `QHttpServer` with request routing on
URL patterns and handler return types, responses built from Qt types, and the upgrade of a
connection to a WebSocket. Applications embed it to serve a local interface;
[arianna](#port-arianna) serves the open book to its reader view through it.

## Flags
- `-DCMAKE_REQUIRE_FIND_PACKAGE_Qt6WebSockets=ON` — make Qt WebSockets a requirement. Upstream
  looks for it as an optional component, and without it the library builds with no WebSocket
  upgrade and says nothing; [qt6-qtwebsockets](#port-qt6-qtwebsockets) is in `depends`.

## Notes
Like every Qt 6 module but `qt6-qtbase`, it passes no install paths of its own (see [How Qt 6 is built](../../../../docs/kdos/05-developer/writing-ports.md#how-qt-6-is-built)).
