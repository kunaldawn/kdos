# The programs

Every binary KDOS itself ships, what each is, and where it is documented. Software in
`ports/core/` is upstream's and is documented upstream; this page covers only what is written
here.

**Many KDOS binaries answer to several names**, dispatching on the name they were invoked as, so
the number of *commands* is much larger than the number of *binaries*. The full mapping is at the
bottom of this page.

## The compositor and the desktop

| Command | What it is | Page |
|---|---|---|
| `kdos-comp` | The compositor: a frozen fork of labwc with KDOS additions | [kdos-comp](kdos-comp.md) |
| `kdos-shell` | The panel, and twenty-seven other surfaces under other names | [kdos-shell](kdos-shell.md) |
| `kdos-res` | The resource monitor | [kdos-res](kdos-res.md) |
| `kdos-lock` | The lock screen | [The daemons](daemons.md) |
| `kdos-desktop` | Starts a session. A shell script | [The session](../03-architecture/session.md) |
| `kdos-desktop-start` | Brings up audio and portals, then the compositor. A shell script | [The session](../03-architecture/session.md) |
| `kdos-bb` | The forked ASCII-art demo | [kdos-bb](kdos-bb.md) |

## Root daemons

Each runs in the foreground under `ksvc`, owns one socket in `/run`, and authorises by the
caller's credentials. All are documented in [The daemons](daemons.md).

| Command | Owns |
|---|---|
| `kdos-powerd` | Suspend, poweroff, reboot |
| `kdos-energyd` | Reading the CPU energy counter and attributing it per application |
| `kdos-oomd` | Killing something before memory pressure wedges the desktop |
| `kdos-mountd` | Mounting removable media |
| `kdos-packd` | Mounting, installing and composing application packs |
| `kdos-boxsock` | Tagging box clients so the compositor can identify them |
| `xdg-desktop-portal-kdos` | The file chooser, settings and application-chooser portal backends |

Their clients are ordinary user commands:

| Command | Asks |
|---|---|
| `kdos-power` | `kdos-powerd` |
| `kdos-energy` | `kdos-energyd` |

## Privileged helpers

Both are setuid root, and both are deliberately tiny. See
[The security model](../03-architecture/security-model.md).

| Command | Does | Takes |
|---|---|---|
| `kdos-checkpass` | Checks the caller's own password | **No arguments**; the password on stdin |
| `kdos-resctl` | Signals or renices a process; reads the hardware table | Three verbs, no paths, no options |

## Packaging and boxes

| Command | What it is | Page |
|---|---|---|
| `kpkg` | The package manager, under five names | [Packaging](../03-architecture/packaging.md) |
| `kdos-pack` | Builds, signs, indexes and diffs application packs | [Packs and boxes](../03-architecture/packs-and-boxes.md) |
| `kdos-appbox` | Launches boxed applications and generates their launchers | [kdos-appbox](kdos-appbox.md) |
| `kdos-box` | Manages boxes. The same binary as `kdos-appbox` | [kdos-appbox](kdos-appbox.md) |
| `kdos-boxinit` | Process 1 inside a box. Statically linked | [Packs and boxes](../03-architecture/packs-and-boxes.md) |

## System tools

| Command | Does | Page |
|---|---|---|
| `kdos` | The front door: nineteen subcommands | [The kdos command](kdos-command.md) |
| `kinstall` | The installer | [kinstall](kinstall.md) |
| `ksvc` | The service supervisor | [Boot and init](../03-architecture/boot-and-init.md) |
| `service` | The same binary, under the familiar name | [Administration](../02-user-guide/administration.md) |
| `kdos-getty` | Loads the console font and palette, then runs a getty | [Boot and init](../03-architecture/boot-and-init.md) |
| `kdos-bootctl` | Chooses and confirms the A/B root slot | [Boot and init](../03-architecture/boot-and-init.md) |
| `kdos-splash` | The boot splash | [Boot and init](../03-architecture/boot-and-init.md) |
| `kdos-banner` | The login banner | [Boot and init](../03-architecture/boot-and-init.md) |
| `kdos-shot` | Screenshots | [The desktop](../02-user-guide/desktop.md) |
| `kdos-theme` | Generates the GTK, icon and cursor themes | [Theming](../02-user-guide/theming.md) |
| `kdos-sfx`, `kdos-fetch-app`, `kdos-fetch-static` | Small helpers | [The kdos command](kdos-command.md) |

