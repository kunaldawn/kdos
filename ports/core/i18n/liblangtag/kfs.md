## About

liblangtag parses, validates, matches and canonicalises BCP 47 language tags (`en-GB`,
`zh-Hant-TW`, `sr-Latn`) against the IANA language subtag registry and the CLDR data it carries.
[libetonyek](#port-libetonyek) uses it for the languages of the text in iWork documents.
`liblangtag-gobject` is the GObject wrapper, with an introspection typelib.

## Flags

- `--disable-static` — Shared library only; nothing links the archive.
- `--disable-test` — No unit tests, which need the `check` framework.
- `--enable-introspection=yes` — Build `liblangtag-gobject` and its `LangTag-0.6` typelib and
  require [gobject-introspection](#port-gobject-introspection). Upstream's `auto` drops the GObject
  library silently when the scanner is missing.
- `--disable-gtk-doc` — Do not regenerate the gtk-doc API reference. The tarball carries it already
  built, and `make install` installs that copy under `/usr/share/gtk-doc/html/liblangtag`, which
  the package keeps.

## Notes

The source is LibreOffice's mirror of the release tarball, the same file upstream's GitLab release
links, because the tag archive GitLab generates has no `configure`. `configure` looks for
`locale.alias` under the locale directory and builds its alias table from it when it is there;
otherwise the library uses the table shipped in the source. The subtag registry XML is rebuilt from
the registry text in the tarball by a small program the build compiles; nothing is downloaded.

The BCP 47 extensions (`-t-` transformed content and `-u-` Unicode locale) are modules the library
opens at run time from `/usr/lib/liblangtag`.
