# The Plasma shell

`plasmashell` draws everything on a KDOS screen that is not a window: the wallpaper, the panels,
the Start menu, the system tray, the notifications and the on-screen displays. Around it run the
other session programs this chapter covers: KRunner, kded6 with KDOS's module, System Monitor's
sensor daemon, and the Welcome Center. All of them are KDE's. What KDOS adds is the three desks a
person chooses between, two tray applets, a KRunner runner, a notifier, a kded module, a System
Monitor plugin and page, the `library:/` folder, a file-manager action and the welcome tour, each a
port under `src/desktop/`.

This chapter is the reference for those additions: what each one shows, where it reads from, what
it runs and where its state is kept. It is written for anyone who wants to know where a part of
the KDOS desktop comes from, or who is changing one. Using the desktop is in
[The desktop](../02-user-guide/desktop.md), the colours and fonts in
[Theming](../02-user-guide/theming.md), and the session these programs run in, from the login to
the logout, in [The session](../03-architecture/session.md).

Three rules hold for every component here. None of them is a second desktop: each is a plugin,
package or program of the kind Plasma already loads, in the place Plasma looks for it. None runs
a shell: every command is an argument vector, so a box name, a file name or a search reaches the
program it is meant for as one argument. And none reaches the network unasked: what a component
shows comes from files and programs on the machine.

## The desks

A desk is the arrangement of panels on every screen. KDOS ships three, as Plasma layout templates
under `/usr/share/plasma/layout-templates/` from the `kdos-plasma-look` port:

| Desk | Template | Panels on each screen |
|---|---|---|
| KDOS, the default | `org.kdos.desk.kdos` | One opaque bottom panel, not floating: Start (the KDOS mark and the word Start), the virtual desktops, the pinned applications, three meters (processor, memory, network), the system tray, the clock and Show Desktop |
| Dock | `org.kdos.desk.dock` | A top bar with Start (the mark alone), the focused window's application menu, the tray and the clock; and a floating dock, centred, as long as its contents, that moves out of a window's way, with the pinned applications and Show Desktop |
| Tiling | `org.kdos.desk.tiling` | One thin top bar: Start (the mark alone), the desktops by name, the window list, the processor and memory as text, the tray and the clock; and automatic tiling turned on |

The pinned applications are Konsole, Dolphin, Firefox ESR and Kate, the first words of
`~/.config/kdos/favorites`, which is also the list of boxes warmed at login; `testing/lnfcheck.py`
holds the two lists together. The clock of each desk is drawn in the fixed-width face the global
theme set, read from `kdeglobals` when the desk is laid out. The tray hides plasma-pa's microphone
item and Plasma's camera indicator, because the [privacy lamps](#privacy-lamps) show both and name
the box. The KDOS desk also adds the input-method panel when the session's language is one of
those KDE adds it for.

### How a desk is laid out

Every KDOS global theme carries one layout script,
`contents/layouts/org.kde.plasma.desktop-layout.js`, which Plasma runs at a new account's first
login and whenever the Global Theme page is told to apply "Desktop and window layout". It lays out
the desk named by `[Desk] Layout` in `kdosrc` (`kdos`, `dock` or `tiling`; anything else is the KDOS
desk) and never a fixed one, so applying any accent's global theme gives back the same panels. It
records the time it ran in `[Desk] LaidOut`. If the desk's template is missing it lays out Plasma's
default panel, so a screen is never left without one.

`kdos desk kdos|dock|tiling` changes the desk in a running session through plasmashell's own
scripting. Laying a desk out removes every panel, so the command first copies the applets file,
`~/.config/plasma-org.kde.plasma.desktop-appletsrc`, to the undo set, which `kdos desk undo` puts
back, and to a dated backup under `~/.local/share/kdos/desk-backup/` that nothing removes, and
prints the backup's path last. `kdos desk status --json` compares `LaidOut` with the applets
file's time to tell whether the panels were changed by hand since, which is how the welcome tour
knows to warn before it lays a desk over them. The installer writes the machine's choice into
`/etc/xdg/kdosrc`; `kdos desk` writes the person's into `~/.config/kdosrc`. The command is in
[The kdos command](kdos-command.md).

