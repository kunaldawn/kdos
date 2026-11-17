# KDOS settings pages

System Settings is where a KDOS machine is configured, and almost every page in it is KDE's. KDOS
adds five pages for the things only KDOS has (boxes, its firewall, its health check, its accent and
the two system slots) and configures one KDE page, Backups, with a backend of its own. Beside them
runs `kdos-settingsd`, a root daemon that answers the three system interfaces KDE's own pages save
through for the machine's name, clock, time zone, language and keyboard.

This chapter documents each KDOS page: where it is in System Settings, what it shows, what it
changes and through which program or bus name, and what it refuses. Each page names its section
here as its help address, and its Help button opens that section in the Help Center (see
[Help](#help)). It is written for the person using the page who wants to know exactly what
a switch does, and for anyone changing one. Everyday administration (users, networks, printers,
updates) is in [Administration](../02-user-guide/administration.md).

| Page | Where | Port | Changes things through |
|---|---|---|---|
| [Boxes](#boxes) | Security & Privacy → Application Permissions | `kcm-kdos-boxes` | `kdos-box` and `kdos-appbox`, as the user |
| [Firewall](#firewall) | Networking | `kcm-kdos-firewall` | `io.kdos.Power1` on the system bus, served by `kdos-powerd` |
| [Health](#health) | System, and Info Center | `kcm-kdos-health` | nothing; it runs `kdos doctor --json` |
| [KDOS Look](#kdos-look) | Appearance & Style → Colors & Themes | `kcm-kdos-look` | `kdos theme` and `kdos phosphor`, as the user |
| [System Versions](#system-versions) | System | `kcm-kdos-system` | `io.kdos.Slots1` on the system bus, served by `kdos-sysupdated` |
| [Backups](#backups) | Backups | `kup`, with a KDOS backend | Kup's daemon, as the user |

The first five are KDOS's own code, Qt Quick pages over a small C++ controller, built with CMake
against KF6 KCMUtils. Every command a page runs is an argument vector handed to `QProcess`, never a
shell. None writes a file of its own about the thing it shows: the program that owns the setting
writes it, so the page, the command line and KRunner can never disagree. A page that changes a
system-wide setting holds no privilege; the daemon behind it asks polkit, and the Plasma session's
polkit agent shows the password prompt.

## Boxes

The Boxes page lists every box `kdos-box list --json` reports, under its application's name and
icon. Opened as `systemsettings kcm_kdos_boxes --args <box>` it opens that box, which is how
Discover's "Runs in a box" badge links to it. For one box it shows:

| Section | What it holds |
|---|---|
| Lifecycle | Start, stop, restart, recreate, remove, and export as a `.ktar` set |
| Colour | The title-bar colour KWin gives the box's windows: one swatch per installed KDOS colour scheme, and Neutral, the default. Each swatch is a small title bar in its scheme's own colours: the background, a sample of the title text and a stripe of the scheme's accent |
| Permissions | Network, the home folder and what the box writes, devices, the graphics card and sound, memory, processor and process limits, when it stops while idle, whether its windows show, when it may use the X11 display, and which desktop-wide Wayland and bus grants it holds |
| Confinement | While the box runs, which boundary held for it, and why |

The page reads the box's profile, `~/.config/kdos/boxes/<box>.conf`, and shows beside each control
what `kdos-box profile <box>` says enforces that key, with every `!` line about a value the box
cannot be given. The words are kdos-box's; the page sets them as prose, with a capital first word
and each quoted word or `key=value` in code. A change runs `kdos-box profile <box> key=value` for
each key that changed; lifecycle buttons run `kdos-box` and `kdos-appbox` the same way, as the user
and without root. A command that fails says so with the last line it printed as the reason, and
with no reason when it printed nothing; the busy line shows only the running command's output,
never the listing that refreshes beside it. The page never calls podman. The profile's keys are in [kdos-appbox](kdos-appbox.md), and the
desktop-wide grants, which KWin reads from the same file, in [KWin](kwin.md#what-a-box-may-bind).

### What takes effect when

A namespace, a volume or a podman limit cannot change on a running container, so a change to the
network, the home folder, what the box writes, devices, the graphics card, the init process or a
limit, on a box that has a container, is written at once and takes effect only when the box is
created again. The page remembers such changes in `$XDG_STATE_HOME/kcm_kdos_boxesstaterc` and shows
a Recreate banner until the box has been made again; Recreate stops, removes and creates the box.
Only a pack box keeps its writes outside its container. Any other box keeps them inside it, so
recreating or removing one deletes them, and the confirmation says so. The colour and the grants
apply to the next window the box opens.

### Confinement

`kdos-appbox` records, at every launch, which boundary held, in
`$XDG_RUNTIME_DIR/kdos/boxes/<box>.state`. The page shows it in three words:

| State | Means |
|---|---|
| Confined | The box's Wayland socket is tagged, its private runtime folder has no session Wayland socket and no `bus`, its bus goes through `xdg-dbus-proxy`, and no X11 display is within its reach |
| Tagged | KWin's restrictions apply to the tagged socket, but the box can reach past it: through a runtime folder shared with the session, an unfiltered bus or an X11 display. The reason is shown |
| Not confined | Its last launch had no tagged socket, so nothing marks its windows as a box's |

A box that is not running, or a launch that recorded nothing, reads as unknown, and the page says
it cannot tell: no file is never read as confined. The same state draws the
[sandbox badge](kwin.md#the-sandbox-badge) in the box's title bars.

## Firewall

The Firewall page lists the named services KDOS's firewall knows (`ssh`, `http`, `smb`, `kiwix`
and the rest) with a switch each, and how many connection attempts each closed service has turned
away. It never shows a port: the firewall is a table of named services, and opening one by port
and protocol is refused by design. Both the list and the change go to `kdos-powerd` over its
system-bus face, `io.kdos.Power1`: the page reads `ListFirewallServices` and `ListFirewallKnocks`,
holds the switches until Apply, and then sends one `SetFirewallService(name, open)` per changed row,
in order, stopping at the first refusal. The firewall itself is described in
[The daemons](daemons.md#firewall) and [The security model](../03-architecture/security-model.md).

The page is read-only when polkit, asked without interaction, says no to `io.kdos.firewall.set`
for the page's own connection, which is the question the daemon asks of the same caller. "Yes after
a password" leaves the switches live, and the daemon raises the prompt when the change is applied.
While an apply runs, the Reset button does nothing, so an answer from the daemon is never counted
against the wrong request. The page follows the daemon's `FirewallServicesChanged` signal from the
owner of `io.kdos.Power1` only, and reloads when it holds no change of its own.

### Allow on this network?

`/usr/libexec/kdos/kdos-fwask`, started at login by an autostart entry in
`/usr/share/kdos/xdg/autostart`, is the notification that offers to let a blocked program in. Every
ten seconds it asks `kdos-powerd` for the knock counts, which count only packets from another
machine. When a count rose for KDE Connect, krdp,
krfb or kiwix-serve, the program behind it is running as the session's user, and that user has not
been asked about it on the current NetworkManager connection, it posts one notification with Allow
and Not on This Network. It asks only about services a session program serves: a rising count on
`http` with no web server running is a scan, and asking about it would teach people to click Allow.

Allow opens the service on every network, because the firewall is one table for the machine. What
is remembered per network is the question, in `$XDG_STATE_HOME/kdos/fwask`, one line per network
and service, so a person who declined at a café is asked again at home. A prompt whose service was
opened elsewhere while it was up is withdrawn, and that is remembered as the answer. An
administrator turns the prompt off for everyone with a `kdos-fwask.desktop` carrying `Hidden=true`
in `/etc/xdg/autostart`.

## Health

The Health page runs `kdos doctor --json` and shows one row per check under doctor's own section
headings: passed, not checked on this machine, or needing attention, the last in the negative
colour and named "Needs attention" to a screen reader. Check Again runs doctor again, Copy to
Clipboard copies the report as `kdos doctor` prints it, and a filter narrows the rows. The page
changes nothing, so it has no Apply. Doctor exits 1 when a check warned, which the page shows as a
report like any other; only another exit status, output that is not a report, or a `kdos` that
cannot be started is shown as an error. What doctor checks is in
[The kdos command](kdos-command.md#kdos-doctor).

### About this System

The same port adds six KDOS rows to the About this System page, named by
`/usr/share/kdos/xdg/kcm-about-distrorc`, each a shell script in `/usr/libexec/kdos`:

| Script | Shows | From |
|---|---|---|
| `about-slot` | The A/B slot, its version and state, or `Live medium` | `/run/kdos/slot.json`, or the boot entry's `kdos_slot=`, or an overlay root in `/proc/mounts` |
| `about-update` | The key that verified the last update offered, and when | `/var/lib/kdos/update.json` |
| `about-tree` | The commit and date of the recipe tree | `/ports/TREE` |
| `about-accent` | The KDOS accent, and the login screen's when it differs | the accent files |
| `about-libc` | musl's version | kpkg's record |
| `about-boxes` | How many box profiles this user has | `~/.config/kdos/boxes` |

Info Center runs each script twice on its interface thread and ends System Settings when one prints
fewer than two lines, so each prints a label, ending in a colon like Info Center's own, and a value on every machine and every error (`Unknown`
when it knows nothing), reads only small state files, never asks podman, the ESP or the network, and
sets its own `PATH`. `testing/aboutcheck.sh` runs each on a booted image and asserts the two lines
and a run under 100 ms.

## KDOS Look

The KDOS Look page holds the two things the stock appearance pages cannot: the one action that
carries an accent past Plasma, and Phosphor mode. The eight accents themselves are colour schemes
and global themes on the Colours and Global Theme pages; this page reaches everything else.

**The accent.** A swatch runs `kdos theme <accent>`, which sets the global theme and also repaints
open Konsole windows, the Qt 5 palette, the lock and login screens, the splash, the boot menu and
the text consoles. Each swatch is painted from its own `KDOS<Title>.colors`, and the list is the
one `kdos theme list` prints, so the page offers no accent the command would refuse; an accent whose
scheme is not installed is listed and cannot be picked. There is no Apply: an accent is a command
that repaints the machine, and one runs at a time. A second click while one is running is refused
rather than queued, and so is any click while [kded-kdos](plasma-shell.md#kded-kdos) is running
`kdos theme`, because two runs at once would write the same files in an order nobody chose. What
setting an accent changes is in [The kdos command](kdos-command.md#kdos-theme).

**Where the accent is held.** Below the swatches the page compares the three places an accent
lives: Plasma's `kdeglobals [General] ColorScheme`, the KDOS accent the terminals and command-line
tools wear (`~/.config/kdos/accent`), and `/etc/kdos/accent`, which the screens before login wear.
The line under the swatches names what each surface wears: the login screen wears the greeter set
under `/usr/share/kdos/greeter/<accent>/`, which for `paper` is its dark partner `bone`, so the line
names the partner; the boot menu is named only where `/boot/efi` exists, so a live medium, whose
menu cannot be recoloured, is not told its menu changed. When Plasma is ahead or the machine behind,
Repaint Everything runs `kdos theme --from-plasma
<Plasma's accent>`, the command kded-kdos runs, so Plasma's choice is kept and not applied twice.
The page follows kded-kdos over `io.kdos.Kded1`: while the module is carrying an accent down, the
page is busy and starts nothing, and when its last run failed while the accents still disagree,
its reason is shown above Repaint Everything. The page asks the module with D-Bus activation off,
so a page opened outside a session never starts a second kded.

**Phosphor mode.** The section reads `kdos phosphor status --json` and holds the mode's switch, the
two presets (Subtle: a soft glow and vignette; Retro: scanlines and a curved screen, which lowers
contrast), Draw over fullscreen windows, and Terminus in Konsole and for fixed-width text. Each runs
`kdos phosphor on|off`, `preset subtle|retro`, `fullscreen on|off` or `terminus on|off`. Fine-tune…
opens the CRT effect's own settings, where strength, scanlines and curvature are set by hand; the
page then shows the preset as Custom, and picking a preset replaces the tuning.
Under software rendering the switch and every option are disabled together, because the mode can
never come on and an option recorded for it would wait for an `on` that is refused; a mode already
on keeps them, so it can still be changed and turned off. A sentence says why, from the record's
`refusal`: GPU rendering, or a running Plasma session. A `kdos`
without the verb leaves the section saying the mode is not available. What the mode does is in
[KWin](kwin.md#phosphor-mode).

The page never writes `kwinrc`, `kdeglobals` or a Konsole profile itself.

## System Versions

System Versions shows KDOS's two root slots, A and B, with the version, build date, tree commit and
kernel each holds and the state each is in: running, confirmed, on trial or failed. Its one button,
Start This Version Next Time, arms a one-boot trial of the other slot: the previous version after
an update, or the new one again after a rollback. Its settings are where `kdos update` takes a
newer release from (the KDOS releases, a folder or removable medium, or another address) and how
software is installed (below). Beside it
the page shows what the update service reports (an update available, one already prepared in the
other slot and waiting to be installed from Discover, and why the last one stopped) and opens
Discover to act on it.

A machine with no boot state shows no slot forms. One installed with a single root is told it has
one version of KDOS and nothing to go back to. The live medium (`Live` on `io.kdos.Slots1`) is
named as the live session: it runs from the installation medium, keeps no versions and is not
updated, so the page shows neither the update source, the Installing software section nor the
Discover button, and its one button,
Install KDOS, starts `kdos-installer-gui`.

**Installing software.** The section's two choices are the `install =` and `build-deps =` lines of
`/etc/kdos/update.conf`, read from `io.kdos.Slots1`'s `InstallRoute` and `BuildDeps`:

| Choice | Options | Line |
|---|---|---|
| Install programs: | Download them prebuilt; Build them on this computer | `install = prebuilt\|source` |
| When building, what a program needs: | Download it prebuilt; Build it too | `build-deps = prebuilt\|source` |

They are what [`kdos update install`](kdos-command.md#install-routes-and-groups) does from a
terminal when no flag names a route, and what a build gets for each dependency the machine lacks;
Discover offers both routes for every program the binary host holds through its two origins, "KDOS system" and "KDOS,
built on this computer", and opens the one the first choice names
([Discover](discover.md#host-software)). A program keeps the route it came by at
every update. Apply sends both with `SetInstallRoute`, and Defaults sets both to prebuilt.

The page holds no privilege and touches no file of the boot state. Every change is a call to
`io.kdos.Slots1` on the system bus, served by `kdos-sysupdated`, which asks polkit before it acts:

| Call | Polkit action | Default for the active session |
|---|---|---|
| `Try`, `CancelTry` | `io.kdos.slots.try` | an administrator's password, kept for a while |
| `SetChannel` | `io.kdos.slots.channel` | an administrator's password, kept for a while |
| `SetInstallRoute` | `io.kdos.slots.install-route` | an administrator's password, kept for a while |

`Try` runs `kdos-bootctl try <slot> 1`, and a refusal from kdos-bootctl is shown in its own words. A
location is written whole into the `tree =` line of `/etc/kdos/update.conf`, so a path or address
holding a space or a control character is refused before it is sent. The interface is
`io.kdos.Slots1.xml` beside the page's sources, and both sides are held to it: the page's tests
generate their fake server from it, and `kdos-sysupdated` serves the same members. How slots are
chosen at boot is in [Boot and init](../03-architecture/boot-and-init.md#ab-slot-selection), and how
an update fills one in [Discover](discover.md#system-updates).

## Backups

The Backups page is Kup's, from Plasma. KDOS builds Kup with a restic backend, carried as a patch
written for upstream, so a plan can be an Encrypted Versioned Backup beside Kup's own rsync and bup
types. A restic plan:

- initialises an empty destination, checks the repository when asked, and saves a snapshot tagged
  `kup`;
- keeps the password in the session's wallet, in the folder Kup, keyed by the repository's location,
  and never shows it back; it reaches restic through a pipe on file descriptor 3, never as an
  argument or a variable. A plan may name a password file instead, which must be readable by its
  owner alone;
- with Remove Old Backups Automatically, keeps every snapshot of the last two days, then one a day
  for a month, one a week for six months, one a month for two years and one a year after that,
  counted from the newest snapshot, and removes only `kup`-tagged snapshots, so snapshots
  `kdos backup` saved into the same repository are never removed by Kup;
- is checked before each backup when Verify integrity of backups is on, and a failed check's
  notification offers Repair (the index repaired, then the repository checked again);
- is saved, pruned by that policy, browsed and its log read from the Backup Status applet in the
  tray (Save Backup, Prune Old Backups, Show Files, View Log); the applet has no check action;
- starts with `~/.restic` as its destination when restic is chosen for a new plan, where Kup's own
  default is bup's `~/.bup`; the repository is left out of its own snapshots wherever it is.

Dolphin browses a restic repository through the `restic:` KIO worker, one folder per snapshot, read
with `restic --no-lock` so it works on read-only media and while a backup runs. `kdos backup` is
the same engine from the command line ([Administration](../02-user-guide/administration.md)).

## kdos-settingsd

KDE saves a machine's name, clock, time zone, language and keyboard through three D-Bus interfaces
that systemd defined, and never by writing files. KDOS has no systemd, so the root daemon
`kdos-settingsd` serves `org.freedesktop.hostname1`, `org.freedesktop.timedate1` and
`org.freedesktop.locale1` on the system bus, and these KDE pages and programs work through it
unchanged:

| Page or program | Uses |
|---|---|
| System Settings → Date & Time | timedate1, and nothing else: the zone, the clock and network time |
| The login screen's KWin, run with `--locale1` | locale1's keyboard layout, followed when it changes; `startplasma` copies it into each session |
| plasma-setup, on an OEM first boot | all three |
| System Monitor's system sensors | hostname1 |

Each setting is written to the file the rest of the machine already reads: the hostname to
`/etc/hostname` and `/etc/hosts`, the zone to `/etc/localtime` and
`/etc/profile.d/20-timezone.sh` (with the zone's Wi-Fi country), network time as the
`/etc/service.disabled/chrony` marker, the locale to `/etc/locale.conf`, the console keymap to
`/etc/keymap` and the graphical layout to `/etc/kdos/xkb.conf`, never under `/etc/X11`. So a value
set on a page, in the installer or with an editor is the same value everywhere.

polkit decides who may change what, under systemd's own action ids. The active session may set the
time zone and switch network time without a password; setting the clock, the hostname, the locale
or the keyboard asks for an administrator's password. The Date & Time page is KDE's, and it
authorises setting the clock before it saves anything, so every Apply there asks for an
administrator's password, a zone-only change included, and that one prompt covers its three calls;
`kdos timezone` changes the zone with none. A value is checked (a zone against the zone tree, a keymap against kbd's, a layout
against xkeyboard-config's) before polkit is asked, so a bad value is refused without a prompt.

Every file, every polkit action, the hardware-clock rule, how the daemon is started and how it is
tested are in [The daemons](daemons.md#kdos-settingsd).

## Help

Each KDOS page's metadata names its section of this chapter in `X-DocPath`, as
`kdos/04-programs/kdos-settings.html#<section>`. The Help button opens `help:/` followed by that
text, and the desktop hands any `help:` address to the Help Center. KIO's `help:` worker looks a
path up among KDE's handbooks under `/usr/share/doc/HTML/en`, where `kdos-doc` installs `kdos` as a
link to the book in `/usr/share/doc/kdos`, so the worker serves the chapter's own page and the Help
Center scrolls to the section. Every page of the book carries its stylesheet inside it, because the
Help Center hands each part of a `help:` page over as HTML and would refuse a linked stylesheet.
The same address opens from a terminal as `khelpcenter help:/kdos/04-programs/kdos-settings.html`,
and `kdos-doc 04-programs/kdos-settings` opens the page by its file. The Help Center is not on the
installation medium, because it pulls in Qt WebEngine; `kdos update install khelpcenter` adds it,
and without it `kdos-doc` opens the page in the browser. `testing/anchorcheck.py` fails
when an `X-DocPath` names a section this chapter does not have.

## See also

- [Administration](../02-user-guide/administration.md): users, networks, the firewall, updates and
  backups from the user's side
- [kdos-appbox](kdos-appbox.md): box profiles, and every key the Boxes page edits
- [KWin](kwin.md): the grants, the box colour and Phosphor mode the pages switch
- [The Plasma shell](plasma-shell.md): kded-kdos and the notices that open these pages
- [Discover](discover.md): system updates, and the slot state System Versions shows
- [The daemons](daemons.md): `kdos-powerd` and the other root daemons
- [The security model](../03-architecture/security-model.md): polkit, the firewall and boxes
- [The kdos command](kdos-command.md): `kdos theme`, `kdos phosphor`, `kdos doctor` and
  `kdos backup`

<!-- book-nav -->
---

*Part IV — Programs, chapter 23.* Previous: [22. The Plasma shell](plasma-shell.md) · [Contents](../README.md) · Next: [24. Discover](discover.md)
