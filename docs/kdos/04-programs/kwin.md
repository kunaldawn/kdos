# KWin

KWin is the compositor and window manager of the KDOS desktop. It takes the display and the input
devices, places, decorates and draws every window, runs the effects and the window switcher, holds
the global shortcuts and draws the lock screen. KDOS does not fork it: the `kwin` port builds
Plasma's own release, and what makes it KDOS's is a set of shipped defaults, three carried patches,
four KWin effects and two KWin scripts of KDOS's own, and the window rules `kdos-appbox` writes for
boxes. This chapter is the reference for all of that. It is written for anyone who wants to know
why a window behaves as it does on KDOS, who is changing a KDOS default or one of the patches, or
who is debugging the compositor. Using windows day to day is in
[The desktop](../02-user-guide/desktop.md) and the rules KWin is configured to follow are in
[The window model](../03-architecture/window-model.md); everything KWin does that is not
mentioned here is upstream's and is documented by KDE.

Why KDOS patches KWin rather than forking it, and where the line between the two falls, is
argued in [Decisions](../01-philosophy/decisions.md#kwin-is-the-compositor).

## How KWin runs

On KDOS KWin runs only as `kwin_wayland`, a Wayland compositor. There is no X11 session and no
`kwin_x11`; X11 programs are served by Xwayland, which KWin starts itself (see
[Xwayland](#xwayland)).

Two copies run on a machine with somebody logged in:

| Copy | Started by | Command |
|---|---|---|
| The login screen's | SDDM, from `CompositorCommand` in `/etc/sddm.conf.d/00-kdos.conf` | `kwin_wayland --no-global-shortcuts --no-lockscreen --inputmethod plasma-keyboard --locale1` |
| The session's | `startplasma-wayland`, through `kwin_wayland_wrapper` | `kwin_wayland --xwayland` with the session's options |

The login screen's copy has no global shortcuts, no lock screen and no Xwayland. It starts
plasma-keyboard as its input method, so the greeter has an on-screen keyboard: KWin shows it when
the password field is touched, and the footer's Virtual Keyboard button shows and hides it for a
mouse. `--locale1` takes its keyboard layouts from `org.freedesktop.locale1`, which
[kdos-settingsd](daemons.md#kdos-settingsd) serves, so the greeter types in the layout the
machine was installed with; with more than one, the footer's layout button switches between them.
Both buttons are D-Bus calls to this KWin (`org.kde.KWin` `/VirtualKeyboard`, `org.kde.keyboard`)
on the greeter's session bus, the one user service the `sddm` account gets (see
[Session](../03-architecture/session.md#the-users-services)).

The session's copy runs under `kwin_wayland_wrapper`, which holds the Wayland socket. When KWin
exits with anything but a clean shutdown, the wrapper starts it again, up to ten crashes in a row,
and the session's clients reconnect to the new compositor instead of dying with it. plasmashell is
the exception: it exits when its compositor goes away, and the KDOS kded module starts it again a
few seconds later (see
[how plasmashell is kept running](plasma-shell.md#how-plasmashell-is-kept-running)). Past ten,
the session ends and SDDM's login screen comes back. There is no crash dialog: KDOS builds no
crash handler, and the core-file limit is the kernel's default of zero, so a KWin crash leaves a
line in the session log and the kernel's `potentially unexpected fatal signal` line in
`/var/log/messages`, and no core. A core is collected by starting the program from a shell that
has run `ulimit -c unlimited`.

KWin asks for `SCHED_RR` at the lowest real-time priority for the threads that read input and
commit frames, so a busy machine does not stall the pointer. It gets it from the `rtprio 1` limit
in `/etc/security/limits.d/30-kdos.conf`, which `pam_limits` applies at every login. The file
capability upstream's install puts on the binary does not survive the package archive, so that
limit is the only route; without it KWin logs `Failed to gain real time thread priority` and
composites at normal priority.

## What the port builds

The `kwin` port builds KWin 6.8.2 with its X11 window management on, for Xwayland, and makes each
optional feature KDOS relies on a requirement of the configure rather than something CMake drops
in silence: activities, emulated input through libei (the RemoteDesktop and InputCapture portals,
KDE Connect's remote input), the zoom's AT-SPI focus and caret trackers, the handbooks of
the settings pages, screencasting through PipeWire and game controllers through libevdev. Every
flag, and the reason for it, is in the port's own note,
[`ports/core/kde/kwin/kfs.md`](../../../ports/core/kde/kwin/kfs.md), which *KDOS from Scratch*
renders as the port's page; the note is the authority, and this chapter does not repeat it.

KWin's installed systemd user unit is removed from the package. KWin is not linked to a session
library: it looks for `libsystemd.pc`, which elogind does not install, so the systemd watchdog and
the systemd sandbox check are compiled out and only the security-context tag described under
[Boxes at the compositor](#boxes-at-the-compositor) marks a client as sandboxed.

The port carries three patches, applied in this order. Each opens with a `Subject:` line, and the
port's note describes each in full.

| Patch | Subject | What it does |
|---|---|---|
| `sandbox-policy.patch` | wayland: deny desktop-shell globals to sandboxed clients unless granted | Widens the set of interfaces a box may not bind, and lets a box's profile grant named sets back ([below](#what-a-box-may-bind)) |
| `sandbox-badge.patch` | Expose a window's sandbox engine, app id and instance id | Hands the box a window came from to the decoration, so the title bar can mark it ([below](#the-sandbox-badge)) |
| `kdos-crt.patch` | Add the kdos_crt full-screen CRT effect | Builds the CRT pass of Phosphor mode into KWin ([below](#phosphor-mode)) |

`sandbox-badge.patch` needs the patches of the same name in the `kdecoration` and `breeze` ports;
against an unpatched kdecoration KWin does not compile. The two sandbox patches are offered
upstream. `kdos-crt.patch` is KDOS's own and is rebased at every Plasma minor release.

## Where its settings come from

KWin reads its configuration through KConfig's cascade. For each file the first directory that
sets a key wins:

1. `~/.config`, where System Settings writes a person's own choices.
2. `~/.config/kdedefaults`, where the global theme in force writes its defaults.
3. `/usr/share/kdos/xdg-vm`, put at the front of `XDG_CONFIG_DIRS` only in a session that runs in
   a virtual machine, and `/usr/share/kdos/xdg-live`, only on the live medium.
4. `/etc/xdg`, which belongs to the machine's administrator.
5. `/usr/share/kdos/xdg`, KDOS's defaults.
6. KWin's compiled-in defaults.

KDOS's KWin defaults are all in `/usr/share/kdos/xdg`, the bottom of the cascade, so a setting
changed in System Settings always outranks them, and an administrator overrides one for every
account by setting the same key in `/etc/xdg`. The two exceptions, both described below, are the
global shortcuts, which KWin reads only from the person's own file, and the box colour rules, which
`kdos-appbox` writes into the person's `kwinrulesrc`.

### `kwinrc`

`/usr/share/kdos/xdg/kwinrc` sets five things:

| Group | Keys | What they do |
|---|---|---|
| `[Desktops]` | `Number=4`, `Rows=1`, `Name_1`…`Name_4` = `main`, `www`, `hack`, `misc`, and a fixed `Id_<n>` for each | Four virtual desktops in one row. The ids are fixed because KWin restores a window to its desktop by id, and an id generated at each login would scatter a restored session onto desktop 1 |
| `[TabBox]` | `ApplicationsMode=1` | Alt+Tab lists applications, not windows: one entry per application, its most recent window (KWin's `OneWindowPerApplication`) |
| `[Effect-zoom]` | `EnableTextCaretTracking=true` | The zoom follows the text cursor as well as the pointer ([Accessibility](#accessibility)) |
| `[Plugins]` | `krohnkiteEnabled=false` | The bundled auto-tiler is installed and off ([Tiling](#tiling)) |
| `[Wayland]` | `InputMethod=/usr/share/applications/org.fcitx.Fcitx5.desktop` | KWin starts fcitx5 as the session's input method and restarts it if it dies; nothing else starts it |

Every global theme KDOS ships, one per accent, adds the rest of KWin's look through
`~/.config/kdedefaults` when it is applied:

| Group | Keys |
|---|---|
| `[org.kde.kdecoration2]` | `library=org.kde.breeze`, `theme=Breeze`, and no border size, so KWin takes the size Breeze recommends: none. A window has no side or bottom border, and its edges resize through the margins Breeze keeps for the pointer outside the frame |
| `[WindowSwitcher]` | `LayoutName=thumbnail_grid` |
| `[Windows]` | `Placement=Centered` |

So a window opens centred on the screen and Alt+Tab shows a grid of thumbnails. The title-bar
buttons are KWin's own defaults, which no KDOS file sets: the window menu on the left and
minimise, maximise and close on the right. Applying a global theme writes those defaults,
`ButtonsOnLeft=M` and `ButtonsOnRight=IAX`, into `~/.config/kdedefaults/kwinrc` beside the keys above. The global themes are
described in [Theming](../02-user-guide/theming.md).

### The title bar

The decoration is Breeze's, configured by `/usr/share/kdos/xdg/breezerc`: every corner square, the
active window's one-pixel outline in the accent, no gradient, and the title on the left. Square
corners and the accent outline are `SquareCorners` and `OutlineHighlight`, two keys the
`breeze` port's `square-corners.patch` adds; an inactive window's outline is Breeze's
mix of the frame and text colours. The title bar takes its colours from the `[Colors:Header]` set
of the colour scheme in force, which KWin uses over `[WM]` whenever a scheme has one, and the
outline from the window palette's link colour, which every KDOS scheme holds at the accent itself
(the selection is darker on a dark accent), so both follow all eight accents with nothing
regenerated, and a window rule that forces a colour scheme on a window (a
box's colour, [below](#the-box-colour)) tints its title bar the same way. When Breeze cannot be
loaded KWin falls back to Aurorae, so a window keeps a frame.

### Animation speed

`AnimationDurationFactor=0.5` in the distribution's `kdeglobals` makes every KWin animation half
KDE's length. `0` turns animation off, which every KDOS effect honours as reduced motion. The
Animation speed slider in System Settings writes the same key to `~/.config/kdeglobals`.

## Shortcuts

KDE's defaults keep the window-management chords. KDOS changes two of them and adds chords KDE
leaves free:

| Chord | Does | KDE's default |
|---|---|---|
| Meta+Q | Close the window, beside Alt+F4 | The activity switcher, which KDOS ships no interface for |
| Meta+F1, Meta+/ | The key card ([below](#the-key-card)) | Meta+F1 switches to desktop 1, which keeps Ctrl+F1 |
| Meta+Space | KRunner, beside Alt+Space, Alt+F2 and the Search key | — |
| Meta+Return | Konsole, beside Ctrl+Alt+T | — |
| Meta+Shift+T | Turn auto-tiling on or off | — |
| Meta+Ctrl+Shift+N | Night Light on, warm at once and all day, or off: `kdos toggle night-light` | — |
| Print | Spectacle on the whole screen | — |
| Shift+Print | Spectacle on a rectangle, beside Meta+Shift+Print and Meta+Shift+S | Spectacle's whole desktop |
| Alt+Print | Record a rectangle, beside Meta+Shift+R and Meta+R | — |
| Meta+Shift+M, E, B, U, C, G, W | Run or raise a terminal program ([below](#run-or-raise)) | — |

Meta+Shift+T and Meta+Ctrl+Shift+N are desktop entries in `/usr/share/kglobalaccel/`, installed by
`kdos-tools`, so each is a `kdos` verb with a key; KWin's own *Suspend/Resume Night Light* action
stays without one. Phosphor mode has no chord. The eight chords the bundled auto-tiler would claim
(Meta+L, Meta+D, Meta+T, Meta+., Meta+I, Meta+R, Meta+Shift+R and Meta+Return) are bound to
nothing for it, so turning tiling on can never take the lock key, the terminal key or System
Settings' key from the session; a person binds any of them on the Shortcuts page.

### How the KDOS chords reach a person

KGlobalAccel, which lives inside KWin, reads only `~/.config/kglobalshortcutsrc` and never a copy
in an XDG directory. KDOS's chords are kept in `/usr/share/kdos/xdg/kglobalshortcutsrc`, and
`/usr/share/kdos/xdg/plasma-workspace/env/kdos-shortcuts.sh`, which `startplasma` sources before
KWin starts, copies each entry into the person's file once:

- An entry is copied only when the person's file holds no value of its own for it.
- An entry is copied only if it has never been copied before. What was copied is recorded in
  `$XDG_STATE_HOME/kdos/kglobalshortcutsrc.seeded`, so a chord a person resets to KDE's default
  stays reset at the next login.
- A `kglobalshortcutsrc` in `/etc/xdg` is read as a layer above KDOS's, so an administrator can
  change a chord for every new account.
- The file is rewritten whole through a temporary file and a rename, so KWin never reads half of
  it. A failure costs the KDOS chords for one login and nothing else.

Every chord KDOS ships passes `testing/chordcheck.py`, which holds it against KDE's compiled-in
defaults, every installed desktop entry's `X-KDE-Shortcuts` and fcitx5's hotkeys. A script that is
off but named under `[Plugins]` in a KDOS `kwinrc`, as the auto-tiler is, counts as KDOS's: a chord
it would take from a binding already in force fails the check until the shipped
`kglobalshortcutsrc` unbinds it.

A new account's first login runs every upgrade step KDE ships for `kconf_update`, while KWin is
already up. One of them, KWin's `kwin-6.1-remove-gridview-expose-shortcuts`, deletes the Present
Windows actions to drop Plasma 5's keys, and run then it deletes the ones the Window View effect
has just registered, so Meta+F9, Meta+F10 and Meta+F7 would do nothing until the next login.
`/usr/share/kdos/xdg/kconf_updaterc` records that step as done; `kconf_update` reads its record
along `XDG_CONFIG_DIRS`, so no account runs it. fcitx5's own
group-switch key is moved off Meta+Space by `/etc/xdg/fcitx5/config` for the same reason: KWin
sees a chord first, so fcitx5's switch would otherwise stop working without a word.

### The key card

The key card is a full-screen, searchable list of every global shortcut the session holds,
grouped into Windows, Desktops, Launch, KDOS, Accessibility and System. Meta+F1 or Meta+/ opens
it, the same keys close it, and Esc clears the search and then closes it. Tab, Down and Up walk
from the search field through the rows with a visible focus frame, scrolling the list to keep the
row in sight, and a screen reader speaks each row as "<action>: <keys>"; after a search it
announces how many shortcuts are left. A chord too long for its column wraps inside its chip
rather than being cut short. It is
`kwin-effect-kdos-keys`, a KWin effect written in QML and installed to
`/usr/share/kwin/effects/kdos_keys/`.

Each time it opens it asks kglobalacceld over D-Bus for every component's shortcuts with their
current keys, so a chord changed on the Shortcuts page is on the card the next time it opens. The
grouping is `groups.conf`, a table of component and action patterns read top to bottom and
compiled into the card at build time; the same table is installed as
`/usr/share/kdos/keycard-groups.conf` so `kdos help --keys` prints the chords the way the card
groups them. The build, and `testing/selftest.sh`, fail when a chord KDOS ships falls only to the
table's fallback group.

kglobalacceld keeps reporting a KWin script's keys after the script is unloaded. The card asks
KWin which scripts are loaded each time it opens and leaves out the rows of one that is off, so
the auto-tiler's keys appear only while tiling is on. kglobalacceld sends only the first key of a
multi-key sequence, so a sequence such as Meta+K, Meta+L shows as Meta+K. A QML effect loads
whatever the renderer, so the card opens under software rendering too.

### Run-or-raise

`kwin-script-kdos-runraise` gives seven terminal programs a key each. The key raises the program's
window when one is open and opens one in Konsole when none is; pressed again from one of its
windows, it moves to the next.

| Key | Program |
|---|---|
| Meta+Shift+M | `mc`, files |
| Meta+Shift+E | `aerc`, mail |
| Meta+Shift+B | `lynx`, the web |
| Meta+Shift+U | `rmpc`, music |
| Meta+Shift+C | `ikhal`, the calendar |
| Meta+Shift+G | `iamb`, chat |
| Meta+Shift+W | `micro`, the editor |

Konsole gives every window the one application id `org.kde.konsole`, so a program's window is
found by its caption: the script opens one with
`konsole --hold -p tabtitle=<program> -e <program>`, and also matches Konsole's own
`<dir> : <program>` title for a window opened any other way. `--hold` keeps the window open after
the program exits, so a program that cannot start (`rmpc` with no MPD running, `iamb` with no
configuration) leaves its error on screen; a window held after a normal quit keeps its caption, and
the key raises it until it is closed. A KWin
script cannot start a program, so each program has a key-less desktop entry in
`/usr/share/kglobalaccel/`, and the script opens a window by invoking that entry's `_launch`
action. The entries are listed on the Shortcuts page as "<program> in a New Window". A program
that is not installed keeps its key; Konsole then says it was not found and starts a shell in its
place.

## Window memory

On Wayland a program cannot place its own window. `kwin-script-kdos-window-memory`, on by default,
makes a program's window open where its last one closed, at the same size, and maximised if it
closed maximised. It keeps one place per application id, at most 200, in
`$XDG_STATE_HOME/kdos/window-memory`, written through a temporary file and a rename.

It reads a window's maximised and full-screen state after the commit that carries it, since KWin
reports the new frame first; what it records is the rectangle the window floats at. It leaves alone
a window that opens maximised, full screen or tiled, whose first restore then goes to the remembered
rectangle; a dialog, a panel, a window that cannot be resized, or any window that is not a
program's ordinary top-level window; a splash or first-run notice, a window its process closes within
three seconds before or after opening the next, whose rectangle is not recorded and which, on the
program's next launch, is left where KWin places it while the window after it is restored; a window
that would land exactly on an open window of the same program; and the programs in its exclusion
list. A window rule that forces a position wins.

A remembered place is moved and shrunk into the screen nearest it, so a window remembered on a
screen that is gone opens whole on one that is there; the shrink is not recorded until the window
is resized. A place remembered at another device pixel ratio is restored without its size, which
the program picks for the ratio it is on. A window with no record that KWin centred while its size
was a placeholder is centred again, inside the usable area, when it takes its real size within two
seconds of opening, and one that opens larger than the usable area is moved and shrunk into it in
the same two seconds.

| `kwinrc [Script-kdos_window_memory]` | Default | Effect |
|---|---|---|
| `Exclude` | built in | A comma-separated list of application ids that replaces the built-in list |
| `RememberDesktop` | `false` | Also move a window back to the virtual desktop it closed on |

The script is turned off on System Settings' KWin Scripts page.

## Tiling

Windows float. Dragged to a screen edge a window takes half the screen, dragged to a corner a
quarter, and Meta+T opens KWin's tile editor, where each virtual desktop keeps its own layout of
custom tiles. All of that is KWin's own and on by default.

Automatic tiling is Krohnkite, a KWin script from the `krohnkite` port, installed and off
(`[Plugins] krohnkiteEnabled=false`). Three things turn it on or off, and all three write the same
key and ask KWin to reconfigure, which loads or unloads the script:

- `kdos tiling on|off|toggle`, and Meta+Shift+T, a desktop entry in `/usr/share/kglobalaccel/`
  that runs `kdos tiling toggle`.
- The tiling desk: `kdos desk tiling` turns it on, and the other two desks turn it off
  ([The desks](plasma-shell.md#the-desks)).
- The KWin Scripts page of System Settings.

## Boxes at the compositor

A boxed application's windows reach KWin through a Wayland socket that tells KWin which box they
came from. The box's launcher runs `kdos-boxsock`, which binds a socket in the box's private
runtime directory, `$XDG_RUNTIME_DIR/kdos/box/<box>/`, and registers it with KWin through the
`wp_security_context_v1` protocol: the launcher's id as the application id, the box as the
instance id, and the engine `io.kdos.appbox+private` or `io.kdos.appbox+tagged`. Every client that
connects on that socket is tagged by KWin itself and never sees the tag, so it cannot drop or
forge it. The box's own runtime directory holds no session socket (`wayland-1`), so the tagged
socket is the only one a boxed program can reach. `kdos-boxsock` is described in
[The daemons](daemons.md#kdos-boxsock) and the launch path in
[kdos-appbox](kdos-appbox.md).

KWin gives every window of a tagged client the tag's application id as its own, unless the
window's id already starts with it. That is how the Task Manager and the application menu match a
boxed window to its launcher, `app.<id>.desktop`, which `kdos-appbox genlaunchers` writes.

### What a box may bind

Upstream KWin refuses a tagged client seven interfaces: window management
(`org_kde_plasma_window_management`), fake input, screencasting, activation feedback, the
lock-screen overlay, data control (clipboard managers' access to every clipboard) and the
security-context manager itself. `sandbox-policy.patch` also refuses it the layer shell, output
management and external brightness, DRM leases, DPMS, the Plasma shell surface, screen edges, the
keyboard state and virtual-desktop management: interfaces through which a program reaches the rest
of the desktop rather than its own windows.

A box gets named sets of them back through the `grant` key of its profile,
`~/.config/kdos/boxes/<box>.conf`, which KWin reads once per client for the engine
`io.kdos.appbox` (with any `+<state>` suffix) and no other:

| Grant | Opens |
|---|---|
| `window-list` | Window management (the list of every window, and acting on them), activation feedback and virtual-desktop management |
| `fake-input` | Emulated pointer and key events |
| `screencast` | Streaming outputs and windows |
| `data-control` | Every clipboard, as a clipboard manager sees it |
| `layer-shell` | Panels, docks and overlays |
| `output-management` | Changing the displays' modes and layout, their brightness, and DRM leases |
| `dpms` | Turning displays off and on |
| `plasma-shell` | Plasma's shell surface, which places a window anywhere, screen edges and the keyboard state |
| `input-method` | Nothing: the input-method interfaces stay with the input method KWin starts |

The security-context manager and the lock-screen overlay are never granted. A box granted
`fake-input` or `window-list` can act on the whole desktop, which is what the grant is for. The
profile file and the rest of its keys are in [kdos-appbox](kdos-appbox.md), and System Settings'
[Boxes](kdos-settings.md#boxes) page edits it. What the tag does and does not protect is in
[The security model](../03-architecture/security-model.md).

### The sandbox badge

`sandbox-badge.patch` gives each window three constant properties read from its client's security
context, `sandboxEngine`, `sandboxAppId` and `sandboxInstanceId`, and hands them to the decoration.
Breeze, with its own patch, draws a small box before the caption of a sandboxed window, in the
caption's colour: solid when the box's private runtime directory held (`+private`), dashed when only
the tag held (`+tagged`), and a warning triangle when neither did (`+unconfined`). An X11 window
reads as unsandboxed, because its surface belongs to Xwayland. The Task Manager's tooltip names the
box as well ("In box: www"), from the `plasma-workspace` and `plasma-desktop` patches of the same
name, which read the launcher's `X-KDOS-Box` key.

### The box colour

A box's colour is its security domain. For each box, `kdos-appbox` writes one window
rule into `~/.config/kwinrulesrc`, a group named `kdos-box-<box>`, which matches every window whose
application id is `<box>` or `<box>.<stem>` and forces `decocolor` to the colour scheme
`KDOS<Accent>` of the box's colour, or `KDOSBox` for a box with none. The title bar of a
server-side-decorated window then wears that colour. `KDOSBox` is grey, and its title bars are
lighter than any accent's (36% of the way from its ground to its text when active, 28% when
inactive, where an accent's sits at 7% and on its ground): the default accent is itself nearly
grey, so a neutral box at an accent's own mix would match a host window. `kdos-theme` refuses a
`KDOSBox` whose title bars measure under 1.5:1 against any accent's. A program that draws its own title bar takes
no decoration colour, and KWin 6.8 has no rule that outlines such a window instead.

Only the `kdos-box-*` groups are `kdos-appbox`'s: every other rule in the file, including the
person's own, is kept as it was, and KDOS's rules are listed after the person's. A new account's
`kwinrulesrc` comes from `/etc/skel` with the rules for the boxes the medium carries.

## Phosphor mode

Phosphor mode makes the screen look like a CRT in the current accent: a pass over every output,
CRT motion for windows, the login, the lock screen and logout, and with the bone or paper accent a
switch to the phosphor accent. It is off for every new account and has no chord. `kdos phosphor
on`, the Look page of the welcome tour and System Settings' [KDOS Look](kdos-settings.md#kdos-look)
page turn it on; `kdos phosphor off` puts back every key it changed, as the person's own file had
it. The command is described in [The kdos command](kdos-command.md), and what the mode changes in
the rest of the look in [Theming](../02-user-guide/theming.md). Why it is opt-in is in
[Decisions](../01-philosophy/decisions.md#kdos-modern-with-phosphor-mode-on-request).

Phosphor mode needs GPU rendering. Under a software renderer (llvmpipe, softpipe, the Software
Rasterizer) the CRT pass refuses to load, `kdos phosphor on` refuses before it writes anything, and
the welcome tour's switch is disabled with the reason. A virtual machine with a plain display
renders in software; `make run-hw` and the rig's `--gl` run KWin on the host's GPU.

### The CRT pass

`kdos_crt` is a built-in effect that `kdos-crt.patch` adds to KWin. It paints each output into an
offscreen texture and draws it back through one fragment shader:

- a three-tap horizontal bleed, weighted 1/4, 1/2, 1/4;
- scanlines on every third row of the output's pixels;
- a vignette and barrel curvature, normalised so no curvature crops the desktop;
- a faint floor in the accent, so black is never quite black.

It is built in rather than shipped as a plugin because KWin refuses an effect built outside it
against any other KWin release, point releases included, while a built-in effect is a static plugin
it never checks.

| `kwinrc [Effect-kdos_crt]` | Range | Default | `subtle` | `retro` |
|---|---|---|---|---|
| `Strength` | 0–100 | 55 | 55 | 55 |
| `Scanlines` | 0–100 | 0 | 0 | 60 |
| `Curve` | 0–100 | 0 | 0 | 30 |
| `Fullscreen` | `true`, `false` | `false` | | |

A preset is only a set of values for the three numbers; nothing stores which one was picked.
`kdos phosphor preset subtle|retro` writes them, and the effect's own settings page,
`kwin_kdos_crt_config`, shows whichever preset the values match, or Custom. The effect is on when
`[Plugins] kdos_crtEnabled=true`.

### When a frame goes through the pass

The pass decides per output and per frame. `qdbus6 org.kde.KWin /Effects debug kdos_crt` prints its
state and each output's last verdict:

| Verdict | Meaning |
|---|---|
| `pass` | The frame went through the shader |
| `capture` | A screenshot, a screencast or the colour picker: these get the plain picture, so a capture keeps true pixels |
| `off` | The effect is not running on this output |
| `refused (hdr)` | An HDR output: the shader's arithmetic assumes white at 1.0 |
| `exempt (fullscreen)` | With `Fullscreen=false`, the active window is full screen on this output, which goes back to direct scanout, so a game or a video keeps it |
| `aside (zoom)` | Zoom or the magnifier is on: a magnified picture is read without scanlines |
| `cooldown` | A run-time failure put this output on the plain picture for a while, 5 seconds doubling to 60 and reset by a frame that passes |

While the pass runs on an output, that output gives up direct scanout and the hardware cursor
plane: the pointer is drawn into the picture and curves with it, and every frame is composited
whole. That is the mode's cost in power and latency.

### Degauss and power-down

The effect also moves the whole screen:

| Motion | When | Length |
|---|---|---|
| Degauss | The colour scheme's accent changes | One pulse of at most 400 ms, never more than three a second |
| Collapse | The session locks or quits | About 350 ms to a bright line, then 100 ms to a dot |
| Unfold | The lock screen comes up, or a collapse is undone | The collapse, backwards |

Nothing the effect does may leave a session black. After a collapse the screen stays dark only
until a deadline (2 seconds for a lock, 30 for a logout, 10 for a collapse asked for over D-Bus),
and then the picture comes back whatever is on it. A stronger hold replaces a weaker one and never
the reverse, so a lock during a logout cannot unfold the logout. `AnimationDurationFactor=0` turns
off the degauss and the collapse; the still pass is not motion and stays.

The power-down is exported as `io.kdos.Crt1` at `/io/kdos/Crt1` on the session bus name
`org.kde.KWin`, with the methods `Collapse`, `Unfold` and `Degauss` and the property `Phase`
(`live`, `collapsing`, `dark` or `unfolding`). `kdos phosphor collapse|unfold|degauss` calls them.

### Window motion

`kdos-kwin-motion` holds three KWin effects written in JavaScript, each a package under
`/usr/share/kwin/effects/`:

| Effect | Does | Replaces |
|---|---|---|
| `kdos_unfold` | A window opens out of a hairline across its middle, and closes by folding to a line and pinching to a dot | Scale, Fade and Glide step aside for it, because it takes each window's open and close grabs |
| `kdos_login` | When KSplash closes, the desktop opens out of a line across the splash, with a hot seam on the edge of the opening | KWin's `login` |
| `kdos_quit` | While the session quits, each window folds away, and the desktop is held on screen until KWin exits | KWin's `sessionquit` |

All three are switched on and off with the CRT pass by `kdos phosphor`, which also turns KWin's
`login` and `sessionquit` off while the mode is on. They are switched on only after `kdos_crt` has
loaded, which also refuses a software renderer by name, so they are seen only with GPU rendering.
Every duration goes through KWin's animation time, so `AnimationDurationFactor` scales it. A
scripted effect has no tie to KWin's C++ interface, so a KWin release does not break them: a change
in the scripting interface shows as a script error in the session log and a window that opens
without the motion.

### Inspecting it without looking

`KWIN_KDOS_CRT_DUMP=<prefix>` in KWin's environment writes the pass's input and output once, as
`<prefix>-in.png` and `<prefix>-out.png`; `KWIN_KDOS_CRT_DUMP_FRAME=<n>` waits for the n-th passed
frame. The variable is read only from KWin's own environment, so no client can use it to capture
the screen. `KWIN_EFFECTS_FORCE_ANIMATIONS=1` lets the effect load on a software renderer, for a
test and nothing else.

## Idle, lock and lid

KWin draws the lock screen; kscreenlocker decides when, PowerDevil decides when the screen dims and
turns off, and elogind owns suspend and the lid.

| Event | Default | Set in |
|---|---|---|
| The screen dims | 5 minutes idle, on mains power | `/usr/share/kdos/xdg/powerdevilrc`, `[AC][Display]` |
| The screen locks | 10 minutes idle, with a 5-second grace in which a key or a move unlocks without a password | `/usr/share/kdos/xdg/kscreenlockerrc`, `[Daemon]` |
| The screen turns off | 15 minutes idle, on mains power | `powerdevilrc` |
| On battery | PowerDevil's own shorter timers | — |
| After a resume | The screen is locked | `kscreenlockerrc`, `LockOnResume=true` |
| Meta+L | Locks now | KDE's default |

In a virtual machine `/usr/share/kdos/xdg-vm` turns off the timed lock, both screen timers, idle
suspend and the lid's suspend, because a guest that locks, blanks or sleeps reads from outside as
one that has stopped; Meta+L and lock-on-resume still lock. PowerDevil's own virtual-machine check
asks systemd's manager on the system bus, which KDOS does not run, so without these keys it would
sleep a guest after 15 idle minutes like bare metal. On the live medium `/usr/share/kdos/xdg-live`
turns the timed lock off. Both set the lock timeout to zero as well as turning auto-lock off, so
System Settings → Screen Locking reads "Never" there.

The lock screen's layout is the Plasma shell's own. Its clock and media controls are shown, and its
wallpaper is the accent's, which `kdos theme` moves while it still names a `KDOS-*` wallpaper. A
password is checked through the PAM service `kde`, which includes the system's `system-auth` stack;
a fingerprint through `kde-fingerprint`. The lid's action, suspend and the inhibitor PowerDevil
takes are elogind's and PowerDevil's, configured as
[Administration](../02-user-guide/administration.md) describes.

## Accessibility

The accessibility aids are KWin's own plugins and System Settings' Accessibility page: zoom and the
magnifier, sticky, slow and bounce keys, mouse keys, dwell click, colour-blindness correction and
colour inversion. KWin's zoom follows the pointer and, through `[Effect-zoom]
EnableTextCaretTracking=true` in the shipped [`kwinrc`](#kwinrc), the text cursor, which it learns
from the input method's text-input protocol. It does not follow the keyboard focus: on Wayland KWin
leaves its AT-SPI focus and caret trackers off unless `KWIN_WAYLAND_ZOOM_FORCE_LEGACY_FOCUS_TRACKING`
or `KWIN_WAYLAND_ZOOM_FORCE_LEGACY_TEXT_CARET_TRACKING` is set in its environment, because their
blocking D-Bus calls can stall the compositor. Colour inversion's Meta+Ctrl+I and Meta+Ctrl+U work
once the Invert effect is turned on under Desktop Effects; it is off by default. The on-screen keyboard is chosen on
the Virtual Keyboard page; KDOS's default input method is fcitx5, which KWin starts (see
[`kwinrc`](#kwinrc)).

Phosphor mode stands aside while zoom or the magnifier runs, and the welcome tour does not offer it
while a screen reader or a magnifier is on. How the desktop reads aloud and what a boxed
application's assistive stack sees is in [Accessibility](../02-user-guide/accessibility.md).

## Xwayland

KWin starts Xwayland the first time an X11 client connects, and gives it `DISPLAY`. No X server runs
before that, and the login screen's copy of KWin starts none. KWin listens on the display's sockets
in `/tmp/.X11-unix`, which it does not create: `rcS` makes it at boot, root-owned with mode `1777`
(see [Mount points that must be right](../03-architecture/boot-and-init.md#mount-points-that-must-be-right)),
and without it KWin starts no Xwayland and the session has no `DISPLAY`. Before it takes the X11
window-manager selections, KWin runs every file in `/etc/xdg/Xwayland-session.d` with `DISPLAY` set,
in name order, and then plasma-workspace's `plasma-setup-xwayland`. An X11 window is managed like
any other, but its surface is Xwayland's, so it carries no box tag and no sandbox badge; how an X11
program in a box is given a display is in [kdos-appbox](kdos-appbox.md). Why Xwayland is the only X
server is in [Principles](../01-philosophy/principles.md#no-xorg-server-and-one-carve-out).

## Debugging

| What | How |
|---|---|
| The log | KWin writes to the session's standard error, which SDDM keeps in `~/.local/share/sddm/wayland-session.log` |
| Everything KWin knows about itself | `qdbus6 org.kde.KWin /KWin supportInformation`: version, compositing backend and renderer, every option, loaded effects and scripts, outputs |
| Windows, surfaces and input as KWin sees them | `qdbus6 org.kde.KWin /KWin showDebugConsole` |
| One effect's state | `qdbus6 org.kde.KWin /Effects debug <effect>`, for example `kdos_crt` |
| Frame rate | `qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect showfps`, and `unloadEffect showfps` to take it away; KWin marks the effect internal, so Desktop Effects does not list it |
| A loaded KWin script | `qdbus6 org.kde.KWin /Scripting isScriptLoaded <id>` |
| Restart the compositor without logging out | `kwin_wayland --replace` from a terminal in the session, or `qdbus6 org.kde.KWin /KWin replace`; the wrapper keeps the socket and every window reconnects |

A KWin that keeps crashing ends the session after ten restarts; the session log and the kernel's
`fatal signal` lines in `/var/log/messages` are the record. Whether the session renders on the GPU is in `supportInformation`'s compositing
section, and it decides whether Phosphor mode can run.

## Working on KWin and its plugins

The three patches live beside the recipe in `ports/core/kde/kwin/`. A Plasma minor release is the
moment they are rebased: unpack the new tarball, apply each with `patch -p1`, fix what does not
apply, regenerate the patch and keep its `Subject:` header. The `kdecoration`, `breeze`,
`plasma-workspace` and `plasma-desktop` sandbox-badge patches move with `sandbox-badge.patch`.
Rebuild with `make build BUILD_ARGS="--phases 50_desktop --rebuild kwin"`.

KDOS's own KWin plugins are ports under `src/desktop/`: `kdos-kwin-motion`,
`kwin-effect-kdos-keys`, `kwin-script-kdos-runraise` and `kwin-script-kdos-window-memory`. They are
JavaScript and QML packages, so a KWin release rebuilds none of them; their QML tests run under
`testing/plasmatest.sh` in the development container, which also installs every KWin package with
`kpackagetool6` into a scratch root. `testing/quick.sh` puts a rebuilt package onto a booted ISO
and restarts what reads it: KWin is replaced for a compiled effect, a QML effect or a QML script,
because a loaded plugin or QML type is never reloaded in place, and a JavaScript effect or script
is unloaded and loaded again. Writing a new effect, script or other desktop component is in
[Writing desktop software](../05-developer/writing-desktop-software.md).

## See also

- [The desktop](../02-user-guide/desktop.md): windows, desktops and shortcuts from the user's side
- [The window model](../03-architecture/window-model.md): the placement, focus and tiling rules
  KDOS configures
- [The Plasma shell](plasma-shell.md): the panels, KRunner and the rest of the session around KWin
- [KDOS settings pages](kdos-settings.md): the Boxes and KDOS Look pages
- [kdos-appbox](kdos-appbox.md): box profiles, grants and the launchers KWin matches windows to
- [The security model](../03-architecture/security-model.md): what the box tag protects
- [Theming](../02-user-guide/theming.md): the accents, the title bar's colours and Phosphor mode
- [Decisions](../01-philosophy/decisions.md#kwin-is-the-compositor): why KWin is patched and not
  forked

<!-- book-nav -->
---

*Part IV — Programs, chapter 21.* Previous: [20. The programs](README.md) · [Contents](../README.md) · Next: [22. The Plasma shell](plasma-shell.md)
