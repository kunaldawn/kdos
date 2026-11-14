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
the box, and leaves out Merkuro's contacts widget, a default-on tray applet whose loading starts
Akonadi and its agents in every session. The tray enables a default-on applet it has not seen, so
the layout script takes each of these out of the tray's `extraItems` and writes it into
`knownItems`, where the tray never enables it later. The KDOS desk also adds the input-method panel when the session's language is one of
those KDE adds it for.

A Plasma panel neither wraps nor scrolls. Short of room, it narrows every applet that can give
width toward its least: the task manager's launchers shrink, and the meters narrow their
sparklines, down to none, while every figure stays whole. What still does not fit is cut off at
the screen's edge, clock and Show Desktop first. Everything on the KDOS desk but the pinned
applications is about 69 grid units wide, roughly 1,240 logical pixels without the pinned
applications, which bring it to the 78-grid-unit threshold, so its script reads the screen's
logical width when it lays the panel out: under 78 grid units (1,400 logical pixels at the default
font) Start has no label and the network meter is left out, under 64 (1,150) the pager is left out
as well, and under 50 (900) nothing more gives way and the Dock or Tiling desk is the one that
fits. A change of scale, resolution or font size later does not lay the panel out again, and
the shrinking above is what then keeps it whole; `kdos desk kdos` lays it out for the new width.

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
prints the backup's path last. After laying a desk out it asks plasmashell, read-only, for each
panel's location and alignment and the plugins of its applets in order, and records a digest of
that in `[Desk] LaidOutPanels`. `kdos desk status --json` asks again and compares, so a panel
added, moved or removed, or an applet added, removed or reordered, counts as changed by hand, and
the state Plasma saves into the applets file for itself (a popup's size) does not; that is how the
welcome tour knows to warn before it lays a desk over them. A desk the first login laid out has
no digest yet, and the first `status` in a session records the panels as they are. The installer writes the machine's choice into
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
microphone, the holders of `/dev/video*` and PipeWire's camera nodes for the camera, KWin's
screencast streams in PipeWire for the screen (whether Spectacle or the desktop portal asked for
them), and each process's box from an `app-kdos-<box>-<n>.scope` in its cgroup path or, where it
has none, from the `KDOS_BOX=<box>` in its environment when it runs in a mount namespace other than
the session's. `kdos-appbox` sets that variable in every box's container and podman passes it to
every launch, so a store box and a pack box are named alike; the namespace test keeps a host
program that merely inherited the variable from a `kdos-box enter` shell from being named a box's.
The applet does not poll: it keeps the watcher running and starts it again when it exits. Mute and
Stop run `kdos privacy mute|unmute|stop <id>` for a capture the watcher listed, and only what the
watcher says can be done. While nothing captures the applet is passive and the tray hides it. In
the tray the popup's own header carries the title, and the list draws its own only on a panel or
the desktop.

The applet is enabled by default in its notification-area category, which puts it in every tray
without a layout naming it. A line of a version the applet does not know is ignored, so a newer
watcher leaves the lamps dark rather than wrong. A camera that is captured without PipeWire and
without opening a `/dev/video*` node is not seen.

### Getting Started

`plasmoid-kdos-checklist` is the first-run checklist, a tray entry and the QML module
`org.kdos.checklist` that the welcome tour's Getting Started page also shows. Its items are: set up a backup,
pair your phone (optional), add a language, create your first box, check firmware, and confirm
disk encryption. Which of them apply to this machine, and whether each is done or dismissed, comes
from `kdos checklist --json`, which leaves out an item whose part of the system is not installed or
that cannot be done here: disk encryption on the live medium, whose root is no disk.
The probe runs every five seconds while an item is still to do, so creating a box ticks its row
within that, and not at all once every item is done or dismissed; the tray entry then hides
itself. Not Now runs `kdos checklist dismiss <id>`, Show Dismissed Items `kdos checklist restore`.
Each row's mark is a read-only check box named by the item and takes the keyboard first in its row,
so a screen reader says the item and whether it is done before its buttons, and a done row, which
has no buttons, can still be reached. In the tray the popup's own header carries the applet's
title; the applet draws its own heading only on a panel or the desktop.

Each item's words and the program its button opens are in the module, keyed by the item's id, and
never in the probe's output, so nothing the probe prints is ever run. The buttons open System
Settings' Backups, Region & Language and [Boxes](kdos-settings.md#boxes) pages, KDE Connect,
Discover's updates and the security chapter of this book in the browser.

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
| A `kdos` verb, with its arguments filled in | `accent amber`, `accent next`, `accent prev`, `phosphor on`, `tiling off`, `desk dock`, `night light`, `reduced motion`, `update check`, `doctor`, `box stop www`, `install gimp`, `why /etc/fstab` | `kdos …` (`night light` is `kdos toggle night-light`) or `kdos-box …` |
| A favourite, by its two letters | `FF` | the line of `~/.config/kdos/favorites` that carries `code=FF` |
| A route of `/etc/kdos/menu.conf` | the words of its name, or `route:<name>` exactly; the row reads *Set up network* over `route:setup.network` | the route's program |
| A pack or `.ktar` set on the boot medium | `install gimp` offers `app.gimp (on medium)` | Discover with `--local-filename`, under the `medium` origin |
| The library alone | `lib photosynthesis` | nothing; KRunner opens again on the words with `kdos-library` alone (see [The library](#the-library)) |
| The current shortcuts | `keys <words>` | nothing; the rows show the chords as KGlobalAccel holds them now |
| A question for the local model | `ask what does a box grant?` | nothing; Enter copies the answer (see [Ask](#ask)) |

**The palette ranks first.** KRunner sorts its favourite runners' categories above every other
category, whatever a row's relevance, and `/usr/share/kdos/xdg/krunnerrc` names `kdos` first in
`[Plugins][Favorites] plugins`, ahead of KRunner's own sessions, power, applications and System
Settings runners. A pack on the boot medium therefore answers only after `install`: a bare `gimp`
would otherwise put the medium's pack above the installed GIMP. KDOS's own settings pages are not
palette rows; System Settings' runner finds them by the `X-KDE-Keywords` of their metadata
(`previous version` finds [System Versions](kdos-settings.md#system-versions)).

A verb that prints its answer (`update check`, `doctor`, `why`) opens in Konsole with `--hold`, and
`box enter` runs `kdos-box enter <box> --window`, the box's own Konsole window in its accent
profile. A verb run in the background is watched
by a detached process, which posts the verb's error output as a notification when it fails, so a
refused `kdos theme` is never silent. The programs a row may start are `kdos`, `kdos-box`,
`kstart`, `systemsettings`, `plasma-discover`, `konsole`, `man`, `zeal` and `xdg-open`;
none is a dependency, and a missing one fails only its row.

### The library

The runner `kdos-library` (object `/library`) searches the offline library: headings of this book
and *KDOS from Scratch* under `/usr/share/doc`, each opened in the browser at its heading, the
`kdos*` manual pages, each opened by `man` in Konsole, the entries of every Zeal
docset, a typed coordinate as a `geo:` link, title suggestions from every ZIM file `kiwix-serve`
serves on `127.0.0.1:8080`, and resources from Kolibri on `127.0.0.1:8081`. Its rows rank below
everything else, and `kdos-library` is not among krunnerrc's favourites. The palette's
`lib <words>` asks the library alone: its row calls KRunner's `querySingleRunner` with
`kdos-library` and the words, so KRunner opens again showing only the library. The book index is
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
| "This computer's hardware needs firmware-nvidia", with the device in the body and Install | `kdos firmware sync --dry-run --json` | yes |

A notice is posted once per change of what it is about. Each kind reduces its facts to a key (the
set of processes running replaced code, the set of findings, the boot and its slot, the failed
slot, the pending applications, the firmware ports named) and posts only when the key differs from the last one posted, which
`$XDG_STATE_HOME/kdos/notice` keeps across logins: the same vulnerability list is not announced at
every login, and a changed one replaces the old notification. A notice whose state ends is
withdrawn. A command that does not answer changes nothing. The file is read when kdos-notice
starts, so deleting it announces everything once more at the next login.

Nothing polls a command. An inotify watch on `/var/lib/kpkg/db`, which sees a package entry
written, created, moved in or deleted, and on `/var/lib/kdos/update.json` asks `kdos restarts`, `kdos cve` and `kdos firmware sync
--dry-run` ten seconds after the last change, so the firmware notice is withdrawn once its ports
are installed, and one on
`/run/kdos/bootstate.json` asks `kdos-bootctl`. Only kdos-oomd's kill count is read on a timer,
one line every ten seconds. Everything, the installer's pending list and the firmware included, is looked at
twenty seconds after login, every six hours,
and two seconds after a new owner takes `org.freedesktop.Notifications`, when the notices still
waiting for a person are posted again, because a restarted plasmashell lost them.

The actions are fixed argument vectors. Details on the vulnerability notice opens Konsole on
`kdos cve`, and on the trial and rollback notices opens System Settings at System Versions. Show
Programs on an updated-programs notice opens Konsole on `kdos restarts`. Open Discover on the
pending-applications notice opens Discover at "Selected during install". Install on the firmware
notice opens Konsole on `sudo kdos firmware sync`. The dry run behind the notice runs as the
person logged in, who cannot read the kernel log, so it names what the devices' drivers declare;
a failed load only the log records is found by `sudo kdos firmware sync --dry-run` (see
[kdos firmware](kdos-command.md#kdos-firmware)). Log Out asks Plasma's
logout prompt over D-Bus. A signal
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

`kded-kdos` is a kded6 module, `kf6/kded/kdos.so`, with four jobs.

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

**It starts Samba for a login that can share folders.** `smbd` must run before Dolphin's Share
tab can make the first share, because `net usershare add` asks it to turn the share's user names
into SIDs. The boot starts it once the `sambashare` group has a member; an account just added to
the group is a member from its next login. So in a session that can write into
`/var/lib/samba/usershares`, which only a member's can, the module calls `StartFileSharing()` on
`kdos-powerd`'s `io.kdos.Power1` once when it loads, which starts Samba if it is not running; see
[Starting Samba for a shared folder](daemons.md#starting-samba-for-a-shared-folder).

**It starts plasmashell again when it stops**, as
[How plasmashell is kept running](#how-plasmashell-is-kept-running) describes.

Its state is on the session bus at `org.kde.kded6`, object `/modules/kdos`, interface
`io.kdos.Kded1`: `accent()`, `syncState()` (`idle`, `running` or `failed`), `lastError()` and
`resync()`, with the signals `accentSynced` and `accentSyncFailed`. Its four notifications are the
events `importOffer` ("Application set on a removable drive"), `syncFailed` ("Accent not applied
everywhere"), `shareStartFailed` ("Folder sharing unavailable") and `shellRespawnLimit` ("Desktop
shell keeps stopping") in `kded_kdos.notifyrc`, so the
Notifications page lists them under KDOS and each can be silenced there. The
[KDOS Look](kdos-settings.md#kdos-look) page reads it. The module id is the bare word `kdos`, not a
dotted id, because kded registers each module at `/modules/<id>` and a dot is not allowed in an
object path. `kdos theme` is described in [The kdos command](kdos-command.md#kdos-theme).

### How plasmashell is kept running

plasma_session starts plasmashell once, from its autostart entry, and the session has no service
manager to start it again. KCrash restarts it after a crash signal, but plasmashell turns Qt's
Wayland reconnection off for itself, so when KWin crashes or is replaced it exits normally, and a
`kill -9` or `kquitapp6 plasmashell` leaves nothing to restart it either. kded6 reconnects to the new compositor and
outlives it.

So the module watches `org.kde.plasmashell` on the session bus. When the name has been gone for
three seconds, long enough for `plasmashell --replace` or KCrash's own restart to take it back, it
asks ksmserver's `isShuttingDown()`. If the session is not ending it starts `plasmashell`, and looks
again twenty seconds later in case the new one exited before taking the name. A logout, and a
ksmserver that does not answer, start nothing. It starts plasmashell at most five times in a
minute; past that it raises one notification, `shellRespawnLimit` ("Desktop shell keeps stopping"),
and leaves the desktop to the person, whose way back is `kstart plasmashell` or logging out and in.
A second copy started while one is coming up is harmless: plasmashell is a unique D-Bus service and
the later one exits.

## The welcome tour

The Welcome Center opens at a new account's first login. `kdos-welcome` adds KDOS's pages to it,
QML files in `/usr/share/plasma/plasma-welcome/extra-pages/`, and `intro-customization.desktop`,
which puts the KDOS mark and the "Plasma by KDE" credit on its first page.

| Page | Offers |
|---|---|
| Accessibility | Switches for the screen reader, reduced motion, sticky, slow and bounce keys, mouse keys and dwell click; the Fonts page for text size, the two high-contrast accents and the zoom chords; shown straight after the welcome page (and the live page on the live medium), before anything else is asked |
| Look | The accents, light and dark, Phosphor mode, Night Light, reduced motion |
| Desk | The three [desks](#the-desks), with undo and the backup's path |
| Your System | The two system slots and the version each holds; on a one-slot install, that an update is applied over the running system with `kdos update apply --in-place` and there is no previous version; on the live medium, that it has no slots |
| Keep Your Changes | Only on the live medium: keeping the session's changes in the free space on the stick, or on another USB stick |
| Software and Boxes | Discover, boxes and packs |
| Offline Library | What the library holds, and searching it |
| Keys | The KDOS chords and the key card: for each action the chord KDOS teaches while it is bound, otherwise the first bound chord with a modifier |
| Privacy and Network | The privacy lamps, the firewall and what reaches the network |
| Help | This book and *KDOS from Scratch* in the browser |
| Getting Started | The [checklist](#getting-started) |
| Make It Yours | The groups of programs of `/ports/groups`, from `kdos update groups --json`: each one installed or not, and what installing it downloads; ticked ones are installed prebuilt, one at a time, with a line saying which and how far; then Discover for everything else. The tour's last page; see [Make It Yours](#make-it-yours) |
| What's New | Never part of the tour; opened after an update as `plasma-welcome --pages 90-WhatsNew.qml` |

The Accessibility page is numbered `00-`, and a patch to plasma-welcome shows pages numbered `00-`
to `09-` straight after the welcome page, before Plasma's own. Every switch on a page runs the
command that owns the setting (`kdos theme`, `kdos phosphor`, `kdos desk`, `kdos tiling`, `kdos
persist`, `kwriteconfig6 --notify` for KDE keys Plasma watches, `dbus-send` for the screen reader
and KWin's effects) through one shared runner, `kdos/Run.qml`, which refuses an empty argument or
one holding a space, because Welcome Center splits a command on spaces. A command that changes a
setting greys the page's controls until it exits; a program that stays open, such as a settings
page, Discover or the browser, is started without that, so the page stays usable while it is
open. A command that fails shows Welcome Center's failure sentence, cut to the sentence and the
command's last line of output. The Accessibility, Look and Keys pages read their settings again
whenever they are shown and whenever the window comes back to the front. Phosphor mode is offered
only while neither a screen reader nor a magnifier is on, and is disabled with the page's own
sentence for the reason under software rendering.

On the live medium `/usr/share/kdos/xdg-live/plasma-welcomerc` sets `LiveEnvironment=true` and
`LiveInstaller=kdos-installer`, so the tour opens at every login on its live page, whose big icon
starts the installer ([kinstall](kinstall.md)).

### Make It Yours

`95-MakeItYours.qml` reads `kdos update groups --json` when it is made and after every install,
and shows one row a group: its title and, under it, "Installed", or how many packages installing it
adds and their download size. A group is offered only when it is not installed and has a size: the
command gives a null size when the release's signed binhost index is not in `/var/lib/kdos/binhost`
or when some package of the group is not prebuilt, and a prebuilt install refuses both. When no
group has a size the page says the package list is not on the computer yet, and asks for a
connection and **Check for Updates** in Discover, which fetches it, with **Open Discover** and
**Look Again** buttons; it has no way to fetch the index itself, which needs root. With the index
in place, a group with no size has a package that is not prebuilt, and is left to Discover, which
can build it here.

The install button names the number of ticked groups and their total size. Each group is installed
by `pkexec /usr/libexec/kdos-discover/kdos-ports-helper install <id>`, Discover's prebuilt helper
(`kdos update install --prebuilt <id>`, which installs each member of a group as the port it is;
see [Install routes and groups](kdos-command.md#install-routes-and-groups)), under the polkit action
`io.kdos.ports.install`, whose grant is kept, so one password covers the queue. The groups go one
at a time, with "Installing *title* (*n* of *m*)…" and a progress bar; at the end the page lists
what went in and, for each group that did not, the helper's last line. The rules (which rows are
offered, the size, the queue) are `kdos/groups.js`, tested by `tests/tst_groups.qml.in` under
`qmltestrunner`. On the live medium the page adds that an install runs from memory and is gone at
the next start.

## System Monitor

System Monitor and the meters on the KDOS desk read sensors from ksystemstats. `ksystemstats-kdos`
adds a sensor plugin, `ksystemstats/ksystemstats_plugin_kdos.so`, a sensor face called KDOS Meters,
and two System Monitor pages, Overview and KDOS. `/usr/share/kdos/xdg/systemmonitorrc` opens System
Monitor on that Overview, puts the KDOS page second and the stock Applications page third, and
hides the stock Overview. The stock page asks for
per-disk, per-interface and GPU sensors, which the live medium (its root is a RAM overlay with no
block device), a virtual machine and a graphics card ksystemstats cannot read do not have, and a
page that names a missing sensor opens under a "missing some sensors" banner. KDOS's Overview
reads the disk and network totals instead and follows `gpu/gpu<n>/usage`, so it shows each card
that reports its use and nothing where none does.

### The sensors

The plugin publishes one container, `kdos`:

| Sensors | What they read |
|---|---|
| `kdos/box/<box>/{name,icon,cpu,memory,memoryLimit,ioRead,ioWrite,processes,energy}` | Each running box's cgroup scope, `app-kdos-<box>-<n>.scope`, or, for a box with none, its processes: those of the session's user, in a mount namespace other than the plugin's, whose environment carries `KDOS_BOX=<box>`; the name and icon of the launcher whose `X-KDOS-Box` names the box |
| `kdos/box/all/{boxes,cpu,memory,ioRead,ioWrite,processes}` | The same, summed over every box |
| `kdos/energy/<app>[@<box>]/{name,app,box,impact,cpuTime,gpu}` and `kdos/energy/all/…` | `report-json` on `/run/kdos-energyd.sock`, from [kdos-energyd](daemons.md#kdos-energyd) |
| `kdos/slot/{status,running,runningVersion,runningState,other,otherVersion,otherState,next,trial,attempts}` | `/run/kdos/bootstate.json`, the copy of `kdos-bootctl status --json` every user may read |
| `kdos/look/phosphor` | `kwinrc [Plugins] kdos_crtEnabled`: 1 while Phosphor mode is on |

A box's figures are its scope's own `cpu.stat`, `memory.current`, `memory.max`, `io.stat` and
`pids.current`, which cgroup v2 sums over every descendant, so podman's container and conmon are
inside the totals. Only scopes the session's own user owns are read. No box gets a scope on KDOS:
elogind leaves `/sys/fs/cgroup/<session>/cgroup.procs` root's, cgroup2 then refuses the move into
the delegated subtree, and every box runs in the login's `session` cgroup. Such a box's figures are
its processes' sums instead: CPU from each one's `utime`, `stime` and reaped children's time in
`/proc/<pid>/stat`, memory from its proportional set size (`Pss` in `smaps_rollup`), disk from
`read_bytes` and `write_bytes` in `/proc/<pid>/io`, and `memoryLimit` 0, because nothing enforces
its profile's limits, which `kdos-box profile` says. A process that exits takes its CPU time out
of the sum, and that sample shows no CPU or disk rate. See
[Boxes are measured by their processes, and their limits are unenforced](../06-reference/known-gaps.md#boxes-are-measured-by-their-processes-and-their-limits-are-unenforced). kdos-energyd is asked every ten seconds while an energy sensor is watched
and every minute while none is. The energy ledger is root's and `wheel`'s alone; for anyone else the
status sensor says so and no application sensors exist.

The plugin is built against libksysguard's sensor interface, which is not stable, so it is rebuilt
and tested again at every Plasma minor release.

### KDOS Meters and the KDOS page

KDOS Meters draws one row per sensor: its name, a sparkline in the accent and its figure in the
fixed-width face, over a faint scanline ground while `kdos/look/phosphor` reads 1. A figure is as
wide as the widest value its unit formats to, so it neither clips nor shifts as it changes; a text
sensor has no sparkline and its text wraps after the name. The name is at most eight grid units
wide, the same in every row, so the sparklines of a stacked list start and end together, and the
sparkline takes the rest of the row's width. A narrow row, such as a third-width card on the KDOS
page, narrows the sparkline to two grid units first and then the name, which elides; the figure
stays whole. The face asks for the height of its rows, so a page row or a desktop widget holding
it is tall enough for every row, and the KDOS
page's rows take their faces' height and the page scrolls. Opened from a panel, the face is a
popup chart: 24 grid units wide, each sparkline four grid units high, name and figure at the top of
its row, and the sparklines share whatever height the popup has, so a popup resized larger, whose
size Plasma remembers, draws larger lines. In a horizontal panel the face drops the
names and puts a meter's sensors side by side, one sparkline and figure each, with ↓ and ↑ before
a download and upload (or read and write) figure, and the applet is as wide as they are. Its least
width is the same without the sparklines: a panel short of room narrows them, and one narrower
than a grid unit is not drawn, so the figures stay whole and the task manager is not left to
give up all the width alone. Its
`sensorPattern` setting follows every sensor whose id matches a regular expression as sensors come
and go, so a box started after the face loaded gets a row; a stock face resolves its list once,
when it loads. The KDOS page has four sections: Overview (this machine, all boxes, the system),
Boxes (processor, memory and disk per box), Energy (impact per application, and the ledger) and
Slots (the running slot and the other one), every card a KDOS Meters face. The three meters on the
KDOS desk are KDE's processor, memory and network applets drawn with the KDOS Meters face; the
network meter is its download and upload side by side. The desk names each meter's total sensor
(processor usage, used memory, download rate; the Tiling desk's two meters the same), which is
what the applet's tooltip shows and a screen reader speaks on the panel button: "Open" followed by
the sensor's name and value. An applet with no total sensor, as the stock network one is, speaks
"Open" alone.

Acting on a process (ending it, changing its priority) is System Monitor's own Processes page, whose
helper asks polkit for an administrator's password before it touches another user's process.

### Boxes on the Applications page

The stock Applications page, and the Applications table on the Overview page, group processes by
the cgroup scope each application runs in. Upstream they read only
`/user.slice/user-<uid>.slice/user@<uid>.service`, the cgroup of a systemd user manager, which a
KDOS login does not have: elogind runs the login in `/<session>/session` and gives the user
`/<session>/delegated`. libksysguard carries two patches. One roots the page at `/<session>`, the
parent of the cgroup System Monitor itself runs in, whenever a `delegated` directory sits beside
it; the other counts the processes of a scope's descendant cgroups with the scope's own, which is
where podman puts a box's container and conmon. With them, a box in an
`app-kdos-<box>-<n>.scope` under `delegated` would be one row under its launcher's name and icon.
No row results on KDOS: elogind leaves `/<session>/cgroup.procs` root's, so `kdos-appbox` cannot
move a box into a scope, and every box, like every program started on the host, runs in the
login's `session` cgroup, which is no application. The page is empty and every process is on the
Processes page. See
[Host applications and boxes reach the portals unidentified](../06-reference/known-gaps.md#host-applications-and-boxes-reach-the-portals-unidentified)
and [System Monitor's Applications page lists nothing](../06-reference/known-gaps.md#system-monitors-applications-page-lists-nothing).

## The library as a folder

`kio-library` serves `library:/`, a KIO folder whose five shelves list what is on this machine:

| Shelf | Holds | Found in |
|---|---|---|
| Books | ZIM archives, this book and *KDOS from Scratch* | `/var/lib/kiwix/library.xml`, Kiwix Desktop's library, `/usr/share/kiwix`, `~/.local/share/kiwix` |
| Maps | Organic Maps' `.mwm` files | `/usr/share/organicmaps/data`, `~/.local/share/OMaps` |
| Models | llama.cpp's `.gguf` models, whisper.cpp's `ggml-*.bin`, Piper's voices | `/usr/share/{llama.cpp,whisper.cpp,piper}/…` and the same under `~/.local/share` |
| Docsets | Zeal's `.docset` directories | `~/.local/share/Zeal/Zeal/docsets`, `/usr/share/zeal/docsets` |
| Courses | Kolibri's channels | `/var/lib/kolibri/content/databases` |

Each item shows its size; opening it opens the file with the program the menu would choose. The
data pack or package an item came from is the worker's one extra field, `Pack` (`ExtraNames` in
its metadata); Dolphin shows a worker's extra fields only for `trash:/`, so no Dolphin column
carries it. The worker writes nothing. A data pack
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
`[KDE Action Restrictions][$i] ghns=false` in the distribution's `kdeglobals`: the settings pages
show no Get New button, the wallpaper dialog's Get New Plugins is greyed, and Add Widgets' Get New
menu holds only Install Widget From Local File. In its place `kdos-addons` installs a curated set where Plasma already looks: four
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
