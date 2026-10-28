## About

[AbiWord]{.idx} is a GTK 3 word processor with its own `.abw`
format and import and export plugins for Word, OpenDocument, RTF, HTML, WordPerfect, EPUB, LaTeX
and others. Word documents are read through [wv](#port-wv), and charts embedded in a document are
drawn by [goffice](#port-goffice).

## Flags

- `--enable-spell`, `--enable-print` — Spell checking, through [enchant](#port-enchant), and
  printing. Printing is upstream's default; left to itself, configure turns spell checking on only
  when it finds Enchant.
- `--enable-plugins` — The plugin list is explicit, so a plugin whose
  library is missing fails configure instead of silently dropping a file format. Left out are
  `collab`, `google`, `wikipedia`, `urldict` and `gdict`, which only reach network services;
  `grammar`, `aiksaurus`, `mathview`, `ots`, `gda`, `psion` and `wmf`, whose libraries are not
  ports; `garble`, a tool that scrambles a document's text for sending in a bug report; and `rsvg`
  (see Notes).
- `--with-goffice` — Link the system [goffice](#port-goffice) instead of the copy of parts of it
  that AbiWord carries in `goffice-bits`.
- `--with-gio` — Use GIO for file access.
- `--without-gnomevfs` — No gnome-vfs, which configure considers only for the GTK 2 front end.
- `--without-redland`, `--without-evolution-data-server`, `--without-libical`,
  `--without-champlain` — No RDF semantic items: Redland stores a document's RDF metadata, and
  Evolution Data Server, libical and libchamplain give the contact, calendar-event and map views
  of it. Redland, Evolution Data Server and libchamplain are not ports; configure would pick up
  [libical](#port-libical) if it were left to probe.
- `--disable-introspection` — Generate no GObject introspection data. It is upstream's default.

## Patches

- `enchant-2-pkgconfig.patch` — makes `configure.ac` ask pkg-config for `enchant-2 >= 2.0.0` instead of `enchant >= 1.2.0`, the module name Enchant 2 installs.
- `enchant.patch` — moves the spell checker to the Enchant 2 API: `enchant_dict_free_string_list`, `enchant_dict_add` and `enchant_dict_is_added` in place of the Enchant 1 calls `enchant_dict_free_suggestions`, `enchant_dict_add_to_personal` and `enchant_dict_is_in_session`, which Enchant 2 does not have.
- `musl-1.2.3.patch` — writes `(char) 0` in place of `(char) NULL` in `xap_Dialog.cpp`. musl defines `NULL` as `nullptr` in C++, which cannot be cast to `char`.

## Notes

The source is a git archive of the release tag with no generated `configure`. `autogen-common.sh`
writes the plugin lists that `configure.ac` includes, and `autoreconf -fi` generates the rest;
`enchant-2-pkgconfig.patch` has to be applied before `autoreconf`, or the generated `configure`
asks pkg-config for the Enchant 1 module, which Enchant 2 does not install, and `--enable-spell`
stops configure.

SVG needs no plugin: the GTK front end draws SVG through [librsvg](#port-librsvg) itself, and
configure leaves the `rsvg` plugin unconfigured with GTK, so naming it would build it with no
compiler flags.

`build.sh` replaces upstream's desktop entry with one whose `MimeType` names AbiWord's own format
only. LibreOffice is the word processor that claims the shared office types (Word, OpenDocument,
RTF, WordPerfect); a second claim would open them in whichever entry sorted first.

Every interface translation is a `.strings` file that is installed whatever configure is told, so
all but the English ones are deleted from the package.
