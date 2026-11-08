## About

kde-dev-scripts is the KDE project's collection of developer scripts: include and moc-include
fixers (`fixkdeincludes`, `includemocs`, `addmocincludes`), `extractrc` and `extractattr` for
message extraction, `kde_generate_export_header`, class renaming and copying helpers, `uncrustify`
configurations for KDE's code style, and a set of CVS and Subversion helpers. Nothing is compiled.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6=ON` — Make the `find_package(KF6 … COMPONENTS DocTools)`
  call required, so [kdoctools](#port-kdoctools) is a requirement. Upstream finds it quietly and
  builds the manual pages of the scripts only when it is found.
- `-D KF_SKIP_PO_PROCESSING=ON` — Passed as every KDE recipe passes it. The module calls no
  `ki18n_install()`; its `po/` holds translated manual pages, which `kdoctools_install(po)` builds
  and kpkg's language rule removes (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Notes

`build.sh` removes `kde-systemsettings-tree.py` from `/usr/bin`: it is Python 2 (a `print`
statement) and no Python here can run it. The other Python scripts run under
[python3](#port-python3), the shell scripts under sh and [bash](#port-bash), and the Perl ones
under [perl](#port-perl), all in `depends` for run time; several drive [git](#port-git).

About a third of the scripts drive CVS or Subversion (`cvs*`, `svn*`, `colorsvn`), and
`uncrustify-kf5` runs uncrustify; none of those three tools is a port, so those scripts stop with
the tool not found. They are installed as upstream lists them.
