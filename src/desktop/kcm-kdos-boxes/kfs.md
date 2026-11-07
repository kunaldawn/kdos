## About

Boxes is the System Settings page for KDOS's boxes, under Security & Privacy → Application
Permissions. It lists every box `kdos-box list` reports, under its application's name and icon,
and for one box shows its lifecycle (start, stop, restart, recreate, remove, export as a `.ktar`
archive), its colour (the title-bar colour KWin gives its windows, one swatch per installed KDOS
colour scheme) and its permissions: network, home folder and what it writes, devices, graphics
card and sound, memory, processor and process limits, when it stops while idle, whether its
windows show, when it may use the X11 display, and which desktop-wide Wayland and bus grants it
holds. While a box runs, the page says which boundary held for it — confined, tagged or not
confined — and why, in the words kdos-appbox recorded at its last launch.

The page writes no file of its own about a box and never calls podman. It reads the profile
from `~/.config/kdos/boxes/<box>.conf`, shows beside each control what `kdos-box profile <box>`
says enforces that key and every `!` line about a value it cannot deliver, and applies a change
with `kdos-box profile <box> key=value` for the keys that changed. Lifecycle buttons run
`kdos-box` and `kdos-appbox` the same way, as the user and without root. Opening it as
`systemsettings kcm_kdos_boxes --args <box>` opens that box, which is how Discover's "Runs in a
box" badge links to it.

## Flags

- `-D BUILD_TESTING=OFF` — Leave the QtTest suites out of the package. They build the real
  `kdos-box` from `src/system/kdos-appbox` with that port's own `build.sh` and run it over a fake
  container engine, which `testing/plasmatest.sh` does in the development container.

## Notes

A namespace, a volume or a podman limit cannot change on a live container, so a change to
network, home folder, what it writes, devices, graphics card, the init process or a limit, on a box
that has a container, is written and takes effect only when the box is created again. The page
remembers such changes in `$XDG_STATE_HOME/kcm_kdos_boxesstaterc` and shows a Recreate banner until
the box has been made again; Recreate stops, removes and creates the box with kdos-box. Only a
pack box writes outside its container; any other box keeps its writes inside it, so recreating or
removing one deletes them, and the confirmation says so.

The list is read from `kdos-box list --json` alone. The table `kdos-box list` prints is for a
person, and its widths and its two-word STATE make it no format to parse.

The swatches are read from `/usr/share/color-schemes/KDOS<Accent>.colors` and `KDOSBox.colors`
through KColorScheme. An accent whose scheme is not installed cannot be chosen, and Neutral, the
default, can always be.

The QML and the C++ share the translation domain `kcm_kdos_boxes`, the plugin id, because
`KQuickConfigModule::mainUi()` sets the QML engine's domain to the plugin id. `X-DocPath` is
`kdos/04-programs/kdos-settings.html#boxes`, a path under `help:/`: System Settings' Help button puts
`help:/` in front of it, and KIO's `help:` worker finds the page through
`/usr/share/doc/HTML/en/kdos`, the link [kdos-doc](#port-kdos-doc) installs (see
[Help](../../../docs/kdos/04-programs/kdos-settings.md#help)). The test asserts the `kdos/` form.
