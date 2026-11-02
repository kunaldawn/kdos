## About

Kate is KDE's advanced text editor: tabs and split views, sessions and projects, a language
server client, search and replace across folders, a terminal panel through
[konsole](#port-konsole)'s part, Git integration, a SQL client and a GPG plugin. Its editing
component is [ktexteditor](#port-ktexteditor)'s.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`; the image is
  English only (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D USE_DBUS=ON` — Builds the D-Bus interface through which `kate` opens a file in the
  instance already running. Upstream's default on Linux, named.
- `-D BUILD_kwrite=OFF` — Leaves out KWrite, the single-document editor built from the same
  tree; Kate is the one text editor.
- `-D BUILD_compiler-explorer=OFF` — Leaves out the Compiler Explorer plugin, which sends the
  open file to the compiler-explorer web service and shows the assembly that comes back.
- `-D CMAKE_DISABLE_FIND_PACKAGE_KF6UserFeedback=ON` — No KUserFeedback, through which Kate
  would offer to submit usage statistics. It is not a port; refusing it means a stray copy in the
  build root cannot be picked up.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6Keychain=ON` — Requires [qtkeychain](#port-qtkeychain), in
  which the SQL plugin keeps database passwords. Upstream leaves the plugin out without it.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Gpgmepp=ON` — Requires [gpgmepp](#port-gpgmepp) for the GPG
  plugin, which signs and encrypts the open document. Upstream leaves the plugin out with a warning
  without it.

## Notes

The loop at the end of `build.sh` removes every directory under `/usr/share/man` that is not a
`man<section>` one, where `kdoctools_install()` puts the translated manual pages; kpkg's
language rule removes the translated handbooks.
