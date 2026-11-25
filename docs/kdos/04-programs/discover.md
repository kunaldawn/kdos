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
from KDOS medium" or "Install from Debian, built in a box". A `kdos-store` application the catalogue
builds from its publisher's own `.deb` (a `deb` row; VSCodium is the one shipped) keeps that origin
for the filter and the sidebar, and its button names the row's publisher, the `about` row's author
or else the application's name: "Install from VSCodium's own package, built in a box". No Debian
component describes such an application, so its page takes its licence, author, homepage and
description from the catalogue's `about` row for it.

The KDOS backend is Discover's default for an application install when more than one source could
take it (`[ResourcesModel] currentApplicationBackend=kdos-backend` in
`/usr/share/kdos/xdg/discoverrc`). The same file sets `[DiscoverUI] appsListPageSorting=256`, sort by
name, for a category: Discover's own default sorts by the number of reviews, and no KDOS source has
reviews. A search lists by relevance whatever that key says, because Discover sets that order for
every search and never stores it, and the Installed page sorts by `installedPageSorting`, which
Discover's own default leaves at name.

The KDOS backend ranks each result, and a result takes the best rank it earns. Discover sorts a
search by its own relevance, which adds a term for how close the name is to the search and one for
a name that holds it; the backend scales its rank so that the rank decides the order, and
Discover's name terms, then the name itself, only order the results of one rank. Within one rank
the binary host's ports and packs list before a Debian build: the backend adds half a rank step to
every result but a Debian package's, more than Discover's name terms and less than a step. Highest first:
the name is the whole term; for a term that names no category, a keyword or a component name of
the package is the whole term; the application's category is the one the term stands for
(`browser` and `web` for WebBrowser, `mail` and `email` for Email, `office` for Office, `video`
for Video, and a few more in `searchQuery`), raised when its summary says a word for that category
and again when its name holds the term; for a term that names a category, a keyword that is the
whole term on a program outside the category; the name starts with the term; the name holds it as
a whole word; the name holds it at all; the package name; a keyword holds it; the summary holds
it. A search for "browser" therefore lists the web browsers before AT-SPI Browser, "chrome" lists
Chromium, whose keyword it is, before mkchromecast, "mail" lists the mail clients (KMail,
Thunderbird, Claws Mail) ahead of the tools that only file themselves under Email, such as Account
Wizard and KTnef, and "video" lists the video players and editors ahead of Qt V4L2 test Utility,
which carries the keyword but files itself under AudioVideo alone. A catalogue row or a pack names
one main category, Network rather than WebBrowser, so for one of those a summary that says a word
for the category counts as the category, raise and all: Surf's "Simple web browser" lists it with
the web browsers. An application filed under AudioVideo with a narrower category the term does not
name (AudioVideoEditing, Player, Recorder) counts as a Video or Music application, raise and all,
when the head of its summary, the words before its first "for", "with" or "to", says the word:
Kdenlive's "Video editor" and Dragon Player's "Play video files" list them with the video
players. Any other application that does not file itself there takes the category's base rank
when the head of its summary says a word for what the category's programs handle (mail, chat,
music, games): level with the members that do not say it, below the ones that both file themselves
there and say it, and above every name, keyword or summary match. Kontact, filed under Office
alone, "Handle all your emails", lists with the mail applications, ahead of KMail Import Wizard
(its name) and gLabels (its "mail merge" keyword); SieveEditor, "Editor for email filter Sieve
scripts", does not. The words for browsers, editors, terminals, video and office also describe
other programs, so they take no such stand-in: "browser" does not list CUPS's "CUPS Web Interface"
among the browsers, nor "video" Chocolate Doom Setup's "Controls, sound, video and network
settings". A Debian build in a box takes the category's base rank, with neither raise, so every
native application of the category that says the word lists above Light Video or OpenShot. A program
that runs only in a terminal (category `ConsoleOnly`, or an AppStream console application) takes the
keyword and category ranks below every name match, so "mail" lists the mail clients that draw a
window ahead of aerc, which "aerc" still lists first. An accessory, a program whose summary calls it
a notifier, tray icon, daemon, applet, indicator, gateway, plugin, thumbnailer or thumbnail
generator before any "for", "with" or "to" (so Ymuse, a "client for Music Player Daemon", is not
one), takes the keyword and category ranks below every application of the category and above a name
that starts with the term, so "mail" lists Evolution, Geary and Thunderbird ahead of Mailnag and
Birdtray, "video" lists FFmpeg Thumbnailer after the video applications, and "mailnag" still lists
Mailnag first. Media plumbing takes the same place for "video" and "music", the terms whose
categories belong under AudioVideo, and for no other: an application filed under both Audio and
Video names no medium, so the head of its summary has to say the term's word, media or player, or it
lists after every application of the category, Debian builds included. "video" lists qpwgraph, "A
PipeWire Graph Qt GUI Interface", after the video applications, and "music" after the music players,
while mpv and SMPlayer, each a "media player", stay with the players. kdos-discover-catalog writes
`ConsoleOnly` into the component of any entry that sets `Terminal=true`, so w3m and Lagrange list
after the graphical browsers.

