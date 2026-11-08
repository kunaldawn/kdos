## About
`ksystemstats-kdos` is three things System Monitor reads: a ksystemstats sensor plugin,
`ksystemstats/ksystemstats_plugin_kdos.so`; a sensor face, KDOS Meters (`org.kdos.meters` under
`/usr/share/ksysguard/sensorfaces`); and System Monitor's KDOS page,
`/usr/share/plasma-systemmonitor/kdos.page`.

The plugin publishes one container, `kdos`:

| Sensors | What they read |
|---|---|
| `kdos/box/<box>/{name,icon,cpu,memory,memoryLimit,ioRead,ioWrite,processes,energy}` | Each running box's cgroup scope, `app-kdos-<box>-<n>.scope`, in a login's `delegated` subtree or under `user@<uid>.service/app.slice`; the name and icon of the launcher whose `X-KDOS-Box` names the box |
| `kdos/box/all/{boxes,cpu,memory,ioRead,ioWrite,processes}` | The same, summed over every box |
| `kdos/energy/<app>[@<box>]/{name,app,box,impact,cpuTime,gpu}` and `kdos/energy/all/…` | kdos-energyd's `report-json` on `/run/kdos-energyd.sock` |
| `kdos/slot/{status,running,runningVersion,runningState,other,otherVersion,otherState,next,trial,attempts}` | `/run/kdos/bootstate.json`, the copy of `kdos-bootctl status --json` every user may read |
| `kdos/look/phosphor` | kwinrc `[Plugins] kdos_crtEnabled`, 1 while Phosphor mode is on |

KDOS Meters draws one row per sensor: its name, a sparkline in the accent and its figure in the
monospace face, over a faint scanline ground while `kdos/look/phosphor` reads 1. Its
`sensorPattern` setting makes it follow every sensor whose id matches a regular expression as
sensors come and go, which is how the KDOS page gives each box and each application a row of its
own. The page has four parts: Overview (this machine, all boxes, the system), Boxes (CPU, memory
and disk per box), Energy (impact per application and the ledger) and Slots (the running slot
and the other one).

## Flags
- `-S "$PORT_SRC"` — The sources are the port's own directory: a port of KDOS's own has no
  tarball, and the build tree is made in the empty `$SRC`.
- `-D KDE_INSTALL_USE_QT_SYS_PATHS=ON` — Install the plugin into Qt's own plugin directory,
  `/usr/lib/qt6/plugins`, where ksystemstats looks for `ksystemstats/`.
- `-D BUILD_TESTING=OFF` — Build no tests into the package. The QtTest suite in `tests/` is run
  by the Qt regression net in the development container, not by the image build.
- `-D CMAKE_INSTALL_PREFIX=/usr`, `-D KDE_INSTALL_LIBDIR=lib`, `-D CMAKE_BUILD_TYPE=Release` —
  The tree's prefix and library directory, which ECM would otherwise put under `lib64`, and an
  optimised build.

## Notes
The plugin is built against libksysguard's `SensorPlugin`, which has no stable API: it is rebuilt,
and its tests rerun, with every Plasma minor release, alongside the other version-locked units.

ksystemstats splits a sensor path at its first and last `/`, so the object of
`kdos/box/www/cpu` is `box/www` and the sensor tree shows KDOS ▸ box ▸ www. A box name or an
application name never carries a `/` into a path: a scope whose unescaped name holds one is not
taken for a box, and an energy row's id keeps only letters, digits, `.`, `_`, `-` and `@`.

A box's figures are its scope's own `cpu.stat`, `memory.current`, `memory.max`, `io.stat` and
`pids.current`. cgroup v2 counts every descendant in them, so podman's `libpod-<id>` and conmon
children are inside the totals without the plugin walking them. Only scopes the session's own uid
owns are read: another user's boxes are theirs. A box started where no delegated scope could be
made has no scope, and so no row; its limits are unenforced as well, which `kdos-box profile` says.

ksystemstats starts when its first client asks for sensors, and an application's energy sensors
exist only once a report has named them, so the first report is read before the daemon answers
anyone, waiting at most a second. After that kdos-energyd is asked every ten seconds while an
energy sensor is watched and every minute while none is, so a page opened later still finds the
applications. The ledger is root's and wheel's alone; for anyone else the status sensor says so
and no application sensors exist.

A pattern in a face's sensor list is resolved once, when the face loads, so a box started later
would never be shown by a stock face. KDOS Meters matches its `sensorPattern` against the live
sensor tree instead, which is why the KDOS page's per-box and per-application faces are KDOS
Meters with an empty sensor list.
