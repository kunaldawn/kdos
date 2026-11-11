## About

Ark is KDE's archive manager. It opens, browses, extracts and creates archives through plugins:
one on [libarchive](#port-libarchive) for tar and its compressed forms, one on
[libzip](#port-libzip) for zip, and plugins that drive command-line tools for the rest. The
`7z`, `zip` and `unzip` programs those plugins run are in `depends` ([7zip](#port-7zip),
[zip](#port-zip), [unzip](#port-unzip)); `unrar` and `rar`, `lsar` and `unar`, and `arj` are not
ports, so the RAR, unarchiver and ARJ plugins find nothing to run. RAR archives are read by the
read-only libarchive plugin instead (see Patches).

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — `ki18n_install(po)` returns at once, so none of the message
  catalogues in `po/` is compiled or installed. kpkg removes every non-English catalogue from the
  package anyway (see
  [size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so this only saves build time. The translated handbooks under `po/<lang>/docs/` are still built by
  `kdoctools_install(po)`, and kpkg removes those too.
- `-D BUILD_DOC=ON` — Build Ark's handbook with [kdoctools](#port-kdoctools), which is then a
  required package. On by default upstream; named so the handbook cannot drop out.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_LibZip=ON` — Ark looks for LibZip as optional and builds the
  libzip plugin only when it is found. Made required, a missing libzip stops the configure instead
  of quietly leaving zip to the command-line plugin alone (see
  [a graphical application](../../../../docs/kdos/05-developer/writing-ports.md#a-graphical-application)).

## Notes

Upstream lists RAR only for the `unrar` and `unarchiver` plugins, and a plugin is used only when
its programs are found, so without `rar-libarchive.patch` Ark would claim RAR, fail with "No
suitable plugin found" and leave `ark --batch` waiting with nothing to extract. libarchive reads
RAR 4 and RAR 5 itself; with `application/vnd.rar` in the read-only libarchive plugin's types
Ark opens, browses and extracts RAR, read-only, and an encrypted RAR is not opened. The unrar
plugin's higher priority keeps it first wherever `unrar` is installed. The patch is to be offered
upstream to Ark.
