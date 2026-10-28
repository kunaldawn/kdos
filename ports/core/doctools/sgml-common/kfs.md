## About

sgml-common is the base of the SGML side of the DocBook tools: the ISO 8879 character-entity sets
in SGML and XML form, under `/usr/share/sgml`, with a catalog for each, the SGML declaration for
XML (`xml.dcl`), and `install-catalog`, the script that adds a catalog to a centralized catalog and registers that
one in `/etc/sgml/catalog`. In KDOS it is a dependency of [docbook-xml](#port-docbook-xml) and of
[opensp](#port-opensp).

The source archive's `install-sh`, `missing` and `mkinstalldirs` are not files but absolute
symbolic links to `/usr/share/automake/…`, a directory no automake of the tree installs
(automake's files are under `/usr/share/automake-<version>`). The first three lines of `build.sh`
replace each link with the installed automake's copy before `configure` runs.

## Flags

- `--remove-destination` — Remove each symbolic link before copying over it, so the copy lands in
  the source directory instead of following the link to a directory that does not exist.
- `docdir=/usr/share/doc` — `configure.in` sets `docdir` to `$(prefix)/doc` and offers no switch
  for it, so it is overridden on the `make install` line. The HTML manual then goes under
  `/usr/share/doc/sgml-common-<version>/html` instead of `/usr/doc/…`, a directory kpkg's prune
  never looks in. Under `/usr/share/doc` the prune removes the `html` directory, as it removes HTML
  manuals from every package, so of the documentation only the `install-catalog(8)` manual page
  ships.

## Notes

The recipe registers nothing: no port runs `install-catalog`, and `/etc/sgml/catalog` does not
exist on KDOS, so a tool that looks these catalogs up through that file does not find them.
[opensp](#port-opensp) is built with that file as its default catalog
(`--enable-default-catalog=/etc/sgml/catalog`).
