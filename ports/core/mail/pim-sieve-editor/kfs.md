## About

Sieve Editor writes and manages Sieve scripts, the mail filters that run on an IMAP server rather
than in the client. It connects to servers over ManageSieve, lists, activates, imports and
exports their scripts, and edits them as text with completion and syntax checking or through a
graphical rule editor, all from [libksieve](#port-libksieve)'s `KSieveUi`. [kmail](#port-kmail)
embeds the same editor for its own IMAP accounts; this program is the standalone one, with its own
list of servers whose passwords it keeps through [qtkeychain](#port-qtkeychain).

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — compile none of the translation catalogues in `po/`. kpkg
  removes every non-English catalogue from the package anyway (see
  [size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so this only saves build time.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — the handbook is built only when
  [kdoctools](#port-kdoctools) is found; required, a missing kdoctools stops the configure instead
  of quietly dropping the handbook.
- `-D CMAKE_DISABLE_FIND_PACKAGE_KF6UserFeedback=ON` — no KUserFeedback, through which the editor
  would submit usage statistics. It is not a port, and refusing the search keeps a stray copy in
  the build root from turning telemetry on.
- `-D OPTION_ADD_AUTOGENERATETEXT=OFF` — no AI text-generation plugin. With the option on, which
  is upstream's default, the editor looks for ktextaddons' `TextAutoGenerateText` library and,
  when it finds one, offers to write scripts through a network language-model service.

## Patches

`autogen-include.patch` is what lets the editor build with that option off. Every use of the
text-generation manager is under `HAVE_TEXT_AUTOGENERATE_TEXT`, but `sieveeditormainwidget.cpp`
includes its header unconditionally, and the header's directory reaches the compile only through
the `KF6::TextAutoGenerateText` target, which is linked only when the option finds the library.
The patch puts the include under the same condition; a release whose include is already guarded
needs no patch.

## Notes

[libksieve](#port-libksieve) is built with the same option off, and it has to be. Its installed
header `config-libksieveui.h` defines `HAVE_TEXT_AUTOGENERATE_TEXT` too, with no include guard, so
a libksieve built with the option on sets the macro to 1 inside the editor's sources wherever its
headers are included after the editor's own configuration header, and the build then reaches the
text-generation headers it has no path to and stops.

`USE_DBUS` defaults to on for Linux, and the build then keeps a second start of the editor in the
first through D-Bus with [kdbusaddons](#port-kdbusaddons); off, it would need
`KDSingleApplication` instead. The default stands, so the recipe does not name it.
[qtkeychain](#port-qtkeychain) and ktextaddons' `TextAddonsWidgets` are marked `TYPE REQUIRED`,
and the feature summary stops the configure without them.

The program sets `org.kde.sieveeditor` as its desktop file name, the name of the entry upstream
installs, which carries no `StartupWMClass`, so the task manager matches the window to it with no
change.

`KF_SKIP_PO_PROCESSING` stops only `ki18n_install()`: `kdoctools_install()` still builds the
handbook in every translated language, and kpkg's language rule removes
`/usr/share/doc/HTML/<lang>/` from the package.
