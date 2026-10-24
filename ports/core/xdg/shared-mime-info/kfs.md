## About

shared-mime-info is the freedesktop.org MIME database, the XML that says which file names and which
contents make each type, and `update-mime-database`, which compiles every package's XML under
`/usr/share/mime/packages` into the caches programs read. GLib's content-type lookup, and with it
every GTK program, reads those caches, as do Qt, `kdos-appbox` and the file managers.

## Flags

- `-D update-mimedb=false` — Do not run `update-mime-database` at install time. It is upstream's
  default, and upstream warns not to turn it on under `DESTDIR`. The caches under `/usr/share/mime`
  belong to no package: `kpkg` rebuilds them after every install that touches
  `/usr/share/mime/packages` (see
  [Shared indexes](../../../../docs/kdos/05-developer/writing-ports.md#shared-indexes)).
- `-D build-tools=true` — Build and install `update-mime-database`. It is upstream's default; the
  shared index above needs the program.
- `-D build-tests=false` — Do not build the test suite, which needs `xmllint`.
- `-D build-spec=false` — Do not build the HTML specification, which needs `xmlto` and
  docbook-xsl and which `install` does not install in any case.
