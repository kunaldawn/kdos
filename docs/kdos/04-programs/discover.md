# Discover

Discover is Plasma's software centre, and on KDOS it is the one place where software is found,
installed, removed and updated: the native ports, the catalogue's boxed applications and packs,
the boxes already on the machine, firmware, and the operating system itself. KDOS builds Discover
with three backends and no others. Its own backend, `kdos-discover`, lists ports, packs and boxes.
Discover's own `sysupdate1` backend, with a patch that reads its error replies, shows system
updates, which `kdos-sysupdated` answers over KDOS's two root slots. The stock fwupd backend lists firmware. There is no Flatpak, Snap,
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
| `kdos-packs` | Every set in the catalogue, the packs in the pack store, and each catalogue application whose pack is there | KDOS |
| `medium` | Packs and `.ktar` sets on the boot medium, `/mnt/iso/packs`, and each catalogue application whose pack is only there | KDOS |
| `kdos-store` | Catalogue applications with no pack anywhere, and Debian desktop applications no catalogue row names; each is built into a box on request | KDOS |
| `kdos-boxes` | Boxes that none of the above accounts for | KDOS |
| `kdos-ports` | Host software: the ports installed, and those the release's signed binary host holds | KDOS |
| `kdos-ports-source` | Each port the binary host holds and the machine has not installed, built on this computer instead | KDOS |
| `local` | A `.ktar` or `.kpack` opened from a file manager | KDOS |
| the system | The next KDOS release, for the two root slots | sysupdate1 |
| firmware | Firmware updates for this machine's devices | fwupd |

The origin is also what an application's Install button names: "Install from KDOS packs", "Install
from KDOS medium" or "Install from Debian, built in a box".

The KDOS backend is Discover's default for an application install when more than one source could
take it (`[ResourcesModel] currentApplicationBackend=kdos-backend` in
`/usr/share/kdos/xdg/discoverrc`). The same file sets `[DiscoverUI] appsListPageSorting=256`, sort by
name, for a category: Discover's own default sorts by the number of reviews, and no KDOS source has
reviews. A search lists by relevance whatever that key says, because Discover sets that order for
every search and never stores it, and the Installed page sorts by `installedPageSorting`, which
Discover's own default leaves at name.

No source on KDOS has ratings or reviews, and the `discover` port's `ratings-need-reviews.patch`
shows an application's rating row and its Reviews section, with Write a Review, only for a
resource whose backend has a reviews backend. The same patch leaves the Home page's Highest-Rated
Games and Highest-Rated Developer Tools rows empty when the current application backend has none,
so on KDOS the Home page is Editor's Choice: the entries of `featured.json`, seven catalogue
applications (Scribus, Hugin, Xournal++, KStars, MeshLab, VSCodium and GNOME Chess). The build of the `discover` port fails unless
`/usr/lib/qt6/plugins/discover/` holds exactly the fwupd and sysupdate1 backends, so a stock
backend that appears or goes missing stops the build instead of changing what Discover offers.

## The KDOS backend

`kdos-discover` installs five things:

