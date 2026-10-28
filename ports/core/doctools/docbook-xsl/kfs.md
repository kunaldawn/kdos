## About

The DocBook XSL stylesheets turn DocBook XML into manual pages, HTML and other formats when
`xsltproc` runs them. In KDOS they serve the builds above all; the desktop itself never reads
them. A port that renders its manual pages from DocBook, such as dbus, polkit, gtk3, bubblewrap
and postgresql, runs `xsltproc` over this tree, and so do the generators [xmlto](#port-xmlto) and
[asciidoc](#port-asciidoc)'s `a2x`. The package installs no program and compiles nothing.

Two things set it apart from a plain copy of the stylesheets. It ships both of upstream's
releases, the *nons* stylesheets written for DocBook 4 input and the namespaced ones written for
DocBook 5, because ports ask for both. And it owns the system's root XML catalogue,
`/etc/xml/catalog`, which maps every web address a project names a stylesheet by onto the copy
installed here. The build has no network (see
[A build that reaches the network](../../../../docs/kdos/05-developer/build-troubleshooting.md#a-build-that-reaches-the-network)),
so a stylesheet address the catalogue does not map is a failed build, not a slow one.

## Flags

- `--strip-components=1` — Each release is unpacked again by `build.sh`, into `nons` and `ns`, with
  its top directory stripped. The namespaced tarball's top directory is `docbook-xsl-$version`,
  which is the name of `$SRC` itself, so kpkg's own unpacking of the second source lays it over the
  first, file by file, and the working directory holds neither release whole (see
  [Sources, and what each one becomes](../../../../docs/kdos/05-developer/writing-ports.md#sources-and-what-each-one-becomes)).
- `--create` — `xmlcatalog --create` writes an empty catalogue at `$PKG/etc/xml/catalog`. Every
  `--add` after it adds one entry to that file.
- `delegatePublic` — Two entries hand every lookup of a DocBook XML public identifier, the
  `-//OASIS//DTD DocBook XML` and `-//OASIS//ENTITIES DocBook XML` families, to the catalogue
  `/etc/xml/docbook`, which [docbook-xml](#port-docbook-xml) writes: it maps the public
  identifiers and the `oasis-open.org` addresses of each DocBook XML version onto that version's
  installed DTD files.
- `delegateSystem` — Hands every system identifier under `http://www.oasis-open.org/docbook/` to the
  same catalogue; the `delegateURI` entry does the same for URI lookups.
- `rewriteSystem` — The two loops add a `rewriteSystem` and a `rewriteURI` entry for every address
  each release is known by. For the nons tree these are `http://docbook.sourceforge.net/release/xsl`
  and the cdn's `release/xsl-nons`, over http and https; for the namespaced tree,
  `http://docbook.sourceforge.net/release/xsl-ns` and the cdn's `release/xsl`, over http and https.
  Each base is entered twice, under the release's own version and under `current`. A stylesheet named by
  any of these resolves to a file under `/usr/share/xml/docbook/`; one named by an address that
  matches no entry is fetched, and `xsltproc --nonet` fails.

## Patches

- `string-subst.patch` — Applied to both trees: it declares the EXSLT strings namespace in `lib/lib.xsl` and gives the `string.subst` template a first branch that calls `str:replace` when the processor provides it (libxslt's libexslt does). The stock template recurses once per match, so a long page, such as git's `git-config.1`, passes `xsltproc`'s default limit on nested template calls and the build fails; see [Data with no other source form](../../../../docs/kdos/01-philosophy/why-kdos.md#data-with-no-other-source-form).

## Notes

`build.sh` copies a named list of directories from each tree rather than the whole release. It
leaves out `extensions/` and `tools/`, which hold prebuilt Java jars for Saxon, Xalan and the
webhelp indexer; `docsrc/`, the sources of the stylesheets' reference documentation; and
everything under `webhelp/` but `webhelp/xsl`.
`xsltproc` reads none of what is left out. A stylesheet directory a new release adds is not
installed until it is added to the list.

`/etc/xml/catalog` belongs to this package alone. A port that needs its own entries in the root
catalogue cannot add them to this file without the two packages claiming one path, which kpkg
refuses (see
[Who owns a file](../../../../docs/kdos/03-architecture/packaging.md#who-owns-a-file));
such a port writes a catalogue of its own, as docbook-xml does, and this one delegates to
it.

libxml2 reads `/etc/xml/catalog` by default (see [libxml2](#port-libxml2)), and many recipes also
export `XML_CATALOG_FILES=/etc/xml/catalog` before running `xsltproc`.
