## About

libxslt is the XSLT processor built on [libxml2](#port-libxml2), with libexslt for the EXSLT
extension functions, and `xsltproc` is its command line. In KDOS its main work is at build time:
every port that renders manual pages from DocBook runs `xsltproc` with the
[docbook-xsl](#port-docbook-xsl) stylesheets, directly or through [xmlto](#port-xmlto) and
[asciidoc](#port-asciidoc)'s `a2x`. Applications link it too, among them GIMP, Inkscape,
LibreOffice, darktable and digiKam.

docbook-xsl's patch makes its string replacement call `str:replace`, an EXSLT function libexslt
provides, when the processor has it; see that port's page.

## Flags

- `--with-python=/usr/bin/python3` — Builds the Python bindings, the `libxslt` module and its
  `libxsltmod.so`. configure tests only whether the value is `no`, which upstream's default is
  not, and finds the interpreter itself with `AM_PATH_PYTHON`; the path given here is not read.
