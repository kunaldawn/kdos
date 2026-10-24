## About

procs lists processes like `ps`, in coloured columns with a search over every field, and can show
a tree, a watch mode and each process's ports and Docker container. The package adds a desktop
entry, *Processes*, that opens it in a terminal from the launcher.

## Flags

- `XML_CATALOG_FILES=/etc/xml/catalog` — Make `xsltproc`, which `a2x` runs to turn the DocBook it
  generates into a manual page, resolve the DocBook DTD and stylesheets through the system catalog
  rather than by fetching their URLs. The catalog is written by
  [docbook-xml](#port-docbook-xml) and [docbook-xsl](#port-docbook-xsl).

## Notes

The crates come from the vendor bundle, `procs-vendor-<version>.tar.xz`, made by `ports/fetch`
with `cargo vendor` and unpacked into the top of the source tree, where cargo finds its
configuration; the build runs `--frozen --offline` (see
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring)).

Upstream ships its manual as AsciiDoc, `man/procs.1.adoc`, and cargo does not build it. `build.sh`
renders it with [asciidoc](#port-asciidoc)'s `a2x -f manpage`, which is why `asciidoc`,
`docbook-xsl` and `libxslt` are in `depends`. The desktop entry is written by the recipe because
upstream ships none; see
[Desktop entries belong to the port](../../../../docs/kdos/05-developer/writing-ports.md#desktop-entries-belong-to-the-port).
