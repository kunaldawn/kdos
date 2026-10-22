## About

DocBook XML is the set of document type definitions (DTDs) a DocBook document is validated
against. Many projects write their manual pages and manuals in DocBook and turn them into roff or
HTML at build time with `xsltproc` or `xmlto`; the dbus, polkit, udisks2, gtk3, PostgreSQL, samba
and KDE `kdoctools` ports are among those that depend on this one. A document names the DTD
version it was written against in its `DOCTYPE`, so the port installs several 4.x versions side
by side, each in `/usr/share/xml/docbook/xml-dtd-<version>`, and lists all of them in an XML
catalog, `/etc/xml/docbook`.

The catalog lets every lookup be answered from disk. The build has no network, and `xmllint
--nonet` and `xmlto` validate only when each DTD the document asks for resolves to a local file.

## Notes

Each version's public identifiers (`-//OASIS//DTD DocBook XML V<version>//EN` and the rest) map straight
to `file:///usr/share/xml/docbook/xml-dtd-<version>/…`, not to the `http://www.oasis-open.org/…`
URL left for a `rewriteSystem` entry to turn into a local path. libxml2 does not pass the result of a public
lookup through the `rewriteSystem` entries, so a public entry that names the URL sends a document
whose system identifier is relative (`"docbookx.dtd"`, as gnuradio's gr-trellis manual writes it)
to the network, and validation fails. The `rewriteSystem` and `rewriteURI` entries serve a
document that names the DTD by its URL alone.

`/etc/xml/docbook` is not the catalog libxml2 reads by default; that is `/etc/xml/catalog`, which
the [docbook-xsl](#port-docbook-xsl) port writes. It delegates the DocBook XML DTD and entity
public identifiers, and the `http://www.oasis-open.org/docbook/` system identifiers and URIs, to
`/etc/xml/docbook`. A build that has this port but not docbook-xsl resolves nothing through the
default catalog. Every other port that depends on this one also depends on docbook-xsl.

The 4.1.2 archive is named `docbkx412.zip` rather than `docbook-xml-4.1.2.zip`, so `build.sh`
unpacks it on its own, after the loop over the others. The files are unpacked with `bsdtar`, from
[libarchive](#port-libarchive), which reads zip archives, and `chown -R root:root` gives them to
root before `cp -af` copies them with their ownership.
