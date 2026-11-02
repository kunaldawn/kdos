## About

KService finds desktop entries and plugins for KDE programs: it reads the `.desktop` files under
`/usr/share/applications` into a binary cache, the system configuration cache (sycoca), and answers
which application opens a MIME type. `kbuildsycoca6` builds that cache. [kio](#port-kio) and
[kparts](#port-kparts) are built on it.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Build none of the gettext catalogues under `po/`, which
  `ki18n_install()` would compile into `.mo` files. The strings in the source are English, and the
  package-wide English-only rule (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
  would remove every catalogue in another language. The switch does not reach
  `kdoctools_install()`, which still builds the translated manual pages of `kbuildsycoca6`; the
  same rule removes them from the package.

## Notes

[kdoctools](#port-kdoctools) is in `depends` for the manual page of `kbuildsycoca6`, which the build
generates from DocBook when it finds KDocTools; upstream treats it as optional.
