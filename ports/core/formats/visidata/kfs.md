## About

VisiData is a terminal spreadsheet: `vd` opens a CSV, TSV, JSON, Excel, SQLite, HTML or YAML file,
or a URL, as a sheet to sort, filter, pivot, chart and edit from the keyboard. In KDOS it is the
terminal program a `.csv` file can open in, beside LibreOffice Calc.

## Flags

- `--ignore-installed` — Writes the packages into `$PKG` even when a copy is already in the build
  root; without it pip would uninstall that copy from the root first (see
  [The three Python keys](../../../../docs/kdos/05-developer/writing-ports.md#the-three-python-keys)).
- `--no-deps`, `--no-index`, `--find-links=vendor` — Installs exactly the two named things,
  `standard-mailcap` and VisiData itself, from the vendor directory alone, resolving nothing and
  reaching no index.

## Notes

The vendor bundle, `visidata-vendor-<version>.tar.xz`, is a Python bundle that `ports/fetch` makes
from the source distribution's own metadata, as source distributions with their build requirements,
and the recipe pins by its `sha256` (see
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring)).
`pyrequirements = no` is what keeps the bundle small: VisiData's `requirements.txt` is the optional
loader set and reaches pandas, scipy and a Fortran compiler, where its `install_requires` is
python-dateutil, with `standard-mailcap` on Python 3.13 and later (see
[The three Python keys](../../../../docs/kdos/05-developer/writing-ports.md#the-three-python-keys)).

`standard-mailcap` is the one run-time module the package takes from the bundle. Python 3.13
removed `mailcap` from the standard library, it is not a port, and VisiData's `sysopen-mailcap`
commands, which open a file in the program its mailcap entry names, import it. python-dateutil (the
date column type), lxml (HTML and XML), openpyxl (xlsx), PyYAML and requests (a URL as a source)
are ports and come in through `depends`. VisiData imports each loader's library only when that
format is opened, so a missing one costs only that format.

`build.sh` replaces the desktop entry upstream installs, `visidata.desktop`, with one of the same
name: the same `Exec`, an icon the shipped icon theme carries, and the `text/csv` MIME type
upstream leaves off. Keeping the name keeps one menu row for the program.
