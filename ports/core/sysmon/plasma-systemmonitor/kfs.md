## About

Plasma System Monitor is the desktop's resource monitor: editable pages of sensor faces (line and
bar charts, gauges, tables) for CPU, memory, disks, network and GPUs, a History page and the
process table, each face exportable to the panel or desktop as a widget. The sensors come from the
[ksystemstats](#port-ksystemstats) daemon over D-Bus, read through
[libksysguard](#port-libksysguard), which also kills and renices processes through its KAuth
helper. Meta+Esc opens it: the install puts a copy of the desktop entry under
`/usr/share/kglobalaccel`, whose `X-KDE-Shortcuts` the global shortcut daemon reads.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Kirigami=ON` — With
  `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6QuickCharts=ON`: upstream looks for
  [kirigami](#port-kirigami) and [kquickcharts](#port-kquickcharts) only as run-time
  dependencies, and a build without them succeeds. The whole interface is Kirigami QML and every
  chart face is a QuickCharts item, so both are required here and their absence stops the
  configure.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The Applications page, and the Applications table on the Overview page, group processes by the
cgroup each application was started in, through [libksysguard](#port-libksysguard)'s application
model, whose KDOS patches root it at the login's cgroup and count a box's container processes with
its scope, so each running box is one application. The distribution defaults set `pageOrder` in
`systemmonitorrc` under `/usr/share/kdos/xdg` to `overview.page`, `kdos.page`,
`applications.page`, `history.page` and `processes.page`; `pageOrder` only sorts, and a page left
out of it is still shown, after the listed ones. The file is `systemmonitorrc`, the name the
program's `.kcfg` gives, and it is read through `XDG_CONFIG_DIRS`, so a person's own
`~/.config/systemmonitorrc` overrides it.

The "Get New Pages…", "Get New Presets…" and "Get New Display Styles…" actions are KNewStuff
downloads (`plasma-systemmonitor.knsrc`, `systemmonitor-presets.knsrc` and
`systemmonitor-faces.knsrc`). KDOS turns KNewStuff off with `ghns=false` under
`[KDE Action Restrictions]` in `/usr/share/kdos/xdg/kdeglobals`; KNewStuff's QML action is then
disabled and hidden. The File menu's "Get New Pages…" calls the action directly and the sidebar's
context menu shows it greyed out, so `kiosk-get-new.patch` gives both the kiosk check's visibility.
The menus then offer none of them and nothing is downloaded.

[ksystemstats](#port-ksystemstats) is in `depends` for run time; the configure only looks for its
program and reports it missing in a build root without it. The session bus starts it on the first
sensor request. [python3](#port-python3) is there for `plasma-systemmonitor-replace-vmpss.py`,
the `kconf_update` script that rewrites an older configuration's memory column, which
`kconf_update` runs once per account.
