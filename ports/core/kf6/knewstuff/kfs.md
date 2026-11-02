## About

KNewStuff lets a KDE application download and install add-on content, such as themes, templates,
wallpapers or scripts, from the providers listed at `autoconfig.kde.org`, the KDE Store among
them, over the Open Collaboration Services API, which [attica](#port-attica) implements. It has a
Qt Widgets dialog and a Qt Quick one built on [kirigami](#port-kirigami), and the
`knewstuff-dialog6` command opens the dialog for a given `.knsrc` file.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Syndication=ON` — Make KSyndication, the RSS and Atom parser
  of [syndication](#port-syndication), a requirement. Upstream looks for it as optional and, when it
  finds it, builds the OPDS provider, through which a program offers the books of an OPDS e-book
  catalogue as it offers store content; without it the provider is left out with only a line in
  the configure summary.
- `-D BUILD_DESIGNERPLUGIN=OFF` — Build no Qt Designer plugin for the widgets, which upstream
  builds by default.
- `-D KF_SKIP_PO_PROCESSING=ON` — Build none of the gettext catalogues under `po/`, which
  `ki18n_install()` would compile into `.mo` files. The strings in the source are English, and the
  package-wide English-only rule (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
  would remove every catalogue in another language.

## Notes

The Qt Quick dialog imports `org.kde.kirigami` at run time, which upstream's configure reports as
a run-time package; [kirigami](#port-kirigami) is in `depends` for it.
