## About

libksysguard holds the libraries behind KDE's system monitoring: `processcore`, which reads the
process list from `/proc` and changes a process's priority or sends it a signal; the sensor
client that talks to ksystemstats over D-Bus; the formatter for units and sizes; the chart
"faces" that System Monitor and the system monitor widgets draw with; and `systemstats`, the
plugin interface ksystemstats loads its sensor providers through. KDevelop's attach-to-process
dialog uses its process list, and [plasma5support](#port-plasma5support) uses it to name the
programs that keep a device busy.

## Flags

- `-Wl,--strip-debug` — Drop debug information at the link. Upstream compiles the KAuth helper
  with `-ggdb` in that target's own compile flags, which CMake places after the exported
  `CXXFLAGS`, so no `-g0` there can cancel it (see
  [What kpkg does around build.sh](../../../../docs/kdos/05-developer/writing-ports.md#what-kpkg-does-around-buildsh)).
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the translations of its messages, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D ENABLE_KAUTH_HELPER=ON` — Build `ksysguardprocesslist_helper`, the KAuth helper that kills,
  renices and reschedules a process that belongs to another user, and install its polkit actions
  and D-Bus system service. Every action is `auth_admin`, so polkit asks for an administrator's
  password through the session's agent (see
  [Security model](../../../../docs/kdos/03-architecture/security-model.md)).
- `-D CMAKE_DISABLE_FIND_PACKAGE_libpcap=ON` — No per-application network usage plugin. It
  captures packets with libpcap from a helper that needs `CAP_NET_RAW` as a file capability,
  which kpkg's package tar does not carry, so the plugin could not start.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_UDev=ON` — Make libudev, from [eudev](#port-eudev), a
  requirement. The process list's GPU plugin finds the graphics cards through it; upstream looks
  for it quietly and leaves the plugin out without it.

## Patches

`aggregate-scope-children.patch` counts, for an application's `.scope`, the processes of the
scope's descendant cgroups as well as its own `cgroup.procs`, skipping a descendant that is a
`.scope` or `.service` of its own, which the model lists and counts as a node. A box's processes
live in podman's `libpod-<id>` and conmon children of the box's `app-kdos-<box>-<n>.scope`, and
without the patch System Monitor's Applications page finds such a scope empty. The walk stops at
eight levels and 1024 cgroups. Nothing in it is KDOS's: it serves any engine that nests a cgroup
under an application scope, Flatpak's helpers among them.

`login-cgroup-root.patch` chooses where the application model starts. Upstream it reads only
`/user.slice/user-<uid>.slice/user@<uid>.service`, the cgroup of a systemd user manager, which a
KDOS login does not have. elogind runs a login in `/<session>/session` and gives the user
`/<session>/delegated`, where `kdos-appbox` makes each box's scope; with the patch, a process whose
own cgroup is `<login>/session` beside a `<login>/delegated` directory roots the model at
`<login>`, so the Applications page and the Overview page's Applications table list each running
box as one application, with its processor, memory and process count, under its launcher's name
and icon where the box has one launcher and under the box's name where it has several (see
[System Monitor](../../../../docs/kdos/04-programs/plasma-shell.md#system-monitor)). A system with
`user@<uid>.service` keeps upstream's root, and any other layout keeps it too, which the model
reports as unavailable. `/proc/self/cgroup` is read whole, not line by line until `atEnd()`:
procfs gives the file a size of 0, so `atEnd()` holds before the first read. Nothing in it is
KDOS's either: any elogind system that delegates session cgroups has the same layout. Both patches
are to be offered upstream.

## Notes

The configure requires [libnl](#port-libnl) for socket information over `sock_diag`,
[lm-sensors](#port-lm-sensors) for the hardware sensors, [libdrm](#port-libdrm) and
[zlib](#port-zlib), with no switch for any of them. The faces are QML that imports
`org.kde.quickcharts`, `org.kde.kirigami`, `org.kde.kitemmodels`, `org.kde.kquickcontrols`,
`org.kde.newstuff` and `Qt5Compat.GraphicalEffects` at run time, so
[kquickcharts](#port-kquickcharts), [kirigami](#port-kirigami), [kitemmodels](#port-kitemmodels),
[kdeclarative](#port-kdeclarative), [knewstuff](#port-knewstuff) and
[qt6-qt5compat](#port-qt6-qt5compat) are in `depends`; the build does not check for them.