## Build and development tools

These run on a build host and **never ship on the target**.

| Command | Does | Page |
|---|---|---|
| `kdosbuild` | The build orchestrator | [The build system](../05-developer/build-system.md) |
| `kdos-portup` | Checks every port for a newer upstream release | [Writing ports](../05-developer/writing-ports.md) |
| `ports/fetch` | Downloads and vendors sources | [Writing ports](../05-developer/writing-ports.md) |
| `ports/update` | The front end to the version checker | [Writing ports](../05-developer/writing-ports.md) |
| `ports/sources` | Publishes and fetches release assets | [Writing ports](../05-developer/writing-ports.md) |
| `ports/appbox/bake` | Bakes the application catalogue | [Packs and boxes](../03-architecture/packs-and-boxes.md) |
| `testing/*` | The test and rig harnesses | [Testing](../05-developer/testing.md) |

## Multi-name binaries

Three binaries provide most of the commands on the system. If you find a command on your `PATH`
and want its documentation, find it here.

### `kdos-shell` — 28 names

The panel plus every surface that pops up from it. One binary, dispatched on its own name; the
authoritative list is the name table in its own `main.c`.

| | | | |
|---|---|---|---|
| `kdos-shell` | `kdos-start` | `kdos-launcher` | `kdos-menu` |
| `kdos-desk` | `kdos-pick` | `kdos-ascii` | `kdos-run` |
| `kdos-prompt` | `kdos-notifyd` | `kdos-notify` | `kdos-osd` |
| `kdos-cal` | `kdos-display` | `kdos-keys` | `kdos-teams` |
| `kdos-saver` | `kdos-slit` | `kdos-doc` | `kdos-settings` |
| `kdos-openwith` | `kdos-audio` | `kdos-net` | `kdos-bt` |
| `kdos-devices` | `kdos-clip` | `kdos-status` | `kdos-tip` |

All are documented in [kdos-shell](kdos-shell.md).

### `ksvc` — 9 names

The supervisor, and the tools that ride on the same binary.

| Name | Is |
|---|---|
| `ksvc` | The service supervisor |
| `service` | The same, under the conventional name |
| `kdos-getty` | The console font and palette wrapper around a getty |
| `kdos-bootctl` | A/B slot selection, also copied into the initramfs |
| `kdos` | The front door and its nineteen subcommands |
| `kdos-banner` | The login banner |
| `kdos-shot` | Screenshots |
| `kdos-sfx` | Sound effects |
| `kdos-fetch-app`, `kdos-fetch-static` | Fetch helpers |

### `kpkg` — 5 names

| Name | Is |
|---|---|
| `kpkg` | The front end |
| `kpkgadd` | Install a prebuilt package file |
| `kpkgbuild` | Build a port without installing it |
| `kpkgdel` | Remove a package |
| `kpkgdepends` | Print the resolved install order |

### `kdos-appbox` — 2 names

| Name | Is |
|---|---|
| `kdos-appbox` | Launching boxed applications, and generating their launchers |
| `kdos-box` | The box manager |

It is also invoked through a **shim named after each installed application**, so `gimp` on your
`PATH` is this binary dispatching on that name.

## See also

- [Command index](../06-reference/command-index.md) — every command alphabetically
- [Architecture overview](../03-architecture/overview.md) — how these fit together
- [Repository layout](../06-reference/repository-layout.md) — where each one's source lives
- [The C libraries](../05-developer/c-libraries.md) — what they are all built on
