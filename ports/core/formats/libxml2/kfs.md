## About

libxml2 is GNOME's XML library: a parser with DTD and schema validation, XPath, XInclude, XML
catalogues and an HTML parser, with `xmllint` and `xmlcatalog` as its command-line tools. Many of
the desktop's applications read XML through it, and so does the compositor:
[kdos-comp](#port-kdos-comp) is a fork of labwc and reads `rc.xml` and `menu.xml` with it (see
[Configuration](../../../../docs/kdos/04-programs/kdos-comp.md#configuration)).
[libxslt](#port-libxslt), and with it every manual page rendered from DocBook, is built on it.

Its catalogue support is what lets the documentation builds run with no network. libxml2 reads
`/etc/xml/catalog`, written by [docbook-xsl](#port-docbook-xsl), and resolves every DocBook DTD
and stylesheet address through it to a local file.

libxml2 has no HTTP client, so a program that hands it an `http://` address gets an error rather
than a download; see
[SpatiaLite cannot load a WFS layer from a URL](../../../../docs/kdos/06-reference/known-gaps.md#spatialite-cannot-load-a-wfs-layer-from-a-url).

## Flags

- `PYTHON=/usr/bin/python3` — Names the interpreter configure's `AM_PATH_PYTHON` checks and builds
  the Python bindings for, instead of the first one it finds on `PATH`.
- `--sysconfdir=/etc` — For libxml2 this is also where its default catalogue lives: the library
  reads `file://` + sysconfdir + `/xml/catalog` whenever `XML_CATALOG_FILES` is unset, which
  makes it `/etc/xml/catalog`.
- `--with-icu` — Builds ICU in as a further converter for character encodings. For an encoding
  its own built-in converters do not handle, libxml2 asks iconv, which here is musl's, and then
  ICU for any encoding iconv does not know.
- `--with-history` — Builds `xmllint --shell` with readline and its history, so the interactive
  shell has line editing and remembers commands. It turns on `--with-readline` as well.
- `--with-zlib` — Lets libxml2 read gzip-compressed XML when a program asks for it with the
  `XML_PARSE_UNZIP` option, and `xmllint` read compressed files. Upstream leaves zlib off by
  default.
- `--with-python` — Builds the Python bindings, the `libxml2` module, which upstream leaves off by
  default. [itstool](#port-itstool), which merges translations into XML documentation for many
  GNOME-style builds, is a Python program that imports it. The bindings are generated from the
  API description doxygen writes, which is why doxygen is in `depends`.

## Notes

The three manual pages are installed by hand. Upstream installs them only with `--with-docs`, the
switch that also renders `xmllint.1` and `xmlcatalog.1` again from their DocBook sources, which
needs `xsltproc` and the DocBook stylesheets. Those are libxslt and docbook-xsl, and both depend
on libxml2, so the pages are taken as the release tarball ships them: `doc/xml2-config.1`,
`dist-doc/xmllint.1` and `dist-doc/xmlcatalog.1`.

[gettext](#port-gettext) builds its own copy of libxml2's parser rather than linking this one,
because libxml2's dependency on doxygen reaches back to gettext through bison.