No source on KDOS has ratings or reviews, and the `discover` port's `ratings-need-reviews.patch`
shows an application's rating row and its Reviews section, with Write a Review, only for a
resource whose backend has a reviews backend. The same patch leaves the Home page's Highest-Rated
Games and Highest-Rated Developer Tools rows empty when the current application backend has none,
so on KDOS the Home page is Editor's Choice: the entries of `featured.json`, seven catalogue
applications (KStars, Xournal++, GNU PSPP, Okteta, HexChat, Meld and GNOME Chess). A catalogue
application not yet installed is drawn with the stock icon its Debian component names when the
host's icon theme has it, else with its category's icon (Graphics' is `kcolorchooser`). A catalogue
application's page takes its author, licence and description from its package's Debian
component; the author is the component's developer, else its project group, so KStars and Okteta
read "KDE". A package with several components, as paraview has, takes the one with a licence,
then a developer or project group, then a description, and of equals the first. The featured
entries are applications whose Debian data gives them an icon of their own or their category's,
an author, a licence and a description, and the colour wheel, KColorChooser's own icon, is not
one a featured card borrows: the backend test `featuredIcons` fails when an entry is not a
catalogue application or would draw `kcolorchooser` or `applications-other`, and
`featuredComplete` when its page would lack an author, a licence or a description. The build of the `discover` port fails unless
`/usr/lib/qt6/plugins/discover/` holds exactly the fwupd and sysupdate1 backends, so a stock
backend that appears or goes missing stops the build instead of changing what Discover offers.

## The KDOS backend

`kdos-discover` installs five things:

| File | Is |
|---|---|
| `/usr/lib/qt6/plugins/discover/kdos-backend.so` | The backend: the origins above |
| `/usr/lib/qt6/plugins/discover-notifier/KdosNotifier.so` | The tray notifier for system updates ([below](#the-update-notifier)) |
| `/usr/bin/kdos-discover-catalog` | Writes `/usr/share/swcatalog/xml/kdos-ports.xml`, the AppStream catalogue of the installed ports, from kpkg's manifests, a component with no summary taking its desktop entry's `Comment`, a `GenericName` that is not its name, or the port's `description =`; and, at the image build, `kdos-debian-exclude.xml`, which takes the Debian applications `debian-exclude` names out of the AppStream pool |
| `/usr/libexec/kdos-discover/kdos-ports-helper` | The root step of installing host software prebuilt ([below](#host-software)) |
| `/usr/libexec/kdos-discover/kdos-ports-builder` | The root step of building host software on this computer and installing it |

It also points Discover's featured list at `/usr/share/kdos/discover/featured.json`, ships the
Debian lane's exclusion list `/usr/share/kdos/discover/debian-exclude` ([below](#debian-applications)), defines the
MIME types `application/vnd.kdos.ktar` and `application/vnd.kdos.kpack`, and installs a hidden
desktop entry that opens both in Discover.

### Where each answer comes from

The backend keeps no database of its own. Every answer comes from the tool that owns it, asked at
each refresh, and **Refresh** on Discover's Updates page is the same refresh. kpkg is the authority
on which ports are installed; `kdos-ports.xml` only describes them, and a package it names that
`kpkg list --json` does not list (one taken off with `kpkg remove`, which leaves the catalogue as it
was) is not installed, so the binary host's catalogue offers it again. Only when kpkg does not
answer, failing or printing no package list, does the catalogue's own version stand for the
installed one:

| Question | Asked of |
|---|---|
| What the catalogue offers, and what is installed | `kdos-appbox catalogue` |
| What each box is made of | `kdos-box list --json`, a JSON array |
| What the pack store and the medium hold | the [kdos-packd](daemons.md#kdos-packd) socket |
| Which ports are installed | `kpkg list --json`, each described by its `kdos-ports.xml` component |
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

Each page under Sets is one group of the box catalogue, named after it, such as *Office set*, and installs that
group's boxed applications. The backend has no resource for a native group of ports, the
`/ports/groups` lists `kdos update groups` shows: a native group is installed with
`sudo kdos update install <group>` or in the installer, and Discover offers only each of its
ports on its own page
([Native groups](../02-user-guide/applications.md#native-groups)).

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
"Please try again later", with the same reason behind See Technical Details. The reason is the
failed tool's last standard-error lines, each a clause, joined with "; ". The helpers run `kdos
update` under `KDOS_UPDATE_FRONT=discover`, so a download that is cut off ends "nothing was
installed — pressing Install again continues the download", and pressing Install again asks the
server for the rest. `kdos-ports-builder` copies every line the run prints, on the stream it came
on, into `/var/log/kdos-build/<port>.log` (mode 0644, under a 0755 directory, so the session user
can read it) and names it under `KDOS_UPDATE_LOG`; a failed build's reason ends "the build's log is
in /var/log/kdos-build/<port>.log", since the build's own output is shown nowhere and its work
directory is removed. The next build of the port replaces the file, and a build that succeeds
removes it. A step that downloads is
refused before it starts whenever Qt's network information says anything but online, the same test
as Discover's own "Discover is Offline" sheet (a network with no route out, or NetworkManager with
networking switched off, counts as offline), and gives the reason "it needs a network connection,
and this computer is offline".

Removing an application that runs in a box stops the box first and then removes it, because a box
stays up for a while after its application's last window closes and `kdos-box remove` refuses a
running one. A box of the `kdos-boxes` origin, one no application accounts for, is asked about
first, before anything runs: "Remove the box <name>?", saying that the container goes with every
package installed in it, its profile and every launcher exported from it, and that the files in the
home folder stay; for a box made over a pack, its writable layer, snapshots and private home in
`~/.local/share/kdos/boxes/<name>` stay too. Cancel leaves the box as it was; Proceed runs
`kdos-box stop` and `kdos-box remove <name> --purge`. How a box is built and launched is in [kdos-appbox](kdos-appbox.md), and the pack store in
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
made for one only when a search or a category asks for it, or when its box exists. A search offers
a Debian application only when every word of the query, case-folded, occurs in its name, its
component id, its package name or one of its keywords; AppStream's own search also matches
summaries and descriptions, which over thousands of applications answers almost any query. Installing one
runs `kdos-appbox install deb:<package>`, which builds a box named `deb.<package>` (each `+` in the
package name spelled `_`); the backend reads that name back to tell the application is installed.
A Debian application whose component id or desktop-id launchable matches any component of the
installed ports' catalogue `kdos-ports.xml` or of the binary host's
`/var/lib/kdos/binhost/swcatalog/xml/kdos-binhost.xml`, compared without a trailing `.desktop`, is
never offered in a box, by a search, a category or an `appstream://` link, so with the binary-host
index fetched LibreOffice and Filelight are offered only natively. Nor is one that has the name of a
desktop application of either catalogue, case-folded, in the session's language (a pair whose
name only one catalogue translates is not caught and is offered twice), and either the same
homepage host (without `www.`; on a code-hosting site or an application directory such as
`github.com`, `sourceforge.net` or `apps.kde.org`, the host and the first two segments of the
path, so two programs called Notes on `github.com` stay apart) or a package stem, the package name's leading run of ASCII letters, of four letters or more
that begins the other's or is begun by it: Debian's Schism Tracker (`schism`) is the
`schismtracker` port and Debian's MuseScore 3 (`musescore3`) the `musescore` port, and an
`appstream://` link to either opens the port. The same name alone is not enough, so GNOME
Calculator is still offered in a box beside Kalk; the cost is that a Debian build of another major
version of a port's program is not offered while the port is. Nor, finally, is one whose package `/usr/share/kdos/discover/debian-exclude` names: one package a line,
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
and without it would offer, as "Get …", an application this lane refuses. A Debian application's
name, summary and description are libappstream's pick for the session's locale: its own locale
(`en_US`, then `en`), else the catalogue's untagged `C` text, which is whatever language the
application's desktop entry or metainfo gave its untagged key, so MystiQ Video Converter's summary
reads in Spanish in an English session ([known gaps](../06-reference/known-gaps.md#a-debian-applications-summary-can-be-in-another-language)). Debian's icons and screenshots are not installed, so a Debian application
shows its stock icon when the host's icon theme has it, otherwise the icon of its category, and no
screenshot; KRunner's and Kickoff's "Get …" results show the same stock or category icon.
Those results come from Plasma's own Software Center runner, which offers an application only
when every word of the query, case-folded, occurs in its name, id, package or keywords, the rule
Discover's own search follows, and offers none when an installed application or command is named
exactly what was typed (KCalc for `kcalc`, the `htop` command for `htop`); a fuzzy application
match (KCalc for `galculator`) does not hold it back.
A catalogue application with no AppStream icon shows its category's icon too: every catalogue
category is a main desktop-entry category (AudioVideo, Development, Education, Game, Graphics,
Network, Office, Science, Settings, System or Utility), and each has one.

### Host software

The `kdos-ports` origin lists the host software that is installed. Once `kdos update fetch` has put
the release's signed binary-host index and its catalogue in `/var/lib/kdos/binhost`, each taken
where the release's signed [release index](../03-architecture/packaging.md#the-release-index) says
and kept only with the hash it gives, a port that catalogue lists can be installed, by either of
two origins:

| Origin | Shown as | Install runs, through `pkexec` | polkit action |
|---|---|---|---|
| `kdos-ports` | KDOS system | `kdos-ports-helper install <port>`, which runs `kdos update install --prebuilt <port>` | `io.kdos.ports.install`: an administrator's password, kept for a while |
| `kdos-ports-source` | KDOS, built on this computer | `kdos-ports-builder install <port>`, which runs `kdos update install --source <port>` | `io.kdos.ports.build`: an administrator's password, asked every time |

**What is part of the system is decided by the world set.** `/var/lib/kdos/world` lists the packages
somebody installed on purpose, and a package of the image has no line there unless `kdos update
install` named it. An installed port is part of the system when it has no world line, or when an
installed package depends on it (as `kdos update removable` reads the edges); it is updated with the system, its page says "Part of the
KDOS system: it is updated with the system and is not removed on its own", or "Needed by <package>"
for a world port something needs, and it has no Remove button. A world port nothing installed
depends on is removable, whatever brought it: LibreWolf installed from the binary host, a port built
here, or a package of the image that `kdos update install` named. Its page says "Installed on
request. Removing it also takes out what it brought that nothing else installed needs." The backend
learns which at each refresh from `kdos update removable --json`, beside `kpkg list --json`; an
answer that does not come leaves every port part of the system. Remove runs `kdos update remove
--dry-run <port>` as the user, which refuses without a password when the machine changed since the
list was read; its `remove <port>` line becomes Discover's question, "Remove?" with "**<Name>**
is removed from this computer. Discover can install it again.", or, when the dry run names N more
`remove <pkg>` lines, "**<Name>** is removed from this computer, with N packages it brought that nothing
else needs. Discover can install it again.", and output without the port's line fails
the removal before the password. Only a yes runs `kdos-ports-helper remove <port>` through `pkexec`, under `io.kdos.ports.remove` (an
administrator's password, kept for a while), which runs `kdos update remove <port>` and the
catalogue writer again. `kdos update remove` refuses a port that is not in the world set or that an
installed package needs, and runs `kpkg remove`, which takes the port off the world set and the
origin record too, so the next update does not bring it back. It takes the packages the install
brought with it as well, each one that nothing else installed needs, and the downloaded files of
every package not installed leave the binhost cache (see [Removing a port](../02-user-guide/applications.md#removing-a-port)). Both resources of an installed port
offer Remove and remove the same package by the same steps; one not installed has nothing to
remove.

A port is one resource per package, listed by its best component: a desktop application before a
console application before any other kind, then one from the package's own metainfo (with a
description) before one read from a bare desktop entry, then one with an icon, then the shortest
name, then the first in the catalogue. A suite is named by its face: when the package's
shortest-named application is a bare desktop entry whose name begins the chosen component's, as
LibreOffice's start centre "LibreOffice" begins "LibreOffice Base", the port takes that entry's
name, icon and launchable, the chosen metainfo's licence, developer, categories and links, and a
description of the suite: "The suite holds:" and each of the package's metainfo applications in
catalogue order by its name less the suite's and its summary ("Writer: …", "Calc: …"), since any one
application's description would describe that application under the suite's name. The entry's own
summary is not repeated in it: Discover shows the summary as the heading above the description.
LibreOffice is therefore listed as "LibreOffice", with The Document Foundation, MPL-2.0 and a page
that lists each application the package ships metainfo for (Writer, Calc, Impress, Draw and Base),
not as one of its applications; any other bare entry leaves the chosen component as it is, so a
package's minor tool never names it. An `appstream://` link to another of its components, such as
LibreOffice Writer, opens the same port, and a search finds the port by the name or a keyword of any
of its package's components, so "writer" lists LibreOffice. The catalogue writer,
`kdos-discover-catalog`, takes a metainfo component with no desktop-id launchable, or one naming
only entries its package does not install (Audacity's `audacity.desktop`), to launch
`usr/share/applications/<id>` (or `<id>.desktop`), or an entry it names in another case
(Stellarium's), when its package owns that entry, or the package's one desktop entry when the
package has one metainfo (Viking's), and writes it with that entry added to its launchable and, where the metainfo names none, the entry's icon and
categories; the entry is not written again as a component of its own. Discover launches the first
of a component's entries that is installed. A component with no `<project_license>` takes its
recipe's `license =` as written, an SPDX expression, and one with no homepage `<url>` its `homepage
=`, read from the first `kpkgbuild` of the port under `/ports/core`, `/ports/src` or `/kdos/src` in
the root it describes (the 70_image chroot binds the build's ports tree at `/ports` and the
repository at `/kdos`), so every bare desktop entry, Audacity's and Stellarium's among them,
shows its recipe's licence, "Licenses: GPL-2.0-or-later" and the like. A component with no
untranslated name takes its desktop entry's `Name` (the bare entry itself, or the first entry a
metainfo component launches), and one with no summary takes that entry's `Comment`, else a
`GenericName` that is not its name compared without case, else the recipe's `description =`, so
every row of a Discover list has a summary line under its name: Contact Theme Editor reads "Theme
editors for KMail message headers and KAddressBook contact views and printouts" and QMapShack "GPS
device mapping utility". The writer marks each component read from a bare desktop entry with the custom value
`KDOS::BareEntry=1` and gives no other component the key, and in a catalogue with any such value
the mark alone is how the backend tells a suite's face from the package's metainfo applications;
a licence tells nothing, since every component carries its recipe's. A catalogue with no mark
anywhere (the published v0.2 binhost's, which a fresh machine reads until its own release's
catalogue is fetched) is read by what a desktop entry lacks: a desktop application whose id is a
`.desktop` name and which has no description is a bare entry, since the desktop-entry reader
writes none, so such a catalogue still lists LibreOffice as the suite and names each of its
metainfo applications once, beside the bare entries of the same ids it also holds. The author
comes only from a metainfo file the package installs, its developer's name, else its project
group (a KDE module's `<project_group>KDE</project_group>` reads "KDE"); a recipe names no author,
and a port without either shows "Unknown author". A port whose source carries no metainfo writes its own in `build.sh`
when Discover should name its developer, as LibreWolf's does with
`usr/share/metainfo/librewolf.metainfo.xml` under the id `librewolf.desktop`. The component's AppStream kind sets where Discover lists the port:
`addon` under Add-ons, `desktop-application` (or no kind) under Applications, and a console
application or any other kind under Application Support, as a technical package. A desktop
application with no category at all is filed under Other, so the All Applications page, which
matches by category, lists it and finds it in a search. A port the binary host offers carries its icon in the binhost's catalogue, base64 in the
custom value `KDOS::IconData`: the icon its metainfo names, else the `Icon=` of the desktop entry it
launches, whichever package installs that entry, from the hicolor theme at 64x64, else 48x48, else scalable, else a larger PNG scaled to
64x64, else a smaller one, else `/usr/share/pixmaps`, and never over 32 KiB. Discover writes it to
`~/.cache/kdos-discover/icons` and draws it, so an application not yet installed shows its own icon.
An installed application whose stock icon the loaded icon theme does not find, as for one Discover
has just installed, is drawn from that icon's file in hicolor or `/usr/share/pixmaps`.
A port whose icon the theme does not have and that carries none, as for one whose only icon is an
SVG over 32 KiB in its own uninstalled package, shows `utilities-terminal`
for a console application, `preferences-plugin` for an add-on, its first category's icon for an
application, and for any other kind its category's icon or, with no category, one of its kind:
`application-x-sharedlib` for a generic component such as libfprint or pcscd,
`preferences-system-services` for a service, `hwinfo` for a driver or firmware,
`preferences-desktop-font` for a font. The Graphics category's icon is `kcolorchooser`,
KColorChooser's colour wheel, in breeze and breeze-dark alike: Breeze's `applications-graphics` is
a plain sphere and `image-x-generic` a file-type placeholder, and either reads as a missing icon
in a list.

Each port the binary host holds is two resources with one AppStream id, installed or not,
`kdos-backend://port/<id>` and `kdos-backend://port-source/<id>`, so Discover lists one application
and offers both origins in its source picker. The built-here twin of an installed port carries the
port's state, installed version, notice and Remove, and a list leaves it out, so an installed port is one
row, "KDOS system". Discover's application page keeps the most sources it has seen while it is
open, so a twin that left with the install would leave the page after a removal with one source:
no source picker, and an "Install from KDOS system" button. **The origin Discover opens follows `install =`** in `/etc/kdos/update.conf`, the
route System Settings › System Versions sets and `kdos update install` takes at a terminal: under
`install = source` the built-here resource, otherwise the prebuilt one. Discover keeps one resource
per AppStream id in a list, and which of two that sort equal survives is not a backend's to decide,
so a search or a category lists only the preferred one, or the binary host's once the port is
installed; the application's link answers both,
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
root, and its `total` line becomes the question of Discover's confirmation dialog, titled
"Install?", and every confirmation text opens with the program's name in bold, "**<Name>**: ",
since the dialog is narrow and elides a long title where its text wraps. The line counts every
prebuilt package and, in bytes, only the files not already in the binhost cache
`/var/lib/kdos/binhost` (and of a cut download only the rest), which keeps every package it
downloaded until a removal clears those not installed, so after the name the prebuilt route reads "Installs N packages; downloads (size).", or
"Installs N packages, already downloaded." when the total has no bytes, the number and unit of the
size never split across lines; the built one reads "Builds N packages here and installs M
prebuilt, downloading (size); building can take hours." (or "and installs M prebuilt, already
downloaded"), with the packages built here named.
The total also counts the `<pkg> dev <file> <bytes>` lines, each restoring the development files of
a package already installed so a build here finds its headers; they are said apart, "and restores
the development files of K installed packages", M is the prebuilt count without them, and the size
stays the whole download.
Proceed runs the helper through `pkexec`, which asks for the password; until the helper writes its
first line, which it can only once polkit has the password, the install's status is Waiting,
Downloading from then on, and Installing from the first `==> installing <pkg>` or `==> restoring
the development files of <pkg>` line, since `kdos update install` fetches everything before it
installs or builds anything. Its first line comes as the first download starts, `==> downloading
<file> (<size>)`, so a slow download reads Downloading, not Waiting. Cancel, or closing the
dialog, ends the install with nothing run. Cancel once the helper runs closes its standard input,
the one stop a user can give a root process: the helper sends SIGTERM to the process group it
runs `kdos update` in, so kpkg, the build and its compilers stop with it, then SIGKILL after 60
seconds, and exits 130 without writing the catalogue. The packages installed before the stop stay
installed, the dependencies a source install built on the way among them; those are not in the
world set, so `kdos update remove` refuses them as part of the system. Discover disables the cancel button, keeps the status and ends the install Cancelled
only when the helper has exited, so the refresh after it never meets a run still holding the
package lock. A dry run that refuses, such as a prebuilt install the
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

Opening Discover asks the release server for one thing: its signed indexes (the tree, the
binary-host index and the image manifest, or the binary-host index alone on a machine with one
root slot), at most once an hour, through the same `CheckNew` that
**Refresh** calls ([below](#checking-for-an-update)). It downloads no package and no source. Each of
Discover's own paths to the network is closed:

| Path | Closed by |
|---|---|
| The Home page's featured list, from `autoconfig.kde.org` | `/usr/share/discover/featuredurlrc` points at `/usr/share/kdos/discover/featured.json` on the machine |
| Application ratings from `odrs.gnome.org`, fetched at every start whose cache is a day old | `odrs-offline.patch` adds `[Software] FetchOdrsRatings`, which `discoverrc` sets to `false`; the Most Popular list is not shown |
| Firmware metadata, refreshed for every enabled download remote at each start | fwupd's LVFS remote is installed switched off ([Firmware](#firmware)) |
| Screenshots | Only a screenshot that is a file on the machine is shown |
| KNewStuff add-ons from `store.kde.org` | The KNewStuff backend is not built, and `ghns=false` in `kdeglobals` turns off every Get New download: the settings pages show no Get New button, the wallpaper dialog's Get New Plugins is greyed, and Add Widgets' Get New menu holds only Install Widget From Local File |

Everything else that reaches the network is what a person starts: a store-lane box, a host port from the binary
host or the sources of one built here, a firmware install and a system update. [Principles](../01-philosophy/principles.md#offline-by-construction)
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
entry for it until an update writes it again; with nothing behind, `kdos update apply --mount`
writes it again and deploys it without a trial. `kdos-sysupdated`'s `ListSlots` reports it as
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
Discover calls when it opens and when a person presses **Refresh** on its Updates page, fetches the signed tree and the release
index, and through that index the binary-host index and image manifest, at most once an hour, then
answers from the document. A fetch that found no network counts as none, so the first Refresh once
the machine is online asks again at once. A document belongs to the
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
the release publishes one and the machine has an inactive slot to write it into, or else the
release's packages, only when the signed binary host holds every one of them that is not built here,
matched to this machine's build; a package whose origin is `source` is built again in the slot and
counts as covered. A slot image carries the base alone, so the packages installed on purpose follow
it into the slot from the same release's binary host, and an image is refused before the slot is
written when that binary host does not cover one of them that came prebuilt. Either is offered only
beside the cached release index that names it, which is where `apply` downloads every file from. A
machine with a package the binary host does not match is told so before any slot is touched.

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
page's list that names where the failed step looked and adds what failed (its `detail`) in
brackets, such as
"Updates could not be fetched from file:///mnt/bh. (file:///mnt/bh/kdos-ports.tar.xz could not be
downloaded)". A tree or image problem names the document's `channel`. A `binhost-` problem names
its `binhost_base`, the binary host `update.conf`, `$KDOS_UPDATE_BINHOST` or the tree's release
names, else its `binhost`, where the index in place came from, else "the binary host"; never the
channel, which is another place. A binary host at `/root/bh` that publishes nothing reads "No
package index is published at file:///root/bh." with no detail after it: the tool's words for that
code name the same URL again. The tool's words follow the sentence in brackets only when they add
something: never for a `no-network` code, whose sentence says what its words say, and never when
they name the URL the sentence names, so the URL is said once. `no-network` is recorded for a name
that did not resolve, or for a refused or timed-out connection on a machine with no default route;
a server that refused the connection or did not answer while a default route exists is a
`fetch-failed` whose words are "the server at <host:port> did not answer", and
the page reads "The release's package index could not be fetched from https://10.0.2.2:18999.
(the server at 10.0.2.2:18999 did not answer)". Each
code `kdos update check` reports has its sentence: the fetch's `no-network`, `fetch-failed` and
`no-key`; the binary-host index's and the slot image's `binhost-` and `image-` codes
(`no-network`, `fetch-failed`, `no-key`, `unverified`), each naming the package index or the system
image, and `binhost-absent`, "No package index is published at …", for a named binary host that
publishes none; and the last apply's `unverified`, `build-failed` and `needs-source`, said as an
update that installed nothing. A bare `no-network` or `fetch-failed` may be the fetch's or the
apply's, so its sentence fits either. `no-tree` is not said: with no tree nothing can be behind.
Nor is the fetch's `fetch-failed` whose detail is "the published tree … is older than this
machine's": that machine is ahead of the release, not cut off from it, and an image the document
offers is listed with no warning beside it. `kdos update check` ranks that refusal below every
other problem, so it is the document's problem only when nothing else is wrong, and a binary-host
problem on a tree ahead of the release is still said. The backend knows the refusal by those words,
as `kdos update`'s own binary-host fallback does. Without the warning the page would say "Up to
date" about a release nothing could read. The next check with no problem takes it away; the
backend reads the document at each refresh, and a change to `/var/lib/kdos` starts one.

### Restart, trial and confirmation

After `Install` the other slot is armed for one trial boot. A new version is kept only when that
boot is confirmed by two marks under `/run/kdos/bless/`, both regular files owned by root:

| Mark | Written when |
|---|---|
| `userspace` | The end of `rcS`: every service started |
| `graphical` | A Plasma session answered on the bus for a minute (`kdos-notice --bless` calls `io.kdos.Slots1.Bless`), or the login screen stayed up for a minute on a machine SDDM does not log in automatically |

When both exist, `kdos-sysupdated` runs `kdos-bootctl mark-good` once and writes
`/run/kdos/bless/confirmed`. Either mark alone confirms nothing: a system that boots with a desktop
that crashes is the failure a trial exists to catch. polkit grants `io.kdos.slots.bless` to the
active local session alone, with no password. The login screen counts only where nobody has logged
in this boot and SDDM logs nobody in automatically, so a login screen that came back because a
session crashed never confirms the slot that crashed it. There the login screen is the graphical
check, and a desktop that fails only after a login is kept; System Settings > System Versions >
Start This Version Next Time on the other slot is the way back from it. A machine whose `inittab` starts no
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
would answer it with "The system needs to be restarted for the updates to take effect", for an
update there is none of. System Versions and `kdos-notice`'s trial notice speak
for that switch.

When an update arms a slot, Discover's notifier posts the persistent "Restart is
required" notification, the `UpdateRestart` event of the `discoverabstractnotifier` component,
once. It is shown, and sent once, only through the `discover` port's
`notifyrc-update-restart.patch`: upstream builds it with `KNotification::event()`, which queues a
send of its own before Discover sends it again, posts it as a default event, which KNotification
looks up in `plasma_workspace`, whose notifyrc has no such event, and its own notifyrc misspells the
event's group; KNotification shows nothing for an event its component does not declare.
Its two buttons, "Restart" and "Shut Down", open the logout prompt's restart and shut-down pages;
the update is already staged in the other slot, so neither installs anything, and the `discover`
port's `notifier-restart-labels.patch` gives them those labels where upstream's read "Update and
Restart" and "Update and Shut Down".
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
a slot image and falling back to the binary host, which is the one lane a machine with one slot
takes, since a slot image has nowhere to go there; `--image` and `--binhost` choose a lane by name;
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