| File | Is |
|---|---|
| `/usr/lib/qt6/plugins/discover/kdos-backend.so` | The backend: the origins above |
| `/usr/lib/qt6/plugins/discover-notifier/KdosNotifier.so` | The tray notifier for system updates ([below](#the-update-notifier)) |
| `/usr/bin/kdos-discover-catalog` | Writes `/usr/share/swcatalog/xml/kdos-ports.xml`, the AppStream catalogue of the installed ports, from kpkg's manifests; and, at the image build, `kdos-debian-exclude.xml`, which takes the Debian applications `debian-exclude` names out of the AppStream pool |
| `/usr/libexec/kdos-discover/kdos-ports-helper` | The root step of installing host software prebuilt ([below](#host-software)) |
| `/usr/libexec/kdos-discover/kdos-ports-builder` | The root step of building host software on this computer and installing it |

It also points Discover's featured list at `/usr/share/kdos/discover/featured.json`, ships the
Debian lane's exclusion list `/usr/share/kdos/discover/debian-exclude` ([below](#debian-applications)), defines the
MIME types `application/vnd.kdos.ktar` and `application/vnd.kdos.kpack`, and installs a hidden
desktop entry that opens both in Discover.

### Where each answer comes from

The backend keeps no database of its own. Every answer comes from the tool that owns it, asked at
each refresh, and **Refresh** on Discover's Updates page is the same refresh:

| Question | Asked of |
|---|---|
| What the catalogue offers, and what is installed | `kdos-appbox catalogue` |
| What each box is made of | `kdos-box list --json`, a JSON array |
| What the pack store and the medium hold | the [kdos-packd](daemons.md#kdos-packd) socket |
| Which ports are installed | `kpkg list --json`, with `kdos-ports.xml` |
| Which ports the binary host offers | `/var/lib/kdos/binhost/swcatalog/xml/kdos-binhost.xml`, read again at every refresh |
| What the installer was asked for and could not install | `/var/lib/kdos/apps-pending` |
| What a box is allowed | `kdos-box profile <box>`, asked only when its page opens |
| A Debian application's description and categories | the DEP-11 catalogue, through libappstream |

An installed application's page carries a "Runs in a box" group for each resource that has one:
the box; what it is made from (a pack by its name, or "Debian packages, built on this computer");
whether it is running, stopped, or "Made at its first start" for a box whose container does not
exist yet; its colour, or "Neutral" for a box with no accent, whose title bar is the neutral one;
and whether it keeps its files. A link opens the box in System Settings'
[Boxes](kdos-settings.md#boxes) page. An application that is not installed shows no box group,
whatever box profile is left behind, and is not on the Boxes page. A store box is created with its
container at install, so one that `kdos-box list` reports `not created` over its own image,
`image:kdos/<name>`, is the profile an uninstall keeps: the backend counts it as no box, and the
Debian application it belonged to is offered for install again.

Each resource has an `appstreamId` of its own, because Discover's lists keep only the first of
the resources that share one: a host port has its component's id, and every other resource an id
made from its lane and its id under `io.kdos.`, which no link names. Its link stays
`kdos-backend://<lane>/<id>`, the form the featured list uses.

Beside the stock categories the backend adds Sets and Add-ons; Boxes, while at least one box
exists; and a "Selected during install" category that is there only while
`/var/lib/kdos/apps-pending` names something: the
applications and groups the installer was asked for and could neither import nor build. Each can
be installed from there; `kdos app install --pending` builds them all from a terminal and removes
the file after a run with no failure, and the category goes with it. `kdos-notice` points at the
category
([The Plasma shell](plasma-shell.md#kdos-notice)).

An `appstream://` link, which KRunner's software results and other programs hand to Discover,
opens the KDOS resource with that component id. For an application only Debian's catalogue knows,
it opens the catalogue row that packs its package when there is one, and otherwise the Debian
lane's resource. A link is answered once the backend has read the machine, and an `appstream://`
link once the AppStream pool has loaded as well, so Discover started on a link, or with its
featured list cached, opens the application rather than "No entry".

### Installing and removing

An install or a removal is a list of argument vectors and kdos-packd requests, run as one
transaction, one transaction at a time, with no shell. A catalogue row whose pack is already in the
store or on the medium becomes a box over that pack, offline. Only a row with no pack anywhere is
built by the store lane, which reaches the Debian archive, and the resource says so before it
starts. The size such a row shows is an estimate: the catalogue's size of the application, plus
the measured size of each base and runtime row up its chain whose image this machine has not built
yet, as `kdos-appbox catalogue --chain` reports them. A set counts each such row once, however many
of its applications sit on it. The notice says the size is an estimate that counts those rows.
A size nobody knows, such as an installed host port's, is not shown.

When an install or a removal fails, the application's page shows an error notice, "<Name> was not
installed: <reason>" or "<Name> was not removed: <reason>", which stays through the refresh after
it until the next install or removal of that resource starts. Discover's own dialog says only
"Please try again later", with the same reason behind See Technical Details. An install that needs the
network while the computer is offline gives the reason "it needs a network connection, and this
computer is offline".

Removing an application that runs in a box stops the box first and then removes it, because a box
stays up for a while after its application's last window closes and `kdos-box remove` refuses a
running one. How a box is built and launched is in [kdos-appbox](kdos-appbox.md), and the pack store in
[Packs and boxes](../03-architecture/packs-and-boxes.md).

A `.ktar` set or a `.kpack` opened from Dolphin, from the stick offer of
[kded-kdos](plasma-shell.md#kded-kdos) or from KRunner's medium rows opens in Discover through the
hidden desktop entry, and the backend imports it offline and verifies every pack. A tar whose
`SELECTION` names no application opens as a page that says "This file is not a KDOS set.", and a
file `kdos-pack extract-meta` cannot read as one that says "This file is not a KDOS pack, or it is
damaged."; neither installs anything.

### Debian applications

The `kdos-store` origin lists Debian desktop applications from the DEP-11 catalogue the
`debian-dep11` port installs at `/usr/share/swcatalog/yaml/debian-trixie-main.yml.gz`, a dated
snapshot of Debian trixie's `main`. The catalogue holds thousands of applications, so a resource is
made for one only when a search or a category asks for it, or when its box exists. Installing one
runs `kdos-appbox install deb:<package>`, which builds a box named `deb.<package>` (each `+` in the
package name spelled `_`); the backend reads that name back to tell the application is installed.
A Debian application whose component id or desktop-id launchable matches any component of the
installed ports' catalogue `kdos-ports.xml` or of the binary host's
`/var/lib/kdos/binhost/swcatalog/xml/kdos-binhost.xml`, compared without a trailing `.desktop`, is
never offered in a box, by a search, a category or an `appstream://` link, so with the binary-host
index fetched LibreOffice and Filelight are offered only natively; and
neither is one whose package `/usr/share/kdos/discover/debian-exclude` names: one package a line,
each a program that cannot do its job from a box on KWin's Wayland session, such as KMag and
KMouseTool, which work only on an X server's own screen, and firewall front ends such as Firewall
Configuration and Gufw, which configure a firewall daemon the host does not run (the host's packet
filter is set from the [Firewall](kdos-settings.md#firewall) page), or a program whose job is a
Debian system's: Synaptic and GNOME Software, which manage its packages, and Install Debian
(Calamares), Configure-Debian, GkDebconf, G Alternatives, Debian Reference and reportbug's window,
which install, reconfigure, document it and report bugs against it. A
box already made for one
stays listed, so it can be removed. The image build also writes
`/usr/share/swcatalog/xml/kdos-debian-exclude.xml`, a `remove-component` merge for each of them
whose id no host port has (a merge removes every component of its id, so Discover, GParted and
Disks, which the host ships, keep theirs), which takes them out of every AppStream pool: KRunner's software results read the pool directly,
and without it would offer, as "Get …", an application this lane refuses. Debian's icons and screenshots are not installed, so a Debian application
shows its stock icon when the host's icon theme has it, otherwise the icon of its category, and no
screenshot; KRunner's and Kickoff's "Get …" results, which take the pool's cached icon, show none.
A catalogue application with no AppStream icon shows its category's icon too: every catalogue
category is a main desktop-entry category (AudioVideo, Development, Education, Game, Graphics,
Network, Office, Science, Settings, System or Utility), and each has one.

### Host software

Host software arrives and leaves with the system version, so the `kdos-ports` origin lists what is
installed and offers no removal. Once `kdos update fetch` has put the release's signed binary-host
index and its catalogue in `/var/lib/kdos/binhost`, each taken where the release's signed
[release index](../03-architecture/packaging.md#the-release-index) says and kept only with the
hash it gives, a port that catalogue lists can be installed, by either of two origins:

| Origin | Shown as | Install runs, through `pkexec` | polkit action |
|---|---|---|---|
| `kdos-ports` | KDOS system | `kdos-ports-helper install <port>`, which runs `kdos update install --prebuilt <port>` | `io.kdos.ports.install`: an administrator's password, kept for a while |
| `kdos-ports-source` | KDOS, built on this computer | `kdos-ports-builder install <port>`, which runs `kdos update install --source <port>` | `io.kdos.ports.build`: an administrator's password, asked every time |

A port is one resource per package, listed by its best component: a desktop application before a
console application before any other kind, then one from the package's own metainfo (with a
description and a project licence) before one read from a bare desktop entry, then one with an
icon, then the shortest name, then the first in the catalogue. A suite is named by its face: when
the package's shortest-named application is a bare desktop entry whose name begins the chosen
component's, as LibreOffice's start centre "LibreOffice" begins "LibreOffice Base", the port takes
that entry's name, icon and launchable, the chosen metainfo's licence, developer, categories and
links, and a description of the suite: "The suite holds:" and each of the package's metainfo
applications in catalogue order by its name less the suite's and its summary ("Writer: …",
"Calc: …"), since any one application's description would describe that application under the
suite's name. The entry's own summary is not repeated in it: Discover shows the summary as the
heading above the description. LibreOffice is therefore listed as "LibreOffice", with The Document Foundation,
MPL-2.0 and a page that lists each application the package ships metainfo for (Writer, Calc,
Impress, Draw and Base), not as one of its
applications; any other bare entry leaves
the chosen component as it is, so a package's minor tool never names it. An `appstream://` link to another of
its components, such as LibreOffice Writer, opens the same port, and a search finds the port by the
name or a keyword of any of its package's components, so "writer" lists LibreOffice. The
catalogue writer, `kdos-discover-catalog`, takes a metainfo component with no desktop-id
launchable to launch `usr/share/applications/<id>` (or `<id>.desktop`) when its package owns that
entry, and writes it with that launchable and, where the metainfo names none, the entry's icon and
categories; the entry is not written again as a component of its own. The component's AppStream
kind sets where Discover lists the port: `addon` under Add-ons, `desktop-application` (or no kind)
under Applications, and a console application or any other kind under Application Support, as a
technical package. A desktop application with no category at all is filed under Other, so the All
Applications page, which matches by category, lists it and finds it in a search. A port whose icon
the theme does not have, as for one whose only copy of its icon is in its own uninstalled package,
shows `utilities-terminal` for a console application, `preferences-plugin` for an add-on, its first
category's icon for an application, and for any other kind its category's icon or, with no
category, one of its kind: `application-x-sharedlib` for a generic component such as libfprint or
pcscd, `preferences-system-services` for a service, `hwinfo` for a driver or firmware,
`preferences-desktop-font` for a font. The Graphics category's icon is `image-x-generic`, not
Breeze's `applications-graphics`, whose blue sphere reads as a missing icon in a list.

Each port the binary host holds and the machine has not installed is two resources with one
AppStream id, `kdos-backend://port/<id>` and `kdos-backend://port-source/<id>`, so Discover lists
one application and offers both origins in its source picker; an installed port is one resource,
"KDOS system". **The origin Discover opens follows `install =`** in `/etc/kdos/update.conf`, the
route System Settings › System Versions sets and `kdos update install` takes at a terminal: under
`install = source` the built-here resource, otherwise the prebuilt one. Discover keeps one resource
per AppStream id in a list, and which of two that sort equal survives is not a backend's to decide,
so a search or a category lists only the preferred one; the application's link answers both,
preferred first, and Discover opens the first resource a link answers and fills the source picker
from the same link. A list browsed by origin lists every resource of that origin. The file is read
at every search, so a change on the settings page is what the next list shows. The prebuilt route downloads the port and every package it lacks from the binary
host and checks each against the signed index. The built route fetches the port's sources and
builds it from its recipe, which can take minutes or hours, and each dependency the machine lacks
is downloaded prebuilt or built too as `build-deps =` in `/etc/kdos/update.conf` says; the builder
passes `--build-deps=` to `kdos update install` only when it is given one. Either route records
each package's route in `/var/lib/kdos/origin`, so the next update takes it the same way, and adds
the port to `/var/lib/kdos/world` so the next update keeps it; then the helper writes
`kdos-ports.xml` again. The routes are in
[The kdos command](kdos-command.md#install-routes-and-groups).

Both helpers check the port's name and replace the environment with a fixed `PATH` and the C
locale before they run anything. They are programs of their own because pkexec's grant names a
program and never its arguments, and granting `/usr/bin/kdos` would grant every subcommand; they
are two programs, one source compiled twice, because pkexec picks the action by the program's path,
and a password kept from a prebuilt install must not cover a build that runs a recipe's `build.sh`
as root. The image's own copy of `kdos-ports.xml` is written by the build, over the finished tree.
Both routes download, and each resource says so.

**Discover says what an install will do before it asks for the password.** Install first runs
`kdos update install --dry-run --prebuilt <port>` (or `--source`) as the user, which needs no
root, and its `total` line becomes the question of Discover's confirmation dialog, "Install
<name>?": "Downloads N packages (size)." for the prebuilt route, the number and unit of the size
never split across lines, and for the built one "Builds N
packages here, downloads M (size); building can take hours.", with the packages built here named.
Proceed runs the helper through `pkexec`, which asks for the password; Cancel, or closing the
dialog, ends the install with nothing run. A dry run that refuses, such as a prebuilt install the
binary host does not cover, ends it there with the dry run's reason, before any password. The
reader is the model library's `parseInstallPreview`, tested over fixture outputs.

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
| KNewStuff add-ons from `store.kde.org` | The KNewStuff backend is not built, and `ghns=false` in `kdeglobals` turns off every Get New download: the settings pages show no Get New button, the wallpaper dialog's Get New Plugins is greyed, and Add Widgets' Get New menu holds only Install Widget From Local File |

What does reach the network is what a person starts: a store-lane box, a host port from the binary
host or the sources of one built here, a firmware install, a system update, and a system update check made with
**Refresh** on the Updates page ([below](#checking-for-an-update)). [Principles](../01-philosophy/principles.md#offline-by-construction)
states the rule.

## System updates

An operating-system update on KDOS is a whole release written into the root slot that is not
running and booted on trial; the running system is never changed. Discover shows it through its
SystemdSysupdate backend, built with `-DBUILD_SystemdSysupdateBackend=ON` and carrying
`sysupdate-errors.patch`, which talks to `org.freedesktop.sysupdate1` on the system bus. With the
patch an error reply to `GetVersion` or `CheckNew`, such as a daemon that cannot be reached, is
shown as a message and lists no update, where upstream would list an update to a version named after the
error. `kdos-sysupdated` owns that name. The update
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
serves `io.kdos.Slots1`, the slots, the armed trial, the update channel, the install route, the job
in progress, `Try`, `CancelTry`, `SetChannel`, `SetInstallRoute` and `Bless`, for the
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

Before its first write into the slot, every lane runs `kdos-bootctl unfill <slot>`, and only the
arm's `deploy` clears that mark. A slot an acquire stopped writing, cancelled or failed, is
therefore incomplete in the boot state: `kdos-bootctl try` refuses it ("slot <x> is incomplete — an
update stopped while writing it; `kdos update apply` writes it again"), and the boot menu has no
entry for it until an update writes it again. `kdos-sysupdated`'s `ListSlots` reports it as
`incomplete`, and [System Versions](kdos-settings.md#system-versions) shows it as incomplete with
no Start This Version Next Time button.

Discover shows progress only for `Acquire`, so all the work is there: the download, writing or
installing into the inactive slot, and checking it. `Install` is the quick part: it carries this
machine's configuration into the slot, records its version, puts its kernel on the ESP and runs
`kdos-bootctl try`.

`Acquire` is answered only once `kdos update` has said `verified` or `refused`. Discover shows a
failed job as nothing more than "failed", so an index or image manifest no trusted key signed, a
package the binary host does not cover, or a slot that cannot be mounted reaches the person as the
call's error, with its reason under the details of Discover's "Update Issue" sheet, and only an
update that can go ahead becomes a job. A refused or failed update leaves nothing pending: the
patch offers the restart only after `Install` ends with status 0, and the update stays listed on
the Updates page, ready to try again (`update-failed-stays-listed.patch`). While the
running boot is itself a trial of the other slot, nothing is offered: the slot an update would fill
is the confirmed way back, so `CheckNew` answers empty and `Acquire` is refused until the trial is
confirmed or rolled back.

Checking for an update needs no password in the active session; acquiring and installing one asks
for an administrator's password, kept for a few minutes. Every polkit action, the daemon's options
and exit statuses, and how it is started are in [The daemons](daemons.md#kdos-sysupdated).

### Checking for an update

The nightly timer runs `kdos update check --out /var/lib/kdos/update.json`, which reads only the
recipe tree, binary-host index and image manifest already on the machine. `CheckNew`, which
Discover calls when a person presses **Refresh** on its Updates page, fetches the signed tree and the release
index, and through that index the binary-host index and image manifest, at most once an hour, then
answers from the document. A document belongs to the
root that wrote it, because `/var/lib/kdos` is in each slot: one whose `installed` version is not
the running one offers nothing, and the daemon rewrites it at start without downloading.

On a machine with no second root slot, the live medium or an install with one root, `CheckNew`
runs `kdos update fetch --binhost-only` instead, at most once per `--refresh-after` (an hour),
which fetches only the release's signed binary-host index into `/var/lib/kdos/binhost`, and answers
empty when it ends: there is no slot to install a system update into, so Discover reports the
system up to date. The index is what the ports and the native groups are installed from, so
Refresh is also how a live session or a one-root machine gets them without a terminal. The hour is
counted from the `binhost` line of `/var/lib/kdos/update.state`, and a fetch that ended
`no-network` or `fetch-failed` does not count, so Refresh after connecting asks again at once. The
KDOS backend reads the fetched catalogue at its next refresh; one that started before the fetch
ended reads the catalogue from before it, so the new ports are listed after a second Refresh.
`Acquire` and `Install` there are refused with the live-medium or no-slot reason, even while that
fetch runs, never with "try again". An installed machine with one root is updated with `kdos
update apply` at a shell. The live medium is not updated at all; the system entry Discover shows
describes it as the live session, and installing KDOS gives a system that is.

What `kdos update check` offers is exactly what `apply` will take: a newer signed slot image when
the release publishes one, or else the release's packages, only when the signed binary host holds
every one of them that is not built here, matched to this machine's build; a package whose origin is
`source` is built again in the slot and counts as covered. A slot image carries the base alone, so
the packages installed on purpose follow it into the slot from the same release's binary host, and
an image is refused before the slot is written when that binary host does not cover one of them
that came prebuilt. Either is offered only beside the cached
release index that names it, which is where `apply` downloads every file from. A machine with a package the binary host does
not match is told so before any slot is touched.

### What the update page says

The AppStream file on `/run` is regenerated whenever `update.json` changes; the copy in
`/usr/share/kdos/update/` is the fallback, which names the system and offers nothing. The
`discover` port's `sysupdate-appstream-after-check.patch` makes the backend ask `GetVersion` and
`CheckNew` before it fetches a target's AppStream, and fetch it only for a target that offers a
version, so the file it reads is the one the check behind that offer wrote, and the first session
that finds an update already shows its size, the signed text and the release notes. A target that
offers nothing, or whose `GetVersion` or `CheckNew` failed, has its AppStream left unread. The
text, which Discover shows above the release notes, begins "Signed by <key> (ed25519 …), verified on
this machine", then names whether the update is the signed system image or the release's packages,
the packages it changes and its size, "to copy" when that lane's origin is a directory or a
`file://` URL, such as a stick, and "to download" otherwise. The origin is the update document's
`image` for the image and its `binhost` for packages, set by the `image =` and `binhost =` lines of
`/etc/kdos/update.conf`, else the release's https defaults; the tree's channel does not decide it.
The release notes after it are the new tree's `RELEASE`, turned from its Markdown into AppStream's
description markup: a `#` heading is a paragraph of its own, a run of `- ` items a bulleted list,
and backticks are dropped, so no Markdown mark reaches the page. The component carries
`<project_license>MIT</project_license>` beside its CC0-1.0 metadata licence, which More Info's
Licenses field shows as MIT through the `discover` port's `sysupdate-licenses.patch`; upstream's
sysupdate resource hands the page bare strings it shows as nothing. The offered release carries the same size as the download size of an
artifact, which the patched backend reads, so the Updates list and its total show it; only an
update document with no size leaves the field out of the page, since `127.0.0.1:3129`, the size
service Discover asks then, has nothing listening.

When the update document records a problem, the KDOS backend puts a warning above the Updates
page's list that names where updates come from (the document's `channel`) and adds what failed (its
`detail`), such as "Updates could not be fetched from file:///mnt/bh.
(file:///mnt/bh/kdos-ports.tar.xz could not be downloaded)". Each code `kdos update check` reports
has its sentence: the fetch's `no-network`, `fetch-failed` and `no-key`; the binary-host index's
and the slot image's `binhost-` and `image-` codes (`no-network`, `fetch-failed`, `no-key`,
`unverified`), each naming the package index or the system image; and the last apply's
`unverified`, `build-failed` and `needs-source`, said as an update that installed nothing. A bare
`no-network` or `fetch-failed` may be the fetch's or the apply's, so its sentence fits either.
`no-tree` is not said: with no tree nothing can be behind. Nor is the fetch's `fetch-failed` whose
detail is "the published tree … is older than this machine's": that machine is ahead of the
release, not cut off from it, and an image the document offers is listed with no warning beside it.
The backend knows the refusal by those words, as `kdos update`'s own binary-host fallback does. Without the warning the page would say
"Up to date" about a release nothing could read. The next check with no problem takes it away; the
backend reads the document at each refresh, and a change to `/var/lib/kdos` starts one.

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
| A restart is needed | A staged update: `State`'s `rebootRequired`, a slot armed and not yet booted, holding a version newer than the running one (`State`'s `version` while its `phase` is `armed`, against the running slot's version from `ListSlots`), followed both ways; `kdos-bootctl status --json` and its `slots` when the daemon does not answer |

It never calls `CheckNew`, which may fetch, so the tray never reaches the network. `State` reports
a job only for an update the daemon runs, one a sysupdate1 client asked for; `kdos update apply`
at a shell runs in place and reports none. A job that begins while `org.kde.discover` is not on the
session bus, such as one started by unattended updates or one Discover was closed on before it
began, gets a job of the notifier's in the notification area ("Preparing the system update", N of
M), since no Discover window reports it. One that begins while Discover is running is taken for
Discover's own, reported by Discover's transaction, so the notifier posts none. The answer is taken
when the job begins and kept for its length, and one update leaves one entry in the notification
history. `State` does not say which client started a job, so one another sysupdate1 client starts
while a Discover window stands idle has no progress entry. A boot on trial is not the
tray's to say: `kdos-notice` posts that notice ([The Plasma shell](plasma-shell.md#kdos-notice)).

A slot armed at the running version or an older one, as "Start This Version Next Time" on the
System Versions page arms it, is a switch between versions and no pending restart here: Discover
would answer it with "The system needs to be restarted for the updates to take effect" and "Update
and Restart", for an update there is none of. System Versions and `kdos-notice`'s trial notice speak
for that switch.

When an update arms a slot, Discover's notifier posts the persistent "Restart is
required" notification, the `UpdateRestart` event of the `discoverabstractnotifier` component,
once. It is shown, and sent once, only through the `discover` port's
`notifyrc-update-restart.patch`: upstream builds it with `KNotification::event()`, which queues a
send of its own before Discover sends it again, posts it as a default event, which KNotification
looks up in `plasma_workspace`, whose notifyrc has no such event, and its own notifyrc misspells the
event's group; KNotification shows nothing for an event its component does not declare.
The notifier says when the staged update comes or goes, and the `discover` port's
`notifier-reboot-clears.patch` makes Discover follow it both ways. An open Discover window's
sysupdate1 backend reads `io.kdos.Slots1`'s `State` when it starts and follows it both ways after.
An armed slot newer than the version the target's `GetVersion` answers, less the `(slot A)` that
answer carries, puts the restart prompt on its Updates page, so an update applied from a terminal
(`kdos update apply --mount`) is shown waiting for its restart rather than "Up to date". A disarmed
slot takes it away, so a trial cancelled from System Versions ("Keep KDOS <version> (slot A)")
withdraws the restart within the session: the tray, asking every backend notifier again, drops its
restart title, tooltip and icon and its "Install Updates and Restart…" and "Install Updates and
Shut Down…" entries, and closes the restart notification; the open window's backend drops "Restart
and Install Updates" from its Updates page and lists the targets again, so the update the disarmed
slot held is offered there once more. With
that update still offered, the tray shows "Updates available" again at once rather than after
`RequiredNotificationInterval`, since it showed the update before the install. Arming again posts
one fresh notification, never a second copy.

`/usr/share/kdos/xdg/PlasmaDiscoverUpdates` sets `UseUnattendedUpdates=false` and
`RequiredNotificationInterval=86400`: an update is never installed unasked, and the "updates
available" notification comes at most once a day. On the live medium the interval is `-1`, and
Discover's notifier then shows neither the notification nor the tray icon for an available update,
which could not be kept there; the icon still appears while an update job runs or a restart is
pending.

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
