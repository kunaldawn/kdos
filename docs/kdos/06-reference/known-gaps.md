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
feature, upgrades, sources and publishing, testing, and this book. Every entry describes the tree
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

### KDOS's own surfaces do not scale fractionally

`libkwl` adopts the integer scale of the output a surface is on, clamped to `KCELL_MAX_SCALE`, and
renders glyphs at that scale, so a high-density display gets a sharp grid rather than a stretched
one. The scale it reads is `wl_output`'s, which is an integer by definition, so a fractional output
scale is never seen as a fraction. `kdos-comp` does offer `wp_fractional_scale_manager_v1`, so a
Wayland application that speaks it, native or boxed, is told the fractional value; the gap is in
the toolkit, not the compositor.

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

### X11 applications get no input method

Xwayland gives its X clients no text-input protocol, and fcitx5 is built without the XIM frontend
that X clients use, so CJK input works in Wayland clients only, host or boxed. See
[the session](../03-architecture/session.md#input-methods).

### There is no input-method configuration tool

The one upstream ships, `fcitx5-configtool`, is a Qt program that is not a port. fcitx5 is configured through
its text files.

### A screen reader reads the applications, not the desktop's own windows

Orca is a host port, with at-spi2-core's accessibility bus, for the native GTK and Qt
applications, whose toolkits publish a tree of accessible objects. It is set up to speak through
speech-dispatcher and to reach braille through BrlAPI. Neither Orca nor at-spi2-core has been
through a build. Nothing starts it: KDOS runs no autostart agent, so it is started by running
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

### The native applications have not been built

The browsers, office suites, editors, games and the rest of the graphical applications are recipes
under `ports/core`, named in `script/04_phase4/packages.txt`, with every source fetched and
hashed. None of them has been through a build, and none has been started on a KDOS image, so
what is written about any of them here and in [the ports catalogue](ports-catalogue.md) describes
its recipe. The published source archive holds almost none of their sources, so `make fetch` on
another clone takes them from upstream. See
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

### An Android screen cannot be mirrored

`scrcpy` is not a port: its device-side server is a jar that cannot be built here, and shipping
upstream's prebuilt one was declined. `adb` and `fastboot` from `android-tools` are on the host.

### The local model and translation programs ship without models or pages

`llama-server` serves its OpenAI-compatible API and no web page: the page is a Svelte application
that is either built with npm or downloaded, and the offline build does neither. A page placed in a
directory is served with `--path`. Translate Locally ships no Bergamot language model; its model
list is fetched from the network only when asked, and a model downloaded elsewhere is imported from
the disk.

### Hatari ships no TOS

The Atari ST emulator needs a TOS or EmuTOS image, and building EmuTOS needs an m68k cross
toolchain that is not a port, so the user supplies the image.

### A 3D mouse reaches no program

FreeCAD and OpenSCAD are configured with 3D-mouse support through `libspnav`, which reaches the device
through the `spacenavd` daemon, and `spacenavd` is not a port, so no device ever answers.
`solvespace-qt` has no 3D-mouse support at all; upstream has it only in its GTK and Windows
interfaces.

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

The firmware tree ships whole and unpruned, which covers a great deal, but nothing here is tested
against a wide range of devices.

### `btop`'s GPU panel is empty on Intel graphics unless btop runs as root

It reads the i915 performance counters with `perf_event_open`, a system-wide event that the
kernel's default `perf_event_paranoid` of 2 refuses to a process without `CAP_PERFMON`, and nothing
grants that capability to the binary or lowers the setting. `sudo btop` shows the panel.

### LVM volume groups are activated only at boot

The initramfs activates them on a disk boot, then the `03_lvm` init script does. A disk carrying LVM
that is plugged in later shows no logical volumes until you run `sudo vgchange -aay`: lvm2's own
hotplug activation runs through `systemd-run`, so it is built off and its udev rule is not
installed.

### A root filesystem on LVM has not been booted

The installer's erase plan creates the volume group and slot A's volume, its reuse plan lists
existing volumes, and the initramfs carries `thin_check`, `cache_check` and the thin, cache and
snapshot targets. The commands were run against a loop device in a container and the installer's
probe and dry run were read back there, but no install onto LVM (plain, encrypted, thin or cached)
has been booted. The installer lays out only one shape, a volume group of one physical volume on the
target disk, and makes no `root_b`: slot B's space is left free, and its volume is made by hand like
the rest of slot B; see
[Activating volume groups](../03-architecture/boot-and-init.md#activating-volume-groups).

### `lsblk` shows a filesystem's type, label and UUID to root only

util-linux is built without libudev, because eudev needs util-linux's libblkid first, so lsblk can
learn them only by probing the device, which it does only as root. `sudo lsblk -f`, or `blkid` as
root, answers.

### The power button and the sleep key work only inside a graphical session

They arrive as keys, which the compositor's `rc.xml` binds. There is no acpid, and nothing below the
session listens for them, so at a text login or on the console desktop the power button does
nothing. Neither binding has been pressed on real hardware.

### udisks2's LSM module does not load

The `udisks2` recipe enables its LSM module, for RAID volume data and a drive's identify and fault
LEDs, and the module connects to `lsmd` over `/var/run/lsm` when it loads. `lsmd` is installed by
`libstoragemgmt` and nothing starts it, so the module fails to load and logs why; the LVM2 and Btrfs
modules do not depend on it. Neither port has been through a build.

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
applications (GNU Radio, SDR++, SDRangel, Gqrx, URH) have not been through a build. SDRplay receivers have a udev permission rule
and no driver, because SDRplay's API is published only as a closed library, so SoapySDRPlay3,
SDRangel's SDRplay input and SDR++'s SDRplay source are all off. XTRX is not supported at all: it
needs the out-of-tree `xtrx_linux_pcie_drv` kernel module, and its libraries are unmaintained.
SDRangel's four remote plugins (remote input, output, sink and source) are not built on x86-64:
they need SSE3 at compile time, which the x86-64 baseline the tree compiles for does not define,
and `cm256cc`, the library they use, needs a processor with SSSE3 there.

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
network. None of the optical paths has read a disc here, because the rig has no drive, and
`libdvdcss` has not been through a build.

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

### An interactive install stays on the Summary page

The wizard's pages are counted from zero, and Summary, Install and Done are pages 8, 9 and 10.
*BEGIN INSTALL* starts the install and moves to page 8, which is Summary itself, so the progress,
the log and the *Retry step* button on the Install page are never shown, and neither is the Done
page. *BEGIN INSTALL* stays live while the install runs, and pressing it again starts a second
install child beside the first. The Install page's *Continue*, when it is reached, moves to page 9,
which is Install again. The install itself runs to the end and writes its log to
`/var/log/kinstall.log` on the live system. An unattended install is not affected: it finds the
Install page by its id. See [The pages](../04-programs/kinstall.md#the-pages).

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
that the wizard would refuse goes to the install as it is. Loading the same file into the wizard
(`kinstall --config <file>`) and stepping through the pages runs the checks. See
[Installing unattended](../02-user-guide/installation.md#installing-unattended).

### The installer's warnings are shown nowhere

The install child reports its progress to the wizard as one-letter lines, and the wizard drops every
warning (`W`) line: it is neither drawn nor written to `/var/log/kinstall.log`. What goes unreported
is a Limine with no `BOOTIA32.EFI`, a failed `limine bios-install` or no `limine` on the path, no
partition index for the EFI partition and so no firmware boot entry, an EFI partition too small for
a second slot's kernel, and an application route that fell back to *pending*. The install finishes
as if none of them happened. See [The install runs in a child
process](../04-programs/kinstall.md#the-install-runs-in-a-child-process).

## Boot and updates

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

### Per-slot kernels have not been booted

`deploy`, the regenerated boot menu, the hand-picked entry and the rollback onto the confirmed
slot's kernel are asserted on the host against a fixture EFI partition, and the `linux`
postinstall's appended initramfs was assembled and inspected, not started. The UEFI `BootNext` trial
has not met a real firmware: the load option is compared byte for byte with the specification's
layout, and the trial menu, rollback and clean-up are asserted against a fixture EFI partition, a
fixture GPT image and a directory standing in for `efivarfs`. A firmware that ignores `BootNext`, or
deletes a `Boot####` entry that is not in `BootOrder`, rolls every candidate back unbooted rather
than looping. On BIOS, a candidate kernel that dies before its initramfs runs spends no attempt,
because Limine counts nothing: the menu keeps leading with it until you pick the confirmed slot's
`/KDOS (slot <x>)` entry by hand; see
[One boot through BootNext](../03-architecture/boot-and-init.md#one-boot-through-bootnext).

A root that has no `/boot/initramfs.modules` builds a new kernel's initramfs from the module set its
own initramfs carries, rather than from a list; that path has been exercised on test archives only.
See [A new kernel](../03-architecture/boot-and-init.md#a-new-kernel).

### A slot keeps the init it was installed with until that slot is updated

This affects a slot whose initramfs `init` does not read `kdos_slot=`, the word on each boot entry
naming the slot its kernel came from (see [Command-line parameters KDOS
reads](../03-architecture/boot-and-init.md#command-line-parameters-kdos-reads)). An update appends
modules to an initramfs and never replaces its `init`, and the confirmed slot's `init` is the one
that carries out a rollback. While an update is on trial on a machine whose confirmed slot has such
an `init`:

- in a trial led by the boot menu, a rollback returns to the confirmed slot's root but still on the
  candidate's kernel, until `kdos-bootctl mark-good` corrects the menu at the end of that boot;
- after a UEFI `BootNext` trial's one boot, the next boot starts the candidate's root on the
  confirmed slot's kernel once, and `mark-good` then sees the confirmed slot's `kdos_slot=` and rolls
  the candidate back.

Both stop once the confirmed slot is itself updated, which gives it an `init` that reads
`kdos_slot=`. See [One kernel per slot](../03-architecture/boot-and-init.md#one-kernel-per-slot).

### There is no single-user mode

The installed boot menu has a `KDOS (single user)` entry, which passes `single` on the kernel
command line, but toybox init ignores its arguments and no script reads the word, so that entry
boots like `KDOS (verbose)`: every service and the desktop start. For a shell without the desktop,
log in on `tty2`; for a machine that cannot finish booting, see
[When a boot stops](../03-architecture/boot-and-init.md#when-a-boot-stops).

## Security

The full statement is
[What is not protected](../03-architecture/security-model.md#what-is-not-protected). In brief:

- there is no mandatory access control, no verified boot and no measured boot;
- the serial console is a root shell with no password, on the live image and on an installed
  system;
- on the live image, `root` and `kdos` both have the password `kdos`, and `sshd` starts at boot
  behind the firewall's drop policy;
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
  runs `kdos update check`, and `kdos update apply` runs only when you run it;
- `kdos-oomd`'s choice between several large processes is tested only against recorded process
  tables.

## Ports built without a feature

Each port, the phase list and group it is built from, and the recipes that are not installed at
all are in [The ports catalogue](ports-catalogue.md).

### Go has no race detector and no BoringCrypto

Both are objects upstream compiles and ships inside the source tarball, and the `go` port does not
install them, so `go build -race`, `go test -race` and `GOEXPERIMENT=boringcrypto` fail to link.
Every other Go build is unaffected.

### GHC has no profiling libraries

The `ghc` port builds Hadrian's `release` flavour without its profiled variants, so `ghc -prof` and
`cabal --enable-profiling` fail to find the profiled `base`. Of GHC's documentation only the
`ghc(1)` manual page is installed. The User's Guide and the Haddock pages of the shipped libraries
are not, so `cabal haddock` writes pages whose references into `base` and the other GHC libraries
are plain text rather than links.

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

### A SPICE viewer redirects only the USB devices you can already open

`spice-gtk` is configured without `spice-client-glib-usb-acl-helper`, the setuid helper that opens
a device node for an unprivileged client, so USB redirection from virt-manager reaches only the
devices whose nodes a udev rule already gives you. Folder sharing (phodav) and smart card
passthrough (libcacard) are enabled.

## Upgrades

### A `tzdata` upgrade can reset the time zone to UTC, once

The `tzdata` port does not own `/etc/localtime`, so upgrading `tzdata` on a machine whose installed
`tzdata` manifest lists the link removes it as an orphan. `rcS` then links UTC on the next boot, and
`TZ=':/etc/localtime'` follows it. kpkg has no rule that keeps a file a package does not own, so run
`kdos-power timezone <Area/City>` to set the zone again. Later upgrades leave it alone.

### Upgrading toybox on its own can delete tools other ports own

This affects a machine whose installed toybox manifest (`/var/lib/kpkg/db/toybox`) lists names
another port owns: `mount`, `umount`, `losetup`, `kill`, `dmesg`, `readelf`, `strings`, `cmp`,
`gunzip`, `insmod`, `lsattr` and the rest. A toybox manifest lists them when toybox is installed
after util-linux, binutils, diffutils, gzip, bzip2, attr, kmod, ncurses, e2fsprogs or procps-ng: an
install to the same path takes the file into the later manifest, so the owning port's manifest does
not list it, and toybox's upgrade removes it as an orphan. On such a machine, upgrade toybox
together with those ports, whose release bumps reinstall the names.

## Sources and publishing

### There is no public binary host

The mechanism is complete: a signed index, three checks that a prebuilt package matches this
machine (architecture, build configuration and recipe hash), and deltas; see
[The binary host](../03-architecture/packaging.md#the-binary-host). `make build KDOS_MAKE_BINHOST=1`
writes a signed one to `build/binhost/`, and nothing publishes it: it is one you run yourself.

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

`script/hooks/pre-push`, which refuses a push naming a source hash the archive does not hold, runs
only after `git config core.hooksPath script/hooks`. Without it, a push can name a source nobody has
published, and every other clone's `make fetch` then depends on upstream still serving that file.
Setting `core.hooksPath` replaces `.git/hooks` as a whole, so git-lfs's hooks, or any other
installed there, stop running in that clone. With the hook on, a push that introduces a new source
hash is also refused when the archive cannot be reached or answers anything but 200 or 404, because
the missing source cannot then be ruled out; such a push made offline needs the bypass. Every push
is refused while `KDOS_SOURCES_BASE` is empty. `KDOS_SKIP_PUBLISH_CHECK=1 git push …` skips the
check for one push.

### A file beside a recipe that no `sha256 =` line names is outside the recipe hash

For a port that names a `source =`, the recipe hash covers `kpkgbuild`, `build.sh`,
`postinstall.sh` and every `.patch`, and each other file in the directory is expected to be checked
by its own `sha256 =` line. A file committed beside the recipe that no such line names is in
neither. Seven are, counted as the git-tracked files under `ports/core/*/` that are not one of the
four recipe kinds and that no `sha256 =` line names: `linux/kdos.config`,
`linux/kdos-logo-mono.pbm`, `linux/genlogo-mono.py`, `doxx/doxx.desktop`, `epy/epy.desktop`,
`ffmpeg/LICENSE.notice` and `pandoc/cabal.project.freeze`. Editing one of them changes nothing the
build compares, so under `KPKG_STRICT_RECIPE=1` the installed package counts as current and keeps
the old file. Bump the port's `release` in the same change. See
[`E:` — the recipe hash](../03-architecture/packaging.md#e--the-recipe-hash).

### An offline `kdos rebuild` from the medium has not been run to the end

A stick built with `KDOS_ISO_SOURCES=1` carries, under `sources/`, the ports tree (`/ports`
without its dot-directories, so every fetched source beside its recipe), `src/`, `script/` and
`fs/`, and `sources/binhost` when the build wrote one.
`kdos rebuild` refuses a tree without `fs/etc`, copies the tree somewhere writable and runs the
orchestrator there with `KDOS_WORKSPACE` naming the copy, which the phase-1 and toolchain
environments use in place of `/workspace`. No rebuild from a medium has been run through every
phase. See [kdos rebuild](../04-programs/kdos-command.md#kdos-rebuild) and, for the build it runs,
[How KDOS is built](../05-developer/how-kdos-is-built.md); the copy itself is described in
[Repository layout](repository-layout.md).

## Testing

### The compositor and the shell are not compiled by the self-test on a bare host

Their Wayland, font, D-Bus and audio libraries are not there, so those blocks report as skipped on
most machines. `testing/devdeps-image.sh` builds a container where they run.

### No test builds the distribution

`testing/selftest.sh` runs the orchestrator end to end against a synthetic two-phase tree (forking
steps, writing logs, taking and restoring snapshots), and `kdosbuild --selftest` asserts its view
geometry and log classifier. `testing/selftest.sh` also builds `kpkg` and drives it against
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
`ports/sources.idx` is well-formed, free of duplicate hashes and sorted.

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
