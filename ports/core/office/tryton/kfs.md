## About

[Tryton]{.idx} is the GTK 3 desktop client of the Tryton business platform, the ERP that GNU
Health runs on: accounting, stock, sales and the rest, served by a Tryton server elsewhere. This
port is the client alone. The recipe follows the long-term series that GNU Health runs on, and a
client speaks only to a server of its own series.

## Flags

- `--ignore-installed` — pip installs the named packages into `$PKG` even when the build root
  already has them, instead of reporting the requirement as satisfied and installing nothing.
- `--no-deps`, `--no-index`, `--find-links=vendor`, `--no-build-isolation` — The offline Python
  install: nothing is resolved or downloaded, the one bundled package comes from `vendor/`, and the
  build backends are the ones in the build root: setuptools for the client, and hatchling with
  [python3-hatch-tryton](#port-python3-hatch-tryton), its Tryton plugin, for GooCalendar.

## Notes

The vendor bundle is made by `ports/fetch` from the recipe's `pypackages` line, which names
GooCalendar alone. GooCalendar is the widget of the client's calendar view, nothing else in the
tree imports it, and it is not a port, so it is installed into this package beside the client.
The bundle also holds the build tools the fetch downloaded with it; `build.sh` installs only
`goocalendar` from it, and the client itself from the source tree. See
[The three Python keys](../../../../docs/kdos/05-developer/writing-ports.md#the-three-python-keys).

Every interface translation under `tryton/data/locale` except the template `tryton.pot` is deleted
from the package. `build.sh` renders the SVG logo to a 128-pixel PNG with `rsvg-convert` and writes
the desktop entry, which registers the `tryton://` URL scheme.

The login dialog's Wayland app id is the program name, `tryton`, which the entry's
`StartupWMClass` names. After login the client registers a GtkApplication whose id names the
server, `org.tryton.Tryton-<major><minor>._<md5 of host:port/database>`, so the main window carries that id
and no `StartupWMClass` can name it.
