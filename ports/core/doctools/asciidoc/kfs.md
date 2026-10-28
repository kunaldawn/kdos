## About

AsciiDoc is a plain-text markup for documents and manual pages, and `a2x` its front end that
renders AsciiDoc through DocBook into a manual page, HTML or other formats. This is the original
Python implementation, `asciidoc-py`, not Asciidoctor, which is a port of its own. In KDOS it is a
build tool: git, calcurse, imv, libtraceevent, ocl-icd and the other ports that depend on it
write their manual pages in AsciiDoc and run `asciidoc` or `a2x` to make them (see
[Manual pages](../../../../docs/kdos/05-developer/writing-ports.md#manual-pages)).

`a2x -f manpage` writes DocBook, checks it with `xmllint --nonet --valid` against the DTD from
[docbook-xml](#port-docbook-xml), and converts it with `xsltproc` and the
[docbook-xsl](#port-docbook-xsl) stylesheets. docbook-xml, libxslt and docbook-xsl are in
`depends`; `xmllint` comes with [libxml2](#port-libxml2), which libxslt depends on.

## Flags

- `XML_CATALOG_FILES=/etc/xml/catalog` — Names the XML catalogue libxml2 reads. `make` renders
  `asciidoc.1` and `a2x.1` by running `a2x` from the source tree, and the manual-page stylesheet
  it applies imports `http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl`; the
  catalogue maps that address onto the installed docbook-xsl. The file named is the one libxml2
  reads when the variable is unset.
- `autoconf` — The source is GitHub's archive of the release tag, which carries `configure.ac` and
  no generated `configure`.
- `--ignore-installed` — Installs the Python package into `$PKG` without pip first removing an
  installed asciidoc from the build root; see
  [The three Python keys](../../../../docs/kdos/05-developer/writing-ports.md#the-three-python-keys).
- `--no-build-isolation` — Builds with the setuptools already installed rather than fetching one
  into a fresh environment, which needs the network.
- `--no-deps` — Installs asciidoc alone; pip resolves no dependencies.

## Notes

The recipe runs `make` and then `pip install` itself, and never `make install`: upstream's install
target calls `python3 -m pip install --root $(DESTDIR) .` with none of the flags above. The two
manual pages `make` renders are installed by hand. The third page it renders, `testasciidoc.1`,
is not installed, and neither is a `testasciidoc` program: the package's entry points are
`asciidoc` and `a2x`.
