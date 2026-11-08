## About

libaccounts-glib is the accounts database of the accounts-sso framework: a per-user SQLite store of
online accounts, and the XML files under `/usr/share/accounts` that describe each provider,
service, service type and application. It ships the `ag-tool` and `ag-backup` commands, the
GObject introspection data and the Vala bindings. [libaccounts-qt](#port-libaccounts-qt) wraps it
for Qt, and KDE's kaccounts stack is built on both.

## Flags

- `-Dtests=false` — no test suite, which requires the `check` unit-test library; it is not a
  port.
- `-Ddocs=false` — no gtk-doc API reference.
- `-Dintrospection=true` — the `Accounts-1.0` GIR and typelib, through
  [gobject-introspection](#port-gobject-introspection) and the GLib GIRs of
  [glib-introspection](#port-glib-introspection), and the `libaccounts-glib` Vala API through
  `vapigen` from [vala](#port-vala). Upstream's default, named.

## Notes

The source is a commit snapshot of upstream's `master`, the commit that added the
`introspection`, `docs` and `tests` options. No release carries them: 1.27 builds its test suite
unconditionally, so its configure requires the `check` library. The version is 1.27 and the
commit's date.

The database lives in `$XDG_CONFIG_HOME/libaccounts-glib/accounts.db`; nothing is installed for
it. The `com.google.code.AccountsSSO.Accounts.Manager` D-Bus interface file goes to
`/usr/share/dbus-1/interfaces` for consumers that generate code from it; the library does not
own a bus name.
