## About

FutureSQL runs SQLite queries on a thread of its own and hands the results back as `QFuture`s, so a
Qt program reads and writes its database without blocking its interface. It maps result rows onto
C++ structs and runs schema migrations from a directory of SQL files. Angelfish, Kontrast and other
Kirigami applications keep their data with it. The library is `libfuturesql6`, found with
`find_package(FutureSQL6)`.

## Flags

- `-D BUILD_WITH_QT6=ON` — Build against Qt 6. The project asks ECM for a version older than KDE
  Frameworks 6, so ECM's `QtVersionOption` would otherwise pick Qt 5.
- `-D BUILD_EXAMPLES=OFF` — Build no example programs. Off is upstream's default too; the recipe
  names it, because the examples, like the tests, make the configure require
  [qcoro](#port-qcoro), which this port's `depends` does not name.

## Notes

The queries go through Qt SQL's `QSQLITE` driver, which [qt6-qtbase](#port-qt6-qtbase) builds over
the system [sqlite](#port-sqlite); the library links no SQLite of its own.
A consumer that `co_await`s a future, as Angelfish does, names qcoro itself.
