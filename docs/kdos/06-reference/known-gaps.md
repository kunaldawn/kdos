# Known gaps

This chapter lists what KDOS does not do, and what it does but has never been exercised on real
hardware or against a real peer. It is for anyone using, administering or contributing to KDOS who
has hit something that seems missing and wants to know whether it is a bug, a limitation or a
choice. If what you are looking for is described here, KDOS does not do it, or has not been shown
to; if it is not described here and it misbehaves, it is probably a bug worth reporting. No earlier
chapter is required reading, but [Architecture overview](../03-architecture/overview.md) gives
context for the component names used below, and terms such as *KWin*, *box*, *pack* and *rig*
are defined in the [Glossary](glossary.md).

Each entry is headed by a one-line statement of the gap, followed by what exactly is missing, why,
and what to do instead where there is something. Some entries are known defects: behaviour the
code has today that contradicts its own documentation or usage text. They are listed so that a
reader who meets one knows it is known. The sections follow the shape of the system: the desktop,
applications and boxes, hardware, installation, boot and updates, security, ports built without a
feature, building, sources and publishing, testing, and this book. Every entry describes the tree
as it stands; a gap that closes is removed rather than marked as closed (see
[Principles](../01-philosophy/principles.md#documentation-describes-the-present)).

Three other chapters answer the neighbouring questions. What is absent on purpose, and the
reasoning behind it, is in [Decisions](../01-philosophy/decisions.md); what that costs next to
other distributions is in [How KDOS differs](../01-philosophy/how-kdos-differs.md#what-you-give-up);
how mature the parts that do exist are is in [Status](status.md).

## Desktop

### The login screen runs on a Wayland greeter upstream calls experimental

SDDM's greeter runs on its own `kwin_wayland`, the way `/etc/sddm.conf.d/00-kdos.conf` asks. SDDM
itself says its Wayland support is experimental, and Plasma's build marks the Wayland session file
for SDDM as not yet for final users. KDOS has no X server to fall back to. That the greeter starts,
logs a person in and out, switches user, locks and survives a suspend has not been shown on real
hardware. See [Boot and init](../03-architecture/boot-and-init.md).

### A greeter that keeps failing is not seen by `kdos-dm`

`kdos-dm` gives a password prompt on `tty1` when SDDM is missing or exits three times in a minute.
A greeter that crashes is restarted by SDDM itself, which stays up, so a greeter that never draws
leaves a black `tty1` and nothing for `kdos-dm` to see. `tty2` is the recovery console for that:
`Ctrl+Alt+F2`, log in, and read `/var/log/sddm.log`.

### The screen reader at the login screen has no sound

`Meta+Alt+S` at the greeter starts Orca, as it does in a session. The greeter's session bus brings
up the accessibility bus, the AT-SPI registry and speech-dispatcher with it, but the `sddm`
account runs no PipeWire, and ALSA's default device is PipeWire's, so speech-dispatcher cannot
open an audio device and Orca says nothing. Inside a Plasma session Orca speaks: it reads System
Settings, Dolphin and the window `Alt+Tab` switches to. See
[Accessibility](../02-user-guide/accessibility.md).

### The overview is silent under the screen reader, and the lock screen only partly read

KWin's overview (`Meta+W`) draws its windows, desktops and search field in a scene inside KWin that
publishes nothing on the accessibility bus, so Orca echoes the keys pressed there and says nothing
else. The lock screen publishes its password field and its Show Password, Virtual Keyboard and user
buttons, which Orca reads, but `Tab` also stops on one control that Orca announces with nothing,
and never reaches the Sleep and Switch User buttons. Both are upstream behaviour of KWin and of the
screen locker; nothing in KDOS changes them. See
[The screen reader](../02-user-guide/accessibility.md#the-screen-reader).

### Phosphor mode needs hardware OpenGL

The CRT pass is a KWin effect that refuses to load on software rendering (llvmpipe), where a
full-screen pass every frame would be the desktop's whole budget. A virtual machine with plain
graphics, the rig among them, cannot turn Phosphor mode on: `kdos phosphor on` refuses before it
changes anything. `make run-hw` and a GPU passed through to a guest can. An output in HDR gets the
plain picture. With several outputs and the effect's full-screen pass off, while a full-screen
window is active on one output, KWin may put the cursor or an overlay on a hardware plane above the
pass on the others. None of it has been photographed. See
[Phosphor mode](../04-programs/kwin.md#phosphor-mode).

### A login that typed no password leaves the wallet locked

The session's Secret Service, `ksecretd`, keeps saved passwords for KDE programs and for anything
using libsecret or QtKeychain. `pam_kwallet5` unlocks it with the password typed at the login
screen. An automatic login types none, so the first program to ask for a saved secret makes the
wallet ask for its password once. The live medium turns the wallet off
(`/usr/share/kdos/xdg-live/kwalletrc`), so a live session keeps no secrets at all: nothing
provides `org.freedesktop.secrets` there, so `secret-tool`, KeepSecret and any other libsecret or
QtKeychain program, and the secret portal, report that the service is missing, and KWallet Manager
offers only to turn the wallet on. See
[The security model](../03-architecture/security-model.md).

### Some programs open their first window at their minimum size

A KXmlGui program with no saved window size sizes its first window from its widgets' minimum size
hints, and on Wayland nothing makes it larger: Kapman opens at 200x178, Bomber at 453x228 and
KNetWalk at 396x141, and KDiskFree, Shotcut and digiKam are small too. The window memory keeps
whatever size the window closes at, so a person who enlarges the window once has it enlarged from
then on; until then, it opens at the program's size. See
[Placement](../03-architecture/window-model.md#placement).

### System Monitor's Applications page lists nothing

Plasma's System Monitor groups processes into applications by the cgroup scope each launch runs
in, and no process on KDOS is in one. A program started on the host has none: without a systemd
user manager nothing gives a launch a scope of its own. A box has none either (see
[Host applications and boxes reach the portals
unidentified](#host-applications-and-boxes-reach-the-portals-unidentified)). Every host program
and every box shares the login's `session` cgroup, so the Applications page, and the Applications
table on the Overview page, stay empty, and every process is on the Processes page only. See
[Boxes on the Applications page](../04-programs/plasma-shell.md#boxes-on-the-applications-page).

### The energy sensors are empty for an account outside `wheel`

`kdos-energyd` serves its ledger to root and `wheel` only. System Monitor's KDOS energy sensors ask
it as the person logged in, so for anyone else they stay empty; widening the tier is a decision
about how much of the processor's energy counter, a side channel, other accounts may see.

### A crashed program shows no crash dialog

KDE's crash handler hands a crash to DrKonqi, which is not a port. A KDE program that crashes is
restarted where its handler restarts it, and nothing offers a backtrace or a report. Nothing
collects a core either: the core-file limit is the kernel's default of zero, so a crash leaves only
the kernel's `potentially unexpected fatal signal` line in `/var/log/messages` and whatever the
program printed to the session log. To collect one, run `ulimit -c unlimited` in a shell and start
the program from it; the core is written to its working directory.

### Session restore reopens applications, not their documents

With the session set to restore the previous one, the default, the next login starts each
application that had a window open, from its desktop entry and with no arguments: no documents,
tabs or window positions come back, and an application with several windows gets one. A window
whose program Plasma cannot match to a desktop entry is not restored at all. Wayland applications
do not speak the X11 session protocol, so ksmserver saves none of them, and Plasma's fallback
restore records only the desktop entries; see
[Ending a session](../03-architecture/session.md#ending-a-session). Boxes and boxed applications
come back through KDOS's own record.

### Non-KDE programs draw Breeze's icons in Breeze's blue

The KDOS icon themes are the KDOS marks over Breeze, and Breeze's symbolic icons follow the colour
scheme only in programs that load them through KDE's icon loader. A GTK, libadwaita or plain Qt
program draws them in the blue baked into the files. The KDOS marks themselves are raster images
and keep their colours under every accent.

### A program started with `pkexec` reads none of KDOS's defaults

`pkexec` clears the environment of the program it starts, so `XDG_CONFIG_DIRS` is unset and a KDE
program run as root that way reads `/etc/xdg` alone, never `/usr/share/kdos/xdg`: it gets KDE's
own defaults, not KDOS's fonts, colours and icons. No KDOS program starts a window that way; the
root work of System Settings, Discover and the installer runs in helpers that draw nothing. A
program a person starts with `pkexec` looks like stock KDE.

### Qt 5 programs look like Qt 5, not like Breeze

KDE's platform theme and the Breeze widget style exist for Qt 6 only. A Qt 5 program (VLC,
KeePassXC, LMMS, Mumble and the rest) loads qt5ct under the same platform theme name, `kde`, and
draws in the Fusion style with the accent's palette, the KDOS icons and the desktop's fonts, which
`kdos theme` keeps in step; its widgets are Fusion's shapes, not Breeze's.

### Tk, FLTK and Java programs keep their own colours

The accent reaches Qt 6 and KDE programs through the platform theme, GTK programs through
Plasma's GTK bridge and Qt 5 programs through qt5ct. Nothing writes colours for any other
toolkit: Tk programs (`wish` and the Tk front ends), FLTK programs (flrig, fldigi) and Java
programs drawn with Swing or AWT (Digital among them) show their toolkit's own light-grey
defaults in every accent, `paper` included. Tk and FLTK read X resources at start under Xwayland,
and KDOS writes none; Swing would need a look and feel that follows a colour file, which no
shipped package provides.

### *Get New…* downloads nothing

KDE's add-on downloads are switched off for the whole system: `ghns=false` is locked in
`kdeglobals`. Each surface shows it its own way: the settings pages have no *Get New…* button, the
wallpaper dialog's *Get New Plugins…* is greyed, and the *Get New…* menu of Add Widgets holds only
*Install Widget From Local File…*. Wallpapers, colour schemes, KWin scripts and
widgets come from the KDOS add-ons on the image instead; see
[Add-ons](../04-programs/plasma-shell.md#add-ons) and
[Decisions](../01-philosophy/decisions.md#nothing-downloads-at-run-time).

### There is no KDOS widget style

The Union style engine is built, and no style is written for it: the widget style stays Breeze in
KDOS's colours.

### Read Aloud appears in the clipboard's actions only after `kdos speech sync`

Klipper's actions have no condition, so the Read Aloud action is added to your `klipperrc` while a
voice is installed and removed when the last one goes, by `kdos speech sync`, which
`kdos speech voice get` and `voice remove` run and its autostart entry runs at each login. Klipper
reads the file when the clipboard applet starts, so a voice fetched in the middle of a session
reaches Klipper at the next login.

### Emoji draw everywhere except the text console on `tty1`

Noto Color Emoji ships, and fontconfig resolves the generic `emoji` family to it, so every program
on the desktop draws them. The Linux virtual terminal's font holds 512 monochrome glyphs, which is a
kernel limit, and a character the font does not carry renders as a blank cell. An emoji on `tty1`
or `tty2` is therefore a blank cell.

### A fingerprint unlocks `sudo` and the lock screen, and nothing else

`pam_fprintd` is in `sudo`'s PAM stack ahead of the password, and in the lock screen's
`kde-fingerprint` stack, which the lock screen offers once a finger is enrolled. The login screen
and the console login ask for the password only. Enrolment is System Settings' Users page or
`fprintd-enroll`. For an account with a fingerprint enrolled, `sudo` over SSH waits on the
machine's reader before asking for the password: `pam_fprintd` is built against basu, which has no
login-session queries, so its only remote-login test is `PAM_RHOST`, which `sudo` never sets. No
reader has been used on this system.

### X11 programs other than GTK ones get no input method

fcitx5 is built without the XIM front end X clients use, and `fcitx5-qt` without its X11 part, so
an application under Xwayland gets no input method unless it is a GTK 3 or GTK 4 program on the
host, which loads `fcitx5-gtk`'s module and reaches fcitx5 over D-Bus. In a box no X client has
one. A Qt 6, GTK 3 or GTK 4 application on Wayland reaches fcitx5 through KWin's
`text-input-v3`; pinyin has been typed into a GTK 4 entry (`zenity --entry`) and into AbiWord, a
GTK 3 program, on the rig. A Qt 5 application on Wayland speaks only text-input-unstable-v2, which
KWin also serves; that has not been tried. See
[the session](../03-architecture/session.md#input-methods).

### AbiWord's candidate window opens at the window's corner

AbiWord's document view gives the input method no cursor position, so fcitx5 draws the candidate
list at the top-left of AbiWord's window, over the menu bar, while the text being composed shows
in the page. The text is typed correctly. Fixing it is AbiWord's; `GTK_IM_MODULE` stays unset
host-wide (see [the session](../03-architecture/session.md#input-methods)).

### A GTK program that builds its own file dialog keeps GTK's

A GTK 4 program asks the file-chooser portal for its Open and Save dialogs, and so does a GTK 3
program that uses GTK's native chooser, which `GTK_USE_PORTAL=1` sends to the portal; both get
KDE's dialog. A GTK 3 program that builds GTK's own dialog widget never asks the portal: AbiWord's
Open dialog is GTK's. Nothing short of changing the program moves it.

### A corporate VPN is a terminal program

`openconnect` ships and speaks AnyConnect, GlobalProtect, Fortinet and Pulse, with `vpnc-script`
giving the tunnel its routes, but NetworkManager's openconnect plugin is not a port, so the Networks
applet and System Settings offer no such connection. Neither is its vpnc plugin. WireGuard and
OpenVPN are the VPN types that appear there.

### Nerd Font icons are blank on `tty1`, and the shipped configurations turn them off everywhere

They are private-use characters, and the console font cannot carry them, for the reason given under
emoji above, so an icon in front of a filename shows as a hole. The face itself is on the image:
`nerd-fonts-symbols` installs Symbols Nerd Font and its Mono variant under
`/usr/share/fonts/nerd-fonts`, and its `66-nerd-font-symbols.conf` accepts the Mono face as a
fallback for `monospace`, `Terminus` and `Terminus (TTF)` and the proportional face for `sans-serif`
and `serif`, so Konsole can draw one. What turns them off is each program's own configuration: the
`yazi` theme that `kdos theme` generates empties all five `[icon]` arrays, the shipped
`starship.toml` uses plain text and box drawing only, the `eza` aliases in `/etc/bash.bashrc` say
`--icons=never` rather than relying on a default, and `lazygit`, for which KDOS installs no
configuration, draws none by default. To turn them back on, configure those four programs one at a
time, knowing the same shell on `tty1` will show holes.

### The `setup.tui` route shows nothing

`setup.tui` in `/etc/kdos/menu.conf` runs `kdos app tui` with no subcommand, which prints a usage
message to standard error and exits with status 2. KRunner and `kdos menu summon` start a route
with no terminal attached, so choosing it shows nothing. Adding a terminal program as an
application takes `kdos app tui add` run in a terminal. See the route table under
[`/etc/kdos/menu.conf`](configuration.md#etckdosmenuconf-merged-under-configkdosmenuconf).

### The on-screen keyboard does not vibrate

`plasma-keyboard` gives haptic feedback through feedbackd's session service, which is not a port, so
a key press is silent to the touch.

### The lock screen's Virtual Keyboard button does nothing while fcitx5 is the input method

Plasma's lock screen shows a **Virtual Keyboard** button whenever KWin has an input method, and
KDOS's default input method is fcitx5, which draws no keyboard, so the button opens nothing.
Choosing Plasma Keyboard on System Settings → Keyboard → Virtual Keyboard makes it work; see
[The on-screen keyboard](../02-user-guide/accessibility.md#the-on-screen-keyboard).

### Info Center has no X Server and no GLX page

They show what `xdpyinfo` and `glxinfo` print, and neither ships: there is no X server outside
Xwayland, and `mesa-demos` is built without X11.

### KSystemLog shows only the cron log

`/var/log/messages`, `/var/log/kern.log` and `/var/log/auth.log` are root's, and `dmesg` is
restricted to root, while KSystemLog runs as you: a root graphical program reaches no display on
the Wayland session, where `kdesu` loses the session's runtime directory. Its system and
authentication tabs say the file cannot be read and its kernel tab is empty, so it opens on the cron
log. Read the others with `sudo less /var/log/messages`, `sudo less /var/log/auth.log` or
`sudo dmesg`.

### The Help Center cannot search manual pages

Its search asks `whatis` for the pages matching a word, and mandoc's `whatis -w` prints file names
instead of matching them, so a manual-page search finds nothing.

### Kup keeps no versioned backups with `bup`

Kup's `bup` backup type needs `bup`, which is not a port; `par2`, the other program it runs,
comes from `par2cmdline-turbo`. Its `rsync` type and the
`restic` type KDOS adds are built; `kdos backup run` can share a restic repository with it.

### There is no Colour Management page and no colour-management daemon

colord and KDE Gear's colord-kde are not ports, so System Settings has no Colour Management page,
no ICC profile importer, and no device is assigned a profile through colord: `cupsd` registers no
printer queue with one, and a scanner or printer has no colour profile beyond the driver's own.
Under Wayland colord would list no display either, since colord-kde's output mapper is X11-only. A
display's ICC profile is chosen in System Settings → Display Configuration, and KWin applies it
without colord.

## Applications and boxes

### A running GTK 4 or libadwaita program takes a new accent only at its next start

`kdos theme` applies a KDOS global theme, and Plasma tells every running KDE program, which repaints.
GTK programs follow through kde-gtk-config's kded module, which rewrites the GTK settings, the
GSettings keys and the `colors.css` files GTK and libadwaita read. A running GTK 3 program repaints
at once: the module serves `org.gtk.Settings`, which loads `colorreload-gtk-module` into it. GTK 4
has no such module and re-reads `~/.config/gtk-4.0/gtk.css` only when the dark or light preference
changes. An accent switch that keeps the preference therefore leaves a running GTK 4 or libadwaita
program in the accent it started with. A switch between dark and light does reload it, but the
portal announces the preference before the module writes the new `colors.css`, so the program
reloads the colours of the switch before: gnome-mines started in `amber` stays amber through
`perfect`, turns `perfect`'s blue on the switch to `paper`, and `paper`'s light colours on the
switch back to `bone`. Each takes the right accent when it next starts. A Qt 5 program reads its
qt5ct palette when it starts.

### Boxes are measured by their processes, and their limits are unenforced

No box runs in a cgroup scope of its own. A store box, the box every catalogue and Debian install
makes, is created with none: its writes are inside its container, and a container whose cgroup
parent went with an ended login could never start again. A pack box's scope is never made (see
[Host applications and boxes reach the portals
unidentified](#host-applications-and-boxes-reach-the-portals-unidentified)). Every box's processes
sit in the login's own `session` cgroup, so the privacy lamps, `kdos hey` and System Monitor's KDOS
page find a box's processes by the `KDOS_BOX` in their environment instead, in a mount namespace
other than the host's. What that cannot give is a cgroup's accounting: a box's System Monitor
figures are the sums of its running processes, so its memory is their proportional set size with
no page cache, the CPU time of a process that has exited leaves the sum (that sample shows no
rate), its `memoryLimit` sensor reads 0, and its `kdos-boxinit`, which runs as a user-namespace
root the session's user does not own, is not counted. A box's `memory` is held by nothing but
`kdos-oomd`'s budget check, and its `cpus` and `pids` by nothing at all.

### GTK 4 and libadwaita programs in a box crash at start on a virtual machine's software renderer

Debian 13's GTK 4.18, which the `rt-gtk` runtime installs, exits on SIGSEGV while it opens the
Wayland display under KWin on llvmpipe: the crash is in its linux-dmabuf feedback handler, on the
scanout tranche KWin sends there, and no `GDK_DISABLE` or `GSK_RENDERER` value avoids it. Sound
Recorder and every other GTK 4 or libadwaita application of the catalogue or Discover's Debian
lane therefore opens no window on such a machine; GTK 3 applications are unaffected, and so is the
host's own GTK 4.24. Whether KWin's feedback on a real graphics card triggers it has not been
measured.

### A running boxed GTK program takes a new accent only at its next start

A boxed GTK program is given its theme as `GTK_THEME` when it starts, and with the variable set GTK
follows no later change of theme or of the dark preference, whatever theme name the settings portal
announces. Its colours are the `colors.css` it read
at start: `colorreload-gtk-module`, which repaints a running GTK 3 program on the host, is loaded by
the modules `org.gtk.Settings` names on the session bus, which a box's filtered bus does not reach.
An accent switch, or a switch between dark and light, therefore reaches a boxed GTK program when it
next starts.

### A box can be granted only the nine named groups of Wayland globals

KWin refuses a sandboxed client the desktop-wide Wayland globals: window management, fake input,
screencasting, the clipboard at any time, layer-shell panels, output management, DPMS, the Plasma
shell's placement and the input-method globals. A box's profile may hand back any of them with its
`grant =` line in `~/.config/kdos/boxes/<name>.conf`, by the names `window-list`, `fake-input`,
`screencast`, `data-control`, `layer-shell`, `output-management`, `dpms`, `plasma-shell` and
`input-method`, each mapping onto its global; `secrets` is the tenth name, and reaches the bus
rather than KWin. Any other global cannot be granted to a box, whatever its profile says, and a
grant cannot be narrower than its row. A running client keeps what it bound. See
[KWin](../04-programs/kwin.md#what-a-box-may-bind) and
[The security model](../03-architecture/security-model.md#what-is-not-protected) for what a grant
costs.

### A boxed application is given your location without being asked

The portal front end asks before sharing location only with an application in a sandbox it
recognises (Flatpak, Snap or Linyaps), and a KDOS box is none of them, so it counts as an
unsandboxed host program. GeoClue runs with no consent agent and an empty whitelist, and answers any
client on the system bus. See
[The session](../03-architecture/session.md#access-and-what-it-unlocks).

### Host applications and boxes reach the portals unidentified

The portal front end names a caller from an `app-<launcher>-<id>-<n>.scope` cgroup, and no caller
is in one. An application Plasma starts runs in the login's own cgroup, because KIO places a child
in a scope only through a systemd user manager, and a box's scope is never made, because elogind
leaves `/sys/fs/cgroup/<session>/cgroup.procs` root's and cgroup2 then refuses the move into the
login's delegated subtree. Every caller therefore has the empty application id: KDE's ScreenCast,
RemoteDesktop, Camera and Screenshot dialogs ask on behalf of "an unidentifiable application", with
"without asking next time" ticked, and an answer remembered there applies to every host
application and every box. Nothing on the desktop shows or changes a stored answer: each portal's
answers are a file under `~/.local/share/flatpak/db/`, and deleting that file makes it ask again.
The same root-owned file keeps a pack box's `memory`, `cpus` and `pids` unenforced, and leaves
System Monitor only a box's processes to measure (see [Boxes are measured by their processes, and
their limits are unenforced](#boxes-are-measured-by-their-processes-and-their-limits-are-unenforced)). See
[The session](../03-architecture/session.md#portals) and
[Containers](../03-architecture/security-model.md#containers).

### A catalogue change reaches an installed application only when it is rebuilt

The application catalogue (see [kdos-appbox](../04-programs/kdos-appbox.md#the-catalogue-file)) is
a list of rows, each naming an image and the packages it adds. An installed application is a built
image, so editing its row changes nothing a running system can see until `kdos app install <id>`
builds it again. For example, the base image's row carries `libva` and the VA-API driver set; a
boxed browser built before the row carried them reports no hardware decoder and decodes every
frame on the CPU. Nothing tells you a row changed: the catalogue has no per-row version, so an image
counts as current until somebody removes it.

### A live session cannot create a persistent box

The home directory is on the boot overlay, and the kernel refuses to stack a box's writable layer
on an overlay. When `kdos-packd` composes a box there, the layer falls back to a tmpfs under
`/run/user/<uid>/kdos/boxes/`, so whatever a box writes outside the shared home directory ends with
the session. `kdos doctor` reports this as a warning about where the session's home directory
lives, not as a failure. `kdos-box create <name> base=pack:<id>` asks podman for a container on
top of that composition, and when podman refuses on a live session it says that a persistent box
needs an installed system, takes the composition down, and leaves the profile it wrote, which
`kdos-box remove <name>` deletes.

### The *Containers* menu entries have not been opened on a built image

`kdos-podman-api` starts `podman system service` for `podman-tui` and `lazydocker`; what is
measured is its start, lock and idle-restart logic against a stand-in engine, not either tool
connecting to a real service. Pods and `--init` rely on `catatonit`, which is measured to build
statically and run a command, not to start a pod.

### A box is not a security boundary against you

By default it shares your home directory in full, along with your network, process table, `/dev`,
`/sys` and `/tmp`. A pack box and a store box each see a private runtime directory holding only
their tagged Wayland socket, a filtered session bus and the audio sockets. Only a development box
that distrobox creates over somebody else's image (`kdos-box create <name> base=image:<ref>`)
shares the session's own runtime directory: it can reach the session bus and connect to KWin
untagged through `wayland-1`, and `kdos-appbox status` marks it `tagged` rather than `confined`. A
box limits what an application is offered by the *desktop*, not what it can do to your data. A profile's
`home = private` gives one box a home of its own. See
[The security model](../03-architecture/security-model.md#what-is-not-protected).

### Twelve applications run on Qt 5, which has no 2026 security fixes

Qt 5.15.19 is the last Qt 5 the tree can build, and no public patch for a 2026 Qt 5 vulnerability
exists. Candle, GNU Radio, KeePassXC, LibreCAD, LMMS, Mumble, QSSTV, Quassel, SMPlayer, DB Browser
for SQLite, VLC and WSJT-X still link it, because no release of any of them builds with Qt 6: each
moves when one does. The reason for each is in
[the ports catalogue](ports-catalogue.md#qt5). Their TLS goes through QtNetwork, and Mumble's
certificates and Quassel's channel encryption through their own OpenSSL calls, to the OpenSSL 3.5
long-term release in the `openssl3` slot, which does still receive fixes.

### A PyQt5 program that also uses Python's `ssl` or `hashlib` can crash

`python3-pyqt5`'s QtNetwork, QtQml, QtQuick, QtQuickWidgets, QtMultimedia, QtMultimediaWidgets
and QtWebSockets modules load `libQt5Network`, which links OpenSSL 3, while Python's own `_ssl` and
`_hashlib` link OpenSSL 4. musl's loader has one symbol namespace, so a process that loads both
resolves each library's calls into the other, and the first such call can fail or crash. A PyQt5
program is affected only when it loads one of those seven modules and also imports `ssl` or
`hashlib`, directly or through a library; `from PyQt5 import Qt`, which GNU Radio's gr-qtgui runs,
loads all seven. A Python program that needs both uses PyQt6, whose Qt links OpenSSL 4.

### Copying from `iamb` or `atuin` has not been confirmed to reach the clipboard manager

Text you copy in either should appear in Klipper's history, and this has not been observed on a
built image. It is expected to work: both programs copy through the Wayland data-control protocol,
compiled into the binary itself, and KWin provides that protocol to programs that are not boxed. If a copy from either goes
missing, that is a bug worth reporting.

### KOrganizer opened after another PIM program shows its calendars unticked

Akonadi makes the default calendar and the birthdays calendar the first time any KDE PIM program
starts it. KOrganizer ticks a calendar that appears while it runs, but saves no choice for one that
was already there at its own first start, and shows it unticked. So when Merkuro, KAddressBook or
KMail is opened before KOrganizer, its first window lists Personal Calendar and Birthdays
unticked, and a new event is reported as created in a calendar that is filtered out. Tick them in
the calendar list once; KOrganizer keeps the choice. No setting changes this, and the collection ids
it would have to name are assigned only when Akonadi runs.

### A video call has never been placed

`baresip` is the SIP phone here, and its interface is a terminal menu. The far end's picture goes in
an `sdl.so` window; sending your own means enabling `v4l2.so`, which the generated configuration
leaves commented out because which camera to send is your choice. The rig (the QEMU harness that
boots a KDOS image and photographs it; see [Testing](../05-developer/testing.md#the-qemu-rig)) has
no second endpoint and no camera, so what is measured is that the modules load. GStreamer's
`webrtcbin` is in the same state: it is built, with `libnice`'s `nicesrc` and `nicesink` and
`libsrtp`, and no pipeline has negotiated with a peer.

### `mbsync` has XOAUTH2 but not OAUTHBEARER, and no synchroniser has met a server

`cyrus-sasl` is the mechanism loader and ships no XOAUTH2 of its own, so the mechanism comes from
`cyrus-sasl-xoauth2` beside it, and that plugin implements only the one. A mail server offering only
OAUTHBEARER cannot be mirrored into a local Maildir, and with it goes `notmuch`'s index and offline
search for that account. `aerc` reads such an account and `msmtp` sends through it: measured on the
image, `msmtp --version` reports `Authentication library: built-in` and lists `oauthbearer` and
`xoauth2`, and `aerc` carries `imaps+oauthbearer`, `smtps+oauthbearer` and its own `xoauth2Client`.

Neither mail-and-calendar synchroniser has run against a server. The rig has no account, no network
and no IMAP or CalDAV server, so what is measured of `mbsync` and `vdirsyncer` is that each runs,
reports its version, does nothing and exits cleanly with nothing configured. That a password account
synchronises is unproven here and can only be proven against a real account. For XOAUTH2, what can
be measured on the image is that `libxoauth2.so` is in `/usr/lib/sasl2` and that `mbsync` links
`libsasl2`; that a provider accepts the token `pizauth` mints cannot.

### The list of Windows shares on the network is whatever answered a broadcast

Samba here is built without winbind and without a domain controller, so there is no browse master to
ask: Dolphin's `smb:/` lists the machines that answered a NetBIOS broadcast, mDNS or a WS-Discovery
probe, and a machine that is asleep, on another subnet, or behind a router that does not forward
broadcasts is missing from it. It can still be reached by typing `smb://host`.

GTK programs' `network://` view lists less: `gvfs` is built without its WS-Discovery (`wsdd`)
backend, because `wsdd` is not a port, so it lists only the hosts Avahi finds.

### An NFSv3 mount needs `-o nolock`

NFSv4 mounts and the v4-only server (`72_nfsd`) work. There is no `rpcbind` port, so `rpc.statd`
cannot register, and a `vers=3` mount that wants locking is refused. Mounting with `-o nolock`
works and keeps locks local to this machine.

### Kerberos is built and has never been given a ticket

`kinit`, `cifs.upcall` and the `request-key` rule that joins them are on the image, so a share can
be mounted with `mount -t cifs -o sec=krb5`, but there is no KDC here and no realm to join, since
the server half of krb5 is not installed. No share has been mounted with a ticket on this image.
`cifs.idmap` is not built either: it needs winbind's client library.

### The graphical passphrase prompt has not been exercised

`pinentry` is `pinentry-qt`, which draws a Qt dialog when the request from `gpg` carries
`WAYLAND_DISPLAY` or `DISPLAY`, and otherwise asks in the terminal `GPG_TTY` names. A program
started from a launcher should therefore get the dialog, but no signature or decryption has been
made through it on this image, so neither the dialog under KWin nor its placement over the
window that asked has been seen.

### `rga` does not search `.htm` files

Its `pandoc` adapter, which reads `.docx`, `.odt`, `.epub`, `.fb2`, `.ipynb` and `.html` into text,
claims `.htm` too and passes the extension as the input format, and pandoc has no reader named
`htm`: each such file is reported as *Unknown input format 'htm'* and yields no match.
`rga --rga-adapters=-pandoc` drops the adapter, and the file is then searched as plain text. The
other formats, `.html` included, are searched through pandoc.

### A transcription has never been read back

What is verified is the model check, the `whisper-cli` command line, starting it and its exit
status, and that Dolphin offers *Transcribe* only while a model is found. The image carries
`base.en` in `/usr/share/whisper.cpp/models`; any other model is fetched by `kdos speech get`,
which picks one from a table, checks its sha256 and writes it to
`~/.local/share/whisper.cpp/models`. What has not been done is running a transcription and checking
its output.

### Live transcription is a terminal program, not a desktop action

`whisper-stream` is built and transcribes a microphone, and nothing on the desktop starts it.
Dolphin's *Transcribe* runs `whisper-cli` over a file you already have. Both need the same model,
and `base.en` ships.

### The native applications have been opened, not used

The browsers, office suites, editors, games and the rest of the graphical applications are recipes
under `ports/core`, named in the lists of the userland phases, `40_lang` to `44_apps`, each with
its sources pinned by hash. Every application with a row in Kickoff has been started from its
desktop entry on the ISO, the way Kickoff starts it, and nearly every one maps a window, on KWin's
Wayland or, for one with no Wayland path, through Xwayland. The few that map none are terminal
programs that exit without a terminal, tray or background programs with no window of their own,
and a handful that opened no window in that launch for a reason of their own. Using them is not
measured:
opening a document, playing media, saving or printing in any of them has not been done on an
image, so what is written here and in [the ports catalogue](ports-catalogue.md) about how any of
them behaves beyond starting describes its recipe. No harness in the tree repeats that launch, so
it is not part of a check. How much of a build of them has been measured is under
[Build times and snapshot sizes are measured for part of the build
only](#build-times-and-snapshot-sizes-are-measured-for-part-of-the-build-only).

### Five applications have no port

- **Anki** needs a cargo and a yarn vendor bundle in one recipe, a musl build of a native rollup
  addon, and about seventeen Python ports that do not exist.
- **Ghidra** builds with Gradle, and Gradle's own build needs a Gradle seed and several hundred
  Maven artifacts that `ports/fetch` has no mode to bundle.
- **LanguageTool** pulls in well over a hundred Java libraries, each of which would have to be
  compiled from its sources, since upstream's prebuilt jars are not accepted.
- **Weasis** needs a Maven build and its own fork of OpenCV's Java binding, whose DICOM codecs have
  no public source. `dcmtk` is the DICOM toolkit on the host.
- **YAAC** ships 58 prebuilt jars, some carrying glibc native libraries, and they fit no class of
  [what is not built from source](../01-philosophy/why-kdos.md#what-is-not-built-from-source).

### Chromium checks no spelling and reaches no Google service

Chromium reads only its own `.bdic` dictionaries, which it downloads, so it cannot use the
Hunspell dictionaries on the host, and its spelling service is off by policy. It is configured with
no Google API keys, so sync, sign-in, Safe Browsing, translation and location services do nothing.

### GoldenDict-ng ships with no dictionary

No port installs dictionary data, and the `goldendict-ng` port starts a new profile with its online
sources (dict.org, Wikipedia and Wiktionary) turned off, so a fresh install finds nothing for any
word. StarDict, DSL, MDict, Babylon or ZIM files put in `/usr/share/stardict/dic`, which a new
profile already lists as a source, are found at the next rescan; a folder anywhere else is added
under *Edit > Dictionaries > Sources > Files*. An online source is ticked in the same dialog.

### Quassel shows no link previews

The previews need Qt 5's WebEngine, which is not a port, so `quassel` is configured with
`WITH_WEBENGINE=OFF`.

### A Quassel core cannot sign users in through LDAP

`quasselcore` keeps its accounts in its own database only. Quassel is a Qt 5 program, so it links
OpenSSL 3 through QtNetwork, and `libldap` links OpenSSL 4; one process cannot load both, so
`quassel` is configured with `WITH_LDAP=OFF`. It returns when a Quassel release builds with Qt 6.

### An Android screen cannot be mirrored

`scrcpy` is not a port: its device-side server is a jar that cannot be built here, and shipping
upstream's prebuilt one was declined. `adb` and `fastboot` from `android-tools` are on the host.

### A phone driving the pointer or keyboard through KDE Connect has not been tried

KDE Connect's remote input, the phone as a touchpad and keyboard, asks the RemoteDesktop portal for
the input, which `xdg-desktop-portal-kde` serves on Plasma. No phone has been paired with an image,
so that path, and the plugins that do not use the portal (notifications, the clipboard, file
transfer), are built and unexercised.

### The local model and translation programs ship without models or pages

`llama-server` serves its OpenAI-compatible API and no web page: the page is a Svelte application
that is either built with npm or downloaded, and the offline build does neither. A page placed in a
directory is served with `--path`. Translate Locally ships no Bergamot language model; its model
list is fetched from the network only when asked, and a model downloaded elsewhere is imported from
the disk.

### Hatari ships no TOS

The Atari ST emulator needs a TOS or EmuTOS image, and building EmuTOS needs an m68k cross
toolchain that is not a port, so the user supplies the image.

### Cantor has no Maxima, Sage or Scilab backend

Cantor's Maxima, Sage and Scilab backends are built and drive the program of that name, and none
of the three is a port: Maxima needs a Common Lisp (SBCL or ECL), which is not a port either. A
worksheet offers them and cannot start one. Python, R, Octave, Lua, Qalculate and KAlgebra work.

### Qt Creator and KDevelop show no Qt documentation

No port installs Qt's or KDE's API documentation as `.qch` files: QDoc, which generates them, is
not built in `qt6-qttools`, and there is no `qt6-qtdoc` port. The Help mode of Qt Creator and the
Qt documentation of KDevelop are empty, and the examples Qt Creator's Welcome page lists come from
that documentation, so none is listed. A `.qch` file downloaded elsewhere is added in Qt Creator's
*Preferences > Help > Documentation*.

### Warzone 2100's campaign has no videos

The campaign briefing videos are upstream's separate `sequences.wz`, several hundred megabytes,
which the port does not ship; the game says on its Campaign screen that they are missing, and the
campaigns play without them. See the port's notes in *KDOS from Scratch*.

### SolveSpace takes no 3D mouse

`spacenavd` serves a 3D mouse to every `libspnav` client, FreeCAD and OpenSCAD among them, but
`solvespace-qt` has no 3D-mouse support at all; upstream has it only in its GTK and Windows
interfaces.

### Audacity reports a recording error on a machine with no microphone

Audacity 4 starts monitoring the input when a project opens. On a machine with no capture device,
a desktop with no microphone or a virtual machine, PortAudio cannot open one and Audacity shows
"Error opening recording device. Error code: -9997 Invalid sample rate" as a modal dialog each
time a project or an audio file opens. Dismissing it is harmless: playback, editing and export
work, and recording works once a capture device is present. No build switch or system default
turns the monitoring off.

### Every program speaks English

There is no non-English interface anywhere on the system. Every package is made without its
translations: message catalogs, Qt translations, translated KDE handbooks and translated manual
pages are removed as the package is built, so setting `LANG` or `LC_MESSAGES` to another language
changes no program's text, because there is nothing for it to load. KDE's catalogues go too, and
four KDE Frameworks keep the English plural forms of their messages only in a catalogue, so a few
counts read "(s)" where an English plural belongs. Text in any language can still
be typed, shown and edited: fonts, keyboard layouts and the input methods for Chinese, Japanese and
Korean ship. The rule is in
[Writing ports](../05-developer/writing-ports.md#size-and-language-policy).

### `texdoc` finds no manuals, and HTML and PDF manuals are not installed

The manuals of the TeX packages are the `texlive-doc` port, which no phase builds, so `texdoc` has
nothing local to open; read them on CTAN. `kpkg install texlive-doc` does not change that as the
tree stands: the package-time prune removes its PDF manuals. Every other port's HTML and PDF manuals are removed as its package is made, and its
manual pages and info pages ship instead. HTML a program opens itself stays: mednafen's manual,
which upstream writes only in HTML, R's help index, the manuals behind the Help menus of geany,
gimagereader, texstudio, WSJT-X, Wireshark and hatari, the pages FreeCAD's and SMPlayer's About
dialogs show, and this book under `/usr/share/doc/kdos`.

## Hardware and platform

### x86-64 only

There is no other build target: the cross toolchain targets `x86_64-kdos-linux-musl`, and every
later phase builds with it.

### Secure Boot is not supported

Limine's EFI binary is unsigned and KDOS enrols no keys, so a machine with Secure Boot enabled
refuses to load it. The installer reports the firmware state on its first page; turning Secure Boot
off in firmware setup is the only way.

### 32-bit UEFI is built and has never been booted

`BOOTIA32.EFI` is on the ISO's EFI system partition tree, on an installed machine's, and in the El
Torito UEFI record, and the installer writes a firmware boot entry naming it where
`/sys/firmware/efi/fw_platform_size` says 32, but the machine that needs it is an early Atom tablet,
and nothing here has one. What is verified is that the binary is built and placed. BIOS and 64-bit
UEFI are the two that have been booted.

### Broad hardware support is not a goal

The firmware tree ships everything an x86 driver can load, which covers a great deal, but nothing
here is tested against a wide range of devices.

### `linux-firmware` leaves out what an x86 kernel cannot load

About 370 MiB of upstream's tree is not installed, by an explicit list in the recipe: the
Qualcomm SoC firmware under `qcom/` (Adreno GPU, Venus and Iris video, and the DSP images of
the arm64 laptop and phone SoCs), every `mlxsw_spectrum*.mfa2` except the three revisions the
`mlxsw` driver names, and Marvell Prestera v2.0 and v3.0. What stays under `qcom/` is what x86
drivers request: `aic100` and `aic200` (`qaic`), and `qdu100`, the `sdx*` modems and the
`prog_firehose_*` loaders (`mhi_pci_generic`). A kernel whose `mlxsw` asks for a newer
Spectrum revision than the recipe keeps finds no file and leaves the switch on the firmware in its
flash.

### The installation medium carries four vendors' firmware

The medium has Intel's, AMD's, MediaTek's and Realtek's firmware and SOF; NVIDIA's, Qualcomm's and
Atheros', Broadcom's and the rest (`firmware-misc`, which holds the Cirrus Logic and TI amplifiers
of many laptops' speakers) are prebuilt packages in the release's binary host. A live session on
such hardware has, until `sudo kdos firmware sync` has run, no Wi-Fi on a Qualcomm or Broadcom
card, software rendering on a Turing or later NVIDIA GPU, and silent speakers behind those
amplifiers, and the command needs a network to fetch the packages: Ethernet, a USB-tethered phone,
or a supported Wi-Fi card. The live session's notice names the port; the installer runs the
command into the new disk when it has a network.

`kdos firmware sync` decides from each device's driver's `firmware:` list, so a driver stands for
its whole vendor port, and from the kernel log, which `kernel.dmesg_restrict` keeps from a user:
the session's notice, which runs the dry run as the person logged in, sees only the drivers'
lists. A chip whose driver declares no firmware and whose load failure only the kernel log records
(a Bluetooth adapter behind `btusb`) is found by `sudo kdos firmware sync --dry-run` and not by the
notice. Neither has been run on hardware; the fixture under `testing/fixtures/firmware` and one
x86 desktop are what it has been measured on.

### `btop`'s GPU panel is empty on Intel graphics unless btop runs as root

It reads the i915 performance counters with `perf_event_open`, a system-wide event that the
kernel's default `perf_event_paranoid` of 2 refuses to a process without `CAP_PERFMON`, and nothing
grants that capability to the binary or lowers the setting. `sudo btop` shows the panel. System
Monitor's Intel GPU busy sensors are empty for the same reason: their helper would need
`CAP_PERFMON`, and a file capability does not survive kpkg's archive.

### LVM volume groups are activated only at boot

The initramfs activates them on a disk boot, then the `03_lvm` init script does. A disk carrying LVM
that is plugged in later shows no logical volumes until you run `sudo vgchange -aay`: lvm2's own
hotplug activation runs through `systemd-run`, so it is built off and its udev rule is not
installed.

### A root filesystem on LVM has been booted in one shape only

An unattended install onto plain LVM, in a virtual machine with UEFI firmware, booted: slot A's
volume `kdos/root_a` was mounted by the initramfs, and a slot B made as an LVM snapshot of it booted
as `/dev/mapper/kdos-root_b`. An encrypted, thin or cached root has not been booted, and neither has
a volume group spread over more than one disk. The initramfs carries `thin_check`, `cache_check` and
the thin, cache and snapshot targets for them. The installer lays out only one shape, a volume group
of one physical volume on the target disk, with `root_a`, `root_b`, swap and `home` on a disk large
enough for two slots and `root_a` alone on a smaller one; that layout has not been booted from the
installer, and its encrypted form not at all; see
[Activating volume groups](../03-architecture/boot-and-init.md#activating-volume-groups).

### `lsblk` shows a filesystem's type, label and UUID to root only

util-linux is built without libudev, because eudev needs util-linux's libblkid first, so lsblk can
learn them only by probing the device, which it does only as root. `sudo lsblk -f`, or `blkid` as
root, answers.

### A USB modem that first appears as a storage device is not switched into a modem

`usb_modeswitch` is not a port, so such a stick shows up as a small read-only disk and ModemManager
never sees it. A built-in WWAN card, and a stick that appears as a modem straight away, connect
through NetworkManager. Mobile broadband, PPPoE and Bluetooth tethering are built and have not been
connected on hardware.

### A UEFI firmware (capsule) update has not been applied

What is verified is that `fwupd-efi` builds its loader with the NX flag and a KDOS SBAT line, and
that the EFI-partition probe the `fwupd` recipe patches in reads the right partition number, offset,
UUID and type from a mounted EFI partition. No capsule has been staged and booted: the rig's
firmware publishes no ESRT, so it has no device to update.

### A phone has not been reached over MTP

Dolphin opens a phone's storage through KIO's MTP worker, and `aft-mtp-mount ~/Phone` and the
Android File Transfer window are the other ways in. No phone has been reached through any of them
on this system.

### Smart cards, drawing tablets and software radios have not met hardware

`opensc` has not been used with a card in a reader, `libwacom`'s pairing with a tablet, or the
SoapySDR modules with a radio; each is built and never tried against the hardware. The SDR
applications (GNU Radio, SDR++, SDRangel, Gqrx, URH) are built and have not been run. SDRplay receivers have a udev permission rule
and no driver, because SDRplay's API is published only as a closed library, so SoapySDRPlay3,
SDRangel's SDRplay input and SDR++'s SDRplay source are all off. XTRX is not supported at all: it
needs the out-of-tree `xtrx_linux_pcie_drv` kernel module, and its libraries are unmaintained.
SDRangel's four remote plugins (remote input, output, sink and source) are not built on x86-64:
they need SSE3 at compile time, which the x86-64 baseline the tree compiles for does not define,
and `cm256cc`, the library they use, needs a processor with SSSE3 there. An aarch64 build, where
NEON stands in, builds all four against the `cm256cc` port.

### Packet radio has no kernel network interface

Linux 7.2 carries no AX.25, NET/ROM or ROSE stack and none of the hamradio device drivers, so there
is no `ax0` interface, no `kissattach` and no connected-mode AX.25 socket. Packet radio runs in
userspace instead: `direwolf` is the modem, Pat reaches it through its AGW port with the
`ax25+agwpe` transport, and Xastir through a KISS serial or network interface. A program that needs
the kernel's AX.25 sockets, such as the `ax25-tools` and `ax25-apps` suites, a NET/ROM node or
LinFBB, has nothing to talk to.

### Intel Quick Sync through oneVPL has no runtime

`libvpl` and `vpl-gpu-rt` are not ports, so ffmpeg's and GStreamer's `qsv` paths find nothing.
VA-API reaches the same decode and encode hardware through `intel-media-driver` and
`libva-intel-driver`.

### Encrypted Blu-ray discs do not play

An AACS-encrypted Blu-ray needs `libaacs`, which is not a port, so `mpv`, `ffmpeg` and GStreamer
open only an unencrypted Blu-ray or a backup. A Blu-ray's BD-J menus do not run either: they are
Java, and `libbluray` is built without its jar (`-Dbdj_jar=disabled`), because the jar is built
with Apache Ant, which is not a port, so a disc plays its titles without them. A CSS-encrypted DVD is opened through `libdvdcss`, which
`libdvdread` is configured to link. An audio CD lists as numbered tracks, because the CDDB lookup
`cmus` and `libcdio` can make needs `libcddb`, which is left out: its one job is a lookup over the
network. None of the optical paths has read a disc here, because the rig has no drive.

### Burning a disc and writing an image are unproved on hardware

K3b burns discs through `cdrdao`, `cdrskin` and `growisofs`, and ISO Image Writer writes an image
to a stick. The rig has no optical drive and no stick, so neither has burnt or written anything,
and no disc has been read back.

## Boot and updates

### On an installed machine the boot messages are drawn over the splash

An installed machine's boot entries carry `console=tty0` alone, so `/dev/console` is the screen.
The initramfs's first line is printed there about a millisecond after `/init` starts, which ends
the console's deferred takeover of the framebuffer before the splash has started. Every later line
the initramfs and `rcS` print (`Running: 55_powerd.sh start` and the rest) is then drawn by the
console on top of the splash animation, until the splash quits. The live medium prints on the
serial line instead and does not show this. Measured in the rig by booting the medium's own kernel
and initramfs with `console=tty0`. See
[The splash](../03-architecture/boot-and-init.md#the-splash).

### A disk-encryption key is typed, not sealed in the TPM

`tpm2-tss` and the `tpm2_*` tools ship, so a key *can* be sealed to the chip by hand, but nothing in
the boot path unseals one: `unlock_root` reads a passphrase from `tty1` and has no other route.
Adding one means a sealed blob on the EFI partition, a PCR policy, and an answer for what happens
when a firmware update changes the measurements, none of which is decided.

### Only an LVM erase install gets a second root slot

The installer lays a disk out for A/B updates when it erases the disk onto LVM and the disk holds
two slots, the swap and a home of their own: it makes `root_a` and `root_b`, copies the finished
system into slot B, and records both. Two slots need each slot's 1.5 times the install, the swap
and a 4 GiB home, about 149 GiB for an install of 46.5 GiB, so a 128 GB disk gets one. A disk too
small for two slots, and every manual layout, get slot A only, and a second slot there is made,
filled and recorded with
`kdos-bootctl set-slot b <uuid> [<luks-uuid>]` by hand; the steps are in
[Installation](../02-user-guide/installation.md#ab-root-slots). A second *encrypted* slot has never
been booted. What is measured is on the build host: rolling back from slot B to A hands back A's
container and not B's, which is the failure the mechanism exists to prevent.

### An update with --root compiles against the running system

`kdos update apply --root DIR` installs into `DIR` by pointing `KPKG_ROOT` at it, which moves the
installs and the package database and nothing else: every package is still built on the running
root, with its compiler, headers and libraries. When one run upgrades a library and a package that
links it, the package is compiled against the running root's older copy, not the one just
installed into `DIR`. An A/B apply does not have this problem, because it builds inside a chroot
of the slot. See [Updating a machine](../03-architecture/packaging.md#updating-a-machine).

### A source install from Discover shows only `kpkg`'s lines as progress

A port built on the machine, with every dependency `build-deps = source` builds alongside it, can
compile for hours. Discover's progress for it is the lines `kpkg` prints, with no estimate of the
time left and no count of the packages still to build, so a long build and a stuck one look the
same until it ends.

### The update path has not been run against a published tree

`kdos update fetch` takes the signed ports tree and the signed release index from the latest
release, and the binhost index and slot image manifest through that index. `ports/publish
--release` publishes all of them (see
[Publishing a release](../03-architecture/packaging.md#publishing-a-release)), but its uploads have
run only as `--dry-run`, and no release carries them yet; until one does `fetch` reports
`fetch-failed`, and Discover offers no system update. An image built without a release key has an empty
`/etc/kdos/keys/tree`, and on it `fetch` stops before the network and reports `no-key`; with no key
in `/etc/kdos/keys` the binhost and image lanes stop the same way. The
signature checks, the refusal of an older tree or image, the exchange with `/ports`, a slot image
unpacked from a local directory, and `kpkg fetch`'s archive and upstream order have been exercised
in a container with no network, but none of it against a real release over the internet. No update
has been applied on an installed machine, so how long one takes, and how much space it needs, is
not measured.

One thing is known short of that. A Discover started again while an update is downloading does not pick the running job back up; the System Versions page shows it. See
[Discover](../04-programs/discover.md#system-updates).

### Per-slot kernels have been booted on virtual firmware only

On an installed virtual machine with UEFI firmware (OVMF), `kdos-bootctl deploy` put slot B's
kernel on the EFI partition, `try b` booted it once through `BootNext` and the `KDOS update trial`
entry, and the boot confirmed it. A trial led by the menu, with `efivarfs` unmounted, booted and
confirmed the same way, and a trial boot that hung in `rcS` was reset and came back on the
confirmed slot with the trial cleared. No physical firmware has run a trial, so a firmware that
ignores `BootNext`, or deletes a `Boot####` entry that is not in `BootOrder`, has not been met: by
the code it rolls every candidate back unbooted rather than looping. Neither BIOS boot of a slot nor
the `linux` postinstall's appended initramfs has been started, as no newer `linux` package exists
to install. On BIOS, a candidate kernel that dies before its initramfs runs spends no attempt,
because Limine counts nothing: the menu keeps leading with it until you pick the confirmed slot's
`/KDOS (slot <x>)` entry by hand; see
[One boot through BootNext](../03-architecture/boot-and-init.md#one-boot-through-bootnext).

A root that has no `/boot/initramfs.modules` builds a new kernel's initramfs from the module set its
own initramfs carries, rather than from a list; that path has been exercised on test archives only.
See [A new kernel](../03-architecture/boot-and-init.md#a-new-kernel).

## Security

The full statement is
[What is not protected](../03-architecture/security-model.md#what-is-not-protected). In brief:

- there is no mandatory access control, no verified boot and no measured boot;
- kernel modules are signed but the signature is not enforced: root loads an unsigned module and
  the kernel only marks itself tainted;
- the live image's serial console is a root shell with no password (an installed system's asks
  for a login);
- on the live image, `root` and `kdos` both have the password `kdos`, and opening `ssh` in the
  firewall starts `sshd` with those accounts as they are; until then the image's
  `/etc/service.disabled/sshd` keeps it stopped;
- the live image logs in as `kdos` automatically; an installed system does so only when the
  installer or System Settings' Login Screen page set it;
- disk encryption protects data at rest only;
- a box shares your home directory by default, and can therefore edit its own profile and widen
  its own compositor grant;
- a box granted `input-method` receives every keystroke on the seat;
- membership of `wheel` is effectively root;
- an OCI base image pulled from a registry is unsigned content;
- an unsigned pack mounts unless `KDOS_REQUIRE_SIG` is set, while a pack whose signature fails
  does not;
- nothing downloads or installs updates on its own: a nightly timer
  (`/etc/kdos/timers.d/10-update-check.timer`) runs `kdos update check` over what the last fetch
  left, and `kdos update fetch` and `apply`, or Discover, run only when you ask;
- `kdos-oomd`'s choice between several large processes is tested only against recorded process
  tables.

## Ports built without a feature

Each port, its shelf and the phase that builds it, and the recipes that are not installed at all
are in [The ports catalogue](ports-catalogue.md).

### Go has no race detector and no BoringCrypto

Both are objects upstream compiles and ships inside the source tarball, and the `go` port does not
install them, so `go build -race`, `go test -race` and `GOEXPERIMENT=boringcrypto` fail to link.
The `testdata` directories of the standard library's source are not installed either (33 MB,
including foreign ELF fixtures), so `go test std` fails; a module's own tests are unaffected.
Every other Go build is unaffected.

### GHC has no profiling libraries

The `ghc` port builds Hadrian's `release` flavour without its profiled variants, so `ghc -prof` and
`cabal --enable-profiling` fail to find the profiled `base`. Of GHC's documentation only the
`ghc(1)` manual page is installed. The User's Guide and the Haddock pages of the shipped libraries
are not, so `cabal haddock` writes pages whose references into `base` and the other GHC libraries
are plain text rather than links.

### SpatiaLite cannot load a WFS layer from a URL

libxml2 2.15 has no HTTP client, and the `libxml2` port builds none of its stand-in stubs, so
SpatiaLite's WFS functions (`load_from_wfs`, the `ImportWFS()` SQL function and the GetCapabilities
helpers) read a local file and fail on an `http://` address. The `libspatialite` port compiles its
proxy reset only where libxml2 has one. Fetch the document first, for example with
`curl -o layer.xml '<GetFeature URL>'`, and load the file.

### nmap has no `jdwp-exec` and no `jdwp-info` script

Both inject Java classes that upstream compiles and ships inside the source tarball, and the `nmap`
port installs neither the classes nor the two scripts, so `--script jdwp-info` names nothing and the
`default` category runs without it. `jdwp-inject`, which injects a class you supply, and
`jdwp-version` are unaffected.

### qemu carries firmware only for its three targets' default machines

That is the x86_64 PC machines, aarch64 and riscv64 `virt`, and microvm, each through its defaults
and its firmware descriptors. The Nuvoton and ASPEED BMC boards inside `qemu-system-aarch64` stop at
startup with *Could not find ROM image* unless `-bios` names one, and there is no 32-bit Arm UEFI,
no 32-bit x86 OVMF and no OVMF build for microvm. The riscv64 `virt` UEFI image is upstream's rather
than compiled here; see [what is not built from
source](../01-philosophy/why-kdos.md#what-is-not-built-from-source).

### A qemu guest cannot join a host bridge as an ordinary user

`-netdev bridge` runs `qemu-bridge-helper`, which is installed without its setuid bit and with no
`/etc/qemu/bridge.conf`, so it serves only root, and only once root writes an `allow <bridge>` line
there. Making it setuid would add a root program to the
[setuid list](../03-architecture/security-model.md), and that is not decided. User-mode networking
through `passt` needs no privilege.

### PROJ has no transformation grids, and no port carries them

A transformation that needs a grid (NAD27 to NAD83, OSGB36, a national geoid) falls back to a
ballpark one or fails, naming the grid it wanted. proj is built with `ENABLE_CURL=OFF`, so grids are
never fetched on demand either; the transformations that need no grid are the ones the bundled
`proj.db` answers exactly.

### presenterm's seven stock syntax themes are not compiled here

`base16-ocean.dark`, `InspiredGitHub`, the Solarized pair and the rest come from the serialized
`default.themedump` inside the vendored `syntect` crate. Their `.tmTheme` sources are in syntect's
repository and not in the crate, and the only way to swap in a rebuilt dump is a patch to
presenterm's theme loading. Its grammars, and bat's themes, are compiled here by the `bat` port.

### `ping` and `tracepath` take no internationalised host name

`iputils` is built with `-DUSE_IDN=false`: with IDN on, both pass glibc's `AI_IDN` to `getaddrinfo`,
which musl rejects, and every lookup fails. A host name is looked up as written, so a non-ASCII name
must be given in its `xn--` form.

### Strawberry has no tag fetcher

The fetcher sends a song's fingerprint to AcoustID and MusicBrainz, so `strawberry` is configured with
`ENABLE_TAGFETCHER=OFF`. Its fingerprints are still taken, locally, so a moved or renamed file keeps
its play counts and ratings.

### LinuxCNC is the simulator only

The recipe removes the setuid bit upstream sets on `rtapi_app` and `linuxcnc_module_helper`, so
LinuxCNC runs its threads without realtime priority and cannot drive a machine. The latency
histogram draws with BLT, which has no Tcl 9 support and is not a port, so it has no menu entry.

### GTK 4 converts half-float pixels without F16C

GTK 4's F16C path is chosen at run time through a GNU indirect function (`ifunc`), and its
configure probe needs the compiler to accept one. musl's dynamic loader resolves no `ifunc`, so
the probe fails and `gtk4` is built without `HAVE_F16C`: every conversion between 16-bit float and
32-bit float pixels, in GDK's memory-format conversions and in the GL renderer, takes the portable
C loop. Rendering is the same; a high-bit-depth or HDR texture costs more CPU to convert. No port can
change this short of a C library with `ifunc` support.

### A SPICE viewer redirects only the USB devices you can already open

`spice-gtk` is configured without `spice-client-glib-usb-acl-helper`, the setuid helper that opens
a device node for an unprivileged client, so USB redirection from virt-manager reaches only the
devices whose nodes a udev rule already gives you. Folder sharing (phodav) and smart card
passthrough (libcacard) are enabled.

### Features whose library has no port

The ports below are built with a feature switched off because the library or program behind it is
not in the tree. A missing library becomes a port when it adds something a person on this desktop
would see; OpenImageDenoise with ispc, Embree, meshoptimizer, libsrt and librist are ports for that
reason. Each one below was weighed the same way and left out, for the reason in its row:

- **one feature**: it serves one optional feature of one or two programs, and the program is whole
  without it;
- **hardware**: it drives devices or buses the image has no other support for;
- **vendor**: it is a closed SDK or runtime, which is not built from source (see
  [what is not built from source](../01-philosophy/why-kdos.md#what-is-not-built-from-source));
- **superseded**: upstream has stopped maintaining it, or a port in the tree does the same work;
- **service**: it talks only to a network service or an account this desktop does not run;
- **large**: it is a stack of its own, with a build system and dependencies larger than the
  feature it adds;
- **X11**: it works only on an X11 session, and the session here is Wayland;
- **rule**: it belongs to something KDOS excludes, such as systemd or SELinux (see
  [Principles](../01-philosophy/principles.md)).

`python3 testing/optdeps.py --claims` lists every `kfs.md` line that says a dependency is not a port,
and `python3 testing/optdeps.py` lists every switch that turns off a feature a port does provide;
see [Testing](../05-developer/testing.md).

| Port | What is off | Missing | Why |
|---|---|---|---|
| `aalib` | The console mouse | gpm | one feature |
| `abiword` | Grammar check, thesaurus, MathML, summaries, the database, Psion and WMF importers; calendar, address-book, RDF and map fields | link-grammar, aiksaurus, gtkmathview, ots, libgda, psiconv, libwmf, Evolution Data Server, Redland, libchamplain | one feature each |
| `aircrack-ng` | The ESSID regular-expression filter | PCRE 1 | superseded |
| `alsa-utils`, `pipewire` | FireWire audio interfaces | libffado | hardware |
| `anthy-unicode` | The Emacs front end | emacs | one feature |
| `ark` | RAR, ARJ and `unar` formats beyond what libarchive reads | unrar, arj, unar | vendor (unrar's licence), one feature |
| `aubio`, `openimageio` | Manual pages | txt2man | one feature |
| `audex` | The AAC encoding profile | faac | one feature |
| `avidemux` | AV1, FAAC, Aften, DCA, Xvid and AMR encoders, VapourSynth input | aom, faac, aften, dcaenc, libdca, xvidcore, opencore-amr, VapourSynth | one feature each; dav1d decodes AV1 in every player |
| `avogadrolibs` | Space groups, symmetry and spectra plots | Spglib, libmsym, JKQTPlotter | one feature each |
| `baresip` | AMR, G.722, G.722.1 and packet-loss concealment codecs; ZRTP | opencore-amr, libg722, spandsp, gzrtp | one feature each |
| `blender` | VR, USD, MaterialX and Hydra; OSL shaders and OpenPGL path guiding in Cycles | OpenXR, USD, MaterialX, OSL, OpenPGL | large; no OpenXR runtime exists on this image |
| `boost` | The `stacktrace_backtrace` variant | libbacktrace | one feature |
| `brltty` | The Libbraille driver | libbraille | hardware |
| `btrfs-progs` | Converting a ReiserFS volume | reiserfsprogs | superseded: the kernel has no ReiserFS |
| `calligra` | EPS import, the Matlab/Octave formula tool, `slidecompare` | pstoedit, Cauchy's M2MML, the OpenOffice SDK | one feature each |
| `cantor` | The Julia back end | julia | large |
| `ceres-solver` | METIS ordering for the sparse solvers | METIS | one feature |
| `clamav` | The mail milter | libmilter | service: no mail transfer agent runs here |
| `codec2` | The FreeDV 2020 mode | LPCNet | one feature |
| `coin` | VRML JavaScript and image textures through simage | SpiderMonkey, simage | one feature; no consumer here uses them |
| `crun` | Container checkpoint and restore | CRIU | one feature |
| `cura` | The MakerBot writer | pyDulcificum | one feature |
| `darktable` | G'MIC and Unity launcher progress | G'MIC, libunity | one feature each |
| `dasher`, `gparted` | The GNOME help | yelp-tools | one feature |
| `deja-dup` | Restoring a Duplicity backup | duplicity | superseded: new backups use restic |
| `discover`, `dolphin`, `plasma-desktop`, `kdenetwork-filesharing` | Installing software through PackageKit | PackageKit | superseded: `kpkg` and Discover's KDOS backend install software |
| `dnsmasq`, `iptables` | Connection-tracking marks and labels | libnetfilter_conntrack, libnfnetlink | one feature; nftables is the firewall |
| `dolphin-plugins` | The Subversion, Mercurial, Bazaar and Dropbox plugins | Subversion, Mercurial, Bazaar, the Dropbox client | one feature each, vendor |
| `elisa` | UPnP browsing | UPnPQt | superseded: marked broken upstream |
| `enchant` | The nuspell, Hspell and Voikko providers | nuspell, hspell, libvoikko | one feature each; Hunspell and Aspell are the providers |
| `falkon` | The GNOME keyring password plugin | libgnome-keyring | superseded: KWallet keeps the passwords |
| `fastfetch` | The VDPAU, EET, RPM, ImageMagick 6 and QuickJS probes | libvdpau, efl, rpm, ImageMagick 6, quickjs | one feature each |
| `ffmpeg` | VDPAU, sndio, CUDA, NVENC and AMF | libvdpau, sndio, the NVIDIA and AMD SDKs | X11, vendor |
| `fluidsynth` | The MidiShare driver | MidiShare | superseded |
| `freecad` | Point clouds | PCL | large |
| `freedoom` | The PDF manuals | asciidoctor-pdf | one feature |
| `gcc-arm-none-eabi`, `gcc-avr`, `gcc-riscv64-unknown-elf` | Graphite loop optimisations | isl | one feature |
| `gdb` | CTF traces, source highlighting through GNU source-highlight, Intel Processor Trace, AMD GPU debugging, debuginfod | babeltrace, source-highlight, libipt, amd-dbgapi; elfutils is built without debuginfod | one feature each, vendor; Pygments colours the source |
| `gdk-pixbuf` | Sandboxed image loading through glycin | glycin | one feature; the built-in loaders read every format |
| `ghostwriter` | The MultiMarkdown exporter | MultiMarkdown | one feature; cmark and pandoc export |
| `gimp` | MNG, WMF and ILBM files; JavaScript and Lua plug-ins; full dashboard backtraces | libmng, libwmf, libilbm, gjs, lua-lgi, libbacktrace | one feature each |
| `git` | `git svn` and `git cvsserver`, which are removed from the package | Subversion's Perl bindings (SVN::Core); DBI with DBD::SQLite | one feature each |
| `glabels` | GNU Barcode and IEC 16022 symbologies; the Evolution address book | GNU Barcode, libiec16022, Evolution Data Server | one feature each; QR comes from qrencode |
| `gmsh` | CGNS, Mmg remeshing and PETSc solvers | CGNS, Mmg, PETSc | large |
| `gnucash` | The SQL back ends, online banking and online price quotes | libdbi, AqBanking, Finance::Quote and its Perl modules (JSON, LWP, HTML::TreeBuilder, DateTime) | one feature each; XML books are built |
| `gnumeric` | Psion and Paradox import, the database plug-in | psiconv, pxlib, libgda | one feature each |
| `goldendict-ng` | EPWING dictionaries | libeb | one feature |
| `gthumb`, `darktable`, `gtk3`, `gtk4` | The display profile in the image viewers; printer colour profiles in GTK's print dialog | colord | one feature: KWin applies a display's ICC profile itself |
| `gvfs` | Approving a WebDAV server's untrusted certificate; Windows network discovery | gcr, wsdd | one feature each; `network://` lists what Avahi finds |
| `ifcopenshell` | COLLADA and USD output, the RocksDB cache | OpenCOLLADA, USD, RocksDB | large |
| `imagemagick` | Liquid rescale, Ultra HDR, MagickCache; the PostScript base-35 font names (`Helvetica`, `Times-Roman`, …) | liblqr, libultrahdr, MagickCache, URW base-35 fonts | one feature each; the Liberation fonts are metric-compatible under their own names |
| `iperf3` | SCTP tests | lksctp-tools | one feature |
| `itinerary` | The vector map of a trip | maplibre-native-qt | one feature |
| `k3b`, `scummvm`, `vlc` | Musepack audio | libmpcdec | one feature |
| `kate`, `dolphin`, `discover`, `plasma-workspace`, `plasma-welcome`, `kmail`, `akregator`, `pim-sieve-editor` | The telemetry settings and usage reports | KUserFeedback | service |
| `kcrash` | The crash dialog | DrKonqi | one feature; a crashed program is restarted |
| `kdeconnect` | Remote input from the phone on X11 | libfakekey | X11 |
| `kdepim-runtime` | The EteSync resource | libetebase | service |
| `kdesdk-kio` | The `perldoc:/` worker | Pod::Parser | one feature |
| `kdev-python` | Style checking | pycodestyle | one feature |
| `kfilemetadata` | AppImage metadata | libappimage | one feature |
| `kget` | The MMS stream plugin | libmms | superseded: no release since 2014 |
| `kimageformats` | JPEG XR | jxrlib | one feature |
| `kinfocenter` | The X Server and GLX pages | `xdpyinfo`, `glxinfo` | X11 |
| `konsole` | The Kapsule container profiles | Kapsule | one feature |
| `kosmindoormap` | Routing inside a building | recastnavigation | one feature |
| `kpty` | A login record for each terminal | libutempter | one feature |
| `kscreenlocker` | Unlocking with a security key | pam_u2f | one feature |
| `ktexteditor` | `.editorconfig` files | EditorConfig | one feature |
| `kup` | Versioned backups with `bup` | bup | one feature; restic keeps versions |
| `ldns` | A default DNSSEC trust anchor: `drill -S` needs `-k` | `/etc/unbound/root.key` (unbound, dns-root-data) | one feature |
| `libbluray` | AACS-encrypted discs; BD-J menus | libaacs, Apache Ant | vendor (keys), large (a JDK under ffmpeg) |
| `libcap-ng` | `cap-audit` | libaudit | one feature; no audit daemon runs here |
| `libgpod` | The SCSI path for older iPods | sg3_utils | hardware |
| `libheif` | The rav1e, kvazaar, uvg266, VVC and OpenJPH codecs | rav1e, kvazaar, uvg266, vvdec, vvenc, OpenJPH | one feature each |
| `libiscsi` | iSER transport | rdma-core | hardware |
| `libisofs`, `libisoburn` | Jigdo templates | libjte | one feature |
| `libmysofa` | `mysofa2json` and the tests | CUnit | one feature |
| `libnice` | UPnP port mapping | gupnp-igd | one feature |
| `libsigrok` | VISA, GPIB and IEEE 1284 instruments | librevisa, libgpib, libieee1284 | hardware |
| `libtiff` | LERC compression | lerc | one feature |
| `libvips` | GIF save through cgif, Ultra HDR, NIfTI, OpenSlide and quantizr | cgif, libuhdr, niftilib, OpenSlide, quantizr | one feature each |
| `libvirt` | The `iscsiadm` storage back end | open-iscsi | one feature; `iscsi-direct` is built |
| `libwacom` | `libwacom-show-stylus` | pyudev, python-libevdev | one feature |
| `libxfce4ui` | Startup notification and the system information pane | startup-notification, libgtop | one feature each |
| `linuxcnc` | The latency histogram | BLT | one feature |
| `lmms` | Carla and sndio | Carla, sndio | one feature each |
| `lokalize` | Translating an ODF document | translate-toolkit | one feature |
| `ltrace`, `lvm2`, `gettext`, `nbdkit` | SELinux and Smack labels | libselinux, Smack | rule |
| `luanti` | The LevelDB and Redis map databases | LevelDB, Redis | one feature each; SQLite stores the worlds |
| `mbpoll` | GPIO RTS control | libpiduino | hardware |
| `mpv` | Colour ASCII output, VapourSynth, JavaScript scripts, CUDA | libcaca, VapourSynth, mujs, CUDA | one feature each, large, vendor; Lua scripts run |
| `mumble` | Remote control over Ice | ZeroC Ice | one feature |
| `nbdkit` | The libblkio, libguestfs and VDDK plug-ins; USDT probes | libblkio, libguestfs, VMware VDDK, systemtap | large, vendor |
| `net-snmp` | The process-table regular-expression match | PCRE 1 | superseded |
| `netgen` | Python type stubs | pybind11-stubgen | one feature |
| `ntfs-3g` | Boot geometry from `libhd` | hwinfo | one feature |
| `obs-studio` | The browser source and What's New panel, `obs-websocket`, WebRTC output, AJA, DeckLink, NVENC and Quick Sync, sndio | CEF, websocketpp, libdatachannel, the AJA, Blackmagic, NVIDIA and Intel SDKs, sndio | large, one feature each, vendor |
| `opencascade` | Image export through FreeImage | FreeImage | one feature |
| `openimageio` | Ptex, Ultra HDR, OpenJPH, RED R3D and Nuke | Ptex, libuhdr, OpenJPH, the R3D SDK, the Nuke SDK | one feature each, vendor |
| `opensc` | The German eID's PACE channel and `npa-tool` | OpenPACE | one feature |
| `opensubdiv` | The Ptex, CUDA and OpenCL back ends | Ptex, CUDA | one feature, vendor |
| `orca` | Mouse review | libwnck | X11 |
| `perf` | libpfm4 event names, CTF output from `perf data`, USDT probes | libpfm4, babeltrace2, systemtap | one feature each |
| `picard` | Installing plug-ins from git | pygit2 | service |
| `pipewire` | ROC streaming, LC3plus, LDAC decoding, spandsp | roc-toolkit, lc3plus, an LDAC decoder, spandsp | one feature each |
| `plasma-keyboard` | Haptic feedback | feedbackd | hardware |
| `plasma-nm` | OpenConnect and vpnc connections | NetworkManager's openconnect and vpnc plugins | one feature each; `openconnect` runs in a terminal |
| `powerman` | Genders node lists and TCP wrappers | genders, tcp_wrappers | one feature, superseded |
| `printrun` | `pronterface` and `plater`, the wx front ends | pyglet, puremagic | one feature; `printcore` and `pronsole` run |
| `profanity` | OTR encryption | libotr | superseded: OMEMO is built |
| `python3-meshtastic` | `mesh-analysis`, the web dashboard over a node's log | dash, dash-bootstrap-components, plotly, pyarrow | large; the `meshtastic` command is whole without it |
| `python3-nbconvert` | The web-PDF exporter | Playwright | large; the LaTeX PDF exporter runs |
| `python3-pyqt5` | The QtBluetooth, QtNfc, QtLocation, QtPositioning, QtSensors, QtWebChannel, QtRemoteObjects, QtTextToSpeech, QtXmlPatterns, QtQuick3D and QtWebKit bindings | those Qt 5 modules | one feature each |
| `qemu` | Guest 3D (`virtio-vga-gl`), the manual pages | virglrenderer, sphinx_rtd_theme | one feature each; a guest renders in software |
| `qgis` | GRASS, Oracle and SAP HANA | GRASS GIS, the Oracle and SAP client libraries | large, vendor |
| `qt6-qtquick3d` | Timeline animations in `balsam` imports | Qt Quick Timeline | one feature |
| `qt6-qtspeech`, `webkitgtk`, `webkitgtk6` | The Flite engine and Spiel; gamepads in WebKit | Flite, Spiel, libmanette | one feature each; speech-dispatcher is the engine |
| `qutebrowser` | The Brave ad-block engine | python-adblock, tldextract | one feature; the host-list blocker works |
| `recoll` | CHM files | libchm | one feature |
| `rmpc` | The visualiser pane | cava | one feature |
| `samba` | Snapper snapshots, GlusterFS, CephFS, FAM, DMAPI, LTTng, `winexe` | snapper, GlusterFS, Ceph, FAM, DMAPI, LTTng, a mingw cross-compiler | large, service |
| `satdump`, `sdrangel`, `urh` | SDRplay, Aaronia and XTRX radios | the SDRplay API, the Aaronia RTSA SDK, libxtrx | vendor, hardware |
| `skopeo` | Sequoia signatures | podman-sequoia | one feature; GnuPG signing is built |
| `slang` | The PCRE module | PCRE 1 | superseded |
| `smplayer` | The MPlayer back end | MPlayer | superseded: mpv is the player |
| `solid` | Naming a music player as one | media-player-info | one feature |
| `sonnet` | The Hebrew and Finnish spelling engines | Hspell, Voikko | one feature each |
| `texlive` | `xindy` | clisp | large |
| `texstudio` | The embedded terminal | QTermWidget | one feature |
| `thunderbird` | The RLBox WebAssembly sandbox | wasi-sdk | large |
| `timg` | OpenSlide images | OpenSlide | one feature |
| `viking` | Map rendering through Mapnik | Mapnik | large |
| `vlc` | RTSP through live555, FireWire capture, SMB shares, Icecast output, teletext, ARIB subtitles, Kate and Tiger subtitles, the visualisations, UPnP and mDNS discovery, CDDB, AV1 encoding, the shine, DCA, Tremor, Daala, OpenAPV, BPG, x262, oneVPL and FluidLite codecs | live555, libdc1394, libraw1394, linsys, libdsm, libsmb2, libshout, zvbi, aribb24, aribb25, libkate, libtiger, goom, projectM, vsxu, libupnp, microdns, libcddb, aom, shine, libdca, tremor, daala, openapv, libbpg, x262, oneVPL, FluidLite | one feature each, hardware |
| `vorbis-tools` | Kate streams | libkate | one feature |
| `w3m` | X-Face headers | compface | one feature |
| `wcslib` | PGPLOT plotting | PGPLOT | one feature |
| `weechat` | PHP and JavaScript scripts | PHP's embed library, V8 | large |
| `wireshark` | cpuinfo, and the AMR, G.729, iLBC and spandsp RTP decoders | cpuinfo, opencore-amr, bcg729, libilbc, spandsp | one feature each |
| `wv` | Embedded Windows Metafile pictures | libwmf | one feature |
| `xastir` | Speech | festival | one feature |
| `yosys` | `show` without `-viewer` | xdot | one feature |
| `zint` | The GS1 Syntax Engine | libgs1encoders | one feature; zint checks GS1 data with its own rules |

## Building

### The base's run-time needs are proved only for packages installed since they were recorded

`kpkgadd` records what each package's files need at run time (sonames, `#!` interpreters, QML and
Python imports, the recipe's `runtime =`), and `087_gates.sh` runs `imageset.py --verify`, which
fails the image when a kept package's edge is met only by a package the trim dropped (see
[Run-time needs](../03-architecture/packaging.md#run-time-needs)). A package installed before the
records existed has none and is counted as unmeasured, not failed, so the gate proves nothing about
a tree until its packages are installed again; every package of the current build tree is
unmeasured. Measured with `kpkg needs --scan` over that tree, the base's QML and Python imports are
all met inside it, and four edges are not: `libisoburn`'s `xorriso-tcltk` runs `#!/usr/bin/wish`
and Tk is not in the base (linkcheck fails the same script), and Mesa's `libclang-cpp` and
`kdeplasma-addons`' two Qt WebEngine libraries are linked by packages built from older recipes,
which the current recipes do not link. What the records cannot see: QML compiled into a program's resources, which
is most of an application's; a library opened with `dlopen()` or a plugin found by directory that
no recipe names in `runtime =`; a Python import inside a submodule, behind `try:` or of a module
installed after the importer; and a module a host program registers in-process, such as KWin's
`org.kde.kwin`, which the gate counts as met by nothing rather than failing.

### The base ISO fits one release asset by about 30 MiB, and no boot is timed per codec

`110_iso.sh` fails a base ISO over 1,900 MiB. The base's run-time files squash to 1,781 MiB with
the default zstd:19, an ISO of about 1,870 MiB (see
[The codec](../05-developer/build-system.md#the-codec)); that is measured on a copy of the files,
not on a built ISO, and a recipe that grows by 30 MiB of compressed files stops a release build
at the gate. The ledger `088_ledger.sh` writes names the largest packages and why each is there,
`KDOS_ISO_COMP=xz-bcj` is about 100 MiB smaller, and `KDOS_ISO_GATE=0` builds the same image as
a developer one. The codec was chosen from decompression measured on one core, not from a boot
to Plasma timed under QEMU, so whether xz-bcj boots within 10% of zstd is not known.

### A KDOS settings page's Help button needs the Help Center

The Help button of a KDOS page in System Settings opens `help:/kdos/...`, the address its
`X-DocPath` names, and the desktop hands a `help:` address to the Help Center, which the
installation medium does not carry because it pulls in Qt WebEngine. Until
`kdos update install khelpcenter` (or the `kde-system` group) adds it, `kdos-doc <page>` opens the
same page in the browser. See [Help](../04-programs/kdos-settings.md#help).

### The image's run-time facets have not been through a built ISO

`017_stage.sh` strips every package of the base to its run-time
[facet](../03-architecture/packaging.md#facets), and the rules are measured on a full build tree
(949 MB of the base's 5.46 GB), but no ISO has yet been built and booted with them: `kpkg facets
--strip` and an install under `FACETS=run` have run against the self-test's fixtures only. Two
things the gates cannot see would show there. A program that `dlopen()`s an unversioned
`lib<x>.so` finds nothing once the development link is gone; no program in the base links one by
`DT_NEEDED`. And what an image update does with a package the machine keeps whole, installing it
whole again in the slot after the world step, has been planned by the self-test's dry run only:
no image update has yet run it into a slot.

### Build times and snapshot sizes are measured for part of the build only

The thirteen phases under `script/phases/` and the ports tree filed on shelves pass preflight,
`testing/phaseclosure.py` and the self-test (see the ports tree under
[Host and packaging](status.md#host-and-packaging) in Status). A phase's progress is the names in
its lists (`packages.d/*.txt`, or `packages.txt`) that `ls build/fs/var/lib/kpkg/db` also shows.
`build/snapshots/timings.json`, the only record of how long a port took to build, holds step times
for every phase from `00_cross` to `43_toolkits` and for part of `44_apps`; `50_desktop`,
`60_kernel` and `70_image` have no figures in it. See [Build times](../../kfs/17-appendices/sbu.md).
How much disk a complete set of snapshots takes has not been measured.
Layered snapshots have been measured only on a synthetic ten-phase tree, where they came to 23% of
full ones. A run writes snapshots
only when given `--snapshot` or the picker's `S`; then every phase up to `50_desktop` takes one,
the first of each path whole and the rest as layers, and `60_kernel` and `70_image` leave
`KDOS_SNAPSHOT_PATHS` empty in their `phase.env` and are re-run instead of restored.

### Some command-line programs build after Qt

A port builds in the phase its whole dependency closure allows, and some closures reach further
than the program does. Each of these builds in `43_toolkits`, after GTK and Qt, for its own reason:

- `podman`, and `distrobox` with it, because `podman` depends on `gpgme`, `gpgme` on `gnupg`,
  `gnupg` on `pinentry`, and `pinentry` on `qt6-qtbase` for its dialog. A terminal-only
  `pinentry` would cut that chain.
- `qemu`, and `libvirt` with it, because `qemu` depends on `gtk3` and `vte3` for its display
  window. Only a `qemu` built without that display could build earlier.
- `sane-backends`, because it depends on `poppler`, which depends on `qt6-qtbase` and on `gpgmepp`.
  Cutting that chain takes a `poppler` without its Qt binding and its signature support.

`kdos-tools` builds in `42_graphics` because `kbd` depends on `libxkbcommon`, which the graphics
stack provides. A build that stops before those phases has none of them.

### The image's installer is rebuilt only when its mark is gone

The `kinstall` on the image is compiled by `10_bootstrap`'s `130_kinstall.sh`, straight from
`src/system/kdos-installer` and `src/system/kdos-appbox/catalogue.c`. That step is skipped
whenever `build/mark/bootstrap/kinstall` exists, whatever changed in either directory, so an edit
to the installer or to `catalogue.c` reaches the image only when the step runs again: remove the
mark, or name the step explicitly with `--steps 10_bootstrap:130_kinstall.sh`, which sets
`KDOS_REPLAY=1` for it.

The `kdos-installer` port builds the same program, with the backend in `src/system/kinstall-core`
compiled into it, and no phase list names it. Its recipe hash covers its own directory and
`src/libs` but neither `kinstall-core` nor `catalogue.c`, so a `kpkg` build of that port counts an
edit to either alone as nothing to do.

### The package store keys only what a port declares

With `KDOS_PKG_STORE=1` a port is installed from `build/pkgstore` when its key matches (see
[The package store](../03-architecture/packaging.md#the-package-store)). The key covers the recipe,
the environment, the bootstrap phases and the exact bytes of every declared dependency and of the
base toolchain, and an `X:` line re-checks every library the package's ELF files link. It does not
cover:

- **A dependency the build detected by itself and did not link.** A configure script that switches
  a feature on because it found a header, a `pkg-config` file or a tool, a plugin opened with
  `dlopen`, or a static library leaves no `DT_NEEDED` entry, so a change to that port reuses a
  package built against the old one.
- **Files under `fs/`.** The overlay reaches the tree through `10_bootstrap` and later syncs; only
  `fs/etc/passwd`, `group` and `ld-musl-x86_64.path` are in the salt.
- **Side effects outside `$PKG`.** A `postinstall.sh` runs on a store hit as on a build, but
  anything a `build.sh` writes into the live tree rather than the package is not in the stored
  package and does not happen on a hit.
- **Caches under `$HOME`.** A build that reads `.cargo`, `.cache` or a similar directory under the
  build's `HOME` (`/var/tmp/kdos-home`) depends on what an earlier build left there, which no key
  names.

A release build uses `KDOS_PKG_STORE=0` or `check`; `check` rebuilds every hit and reports a
difference in `build/logs/pkgstore-check.log`.

### The kernel package differs between two builds of the same tree

Every other package is byte-identical when rebuilt (see
[Reproducible packages](../03-architecture/packaging.md#reproducible-packages)). `linux` pins its
version string, its build user and host and its module-signing key, but signs each module with
ECDSA, which draws a fresh random number for every signature, so every module's appended signature
differs between two builds and `kpkg verify --repro linux` reports them. Whether anything else in
the package differs has not been measured.

## Sources and publishing

### The source archive has one public location

Upstream sources live only as release assets on `kunaldawn/kdos`. If it cannot be reached, `make
fetch` falls back to each recipe's upstream URL, which works only while upstream still serves the
exact file. `KDOS_SOURCES_BASE` can point `make fetch` at another copy laid out the same way, and
nothing publishes such a copy. See
[Developing](../05-developer/developing.md#where-sources-come-from).

### Two emulator cores may not go on sold media

`libretro-snes9x` is under the Snes9x licence, which forbids commercial distribution, and
`libretro-genesis-plus-gx`'s licence forbids selling it or using it in a commercial product. An
image or a medium that is sold must leave both out.

### A clone does not check what it pushes unless you enable the hook

`script/hooks/pre-push` runs only after `git config core.hooksPath script/hooks`. It refuses a push
that:

- breaks the ports layout (a recipe not exactly one listed shelf down, a name held by two ports,
  a reserved or port-named shelf);
- adds or changes a `.patch` or `.diff` under `ports/` or `src/` that has no `Subject:` line saying
  what it changes;
- leaves either book failing `testing/docscheck.sh` or `testing/kfscheck.py`, a stale recipe stamp
  included, which `make book-stamp` or `make book-data` refreshes;
- names a source hash the archive does not hold.

Without it, a push can carry a layout no build resolves, a patch *KDOS from Scratch* cannot
describe, a book that does not match the tree, or a source nobody has published, and every
other clone's `make fetch` then depends on upstream still serving that file. Setting
`core.hooksPath` replaces `.git/hooks` as a whole, so git-lfs's hooks, or any other installed
there, stop running in that clone.

The layout check reads the tree of each pushed ref's commit, not of every commit between it and the
remote, so a commit in the middle of a push that breaks the layout passes when a later one repairs
it; preflight is what checks the working tree. The book checks are the opposite: they read the
working copy, not the pushed commit, so they answer for the pushed commit only when that is what is
checked out. With the hook on, a push that introduces a new source hash is also refused when the
archive cannot be reached or answers anything but 200 or 404, because the missing source cannot
then be ruled out; such a push made offline needs the archive bypass. Unless that bypass is set, every push is refused while
`KDOS_SOURCES_BASE` is empty or names a local directory, since neither can confirm that a source
is published. Each check has its own bypass for one push, leaving the others in force:
`KDOS_SKIP_LAYOUT_CHECK=1`, `KDOS_SKIP_PATCH_CHECK=1`, `KDOS_SKIP_BOOK_CHECK=1` and
`KDOS_SKIP_PUBLISH_CHECK=1`, set in front of `git push`.

### An offline `kdos rebuild` from the medium has not been run to the end

A stick built with `KDOS_ISO_SOURCES=1` carries, under `sources/`, the ports tree (`/ports`
without its dot-directories, so every fetched source beside its recipe), `src/`, `script/`,
`fs/`, the top-level `Makefile` and `Dockerfile`, and `sources/binhost` when the build wrote one.
A `sources/SOURCES` stamp records the number of ports, the size and the build time, and
`kdos rebuild` prints it before it starts. `kdos rebuild` refuses a tree that lacks
`script/kdosbuild.sh`, `ports/core`, `fs/etc` or the orchestrator's sources under
`src/devtools/kdosbuild`, copies the tree somewhere writable and runs the
orchestrator there with `KDOS_WORKSPACE` naming the copy, which `script/env/host.env`, the
environment of `00_cross` and `10_bootstrap`, uses in place of `/workspace`. No rebuild from a medium has been run through every
phase. See [kdos rebuild](../04-programs/kdos-command.md#kdos-rebuild) and, for the build it runs,
[How KDOS is built](../05-developer/how-kdos-is-built.md); the copy itself is described in
[Repository layout](repository-layout.md).

## Testing

### KDOS's Qt components are not compiled by the self-test on a bare host

Their Qt 6, KDE Frameworks, Plasma and KWin libraries are not there, so `make plasmatest` runs only
in the development image (`testing/devdeps-image.sh`), and the self-test's D-Bus daemon blocks
report as skipped on a host without sd-bus headers.

### No test builds the distribution

`testing/selftest.sh` runs the orchestrator end to end against a synthetic two-phase tree (forking
steps, writing logs, taking and restoring snapshots) and a three-phase one whose snapshots are
layers, each restored and compared entry by entry with the tree it came from, and `kdosbuild --selftest` asserts its view
geometry, log classifier, phase retry, jobserver and timings merge. `testing/selftest.sh` also builds `kpkg` and drives it against
synthetic, source-less ports, covering reproducible packaging, shared indexes, file ownership,
recipe-hash skipping, binhost signing and package deltas. A package built twice to compare its
bytes is that synthetic port, not a real one, and no test reconstructs a pack from a pack delta
(`kdos-pack delta`; see [Deltas](../03-architecture/packs-and-boxes.md#deltas)). The real recipes
can only be tested by building the whole distribution, which takes most of a day and a container;
see [What a build costs](../05-developer/how-kdos-is-built.md#what-a-build-costs).

### No harness exercises the source archive end to end

Nothing publishes to an archive and fetches back from it under test. `ports/publish` accepts
`KDOS_GITHUB_API` and `KDOS_GITHUB_UPLOADS`, and `ports/fetch` accepts `KDOS_SOURCES_BASE`, so a
local stand-in could be used, but no test does so. What `testing/preflight.sh` checks is that the
scripts parse and are executable, that git tracks no recipe-hashed archive, that the ignore rules
cover every source suffix and `ports/.srccache/` and ignore no tracked fixture, and that
`ports/sources.idx` holds every format-2 rule of `src_index_problems` in `ports/srclib.sh`.

### `kdos doctor` cannot check hardware a machine does not have

Several of its hardware checks report *skip*, with a reason, when the device they check for is
absent, as it is in a virtual machine: the wireless regulatory database with no wireless device,
the reachability of serial, camera and instrument device nodes with none plugged in, and the Intel
SOF audio and NVIDIA GSP firmware checks on a machine with neither the device nor the firmware. A
skip says the check could not answer, not that it passed.

### The rig cannot show a live microphone level on its own

Its emulated sound card gives the guest no capture signal, so a recording level meter stays flat
even with the rig's `--audio` option on. Loading the `snd-aloop` kernel module in the guest, from a
root script, gives it a loopback capture device to record from. See
[Testing](../05-developer/testing.md#a-real-capture-device-with-no-emulator-flag).

### Phosphor mode is not in any rig photograph

The rig's virtual display puts KWin on software rendering, where the CRT effect refuses to load.
A photograph of it needs the rig's GL option or `make run-hw`.

### The suspend flow finds no screen after resume

`testing/flows/05-suspend-resume.sh` fails on the rig. The kernel completes the suspend, the
session comes back locked and the password unlocks it, but both photographs after resume are QEMU's
*Display output is not active* frame, a pointer move and Shift do not bring the picture back, and
the guest's DRM connector reports the output on throughout. Whether a resumed KDOS loses its
display or the rig's `virtio-vga` loses its scanout over S3 has not been measured on real
hardware. See [The login flows](../05-developer/testing.md#the-login-flows).

## Documentation

### Some measurements in this book are quoted rather than re-taken

Contrast ratios, launch times, freeze ratios and transport throughput figures were each measured
once, under the conditions stated beside them, and are repeated rather than measured again for every
revision. A timing or a ratio depends on the machine it was taken on, so a figure quoted here is a
record of those conditions and not a guarantee for yours.

## See also

- [Decisions](../01-philosophy/decisions.md) — what is absent on purpose, and why
- [Status](status.md) — maturity per subsystem, and the evidence behind each verdict
- [How KDOS differs](../01-philosophy/how-kdos-differs.md) — what the design gives up next to other distributions
- [The ports catalogue](ports-catalogue.md) — every port, so a missing one can be told from one built without a feature
- [The security model](../03-architecture/security-model.md) — the full statement of what is not protected

<!-- book-nav -->
---

*Part VI — Reference, chapter 43.* Previous: [42. Repository layout](repository-layout.md) · [Contents](../README.md) · Next: [44. Status](status.md)
