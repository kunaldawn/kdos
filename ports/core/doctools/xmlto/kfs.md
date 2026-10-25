## About

xmlto converts a DocBook document into a manual page, HTML, text or another format: it picks
the stylesheet for the format asked for, runs `xsltproc`, and hands the result to a
post-processor when the format needs one. `xmlif`, installed with it, selects the conditional
sections of an XML file. In KDOS it is a build tool: git, alsa-utils, zbar and the other ports
that depend on it render their manual pages through `xmlto man` (see
[Manual pages](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#manual-pages)).

xmlto is a shell script. Its `configure` writes the programs it calls into the installed script:
one it finds in the build root by its absolute path, one it does not find by its bare name, which
the script then looks up on `PATH` each time it runs. configure also checks that `getopt` handles long options, which the one from
[util-linux](#port-util-linux) does, and stops when it does not.

## Flags

- `XML_CATALOG_FILES="/etc/xml/catalog"` — Names the XML catalogue libxml2 reads. The build makes
  `xmlto.1` and `xmlif.1` by running the freshly built `xmlto man` over `doc/xmlto.xml` and
  `doc/xmlif.xml`, and the stylesheet it applies is named by its docbook.sourceforge.net address,
  which only the catalogue that [docbook-xsl](#port-docbook-xsl) installs maps onto a local file.
  The file named is the one libxml2 reads when the variable is unset.
- `autoreconf -vfi` — The release tarball carries `configure.ac` and `Makefile.am` and no generated
  `configure`.
- `PAPER_CONF=paperconf` — Presets the variable configure's `AC_PATH_PROG` would otherwise fill
  with the absolute path of `paperconf`, the program of [libpaper](#port-libpaper) that xmlto asks
  for the default paper size. With the bare name, the installed script finds `paperconf` on `PATH`
  each time it runs. libpaper is installed in a later phase, and an absolute path would record
  whether it happened to be present when xmlto was configured (see
  [An optional tool found in the tree](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/build-troubleshooting.md#an-optional-tool-found-in-the-tree)).

## Notes

The `LINKS="/usr/bin/links"` line in `build.sh` sets a shell variable without exporting it, and
`configure` is not passed it, so it changes nothing. The installed `xmlto` looks for `elinks` or
`links` itself, on `PATH`, when it is asked for text output.
