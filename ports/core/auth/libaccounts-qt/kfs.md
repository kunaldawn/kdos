## About

libaccounts-qt is the Qt binding of [libaccounts-glib](#port-libaccounts-glib): `Accounts::Manager`,
`Account`, `Provider`, `Service` and `AccountService` as Qt classes. It is built against Qt 6 and
installs `libaccounts-qt6`, its headers under `/usr/include/accounts-qt6` and the `AccountsQt6`
CMake package that KDE's kaccounts-integration finds.

## Flags

- `-after "SUBDIRS -= tests" "INSTALLS -= documentation"` — assignments qmake applies after reading
  the project files. The first drops the `tests` subdirectory, whose `accountstest` program would
  otherwise be built and installed into `/usr/bin`; the second drops the documentation target,
  which runs doxygen during `make install`.

## Notes

The library name, the pkg-config file and the CMake package all carry `QT_MAJOR_VERSION`, so the
release builds for Qt 6 under `qmake6` with no patch.
