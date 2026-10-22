# Known gaps

This chapter lists what KDOS does not do, and what it does but has never been exercised on real
hardware or against a real peer. It is for anyone using, administering or contributing to KDOS who
has hit something that seems missing and wants to know whether it is a bug, a limitation or a
choice. If what you are looking for is described here, KDOS does not do it, or has not been shown
to; if it is not described here and it misbehaves, it is probably a bug worth reporting. No earlier
chapter is required reading, but [Architecture overview](../03-architecture/overview.md) gives
context for the component names used below, and terms such as *surface*, *box*, *pack* and *rig*
are defined in the [Glossary](glossary.md).

Each entry is headed by a one-line statement of the gap, followed by what exactly is missing, why,
and what to do instead where there is something. Some entries are known defects: behaviour the
code has today that contradicts its own documentation or usage text. They are listed so that a
reader who meets one knows it is known. The sections follow the shape of the system: the desktop,
applications and boxes, hardware, installation, boot and updates, security, ports built without a
feature, upgrades, building, sources and publishing, testing, and this book. Every entry describes the tree
as it stands; a gap that closes is removed rather than marked as closed (see
[Principles](../01-philosophy/principles.md#documentation-describes-the-present)).

Three other chapters answer the neighbouring questions. What is absent on purpose, and the
reasoning behind it, is in [Decisions](../01-philosophy/decisions.md); what that costs next to
other distributions is in [How KDOS differs](../01-philosophy/how-kdos-differs.md#what-you-give-up);
how mature the parts that do exist are is in [Status](status.md).

## Desktop

### Drag and drop carries text and files, and nothing else

`text/plain` and `text/uri-list` are offered and accepted; there is no MIME negotiation, no
deferred transfer and no image payload. Only the trash accepts a drop on the desktop, which is a
narrowing rather than a gap; see [Decisions](../01-philosophy/decisions.md#narrowings). Both
directions are implemented in `libkwl`, and no test asserts either of them: nothing has been dragged
with a pointer between a KDOS surface and a boxed application. See [Status](status.md).

### KDOS's own surfaces have not been seen at a fractional scale

On an output scaled by a fraction, such as 1.5, `libkwl` loads the chrome font at the device size and
hands `kdos-comp` a buffer of device pixels behind a viewport, so the panel, the menus and the other
surfaces are drawn crisp at 1.5 rather than drawn at 2 and shrunk. What is measured is the arithmetic
and the paint: the device cells in a build container with the image's fonts, and the real flush
walked through fractional scales in a harness with a simulated compositor. No KDOS surface has been
photographed on a fractional output, on the rig or on hardware, so the look of the result is
unconfirmed. A toplevel such as `kdos-term` is told its scale only once it is on a screen, so its
first frame is drawn with the font as named and the next at the device size. See
[The fractional scale](../05-developer/c-libraries.md#the-fractional-scale).

### Emoji draw everywhere except the text console on `tty1`

Noto Color Emoji ships, and fontconfig resolves the generic `emoji` family to it, so every surface
with a pixel layer draws them: a `kdos-term` window, the panel, a boxed application. The Linux
virtual terminal's font holds 512 monochrome glyphs, which is a kernel limit, and a character the
font does not carry renders as a blank cell. An emoji on `tty1` is therefore a blank cell.

### A tray menu does not update while it is open

The menu tree is read when the menu opens, and `LayoutUpdated` is not watched. This is deliberate: a
menu whose rows move under your hand activates the wrong row. `AboutToShow` is sent first and its
reply is waited for, so whatever the application fills in at that moment is the one update taken.
The rows, their submenus, toggles, icons and shortcuts are all drawn; see
[kdos-shell](../04-programs/kdos-shell.md). An item that publishes no menu path is sent
`ContextMenu` and opens whatever window it has of its own.

### Workspace occupancy is inferred, not reported

The workspace protocol reports active, urgent and hidden, but has no "there are windows here", so
the panel marks a workspace occupied when a window on it is not minimised. That is right for every
workspace you have visited, and says nothing about the rest.

### A corporate VPN is a terminal program

`openconnect` ships and speaks AnyConnect, GlobalProtect, Fortinet and Pulse, with `vpnc-script`
giving the tunnel its routes, but there is no NetworkManager plugin for it, so it does not appear
in the network surface beside Wi-Fi and OpenVPN. Neither NetworkManager's openconnect plugin nor its
vpnc plugin is a port. Upstream's openconnect plugin needs `webkit2gtk` for the dialog the VPN
service hands authentication to; the `webkitgtk` port provides that library, and no recipe builds
the plugin. WireGuard and OpenVPN are the two VPN types that do appear.

### A fingerprint unlocks `sudo` and nothing else

`pam_fprintd` is in `sudo`'s PAM stack ahead of the password. The lock screen checks the shadow
file through `kdos-checkpass`, and the console login is shadow's, built without PAM, so neither
asks for a finger. Enrolment is a terminal command run as root, `sudo fprintd-enroll <user>`,
because fprintd's polkit actions allow only an active session, which nothing here ever is. No
surface offers enrolment. For an account with a fingerprint enrolled, `sudo` over SSH waits on the
machine's reader before asking for the password: the only remote-login test the module has without
logind is `PAM_RHOST`, which `sudo` never sets.

### X11 and Qt 5 applications get no input method

Xwayland gives its X clients no text-input protocol, and fcitx5 is built without the XIM frontend
that X clients use. Qt 5's Wayland plugin (qtwayland 5.15) speaks only text-input-unstable-v2, and
`kdos-comp` serves text-input-v3, so a Qt 5 application on Wayland gets no input method either;
there is no `fcitx5-qt` port that would give Qt 5 a direct route. CJK input and the on-screen
keyboard's automatic raise therefore reach GTK 3, GTK 4 and Qt 6 applications on Wayland, host or
boxed. See [the session](../03-architecture/session.md#input-methods).

### There is no input-method configuration tool

The one upstream ships, `fcitx5-configtool`, is a Qt program that is not a port. fcitx5 is configured through
its text files.

### A screen reader reads the applications, not the desktop's own windows

Orca is a host port, with at-spi2-core's accessibility bus, for the native GTK and Qt
applications, whose toolkits publish a tree of accessible objects. It is set up to speak through
speech-dispatcher and to reach braille through BrlAPI. Both are built and on the image, and
neither has been run on it. Nothing starts it: KDOS runs no autostart agent, so it is started by running
`orca`. The keys typed into other windows reach it through the compositor's
[keyboard monitor](../04-programs/kdos-comp.md#the-keyboard-monitor), which is the one interface of
its family `kdos-comp` serves: there is no pointer locator (`PointerLocator`), and Orca's mouse
review reports itself unavailable, because it needs Wnck, which is X11-only and not a port.

The desktop's own windows are not read. `kdos-comp` draws pixels, and a KDOS surface composes its
own cells and publishes no accessible tree. Each surface does keep a per-frame record in `libktui`
of what its widgets would say aloud. On every frame, controls of ten roles (button, check box, radio button, input, list, table, tab, choice, text
area and slider), menus included, write their role and their position in the set to it, and a name
and a value where the widget holds them rather than the caller's own draw callback. Nothing carries
that record out of the process: there is no socket, no bus name and no bridge, and the only thing
that reads it is the library's self-test. What is missing is a route and a client rather than the
material. A boxed application has its own accessibility registry, opted into with
`~/.config/kdos/a11y`; whether the host's Orca reaches it has not been tried. See
[Accessibility](../02-user-guide/accessibility.md).

### There is no graphical login screen (greeter)

`tty1` is handed to `agetty` by `kdos-login`, which logs in the account `login.conf` names
automatically and otherwise asks for a password at the ordinary text prompt. There is no account
chooser, no session chooser and no surface of any kind before the shell starts. Adding one needs a
Wayland client that runs before the compositor, in the way the lock screen does, and none exists.

### Nothing starts the screensaver on idle

`kdos-saver` has seven effects under eight names (`rain`, `art`, `bounce`, `matrix`, `pipes`,
`starfield`, `fire`, `clock`, where `bounce` is another name for `art`) and six golden reference
frames under `testing/goldens`, but the compositor's idle sequence dims, starts `kdos-lock` and
powers the outputs off, and never starts a saver. `comp.conf` has `idle_dim`, `idle_lock` and
`idle_off` and no key for a saver; see [Idle, dim, lock and
lid](../04-programs/kdos-comp.md#idle-dim-lock-and-lid). You reach it with Super+Shift+L, which the
shipped `rc.xml` binds, or by running it yourself.

### The control centre has no pointer page

Pointer speed, acceleration profile, natural scroll, tap-to-click, the tap button map and
disable-while-typing are libinput settings that the compositor's `<libinput>` block in
`~/.config/kdos-comp/rc.xml` accepts, and no surface here offers them. The shipped `rc.xml` carries
that block commented out, as a template, and sets none of them, so the compositor's own defaults
apply, with tap-to-click on. The control centre's Input page says where the template is and stops
there.

### A touchpad scroll is not one-to-one with the finger

Every surface here scrolls in whole wheel ticks, and `libkwl` makes one tick of a touchpad's stream
per ten axis units (libinput reports a finger's travel in units it scales like pointer motion),
which a scrolling list spends as three rows
(`SH_WHEEL_ROWS`). So a list moves several times further than the finger did. The movement between
ticks glides and a flick coasts, but the ratio stays: matching it to the finger means knowing how
far each surface moves per tick, and the feel has not been judged on a touchpad. See
[Input the backend cleans](../05-developer/writing-desktop-software.md#input-the-backend-cleans).

### The launcher's `files` setting does nothing

`launcher.conf`'s `files` key is written by the control centre and read by nothing.
`kdos-launcher` searches applications only, and `kdos-palette` always includes files once you have
typed three characters, whatever the file says. Neither reads `~/.cache/kdos/plocate.db`: the
palette runs `fd` over `$HOME`, and the index `kdos-updatedb` builds nightly is used only by the
`locate` command.

### Nerd Font icons are blank on `tty1`, and the shipped configurations turn them off everywhere

They are private-use characters, and the console font cannot carry them, for the reason given under
emoji above, so an icon in front of a filename shows as a hole. The face itself is on the image:
`nerd-fonts-symbols` installs Symbols Nerd Font and its Mono variant under
`/usr/share/fonts/nerd-fonts`, and its `66-nerd-font-symbols.conf` accepts the Mono face as a
fallback for `monospace`, `Terminus` and `Terminus (TTF)` and the proportional face for `sans-serif`
and `serif`, so anything with a pixel layer can draw one. What turns them off is each program's own
configuration: the `yazi` theme that `kdos theme` generates empties all five `[icon]` arrays, the
shipped `starship.toml` uses plain text and box drawing only, the `eza` aliases in
`/etc/bash.bashrc` say `--icons=never` rather than relying on a default, and `lazygit`, for which
KDOS installs no configuration, draws none by default. To turn them back on, configure those four
programs one at a time, knowing the same shell on `tty1` will show holes. Nothing patches the
console font, so on `tty1` the icons cannot be restored.

### Two shipped chords and two routes show nothing

The shipped `~/.config/kdos-comp/rc.xml` binds `W-C-v` to bare `kdos-clip`. Without an argument
that program is the clipboard-history daemon, not the picker, so the chord opens no window; the
second daemon unlinks and rebinds the history socket and takes the history over from the copy the
compositor supervises. Binding the chord to `kdos-clip --pick` in your own `rc.xml` opens the
picker. `W-C-p` runs `kdos-energy`, the per-application energy report, which prints to standard
output and has no window, so the key shows nothing; a right-click on the panel's meters strip opens
the same report in a popup.

Two of the routes in `/etc/kdos/menu.conf` fail the same way. `system.power` runs the same
windowless `kdos-energy`. `setup.tui` runs `kdos app tui` with no subcommand, which prints a usage
message to standard error and exits with status 2. The Start menu and `kdos-palette` start a route
with no terminal attached, so choosing either route, from those surfaces or through
`kdos menu summon`, shows nothing. Adding a terminal program as an application takes
`kdos app tui add` run in a terminal. See
[Keyboard shortcuts](../02-user-guide/desktop.md#keyboard-shortcuts),
[The clipboard](../02-user-guide/desktop.md#the-clipboard) and the route table under
[`/etc/kdos/menu.conf`](configuration.md#etckdosmenuconf-merged-under-configkdosmenuconf).

### Only two ways of ending the session save the list of open windows

`Super+Escape`, and Log Out, Restart and Shut Down in the compositor's root menu (`menu.xml`), run
`kdos-session-save` before they act. The Start menu's footer and the System menu end the session
without it, so the list read back at the next login, when `~/.config/kdos/session-restore` exists,
is whatever the last of the first two routes wrote. See
[Session, screen and media](../02-user-guide/desktop.md#session-screen-and-media).

### Log Out ends the desktop, not the login

Log Out stops the compositor and leaves a shell prompt on `tty1`, still logged in. Everything else
the session started keeps running there: PipeWire, WirePlumber and `pipewire-pulse`, the portal
front end, mpd and its watcher, the per-user timer loops and the `kdos-box gc` loop. They end only
when the login does, with `exit` at that prompt; with autologin on, a new login and desktop start
at once. Measured on the live medium. See
[What `kdos-desktop-start` does](../03-architecture/session.md#what-kdos-desktop-start-does).

### The control centre's *Sync…* row opens the network-share window

On the System page of `kdos-settings`, the row *Sync…* is described as calendars and address
books through `vdirsyncer`, and runs `kdos-connect`, which mounts a share on another machine. No
surface configures `vdirsyncer`; it is set up through its own configuration file. See
[kdos-settings](../04-programs/kdos-shell.md#kdos-settings).

### `kdos-audio` refuses the `--no-icons` its usage line offers

The usage text lists `[--font NAME] [--no-icons] [--dump]`, and the argument loop accepts only
`--at-bottom`, `--font` and `--dump`, so `kdos-audio --no-icons` prints the usage and exits 2.
Nothing shipped passes the flag. See [kdos-audio](../04-programs/kdos-shell.md#kdos-audio).

### `kdos-res` draws its selected row in the accent colour

The design language fills a selected row with `KT_DIM` and keeps `KT_ACCENT` for one cell
(see [A selected row](../03-architecture/design-language.md#a-selected-row-is-kt_dim-under-kt_text-and-the-accent-is-one-cell)).
The list pages of `kdos-res` (Processes, Applications, Boxes, Drives and Network) fill the selected
row with `KT_ACCENT` under `KT_SURFACE` text and the row under the pointer with `KT_MID`, rather
than calling `ktui_sel_row()`. See [kdos-res](../04-programs/kdos-res.md).

## Applications and boxes

### Changing the accent colour reaches running GTK3 applications, not running libadwaita or KDE ones

GTK rebuilds its style when the portal's `gtk-theme` key changes, and `kdos theme` writes each accent's
stylesheet under its own name (`~/.themes/KDOS-<accent>`), so the settings portal's change signal
is enough for GTK3. libadwaita ignores GTK themes entirely and reads
`~/.config/gtk-4.0/gtk.css`, which GTK loads once at startup. A Qt 6 application on the host, where
the session sets `QT_QPA_PLATFORMTHEME=kde`, or in a box on the `rt-kde` runtime, runs under the KDE
platform theme and reads `~/.config/kdeglobals`, which KDE re-reads only
on its own global-settings signal, and there is no KDE daemon here to send it. Both pick up the new
palette the next time they start. libadwaita 1.6 and later follow the portal's `accent-color` on
their own, which changes the accent and not the rest of the palette. A Qt application on the
`rt-qt` runtime uses the GTK3 platform theme instead, and takes its palette from GTK.

### A box can be granted only the eight named groups of Wayland globals

A boxed client sees the compositor's fixed sandbox allowlist, plus whatever its profile's
`grant =` line in `~/.config/kdos/boxes/<name>.conf` names. The names come from a fixed table in
`kdos-comp` (`kdos-grant.c`): `screencopy`, `toplevel-capture`, `export-dmabuf`, `data-control`,
`foreign-toplevel`, `layer-shell`, `input-method` and `output-power`, each mapping onto one to three
protocol globals. Any other global cannot be granted to a box, whatever its profile says, and a
grant cannot be narrower than its row: `screencopy` opens both generations of screen capture
together. The compositor reads a box's profile at that box's first bind and again after a
`SIGHUP`; a running client keeps what it bound. See
[The security model](../03-architecture/security-model.md#what-is-not-protected) for what a grant
costs.

### A boxed application is given your location without being asked

The portal front end asks before sharing location only with an application in a sandbox it
recognises (Flatpak, Snap or Linyaps), and a KDOS box is none of them, so it counts as an
unsandboxed host program. GeoClue runs with no consent agent and an empty whitelist, and answers any
client on the system bus. For the same reason, the Camera and Screenshot questions are asked once
for all boxes together, since the front end sees no application id on any of them. No surface shows
or changes a stored answer: each portal's answers are a file under `~/.local/share/flatpak/db/`,
and deleting that file makes it ask again. See
[The session](../03-architecture/session.md#access-and-what-it-unlocks).

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
needs an installed system, takes the composition down, and leaves the profile it wrote.

### `kdos-box create` with a `box:` base never finishes

`kdos-box create <name> base=box:<other>` copies the other box's base into the new profile and
calls itself again with the same arguments, which set the base back to `box:<other>`, so it
recurses until the program crashes. Naming the other box's own base instead (`base=pack:<id>` or
`base=image:<ref>`, as `kdos-box profile <other>` prints it) gives the same box. `kdos-box clone
<src> <dst>` does not take this path, and copies the other box's writable layer as well. See
[create with a box base](../04-programs/kdos-appbox.md#create-with-a-box-base).

### The *Containers* menu entries have not been opened on a built image

`kdos-podman-api` starts `podman system service` for `podman-tui` and `lazydocker`; what is
measured is its start, lock and idle-restart logic against a stand-in engine, not either tool
connecting to a real service. Pods and `--init` rely on `catatonit`, which is measured to build
statically and run a command, not to start a pod.

### A box is not a security boundary against you

By default it shares your home directory in full, along with your network, process table, `/dev`,
`/sys`, `/tmp` and runtime directory, so a program in it can reach the session bus and connect to
the compositor untagged through `wayland-0`. It limits what an application is offered by the
*desktop*, not what it can do to your data. A profile's `home = private` gives one box a home of its
own. See [The security model](../03-architecture/security-model.md#what-is-not-protected).

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

Text you copy in either should appear in `kdos-clip`'s history, and this has not been observed on a
built image. It is expected to work: both programs copy through the Wayland data-control protocol,
compiled into the binary itself, and `kdos-comp` provides that protocol. If a copy from either goes
missing, that is a bug worth reporting.

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
ask: `kdos-mount browse` is an mDNS query plus a NetBIOS one, and a machine that is asleep, on
another subnet, or behind a router that does not forward broadcasts is missing from it. It can still
be reached by name.

The file managers' `network://` view is the same: `gvfs` is built without its WS-Discovery (`wsdd`)
backend, because `wsdd` is not a port, so it lists only the hosts Avahi finds. A Windows machine,
which announces itself over WS-Discovery rather than mDNS, is reached by typing `smb://host`.

### An NFSv3 mount needs `-o nolock`

NFSv4 mounts and the v4-only server (`72_nfsd`) work. There is no `rpcbind` port, so `rpc.statd`
cannot register, and a `vers=3` mount that wants locking is refused. Mounting with `-o nolock`
works and keeps locks local to this machine.

### Kerberos is built and has never been given a ticket

`kinit`, `cifs.upcall` and the `request-key` rule that joins them are on the image, and
`kdos-mount krb5` mounts with `sec=krb5`, but there is no KDC here and no realm to join, since the
server half of krb5 is not installed. What is measured is the request the daemon makes, not that a
domain controller accepts it; no share has been mounted with a ticket on this image. `cifs.idmap` is
not built either: it needs winbind's client library, and this desktop does not need it, because
every share is mounted with an explicit `uid=` and `gid=`.

### The graphical passphrase prompt has not been exercised

`pinentry` is `pinentry-qt`, which draws a Qt dialog when the request from `gpg` carries
`WAYLAND_DISPLAY` or `DISPLAY`, and otherwise asks in the terminal `GPG_TTY` names. A program
started from a launcher should therefore get the dialog, but no signature or decryption has been
made through it on this image, so neither the dialog on `kdos-comp` nor its placement over the
window that asked has been seen.

### `rga` does not search `.htm` files

Its `pandoc` adapter, which reads `.docx`, `.odt`, `.epub`, `.fb2`, `.ipynb` and `.html` into text,
claims `.htm` too and passes the extension as the input format, and pandoc has no reader named
`htm`: each such file is reported as *Unknown input format 'htm'* and yields no match.
`rga --rga-adapters=-pandoc` drops the adapter, and the file is then searched as plain text. The
other formats, `.html` included, are searched through pandoc.

### A transcription has never been read back

What is verified is the model check, the `whisper-cli` command line, starting it and its exit
status. The image carries `base.en` in `/usr/share/whisper.cpp/models`; any other model is fetched
by `kdos speech get`, which picks one from a table, checks its sha256 and writes it to
`~/.local/share/whisper.cpp/models`. What has not been done is running a transcription and checking
its output.

### Live transcription is a terminal program, not a desktop action

`whisper-stream` is built and transcribes a microphone, and nothing on the desktop starts it.
`kdos-rec`'s *Transcribe* runs `whisper-cli` over the file it has just recorded. Both need the same
model, and `base.en` ships.

### The native applications have not been run

The browsers, office suites, editors, games and the rest of the graphical applications are recipes
under `ports/core`, named in the lists of the userland phases, `40_lang` to `44_apps`, with every
source fetched and hashed. Every one of them has been through a build: the build tree holds every
port of `42_graphics`, `43_toolkits` and `44_apps`, and the image it made carries them (see
[Build times and snapshots are measured for the early phases
only](#build-times-and-snapshots-are-measured-for-the-early-phases-only)). No application has
been started on a KDOS image, so what is written here and in [the ports
catalogue](ports-catalogue.md) about how any of them behaves describes its recipe. The published source archive holds almost
none of their sources, so `make fetch` on another clone takes them from upstream. See
[The source archive has one public location](#the-source-archive-has-one-public-location).

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

### A saved login needs KeePassXC running

Programs that keep a password through the Secret Service (libsecret in GTK programs, QtKeychain,
KWallet's API) find a provider only while KeePassXC is running with its Secret Service integration
turned on in its own settings. The KeePassXC recipe enables that integration, and nothing starts
KeePassXC or turns it on. Without it, such a program has nowhere to store its secret: Nheko, for
one, stores its login through QtKeychain.

### Chromium checks no spelling and reaches no Google service

Chromium reads only its own `.bdic` dictionaries, which it downloads, so it cannot use the
Hunspell dictionaries on the host, and its spelling service is off by policy. It is configured with
no Google API keys, so sync, sign-in, Safe Browsing, translation and location services do nothing.

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

### A phone cannot drive the pointer or keyboard through KDE Connect

KDE Connect's remote input, the phone as a touchpad and keyboard, asks the RemoteDesktop portal for
an EIS connection (`ConnectToEIS`). No backend on KDOS answers RemoteDesktop:
`xdg-desktop-portal-wlr` does not implement it, `xdg-desktop-portal-kdos` does not either, and
`kdos-portals.conf` routes it nowhere, so the request is refused. Pairing, notifications, the
clipboard, file transfer and the other plugins do not use the portal.

### The local model and translation programs ship without models or pages

`llama-server` serves its OpenAI-compatible API and no web page: the page is a Svelte application
that is either built with npm or downloaded, and the offline build does neither. A page placed in a
directory is served with `--path`. Translate Locally ships no Bergamot language model; its model
list is fetched from the network only when asked, and a model downloaded elsewhere is imported from
the disk.

### Hatari ships no TOS

The Atari ST emulator needs a TOS or EmuTOS image, and building EmuTOS needs an m68k cross
toolchain that is not a port, so the user supplies the image.

### SolveSpace takes no 3D mouse

`spacenavd` serves a 3D mouse to every `libspnav` client, FreeCAD and OpenSCAD among them, but
`solvespace-qt` has no 3D-mouse support at all; upstream has it only in its GTK and Windows
interfaces.

### Every program speaks English

There is no non-English interface anywhere on the system. Every package is made without its
translations: message catalogs, Qt translations, translated KDE handbooks and translated manual
pages are removed as the package is built, so setting `LANG` or `LC_MESSAGES` to another language
changes no program's text, because there is nothing for it to load. Text in any language can still
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

### `btop`'s GPU panel is empty on Intel graphics unless btop runs as root

It reads the i915 performance counters with `perf_event_open`, a system-wide event that the
kernel's default `perf_event_paranoid` of 2 refuses to a process without `CAP_PERFMON`, and nothing
grants that capability to the binary or lowers the setting. `sudo btop` shows the panel.

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
of one physical volume on the target disk, and makes no `root_b`: slot B's space is left free, and
its volume is made by hand like the rest of slot B; see
[Activating volume groups](../03-architecture/boot-and-init.md#activating-volume-groups).

### `lsblk` shows a filesystem's type, label and UUID to root only

util-linux is built without libudev, because eudev needs util-linux's libblkid first, so lsblk can
learn them only by probing the device, which it does only as root. `sudo lsblk -f`, or `blkid` as
root, answers.

### The power button and the sleep key work only inside a graphical session

They arrive as keys, which the compositor's `rc.xml` binds. There is no acpid, and nothing below the
session listens for them, so at a text login or on the console desktop the power button does
nothing. Neither binding has been pressed on real hardware.

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

### A phone is mounted by hand

`kdos-mountd` offers block devices only, so an MTP phone never appears in its list. The Android
File Transfer window and `aft-mtp-mount ~/Phone` are the ways in, and no phone has been reached
through either on this system.

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

`kdos-burn` has not burnt a disc: the rig has no optical drive, so what is verified is its frame
and the `--compare` it runs afterwards, not `xorriso`'s write or its `-compare_r` against a real
medium. The `write` verb of `kdos-mountd` is proved under `--fixture-serve`, where the copy and the
read-back run against a scratch file; the `O_EXCL` claim, the cache drop before the verify and the
partition re-read after it act only on a real block device and have not met one. `write` offers
only a disk the daemon lists, which means a disk carrying a filesystem it recognises: a blank stick,
or one holding only a partition type it does not probe, cannot be written through it, and in a live
session any disk with an ISO 9660 partition is refused as the boot disk.

`kdos-verify` checks a BLAKE3 list with `b3sum`, which is not a port, so such a list answers that
`b3sum` is not on this machine. SHA-256, SHA-512 and par2 are checked.

## Installation

### Applications imported or built during an install stay in the live session

Of the three routes the Applications step can take, only *pending* reaches the installed machine:
it records the selection in the target's `/var/lib/kdos/apps-pending`, and the first session
offers to build it. The *import* route runs `kdos-appbox import <archive>` and the *network* route
runs `kdos-appbox install <group>…`, both in the live session with no target root, so what they
install lands in the live system's store and ends with it. The target gets the empty store
directories and nothing in them. See
[The applications step](../04-programs/kinstall.md#the-applications-step) and
[Applications](../02-user-guide/installation.md#8-applications).

### An unattended install skips every check the pages make

`kinstall --config <file> --unattended` goes straight to the install, and the checks each page
makes before it lets you continue are never run: that the target is not the running boot medium
and not read-only, that it is large enough, that the two passphrases and the two passwords match,
that the hostname and user name are well formed, that root is not locked while the user is not an
administrator, and that no other disk already holds a volume group named `kdos`. An answer file
that the wizard would refuse goes to the install as it is. The one page check the Prepare step
also makes is the firmware one: on a machine booted in legacy BIOS mode it refuses a reused GPT
disk with no BIOS boot partition. Loading the same file into the wizard
(`kinstall --config <file>`) and stepping through the pages runs the checks. See
[Installing unattended](../02-user-guide/installation.md#installing-unattended).

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

### Nothing creates or first fills the second root slot

The installer lays down slot A only and writes a state with `slot_b` empty. A second slot's
filesystem has to be made, filled with a copy of the system and recorded with
`kdos-bootctl set-slot b <uuid> [<luks-uuid>]`, all by hand; the steps are in
[Installation](../02-user-guide/installation.md#ab-root-slots). From there `kdos update apply`
installs into the mounted inactive slot, deploys its kernel and marks it to try; the initramfs
selects a slot, unlocks that slot's own encrypted container and counts attempts; and a boot that
reaches the end of initialisation confirms the slot. A second *encrypted* slot has never been
booted. What is measured is on the build host: rolling back from slot B to A hands back A's
container and not B's, which is the failure the mechanism exists to prevent.

### The update path has not been run against a published tree

`kdos update fetch` takes `kdos-ports.tar.xz` from the latest release, and only a release made by
`ports/publish --release` with a release key carries one; until then it reports `fetch-failed`. Its
signature check, the refusal of an older tree, the exchange with `/ports`, `kpkg fetch`'s archive
and upstream order, and the recorded problems have been exercised against local directories in a
container with no network, and `kpkg fetch` is asserted by the self-test, but none of it against a
real release over the internet. No `kdos update apply` has built a
large set on an installed machine, so how long one takes, and how much of `/var/cache/kpkg` it
needs, is not measured.

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
- the live image logs in as `kdos` automatically; an installed system does so only when its
  answer file set `autologin = yes` or `kdos-power autologin` set it later;
- disk encryption protects data at rest only;
- a box shares your home directory by default, and can therefore edit its own profile and widen
  its own compositor grant;
- a box granted `input-method` receives every keystroke on the seat;
- membership of `wheel` is effectively root;
- an OCI base image pulled from a registry is unsigned content;
- an unsigned pack mounts unless `KDOS_REQUIRE_SIG` is set, while a pack whose signature fails
  does not;
- nothing installs updates automatically: a daily timer (`/etc/kdos/timers.d/10-update-check.timer`)
  fetches the signed ports tree and runs `kdos update check`, and `kdos update apply` runs only when
  you run it;
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

`python3 testing/optdeps.py --claims` lists every recipe line that says a dependency is not a port,
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
| `avidemux` | AV1, FAAC, Aften, DCA, Xvid and AMR encoders, VapourSynth input | aom, faac, aften, dcaenc, libdca, xvidcore, opencore-amr, VapourSynth | one feature each; dav1d decodes AV1 in every player |
| `avogadrolibs` | Space groups, symmetry and spectra plots | Spglib, libmsym, JKQTPlotter | one feature each |
| `baresip` | AMR, G.722, G.722.1 and packet-loss concealment codecs; ZRTP | opencore-amr, libg722, spandsp, gzrtp | one feature each |
| `blender` | VR, USD, MaterialX and Hydra; OSL shaders and OpenPGL path guiding in Cycles | OpenXR, USD, MaterialX, OSL, OpenPGL | large; no OpenXR runtime exists on this image |
| `boost` | The `stacktrace_backtrace` variant | libbacktrace | one feature |
| `brltty` | The Libbraille driver | libbraille | hardware |
| `btrfs-progs` | Converting a ReiserFS volume | reiserfsprogs | superseded: the kernel has no ReiserFS |
| `cantor` | The Julia back end | julia | large |
| `ceres-solver` | METIS ordering for the sparse solvers | METIS | one feature |
| `clamav` | The mail milter | libmilter | service: no mail transfer agent runs here |
| `codec2` | The FreeDV 2020 mode | LPCNet | one feature |
| `coin` | VRML JavaScript and image textures through simage | SpiderMonkey, simage | one feature; no consumer here uses them |
| `crun` | Container checkpoint and restore | CRIU | one feature |
| `cura` | The MakerBot writer | pyDulcificum | one feature |
| `darktable` | The map view, display colour management, G'MIC and Unity launcher progress | osm-gps-map, colord, G'MIC, libunity | service (online tiles), one feature each |
| `dasher`, `gparted` | The GNOME help | yelp-tools | one feature |
| `deja-dup` | Restoring a Duplicity backup | duplicity | superseded: new backups use restic |
| `dnsmasq`, `iptables` | Connection-tracking marks and labels | libnetfilter_conntrack, libnfnetlink | one feature; nftables is the firewall |
| `elisa` | UPnP browsing | UPnPQt | superseded: marked broken upstream |
| `enchant` | The nuspell, Hspell and Voikko providers | nuspell, hspell, libvoikko | one feature each; Hunspell and Aspell are the providers |
| `falkon` | The GNOME keyring password plugin | libgnome-keyring | superseded: KWallet keeps the passwords |
| `fastfetch` | The VDPAU, EET, RPM, ImageMagick 6 and QuickJS probes | libvdpau, efl, rpm, ImageMagick 6, quickjs | one feature each |
| `fcitx5-chinese-addons` | The graphical configuration tool | fcitx5-configtool | one feature; the configuration is a text file |
| `ffmpeg` | VDPAU, sndio, CUDA, NVENC and AMF | libvdpau, sndio, the NVIDIA and AMD SDKs | X11, vendor |
| `fluidsynth` | The MidiShare driver | MidiShare | superseded |
| `foot` | A login record for each window | libutempter | one feature |
| `freecad` | Point clouds | PCL | large |
| `freedoom` | The PDF manuals | asciidoctor-pdf | one feature |
| `gcc-arm-none-eabi`, `gcc-avr`, `gcc-riscv64-unknown-elf` | Graphite loop optimisations | isl | one feature |
| `gdb` | CTF traces, source highlighting through GNU source-highlight, Intel Processor Trace, AMD GPU debugging, debuginfod | babeltrace, source-highlight, libipt, amd-dbgapi; elfutils is built without debuginfod | one feature each, vendor; Pygments colours the source |
| `gdk-pixbuf` | Sandboxed image loading through glycin | glycin | one feature; the built-in loaders read every format |
| `gimp` | MNG, WMF and ILBM files; JavaScript and Lua plug-ins; full dashboard backtraces | libmng, libwmf, libilbm, gjs, lua-lgi, libbacktrace | one feature each |
| `glabels` | GNU Barcode and IEC 16022 symbologies; the Evolution address book | GNU Barcode, libiec16022, Evolution Data Server | one feature each; QR comes from qrencode |
| `glib-networking`, `openconnect` | PAC proxy configuration | libproxy | one feature; `http_proxy` and friends are honoured |
| `gmsh` | CGNS, Mmg remeshing and PETSc solvers | CGNS, Mmg, PETSc | large |
| `gnucash` | The SQL back ends and online banking | libdbi, AqBanking | one feature each; XML books are built |
| `gnumeric` | Psion and Paradox import, the database plug-in | psiconv, pxlib, libgda | one feature each |
| `goldendict-ng` | EPWING dictionaries | libeb | one feature |
| `gthumb` | Display colour management | colord | one feature |
| `gvfs` | Approving a WebDAV server's untrusted certificate; Windows network discovery | gcr, wsdd | one feature each; `network://` lists what Avahi finds |
| `ifcopenshell` | COLLADA and USD output, the RocksDB cache | OpenCOLLADA, USD, RocksDB | large |
| `imagemagick` | Liquid rescale, Ultra HDR, MagickCache | liblqr, libultrahdr, MagickCache | one feature each |
| `iperf3` | SCTP tests | lksctp-tools | one feature |
| `k3b`, `scummvm`, `vlc` | Musepack audio | libmpcdec | one feature |
| `kate` | The telemetry settings page | KUserFeedback | one feature |
| `kdeconnect` | Remote input from the phone on X11 | libfakekey | X11 |
| `kfilemetadata` | AppImage metadata | libappimage | one feature |
| `knewstuff` | The RSS provider type | Syndication | one feature |
| `konsole` | The Kapsule container profiles | Kapsule | one feature |
| `ktexteditor` | `.editorconfig` files | EditorConfig | one feature |
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
| `powerman` | Genders node lists and TCP wrappers | genders, tcp_wrappers | one feature, superseded |
| `printrun` | `pronterface` and `plater`, the wx front ends | pyglet, puremagic | one feature; `printcore` and `pronsole` run |
| `profanity` | OTR encryption | libotr | superseded: OMEMO is built |
| `purpose` | Account sign-in for the YouTube and Nextcloud targets | KAccounts | service |
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
| `wl-mirror` | `wl-present` | pipectl | one feature |
| `wv` | Embedded Windows Metafile pictures | libwmf | one feature |
| `xastir` | Speech | festival | one feature |
| `yosys` | `show` without `-viewer` | xdot | one feature |
| `zint` | The GS1 Syntax Engine | libgs1encoders | one feature; zint checks GS1 data with its own rules |

## Upgrades

### A `tzdata` upgrade can reset the time zone to UTC, once

The `tzdata` port does not own `/etc/localtime`, so upgrading `tzdata` on a machine whose installed
`tzdata` manifest lists the link removes it as an orphan. `rcS` then links UTC on the next boot, and
`TZ=':/etc/localtime'` follows it. kpkg has no rule that keeps a file a package does not own, so run
`kdos-power timezone <Area/City>` to set the zone again. Later upgrades leave it alone.

## Building

### Build times and snapshots are measured for the early phases only

The thirteen phases under `script/phases/` and the ports tree filed on shelves pass preflight,
`testing/phaseclosure.py` and the self-test. A build has run every phase from `00_cross` to
`70_image` on these lists and written `build/iso-build/kdos.iso`; its tree holds every port the
lists name but `kdos-kfs` of `41_system` (see the ports tree under [Host and packaging](status.md#host-and-packaging) in Status).
A phase's progress is the names in its `packages.d/*.txt` that `ls build/fs/var/lib/kpkg/db` also
shows. `build/snapshots/timings.json` holds step times for `00_cross` to `31_compilers` only,
because that build's figures for the later phases were not recorded, and nothing else it left
measures a port's build: see [Build times](../../kfs/17-appendices/sbu.md). Only `30_foundation`
has a measured snapshot, 412 MiB whole. Layered snapshots have been measured
only on a synthetic ten-phase tree, where they came to 23% of full ones. A run writes snapshots
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

The `kdos-installer` port builds the same program from the same sources, and no phase list names
it. Its recipe hash covers its own directory and `src/libs` but not `catalogue.c`, so a `kpkg`
build of that port counts an edit to `catalogue.c` alone as nothing to do.

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

### There is no public binary host

The mechanism is complete: a signed index, three checks that a prebuilt package matches this
machine (architecture, build configuration and recipe hash), and deltas; see
[The binary host](../03-architecture/packaging.md#the-binary-host). `make build KDOS_MAKE_BINHOST=1`
writes a signed one to `build/binhost/`, and nothing publishes it: it is one you run yourself, with
`kpkg binhost` by hand. `kdos update` never reads one; every update is built from source.

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
that breaks the ports layout (a recipe not exactly one listed shelf down, a name held by two ports,
a reserved or port-named shelf), and a push naming a source hash the archive does not hold. Without
it, a push can carry a layout no build resolves, or name a source nobody has published, and every
other clone's `make fetch` then depends on upstream still serving that file. Setting
`core.hooksPath` replaces `.git/hooks` as a whole, so git-lfs's hooks, or any other installed
there, stop running in that clone.

The layout check reads the tree of each pushed ref's commit, not of every commit between it and the
remote, so a commit in the middle of a push that breaks the layout passes when a later one repairs
it; preflight is what checks the working tree. With the hook on, a push that introduces a new
source hash is also refused when the archive cannot be reached or answers anything but 200 or 404,
because the missing source cannot then be ruled out; such a push made offline needs the bypass.
Every push is refused while `KDOS_SOURCES_BASE` is empty. `KDOS_SKIP_PUBLISH_CHECK=1 git push …`
skips the archive check for one push and leaves the layout check in force;
`KDOS_SKIP_LAYOUT_CHECK=1` skips the layout check.

### An offline `kdos rebuild` from the medium has not been run to the end

A stick built with `KDOS_ISO_SOURCES=1` carries, under `sources/`, the ports tree (`/ports`
without its dot-directories, so every fetched source beside its recipe), `src/`, `script/` and
`fs/`, and `sources/binhost` when the build wrote one.
`kdos rebuild` refuses a tree without `fs/etc`, copies the tree somewhere writable and runs the
orchestrator there with `KDOS_WORKSPACE` naming the copy, which `script/env/host.env`, the
environment of `00_cross` and `10_bootstrap`, uses in place of `/workspace`. No rebuild from a medium has been run through every
phase. See [kdos rebuild](../04-programs/kdos-command.md#kdos-rebuild) and, for the build it runs,
[How KDOS is built](../05-developer/how-kdos-is-built.md); the copy itself is described in
[Repository layout](repository-layout.md).

## Testing

### The compositor and the shell are not compiled by the self-test on a bare host

Their Wayland, font, D-Bus and audio libraries are not there, so those blocks report as skipped on
most machines. `testing/devdeps-image.sh` builds a container where they run.

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

### The phosphor shader is not in any rig photograph

The rig's virtual display puts the compositor on software rendering, where the pass switches itself
off. What is photographed is the cell grid underneath it.

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
