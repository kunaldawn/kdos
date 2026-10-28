## About
Raptor is the RDF syntax library of the Redland project: it parses and writes RDF, the
triple-based data format of the semantic web, in RDF/XML, Turtle, N-Triples, N-Quads, TriG, JSON
and other syntaxes, and installs the `rapper` command-line converter. It is in the tree for
[lrdf](#port-lrdf), the library audio programs use to read the RDF descriptions of LADSPA plugins.

## Flags
- `--with-www=none` — Build with no WWW library, so Raptor reads only `file:` URIs itself; a
  document at any other address has to be fetched by its caller and handed over. Upstream's
  default is libcurl.
- `--enable-parsers="rdfxml ntriples turtle trig guess rss-tag-soup rdfa nquads json"` — Build these
  parsers: every one upstream offers except GRDDL, which needs libxslt. Named as a list, the set
  does not change with what happens to be installed: left at its default, configure adds GRDDL
  whenever [libxslt](#port-libxslt) is present. The JSON parser needs [yajl](#port-yajl); if it is
  missing, configure drops the parser with only a warning.
- `--enable-serializers="rdfxml rdfxml-abbrev turtle mkr ntriples rss-1.0 dot html json atom nquads"`
  — Build every serializer upstream offers, named for the same reason.
- `--disable-gtk-doc` — Do not rebuild the API reference with gtk-doc. The pre-built reference in
  the tarball is still installed by `make install`, and `build.sh` deletes `/usr/share/gtk-doc`
  afterwards.

## Patches
- `libxml-2.11.0.patch` — Comments out the two lines in `raptor_libxml.c` that set the `checked` field of a libxml2 entity, a member libxml2 dropped from `xmlEntity` in the commit the patch's first line links. Against the tree's libxml2 the lines do not compile.

## Notes
configure finds [icu](#port-icu) through pkg-config and uses it for the Unicode NFC check the
RDF/XML parser performs; without it the check is skipped, with no error.