The tiling desk turns KWin's auto-tiler on and the other two turn it off, so a desk is a whole
arrangement and not half of one (see [Tiling](kwin.md#tiling)). The tiling template writes the
KWin key itself, which KWin reads at its next start; `kdos desk tiling` also runs
`kdos tiling on`, which applies it at once.

A desk template must be in the `panel` containment category, because Plasma finds templates only
through it, and must carry `"NoDisplay": true`, or Add Panel would list it. The build refuses a desk
missing either.

## The Start menu

Start is Plasma's Kickoff. The desks set its icon to `start-here-kdos` and, on the KDOS desk, its
label to Start, and give it five favourites: the web browser, Konsole, Dolphin, System Settings and
Discover. A setup script in every global theme, `plasmoidsetupscripts/org.kde.plasma.kickoff.js`,
gives any Kickoff added later the same mark and label, so one added by hand from Add Widgets does
not show KDE's logo instead.

Boxed applications are in the menu like any other: `kdos-appbox` writes a launcher for each,
`app.<id>.desktop`, and runs `kbuildsycoca6` (see [kdos-appbox](kdos-appbox.md)). The menu's
Library category lists the offline library's items ([below](#the-library-as-a-folder)).

## The system tray

The tray is Plasma's, with KDE's applets for networks, Bluetooth, audio, batteries, removable
devices, the clipboard, notifications and updates. KDOS adds two applets and configures one.

### Privacy lamps

`plasmoid-kdos-privacy`, the applet `org.kdos.privacy`, shows three lamps (microphone, camera and
screen), each only while something is capturing it, and names what is: "Firefox in box www".

| Action | Does |
|---|---|
| Click a lamp | Mutes every microphone capture, or unmutes them when all already are; stops every camera or screen capture that can be stopped |
| Middle click, or Show What Is Capturing in its menu | Opens the list, with a Mute or Stop button for each capture |
| Hover | A tooltip naming every capture |

What is capturing comes from `kdos privacy --watch --json`, which prints one line of JSON at once
and another each time a capture starts, stops or is muted. It reads PipeWire's streams for the
microphone and the screen, the holders of `/dev/video*` and PipeWire's camera nodes for the
camera, the desktop portal's screencast sessions, and each process's box from its cgroup scope.
The applet does not poll: it keeps the watcher running and starts it again when it exits. Mute and
Stop run `kdos privacy mute|unmute|stop <id>` for a capture the watcher listed, and only what the
watcher says can be done. While nothing captures the applet is passive and the tray hides it.

The applet is enabled by default in its notification-area category, which puts it in every tray
without a layout naming it. A line of a version the applet does not know is ignored, so a newer
watcher leaves the lamps dark rather than wrong. A camera that is captured without PipeWire and
without opening a `/dev/video*` node is not seen.

### Getting Started

`plasmoid-kdos-checklist` is the first-run checklist, a tray entry and the QML module
`org.kdos.checklist` that the welcome tour's last page also shows. Its items are: set up a backup,
pair your phone (optional), add a language, create your first box, check firmware, and confirm
disk encryption. Which of them apply to this machine, and whether each is done or dismissed, comes
from `kdos checklist --json`, which leaves out an item whose part of the system is not installed.
The probe runs every five seconds while an item is still to do, so creating a box ticks its row
within that, and not at all once every item is done or dismissed; the tray entry then hides
itself. Not Now runs `kdos checklist dismiss <id>`, Show Dismissed Items `kdos checklist restore`.

Each item's words and the program its button opens are in the module, keyed by the item's id, and
never in the probe's output, so nothing the probe prints is ever run. The buttons open System
Settings' Backups, Region & Language and [Boxes](kdos-settings.md#boxes) pages, KDE Connect,
Discover's updates and the security chapter of this book in the Help Center.

### Updates

Discover's notifier is the tray's update icon. KDOS's plugin for it reports a system update, the
known vulnerabilities it closes and a slot waiting for a restart; it never applies anything. It is
described in [Discover](discover.md#the-update-notifier).

## KRunner and the KDOS runner

KRunner is the search Alt+Space, Meta+Space, Alt+F2 and the Search key open, and the search field
of Kickoff and Overview. `kdos-runner` adds two runners to it. It is KDOS's own C, linked to basu
and SQLite and to nothing from Qt, and it speaks KRunner's D-Bus runner interface,
`org.kde.krunner1`, under the bus name `io.kdos.Runner`. Two files in
`/usr/share/krunner/dbusplugins/` declare it to KRunner, and `io.kdos.Runner.service` lets the
session bus start it at the first query; it exits after ten minutes without a call.

### The palette

The runner `kdos` (object `/runner`) is the KDOS command palette. It offers:

| Type | Example | Runs |
|---|---|---|
| A `kdos` verb, with its arguments filled in | `accent amber`, `phosphor on`, `tiling off`, `desk dock`, `night light`, `update check`, `doctor`, `box stop www`, `install gimp`, `why /etc/fstab` | `kdos …` or `kdos-box …` |
| A favourite, by its two letters | `FF` | the line of `~/.config/kdos/favorites` that carries `code=FF` |
| A route of `/etc/kdos/menu.conf` | the words of its name, or `route:<name>` exactly | the route's program |
| A KDOS settings page, by what it is for | `previous version` | System Settings at [System Versions](kdos-settings.md#system-versions) |
| A pack or `.ktar` set on the boot medium | `gimp` offers `app.gimp (on medium)` | Discover with `--local-filename`, under the `medium` origin |
| The current shortcuts | `keys <words>` | nothing; the rows show the chords as KGlobalAccel holds them now |
| A question for the local model | `ask what does a box grant?` | nothing; Enter copies the answer (see [Ask](#ask)) |

A verb that prints its answer (`update check`, `doctor`, `why`) opens in Konsole with `--hold`, and
`box enter` runs `konsole -e kdos-box enter <box> --shell`. A verb run in the background is watched
by a detached process, which posts the verb's error output as a notification when it fails, so a
refused `kdos theme` is never silent. The programs a row may start are `kdos`, `kdos-box`,
`kstart`, `systemsettings`, `plasma-discover`, `konsole`, `khelpcenter`, `zeal` and `xdg-open`;
none is a dependency, and a missing one fails only its row.

### The library

The runner `kdos-library` (object `/library`) searches the offline library: headings of this book
and *KDOS from Scratch* under `/usr/share/doc`, the `kdos*` manual pages, the entries of every Zeal
docset, a typed coordinate as a `geo:` link, title suggestions from every ZIM file `kiwix-serve`
serves on `127.0.0.1:8080`, and resources from Kolibri on `127.0.0.1:8081`. A plain query ranks
these below everything else; a query beginning with `?` asks the library alone. The book index is
built in a child process once per install of `kdos-doc` or `kdos-kfs` and kept in
`~/.cache/kdos/runner-books`; until it is written the library answers without the books. Place
names are not searched, because neither Organic Maps nor Marble has an offline index another
program can query.

### Ask

`ask <question>` in the palette asks the language model this machine serves. `llama-server` listens
on `127.0.0.1:8082` once a language-model pack has put a `.gguf` model under
`/usr/share/llama.cpp/models` (see [The local servers](../03-architecture/boot-and-init.md#the-local-servers)),
and the runner talks to it there and nowhere else: the address is loopback in the runner's code and
only the port can change. It asks `/v1/models` for the model to name, one already loaded or else
the first offered, since the server runs in router mode and refuses a request that names none, and
sends the question to `/v1/chat/completions` with a short instruction to answer in at most three
sentences and a cap of 256 tokens.

The question is sent once the typing stops: 600 ms after the last keystroke with no newer question,
and a newer question drops the one in flight. The answer is one row at the top, on several lines,
with the model's name under it; Enter or its **Copy the answer** action puts it on the clipboard
through Klipper. A reasoning model's `<think>` block is left out. A server that is not listening
gives the row "No local model is answering", and an error the server's own message.

The exchange has 30 seconds. KRunner stops waiting for a runner's answer after 25, so by 20 the row
reads "Still answering" and the exchange runs on; Enter on that row opens KRunner on the same
question, and an answer that arrived in the meantime, kept for ten minutes, is shown at once. A
timed-out or failed question is asked again the same way.

### Timing

KRunner sends a match on every keystroke. The palette answers from memory, parsing each file again
only when its identity (device, inode, size, modification time) changes, and the tests hold every
palette match under 50 ms. The library's two servers are asked over loopback without blocking, and
that match is answered when both have answered or 250 ms have passed, whichever is first, so a slow
server delays the library's rows and never the palette's. An `ask` holds only its own match, the
same way. A match stays good for ten seconds after
KRunner's teardown, because on Wayland KRunner sends the run request only once KWin has given it an
activation token, and the teardown it sends without waiting can arrive first.

The runner ids `kdos` and `kdos-library` are an interface: `kdos menu` and the tests query one
runner by id, and renaming either breaks every caller.

## Notifications

Plasma draws every notification, keeps their history and honours Do Not Disturb.
`org.freedesktop.Notifications` is plasmashell's; `kdos notify` posts through it (see
[The kdos command](kdos-command.md)).

### kdos-notice

`kdos-notice` tells the person at the session what the system knows and they do not. It is KDOS's
own C, a client of `org.freedesktop.Notifications` linked to basu, started by
`/usr/share/kdos/xdg/autostart/kdos-notice.desktop` in every Plasma session.

| Notice | From | Stays until dismissed |
|---|---|---|
| "Firefox ESR and Konsole were updated", or "The desktop was updated" with Log Out | `kdos restarts --json` | yes |
| "3 known vulnerabilities in installed packages" | `kdos cve --json` | yes |
| "Running KDOS 0.2.14 on trial" | `kdos-bootctl status --json` | no |
| "Your last update didn't start" | `kdos-bootctl status --json` | no |
| "firefox-esr in the box www was closed to free memory" | `status` on `/run/kdos-oomd.sock` | no |
| "3 applications chosen during install are not installed yet" | `/var/lib/kdos/apps-pending`, which the installer writes | yes |

A notice is posted once per change of what it is about. Each kind reduces its facts to a key (the
set of processes running replaced code, the set of findings, the boot and its slot, the failed
slot, the pending applications) and posts only when the key differs from the last one posted, which
`$XDG_STATE_HOME/kdos/notice` keeps across logins: the same vulnerability list is not announced at
every login, and a changed one replaces the old notification. A notice whose state ends is
withdrawn. A command that does not answer changes nothing. Deleting the state file announces
everything once more.

Nothing polls a command. An inotify watch on `/var/lib/kpkg/db` and on `/var/lib/kdos/update.json`
asks `kdos restarts` and `kdos cve` ten seconds after the last change, and one on
`/run/kdos/bootstate.json` asks `kdos-bootctl`. Only kdos-oomd's kill count is read on a timer,
one line every ten seconds. Everything, the installer's pending list included, is looked at
twenty seconds after login, every six hours,
and two seconds after a new owner takes `org.freedesktop.Notifications`, when the notices still
waiting for a person are posted again, because a restarted plasmashell lost them.

The actions are fixed argument vectors: Details opens Konsole on `kdos cve` or `kdos restarts`,
System Settings at System Versions, or Discover at "Selected during install", and Log Out asks Plasma's logout prompt over D-Bus. A signal
that claims to come from the notification server is acted on only when its sender owns
`org.freedesktop.Notifications` at that moment.

Run as `kdos-notice --bless` from `/etc/xdg/autostart/kdos-session-good.desktop`, the same binary
is the session's half of a confirmed boot: once `org.kde.plasmashell` has answered on the session
bus for a minute with no gap or restart, it calls `io.kdos.Slots1.Bless()` once and exits. How that
confirms a new system version is in [Discover](discover.md#restart-trial-and-confirmation).

### Sounds

Plasma plays a sound at login and logout only when the event names one, and nothing in Plasma
ships those events, so `/usr/share/kdos/xdg/plasma_workspace.notifyrc` names `desktop-login` and
`desktop-logout`. They are looked up in the sound theme `kdos`, which inherits Plasma's Ocean and
then the freedesktop theme. The Notifications page writes a person's choice to
`~/.config/plasma_workspace.notifyrc`.

## kded-kdos

`kded-kdos` is a kded6 module, `kf6/kded/kdos.so`, with three jobs.

**It keeps the accent in step with Plasma.** An accent chosen inside Plasma (a KDOS global theme on
the Global Theme page, a KDOS colour scheme on the Colours page, Quick Settings' light and dark
switch) changes the colour scheme and nothing outside Plasma. The module watches `kdeglobals
[General] ColorScheme` and, when it names a scheme KDOS ships (`KDOS<Title>` under
`/usr/share/color-schemes`) that differs from the accent in `~/.config/kdos/accent`, runs `kdos
theme --from-plasma <accent>`. That carries the choice to the terminal profiles, the Qt 5 palette,
the wallpaper, the lock and login screens, the boot menu and the console, and skips applying the
global theme, so the run cannot start another. A scheme that is not KDOS's changes nothing. A run
that has not finished after a minute is stopped; a run that fails raises one notification per
accent and reason in a session, with the last line the command printed.

**It offers the application sets on a stick.** When a removable volume is mounted, its top
directory is listed once for `*.ktar` files, the sets `kdos app export` writes. Up to three are
offered in one notification whose buttons open each in Discover, which imports it offline and
verifies every pack. The module imports nothing itself. Only a regular, non-hidden, non-empty file
ending in `.ktar` in lower case is offered, every name is escaped before it reaches the
notification's text, and the listing stops after 4096 entries.

**It starts Samba for the first shared folder.** `smbd` does not run until something is shared,
and a folder shared from Dolphin's Share tab is a file `net usershare` writes into
`/var/lib/samba/usershares`. The module watches that directory and, when it goes from holding no
share to holding one, calls `StartFileSharing()` on `kdos-powerd`'s `io.kdos.Power1`, which starts
Samba to serve it; see
[Starting Samba for a shared folder](daemons.md#starting-samba-for-a-shared-folder). Only a session
of a `sambashare` member can watch the directory, and only a member can share a folder.

Its state is on the session bus at `org.kde.kded6`, object `/modules/kdos`, interface
`io.kdos.Kded1`: `accent()`, `syncState()` (`idle`, `running` or `failed`), `lastError()` and
`resync()`, with the signals `accentSynced` and `accentSyncFailed`. Its three notifications are the
events `importOffer` ("Application set on a removable drive"), `syncFailed` ("Accent not applied
everywhere") and `shareStartFailed` ("Shared folder not served") in `kded_kdos.notifyrc`, so the
Notifications page lists them under KDOS and each can be silenced there. The
[KDOS Look](kdos-settings.md#kdos-look) page reads it. The module id is the bare word `kdos`, not a
dotted id, because kded registers each module at `/modules/<id>` and a dot is not allowed in an
object path. `kdos theme` is described in [The kdos command](kdos-command.md#kdos-theme).

## The welcome tour

The Welcome Center opens at a new account's first login. `kdos-welcome` adds KDOS's pages to it,
QML files in `/usr/share/plasma/plasma-welcome/extra-pages/`, and `intro-customization.desktop`,
which puts the KDOS mark and the "Plasma by KDE" credit on its first page.

| Page | Offers |
|---|---|
| Accessibility | The screen reader, text size, high contrast, zoom, reduced motion, sticky, slow and bounce keys, mouse keys and dwell click; shown first, before anything else is asked |
| Look | The accents, light and dark, Phosphor mode, Night Light, reduced motion |
| Desk | The three [desks](#the-desks), with undo and the backup's path |
| Your System | The two system slots and the version each holds |
| Keep Your Changes | Only on the live medium: keeping the session's changes in the free space on the stick |
| Software and Boxes | Discover, boxes and packs |
| Offline Library | What the library holds, and searching it |
| Keys | The KDOS chords and the key card |
| Privacy and Network | The privacy lamps, the firewall and what reaches the network |
| Help | This book and *KDOS from Scratch* in the Help Center |
| Getting Started | The [checklist](#getting-started) |
| What's New | Never part of the tour; opened after an update as `plasma-welcome --pages 90-WhatsNew.qml` |

The Accessibility page is numbered `00-`, and a patch to plasma-welcome shows pages numbered `00-`
to `09-` straight after the welcome page, before Plasma's own. Every switch on a page runs the
command that owns the setting (`kdos theme`, `kdos phosphor`, `kdos desk`, `kdos tiling`, `kdos
persist`, `kwriteconfig6 --notify` for KDE keys Plasma watches, `dbus-send` for the screen reader
and KWin's effects) through one shared runner, `kdos/Run.qml`, which refuses an empty argument or
one holding a space, because Welcome Center splits a command on spaces. A command that fails shows
Welcome Center's failure sentence, which ends with the command's last line of output. Phosphor mode
is offered only while neither a screen reader nor a magnifier is on, and is disabled with the
reason under software rendering.

On the live medium `/usr/share/kdos/xdg-live/plasma-welcomerc` sets `LiveEnvironment=true` and
`LiveInstaller=kdos-installer`, so the tour opens at every login on its live page, whose big icon
starts the installer ([kinstall](kinstall.md)).

## System Monitor

System Monitor and the meters on the KDOS desk read sensors from ksystemstats. `ksystemstats-kdos`
adds a sensor plugin, `ksystemstats/ksystemstats_plugin_kdos.so`, a sensor face called KDOS Meters,
and a System Monitor page. `/usr/share/kdos/xdg/systemmonitorrc` puts the KDOS page second, after
Overview, and the stock Applications page third, where each running box is one application.

### The sensors

The plugin publishes one container, `kdos`:

| Sensors | What they read |
|---|---|
| `kdos/box/<box>/{name,icon,cpu,memory,memoryLimit,ioRead,ioWrite,processes,energy}` | Each running box's cgroup scope, `app-kdos-<box>-<n>.scope`; the name and icon of the launcher whose `X-KDOS-Box` names the box |
| `kdos/box/all/{boxes,cpu,memory,ioRead,ioWrite,processes}` | The same, summed over every box |
| `kdos/energy/<app>[@<box>]/{name,app,box,impact,cpuTime,gpu}` and `kdos/energy/all/…` | `report-json` on `/run/kdos-energyd.sock`, from [kdos-energyd](daemons.md#kdos-energyd) |
| `kdos/slot/{status,running,runningVersion,runningState,other,otherVersion,otherState,next,trial,attempts}` | `/run/kdos/bootstate.json`, the copy of `kdos-bootctl status --json` every user may read |
| `kdos/look/phosphor` | `kwinrc [Plugins] kdos_crtEnabled`: 1 while Phosphor mode is on |

A box's figures are its scope's own `cpu.stat`, `memory.current`, `memory.max`, `io.stat` and
`pids.current`, which cgroup v2 sums over every descendant, so podman's container and conmon are
inside the totals. Only scopes the session's own user owns are read. A box started where no
delegated scope could be made has no scope and so no row; its limits are unenforced as well, which
`kdos-box profile` says. kdos-energyd is asked every ten seconds while an energy sensor is watched
and every minute while none is. The energy ledger is root's and `wheel`'s alone; for anyone else the
status sensor says so and no application sensors exist.

The plugin is built against libksysguard's sensor interface, which is not stable, so it is rebuilt
and tested again at every Plasma minor release.

### KDOS Meters and the KDOS page

KDOS Meters draws one row per sensor: its name, a sparkline in the accent and its figure in the
fixed-width face, over a faint scanline ground while `kdos/look/phosphor` reads 1. Its
`sensorPattern` setting follows every sensor whose id matches a regular expression as sensors come
and go, so a box started after the face loaded gets a row; a stock face resolves its list once,
when it loads. The KDOS page has four sections: Overview (this machine, all boxes, the system),
Boxes (processor, memory and disk per box), Energy (impact per application, and the ledger) and
Slots (the running slot and the other one). The three meters on the KDOS desk are KDE's processor,
memory and network applets drawn with the KDOS Meters face.

Acting on a process (ending it, changing its priority) is System Monitor's own Processes page, whose
helper asks polkit for an administrator's password before it touches another user's process.

### Boxes on the Applications page

The stock Applications page, and the Applications table on the Overview page, group processes by
the cgroup scope each application runs in. Upstream they read only
`/user.slice/user-<uid>.slice/user@<uid>.service`, the cgroup of a systemd user manager, which a
KDOS login does not have: elogind runs the login in `/<session>/session` and gives the user
`/<session>/delegated`, where each box's `app-kdos-<box>-<n>.scope` is made. libksysguard carries
two patches. One roots the page at `/<session>`, the parent of the cgroup System Monitor itself
runs in, whenever a `delegated` directory sits beside it; the other counts the processes of a
scope's descendant cgroups with the scope's own, which is where podman puts a box's container and
conmon. So each running box is one row with its processor, memory and process count, under its
launcher's name and icon for a box with one launcher and under the box's name for a box with
several, and every launch of one box shares that row. Programs started on the host share the
login's `session` cgroup and are not rows; they are on the Processes page.

## The library as a folder

`kio-library` serves `library:/`, a KIO folder whose five shelves list what is on this machine:

| Shelf | Holds | Found in |
|---|---|---|
| Books | ZIM archives, this book and *KDOS from Scratch* | `/var/lib/kiwix/library.xml`, Kiwix Desktop's library, `/usr/share/kiwix`, `~/.local/share/kiwix` |
| Maps | Organic Maps' `.mwm` files | `/usr/share/organicmaps/data`, `~/.local/share/OMaps` |
| Models | llama.cpp's `.gguf` models, whisper.cpp's `ggml-*.bin`, Piper's voices | `/usr/share/{llama.cpp,whisper.cpp,piper}/…` and the same under `~/.local/share` |
| Docsets | Zeal's `.docset` directories | `~/.local/share/Zeal/Zeal/docsets`, `/usr/share/zeal/docsets` |
| Courses | Kolibri's channels | `/var/lib/kolibri/content/databases` |

Each item shows its size and, in the Pack column, the data pack or package it came from; opening
it opens the file with the program the menu would choose. The worker writes nothing. A data pack
names its own items with a launcher carrying `Categories=X-KDOS-Library;`,
`X-KDOS-Library-Kind=`, `X-KDOS-Library-Path=` and `X-KDOS-Pack=`; that launcher is also the item's
entry in the menu's Library category, which `/etc/xdg/menus/applications-merged/kdos-library.menu`
adds. Which package owns an item is read from kpkg's database once and kept in
`~/.cache/kdos/library-owners` until the database changes.

`kdos-library-place`, run once per login from `/etc/xdg/autostart`, adds Library to the Places panel
of Dolphin and every file dialog the first time, and records that in `~/.config/kdos/library-place`,
so a person who removes the entry has it stay removed.

## Transcribe

`kdos-transcribe` adds Transcribe to the context menu of Dolphin and every KDE file dialog for audio
and video files. It runs `kdos speech transcribe --notify -- <files>`, which writes what is said into
a `.txt` file beside each one with whisper.cpp, offline, and posts a notification when it is done.
The entry is a file-item action plugin rather than a service menu, so it can decide each time the
menu opens: it appears only while `whisper-cli` is on the `PATH`, `kdos` is installed, every selected
item is a local audio or video file, and a speech model is where `kdos speech` looks for one.

## Add-ons

KDE's online "Get New…" store is switched off on every machine by
`[KDE Action Restrictions][$i] ghns=false` in the distribution's `kdeglobals`, so its buttons are
greyed out. In its place `kdos-addons` installs a curated set where Plasma already looks: four
wallpapers, eight colour schemes with matching Konsole schemes, two KWin scripts (one that keeps a
picture-in-picture video above every window, one that opens each new window on the screen under the
pointer) and two widgets (a clock in the fixed-width face, and a focus timer). Nothing in it is
switched on until a person picks it. Discover lists the set as an add-on of the Plasma shell.

## See also

- [The desktop](../02-user-guide/desktop.md): the panels, the menu, KRunner and the tray in use
- [KWin](kwin.md): windows, shortcuts, the key card and Phosphor mode
- [KDOS settings pages](kdos-settings.md): the settings pages these components open
- [Discover](discover.md): software, the update notifier and the confirmed boot
- [The session](../03-architecture/session.md): what starts these programs, and their environment
- [The kdos command](kdos-command.md): the verbs the runner, the tour and the applets run
- [Theming](../02-user-guide/theming.md): the global themes, the Plasma style and the accents
- [Writing desktop software](../05-developer/writing-desktop-software.md): writing a plasmoid, a
  runner or a kded module

<!-- book-nav -->
---

*Part IV — Programs, chapter 22.* Previous: [21. KWin](kwin.md) · [Contents](../README.md) · Next: [23. KDOS settings pages](kdos-settings.md)
