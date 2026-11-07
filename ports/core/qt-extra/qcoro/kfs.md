## About

QCoro gives Qt C++20 coroutines: a `QNetworkReply`, a D-Bus pending call, a `QProcess`, a timer,
a socket or a QML-facing task can be `co_await`ed instead of chained through signals. It is a set
of shared libraries, `libQCoro6Core` with one library per Qt module beside it, and CMake packages
named `QCoro6` with a component for each. plasma-workspace, powerdevil, plasma-nm and Discover
require it, kio-extras' `sftp:/` worker requires it, and the Gear applications that `co_await` a
[futuresql](#port-futuresql) query build on it.

## Flags

- `-D CMAKE_INSTALL_LIBDIR=lib` — QCoro is a plain CMake project that includes `GNUInstallDirs`
  itself and reads no ECM install variable, so the library directory is named the CMake way.
- `-D BUILD_SHARED_LIBS=ON` — Build shared libraries. QCoro declares its libraries with no type,
  so CMake's default makes them static archives, which every consumer would then copy into
  itself.
- `-D USE_QT_VERSION=6` — Build against Qt 6 only. Upstream looks for Qt 6 and falls back to
  Qt 5; naming the version makes a missing Qt 6 module stop the configure instead of selecting
  the other Qt.
- `-D QCORO_BUILD_EXAMPLES=OFF` — Build no example programs. Upstream builds them by default,
  and they install nothing.
- `-D QCORO_WITH_QTDBUS=ON` — The `QCoro6DBus` component, for `co_await` on a
  `QDBusPendingCall`.
- `-D QCORO_WITH_QTNETWORK=ON` — The `QCoro6Network` component: replies, sockets and servers.
- `-D QCORO_WITH_QTWEBSOCKETS=ON` — The `QCoro6WebSockets` component, over
  [qt6-qtwebsockets](#port-qt6-qtwebsockets).
- `-D QCORO_WITH_QTQUICK=ON` — The `QCoro6Quick` component, an image provider whose loading is a
  coroutine. It uses Qt Quick's private API, so the library is tied to the Qt release it was
  built against.
- `-D QCORO_WITH_QML=ON` — The `QCoro6Qml` component, which hands a coroutine to QML as a
  promise-like task. It uses Qt Qml's private API.
- `-D QCORO_WITH_QTTEST=ON` — The `QCoro6Test` component, header-only helpers that a consumer's
  own tests include.

Each `QCORO_WITH_*` switch is on by default upstream. The recipe names them, so a component a
consumer requires cannot drop out of the package unnoticed.

## Notes

Coroutines need C++20. `QCoro6Coro`'s CMake package installs the `qcoro_enable_coroutines()`
macro, which adds GCC's `-fcoroutines`, and a consumer calls it itself. `find_package(QCoro6)`
with no component is refused by the package, so a consumer always names the components it uses.
