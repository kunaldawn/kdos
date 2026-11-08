# Discover

Discover is Plasma's software centre, and on KDOS it is the one place where software is found,
installed, removed and updated: the native ports, the catalogue's boxed applications and packs,
the boxes already on the machine, firmware, and the operating system itself. KDOS builds Discover
with three backends and no others. Its own backend, `kdos-discover`, lists ports, packs and boxes.
Discover's stock `sysupdate1` backend shows system updates, which `kdos-sysupdated` answers over
KDOS's two root slots. The stock fwupd backend lists firmware. There is no Flatpak, Snap,
PackageKit or KNewStuff backend, and no online store to sign in to.

This chapter describes what each backend shows and where its answers come from, how a system
update goes from Discover's Update button to a confirmed boot of the new version, and what keeps
Discover from reaching the network on its own. It is written for anyone who wants to know what
Discover is doing on a KDOS machine, who administers updates, or who is changing the backend or
the daemon. Installing and using applications day to day is in
[Applications](../02-user-guide/applications.md), and why Discover with KDOS backends was chosen
over a software centre of KDOS's own is in
[Decisions](../01-philosophy/decisions.md#discover-with-kdos-backends-as-the-one-software-centre).

## What Discover shows

Every resource in Discover carries an origin, which names the source it comes from:

| Origin | What it lists | Backend |
|---|---|---|
| `kdos-packs` | Every application and set in the catalogue, and the packs in the pack store | KDOS |
| `medium` | Packs and `.ktar` sets on the boot medium, `/mnt/iso/packs` | KDOS |
| `kdos-store` | Debian desktop applications no catalogue row names, built into a box on request | KDOS |
| `kdos-boxes` | Boxes that none of the above accounts for | KDOS |
| `kdos-ports` | Host software: the ports installed, and those the release's signed binary host holds | KDOS |
| `local` | A `.ktar` or `.kpack` opened from a file manager | KDOS |
| the system | The next KDOS release, for the two root slots | sysupdate1 |
| firmware | Firmware updates for this machine's devices | fwupd |

The KDOS backend is Discover's default for an application install when more than one source could
take it (`[ResourcesModel] currentApplicationBackend=kdos-backend` in
`/usr/share/kdos/xdg/discoverrc`). The build of the `discover` port fails unless
`/usr/lib/qt6/plugins/discover/` holds exactly the fwupd and sysupdate1 backends, so a stock
backend that appears or goes missing stops the build instead of changing what Discover offers.

## The KDOS backend

`kdos-discover` installs four things:

| File | Is |
|---|---|
| `/usr/lib/qt6/plugins/discover/kdos-backend.so` | The backend: the origins above |
| `/usr/lib/qt6/plugins/discover-notifier/KdosNotifier.so` | The tray notifier for system updates ([below](#the-update-notifier)) |
| `/usr/bin/kdos-discover-catalog` | Writes `/usr/share/swcatalog/xml/kdos-ports.xml`, the AppStream catalogue of the installed ports, from kpkg's manifests |
| `/usr/libexec/kdos-discover/kdos-ports-helper` | The root step of installing host software ([below](#host-software)) |

It also points Discover's featured list at `/usr/share/kdos/discover/featured.json`, defines the
MIME types `application/vnd.kdos.ktar` and `application/vnd.kdos.kpack`, and installs a hidden
desktop entry that opens both in Discover.

### Where each answer comes from

The backend keeps no database of its own. Every answer comes from the tool that owns it, asked at
each refresh, and Discover's Check for Updates button is the same refresh:

| Question | Asked of |
|---|---|
| What the catalogue offers, and what is installed | `kdos-appbox catalogue` |
| What each box is made of | `kdos-box list --json`, a JSON array |
| What the pack store and the medium hold | the [kdos-packd](daemons.md#kdos-packd) socket |
| Which ports are installed | `kpkg list --json`, with `kdos-ports.xml` |
| Which ports the binary host offers | `/var/lib/kdos/binhost/swcatalog/xml/kdos-binhost.xml` |
| What the installer was asked for and could not install | `/var/lib/kdos/apps-pending` |
| What a box is allowed | `kdos-box profile <box>`, asked only when its page opens |
| A Debian application's description and categories | the DEP-11 catalogue, through libappstream |

An application's page carries a "Runs in a box" group for each resource that has one: the box,
what it is made from, whether it is running, its colour and whether it keeps its files, with a
link that opens the box in System Settings' [Boxes](kdos-settings.md#boxes) page.

Beside the stock categories the backend adds Sets, Boxes and Add-ons, and a "Selected during
install" category that is there only while `/var/lib/kdos/apps-pending` names something: the
applications and groups the installer was asked for and could neither import nor build. Each can
be installed from there; `kdos app install --pending` builds them all from a terminal and removes
the file after a run with no failure, and the category goes with it. `kdos-notice` points at the
category
([The Plasma shell](plasma-shell.md#kdos-notice)).

An `appstream://` link, which KRunner's software results and other programs hand to Discover,
opens the KDOS resource with that component id. For an application only Debian's catalogue knows,
it opens the catalogue row that packs its package when there is one, and otherwise the Debian
lane's resource.

### Installing and removing

An install or a removal is a list of argument vectors and kdos-packd requests, run as one
transaction, one transaction at a time, with no shell. A catalogue row whose pack is already in the
store or on the medium becomes a box over that pack, offline. Only a row with no pack anywhere is
built by the store lane, which reaches the Debian archive, and the resource says so before it
starts. How a box is built and launched is in [kdos-appbox](kdos-appbox.md), and the pack store in
[Packs and boxes](../03-architecture/packs-and-boxes.md).

A `.ktar` set or a `.kpack` opened from Dolphin, from the stick offer of
[kded-kdos](plasma-shell.md#kded-kdos) or from KRunner's medium rows opens in Discover through the
hidden desktop entry, and the backend imports it offline and verifies every pack.

### Debian applications

The `kdos-store` origin lists Debian desktop applications from the DEP-11 catalogue the
`debian-dep11` port installs at `/usr/share/swcatalog/yaml/debian-trixie-main.yml.gz`, a dated
snapshot of Debian trixie's `main`. The catalogue holds thousands of applications, so a resource is
made for one only when a search or a category asks for it, or when its box exists. Installing one
runs `kdos-appbox install deb:<package>`, which builds a box named `deb.<package>` (each `+` in the
package name spelled `_`); the backend reads that name back to tell the application is installed.
A Debian application whose component id a host port already has is never offered in a box. Its
icon and screenshots are not installed, so it shows a stock icon and no screenshot.

### Host software

Host software arrives and leaves with the system version, so the `kdos-ports` origin lists what is
installed and offers no removal. Once `kdos update fetch` has put the release's signed binary-host
index and its catalogue in `/var/lib/kdos/binhost`, each taken where the release's signed
[release index](../03-architecture/packaging.md#the-release-index) says and kept only with the
hash it gives, a port that catalogue lists can be installed.
Install runs `/usr/libexec/kdos-discover/kdos-ports-helper` through `pkexec` under the polkit action
`io.kdos.ports.install` (an administrator's password, kept for a while), which checks the port's
name, replaces the environment with a fixed `PATH` and the C locale, and runs `kdos update install
<port>`. That command downloads the port and every package it lacks from the binary host, checks
each against the signed index, installs them, and adds the port to `/var/lib/kdos/world` so the
next update keeps it; then the helper writes `kdos-ports.xml` again. The image's own copy is
written by the build, over the finished tree. The helper is a program of its
own because pkexec's grant names a program and never its arguments, and granting `/usr/bin/kdos`
would grant every subcommand. This is the one KDOS lane that downloads, and the resource says so.

The backend reads `kdos-ports.xml` and `kdos-binhost.xml` itself at every refresh rather than
through the AppStream pool, because the pool keys every component of the system catalogue by an
id with the origin `os`, so `org.kde.kate` from the ports and from Debian's catalogue would be one
component, whichever loaded last.

### Locked to one Discover release

libdiscover is private: Discover installs no header and promises no binary interface, and both
plugin interfaces carry Discover's full version in their identifier
(`org.kde.discover.6.8.2.AbstractResourcesBackendFactory`). Discover skips a plugin whose
identifier differs with nothing but a debug line, so a stale build vanishes from Discover without
an error. The `discover` port copies libdiscover's headers to `/usr/include/discover`, and
`kdos-discover` refuses to configure against any Discover but the version it names. Every Discover
release, point releases included, therefore rebuilds the backend, whose tests are run again against
the new headers.

## Nothing downloads on its own

Opening Discover reaches no server. Each of Discover's own paths to the network is closed:

| Path | Closed by |
|---|---|
| The Home page's featured list, from `autoconfig.kde.org` | `/usr/share/discover/featuredurlrc` points at `/usr/share/kdos/discover/featured.json` on the machine |
| Application ratings from `odrs.gnome.org`, fetched at every start whose cache is a day old | `odrs-offline.patch` adds `[Software] FetchOdrsRatings`, which `discoverrc` sets to `false`; the Most Popular list is not shown |
| Firmware metadata, refreshed for every enabled download remote at each start | fwupd's LVFS remote is installed switched off ([Firmware](#firmware)) |
| Screenshots | Only a screenshot that is a file on the machine is shown |
| KNewStuff add-ons from `store.kde.org` | The KNewStuff backend is not built, and `ghns=false` in `kdeglobals` greys out every Get New button |

What does reach the network is what a person starts: a store-lane box, a host port from the binary
host, a firmware install, a system update, and a system update check made with Check for Updates
([below](#checking-for-an-update)). [Principles](../01-philosophy/principles.md#offline-by-construction)
states the rule.

## System updates

An operating-system update on KDOS is a whole release written into the root slot that is not
running and booted on trial; the running system is never changed. Discover shows it through its
stock SystemdSysupdate backend, built with `-DBUILD_SystemdSysupdateBackend=ON`, which talks to
`org.freedesktop.sysupdate1` on the system bus. `kdos-sysupdated` owns that name. The update
itself, and the three ways a slot can be filled, are described in
[Packaging](../03-architecture/packaging.md#updating-a-machine); this section is how Discover drives
it.

### kdos-sysupdated

`kdos-sysupdated` is a root daemon, KDOS's own C on basu, at `/usr/sbin/kdos-sysupdated`. It serves
the part of systemd's `sysupdate1` interface that Discover 6.8.2 calls:

| Object | Members |
|---|---|
| Manager | `ListTargets` (one target, `host`), `ListJobs`, `ListAppStream`, the signal `JobRemoved` |
| Target | `GetVersion`, `GetAppStream`, `CheckNew`, `List`, `Describe`, `Vacuum`, `Acquire`, `Install` |
| Job | `Id`, `Type`, `Offline`, `Progress`, `Cancel` |

Discover decides the backend is usable by pinging the name, so it needs no patch. The daemon also
serves `io.kdos.Slots1`, the slots, the armed trial, the update channel, the job in progress,
`Try`, `CancelTry`, `SetChannel` and `Bless`, for the
[System Versions](kdos-settings.md#system-versions) page and the notifier. It does no update work
itself: every check, download and install is a `kdos update` run, and every boot decision a
`kdos-bootctl` run, each started with an argument vector and read through a progress pipe while the
bus keeps answering. `kdos update` and `kdos-bootctl` are in
[The kdos command](kdos-command.md#kdos-update).

| Discover does | The daemon runs |
|---|---|
| Opens, and lists the system | `GetVersion`, the running version and its slot, such as `0.2.14 (slot A)`; the AppStream file `/run/kdos/sysupdate/io.kdos.system.metainfo.xml` |
| Checks for an update | `CheckNew`, which answers from `/var/lib/kdos/update.json`, the document `kdos update check` writes |
| Update, the long part, with progress | `Acquire`: `kdos update apply --acquire --mount --progress 3` |
| Update, the short part | `Install`: `kdos update apply --arm --mount --progress 3`, then a restart is offered |
| Cancel | `Job.Cancel` on an acquire, which stops its child before the slot is armed; an install is the short arm of a slot already filled and is not cancelled |

Discover shows progress only for `Acquire`, so all the work is there: the download, writing or
installing into the inactive slot, and checking it. `Install` is the quick part: it carries this
machine's configuration into the slot, records its version, puts its kernel on the ESP and runs
`kdos-bootctl try`.

`Acquire` is answered only once `kdos update` has said `verified` or `refused`. Discover shows a
failed job as nothing more than "failed", so an index or image manifest no trusted key signed, a
package the binary host does not cover, or a slot that cannot be mounted reaches the person as the
call's error, with its reason, and only an update that can go ahead becomes a job. While the
running boot is itself a trial of the other slot, nothing is offered: the slot an update would fill
is the confirmed way back, so `CheckNew` answers empty and `Acquire` is refused until the trial is
confirmed or rolled back.

Checking for an update needs no password in the active session; acquiring and installing one asks
for an administrator's password, kept for a few minutes. Every polkit action, the daemon's options
and exit statuses, and how it is started are in [The daemons](daemons.md#kdos-sysupdated).

### Checking for an update

The nightly timer runs `kdos update check --out /var/lib/kdos/update.json`, which reads only the
recipe tree, binary-host index and image manifest already on the machine. `CheckNew`, which
Discover calls when a person presses Check for Updates, fetches the signed tree and the release
index, and through that index the binary-host index and image manifest, at most once an hour, then
answers from the document. A document belongs to the
root that wrote it, because `/var/lib/kdos` is in each slot: one whose `installed` version is not
the running one offers nothing, and the daemon rewrites it at start without downloading.

What `kdos update check` offers is exactly what `apply` will take: a newer signed slot image when
the release publishes one, or else the release's packages, only when the signed binary host holds
every one of them, matched to this machine's build. Either is offered only beside the cached
release index that names it, which is where `apply` downloads every file from. A machine with a package the binary host does
not match is told so before any slot is touched.

### What the update page says

The AppStream file on `/run` is regenerated whenever `update.json` changes; the copy in
`/usr/share/kdos/update/` is the fallback, which names the system and offers nothing. Its text,
which Discover shows above the release notes, names whether the update is the signed system image
or the release's packages, the fingerprint of the key that signed it, the packages it changes and
the size still to download. Discover asks `127.0.0.1:3129` for an update's size, and nothing
listens there, so Discover's own size field shows 0; the size in the text is the one to read.

### Restart, trial and confirmation

After `Install` the other slot is armed for one trial boot. A new version is kept only when that
boot is confirmed by two marks under `/run/kdos/bless/`, both regular files owned by root:

| Mark | Written when |
|---|---|
| `userspace` | The end of `rcS`: every service started |
| `graphical` | A Plasma session answered on the bus for a minute (`kdos-notice --bless` calls `io.kdos.Slots1.Bless`), or the login screen stayed up for a minute on a machine nobody logs in to |

When both exist, `kdos-sysupdated` runs `kdos-bootctl mark-good` once and writes
`/run/kdos/bless/confirmed`. Either mark alone confirms nothing: a system that boots with a desktop
that crashes is the failure a trial exists to catch. polkit grants `io.kdos.slots.bless` to the
active local session alone, with no password. The login screen counts only where nobody has logged
in this boot and SDDM logs nobody in automatically, so a login screen that came back because a
session crashed never confirms the slot that crashed it. A machine whose `inittab` starts no
display manager needs only the first mark. A trial that never gets confirmed uses up its attempts,
and the next boot falls back to the slot that worked; `kdos-notice` then says "Your last update
didn't start". How the slots are chosen at boot is in
[Boot and init](../03-architecture/boot-and-init.md#ab-slot-selection).

## The update notifier

Discover's tray notifier loads only `discover-notifier` plugins, and its sysupdate1 backend ships
none, so `KdosNotifier.so` is what puts a system update in the tray. It has three answers:

| Answer | Read from |
|---|---|
| An update is available | `io.kdos.Slots1`'s `State` field `offer`; `update.json`'s `offer` when the daemon does not answer |
| It is a security update | The offer closes at least one finding of `kdos cve --json`: the finding's port is among the update's packages, at or past its fixed version |
| A restart is needed | `State`'s `rebootRequired`, a slot armed and not yet booted; `kdos-bootctl status --json` when the daemon does not answer |

It never calls `CheckNew`, which may fetch, so the tray never reaches the network. While an update
is being acquired the notifier keeps a job in the notification area ("Preparing the system update",
N of M), because Discover does not reattach to a job it did not start. A boot on trial is not the
tray's to say: `kdos-notice` posts that notice ([The Plasma shell](plasma-shell.md#kdos-notice)).

`/usr/share/kdos/xdg/PlasmaDiscoverUpdates` sets `UseUnattendedUpdates=false` and
`RequiredNotificationInterval=86400`: an update is never installed unasked, and the "updates
available" notification comes at most once a day. On the live medium the interval is `-1`, so the
notification never comes; an update there could not be kept.

## Firmware

Firmware comes from fwupd through Discover's stock fwupd backend. fwupd's LVFS remote is installed
switched off, and firmware is listed from the `kdos-snapshot` local remote: the LVFS metadata as it
stood when the release was cut, its signature checked when the release was made, installed by the
`fwupd-snapshot` port under `/usr/share/fwupd/remotes.d/kdos-snapshot/`. So Discover's Updates page
and `fwupdmgr get-updates` list the firmware that applies to this machine with no network, and
installing an update fetches its cabinet file from the LVFS, which is a download the person
started. The list is as current as the release. `fwupdmgr enable-remote lvfs` turns the live LVFS
on, after which it outranks the snapshot and Discover refreshes its metadata at every start.

## Updating from a terminal

Discover is one face of `kdos update`; the command is the other, and it does everything Discover
does and two things more. `kdos update apply` with no flag acquires and arms in one step, preferring
a slot image and falling back to the binary host; `--image` and `--binhost` choose a lane by name;
and `--source` builds the release's signed recipe tree on the machine instead, which needs no
published binary at all and takes as long as the build. Every lane ends in the same armed trial and
the same confirmation. See [The kdos command](kdos-command.md#kdos-update).

## Testing it

`kdos-sysupdated --session --root <dir>` serves both names on the session bus with every file it
reads and writes under a fixture root, and Discover started with `DISCOVER_TEST_SYSUPDATE=1` looks
for `sysupdate1` on the session bus, so the whole flow can be driven without root.
`testing/selftest.sh` runs the daemon that way over fixture slots and compares its introspection
byte for byte with the `org.freedesktop.sysupdate1.xml` vendored from Discover 6.8.2 and with the
`io.kdos.Slots1` contract; the sysupdate1 interface is not stable upstream, so every Discover
release replaces the vendored copy. The backend's and the notifier's tests read fixtures of every
tool's real output and run under `testing/plasmatest.sh` in the development container.

## See also

- [Applications](../02-user-guide/applications.md): finding, installing and removing software
- [Administration](../02-user-guide/administration.md): updates, firmware and the update timer
- [Packaging](../03-architecture/packaging.md): the binary host, slot images and updating a
  machine
- [Packs and boxes](../03-architecture/packs-and-boxes.md): what the KDOS backend installs
- [KDOS settings pages](kdos-settings.md): System Versions and Boxes
- [The Plasma shell](plasma-shell.md): the tray, the notices and the confirmed boot
- [kdos-appbox](kdos-appbox.md): the box a catalogue or Debian application runs in
- [The kdos command](kdos-command.md): `kdos update` and `kdos-bootctl`
- [Decisions](../01-philosophy/decisions.md#updates-are-signed-slot-images-with-a-binhost-on-the-way):
  why updates are slot images

<!-- book-nav -->
---

*Part IV — Programs, chapter 24.* Previous: [23. KDOS settings pages](kdos-settings.md) · [Contents](../README.md) · Next: [25. kdos-appbox](kdos-appbox.md)
