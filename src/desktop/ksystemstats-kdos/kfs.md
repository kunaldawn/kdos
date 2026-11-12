## About
`ksystemstats-kdos` is three things System Monitor reads: a ksystemstats sensor plugin,
`ksystemstats/ksystemstats_plugin_kdos.so`; a sensor face, KDOS Meters (`org.kdos.meters` under
`/usr/share/ksysguard/sensorfaces`); and two System Monitor pages, the KDOS page,
`/usr/share/plasma-systemmonitor/kdos.page`, and the Overview System Monitor opens on,
`kdos-overview.page`.

The plugin publishes one container, `kdos`:

| Sensors | What they read |
|---|---|
| `kdos/box/<box>/{name,icon,cpu,memory,memoryLimit,ioRead,ioWrite,processes,energy}` | Each running box's cgroup scope, `app-kdos-<box>-<n>.scope`, in a login's `delegated` subtree or under `user@<uid>.service/app.slice`, or, for a box with none, its processes: the session's uid's, in a mount namespace other than the plugin's, with `KDOS_BOX=<box>` in their environment; the name and icon of the launcher whose `X-KDOS-Box` names the box |
| `kdos/box/all/{boxes,cpu,memory,ioRead,ioWrite,processes}` | The same, summed over every box |
| `kdos/energy/<app>[@<box>]/{name,app,box,impact,cpuTime,gpu}` and `kdos/energy/all/…` | kdos-energyd's `report-json` on `/run/kdos-energyd.sock` |
| `kdos/slot/{status,running,runningVersion,runningState,other,otherVersion,otherState,next,trial,attempts}` | `/run/kdos/bootstate.json`, the copy of `kdos-bootctl status --json` every user may read |
| `kdos/look/phosphor` | kwinrc `[Plugins] kdos_crtEnabled`, 1 while Phosphor mode is on |

KDOS Meters draws one row per sensor: its name, a sparkline in the accent and its figure in the
monospace face, over a faint scanline ground while `kdos/look/phosphor` reads 1. A text sensor has
no sparkline, and its text wraps after the name. In a horizontal panel a meter's sensors stand side
by side, a download or read figure marked ↓ and an upload or write figure ↑, and a panel short of
room narrows their sparklines, down to none, before any figure. Opened from a panel, the face is a
popup chart whose sparklines share the popup's height. Its
`sensorPattern` setting makes it follow every sensor whose id matches a regular expression as
sensors come and go, which is how the KDOS page gives each box and each application a row of its
own. The page has four parts: Overview (this machine, all boxes, the system), Boxes (CPU, memory
and disk per box), Energy (impact per application and the ledger) and Slots (the running slot
and the other one). `kdos-overview.page` is upstream's Overview with only sensors every machine
has: the disk and network totals, and a KDOS Meters face following `gpu/gpu<n>/usage` for the
graphics cards, where the stock page's per-disk, per-interface and GPU sensors would put a
"missing some sensors" banner over it on the live medium and in a virtual machine.

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
`kdos/box/www/cpu` is `box/www` and the sensor tree shows KDOS ▸ Boxes ▸ Box www. A tree node
takes its title only from an object at its exact path, so the plugin adds two objects with no
sensors, `box` and `energy`, which title the `kdos/box` and `kdos/energy` nodes; without them the
sensor picker names those groups by their raw paths. A box name or an
application name never carries a `/` into a path: a scope whose unescaped name holds one, or a
`KDOS_BOX` value that does, is not taken for a box, and an energy row's id keeps only letters, digits, `.`, `_`, `-` and `@`.

A box's figures are its scope's own `cpu.stat`, `memory.current`, `memory.max`, `io.stat` and
`pids.current`. cgroup v2 counts every descendant in them, so podman's `libpod-<id>` and conmon
children are inside the totals without the plugin walking them. Only scopes the session's own uid
owns are read: another user's boxes are theirs. No box gets a scope on KDOS: elogind leaves
`/sys/fs/cgroup/<session>/cgroup.procs` root's, cgroup2 refuses the move into `delegated`, and
every box runs in the login's `session` cgroup, with its limits unenforced, which `kdos-box
profile` says. Such a box is found by its processes instead (`kdosBoxesByProcess`): `kdos-appbox`
sets `KDOS_BOX` in every box's container and podman passes it to every exec, and the mount
namespace test keeps out a host program that inherited the variable from a `kdos-box enter`
shell. Its figures are its processes' sums: `utime`, `stime`, `cutime` and `cstime` from
`/proc/<pid>/stat`, so a child reaped inside the box keeps its time in the sum; `Pss` from
`smaps_rollup`, which does not count a page shared between two of its processes twice, or the
resident set from `statm` where that file cannot be read; `read_bytes` and `write_bytes` from
`/proc/<pid>/io`; and a `memoryLimit` of 0. A box with a scope is never counted twice: its name is
passed to the process walk to skip. See
[known gaps](../../../docs/kdos/06-reference/known-gaps.md#boxes-are-measured-by-their-processes-and-their-limits-are-unenforced).

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
