# The session

This chapter explains what happens between typing a password at the login screen and seeing the
desktop, and what keeps running while the desktop is in use: the user's own services, the session
message bus, the audio graph, the portals, screen capture and the clipboard, input methods, and the
environment a boxed application receives. It is written for people who administer or debug a KDOS
desktop, and for anyone changing the session's scripts or a program that runs inside a session. If
*box* and *pack* are new words, read the [architecture overview](overview.md) first; for everything
up to the login screen, read [Boot and init](boot-and-init.md). For using the desktop rather than
understanding it, see [The desktop](../02-user-guide/desktop.md).

A *session*, in this chapter, is one graphical login: the processes SDDM starts for a user after a
password (or an automatic login), from the session wrapper to KWin, the Plasma shell and every
program the session starts. None of it runs as root, and none of it is started by the init system.
Underneath every session of one user run that user's *services*: the session bus, PipeWire and the
user's timers, started once at the user's first login of any kind and stopped at the last. The
session is also where the host and a *box* (a containerised application; see the
[glossary](../06-reference/glossary.md)) meet, and much of the design below exists to make that
meeting work: one bus address, one runtime directory and one set of audio sockets, each of which a
box reaches only through what it is handed.

The desktop is KDE Plasma, started the way Plasma starts on any system without a systemd user
instance. What KDOS adds is small and lives in a few files: the session wrapper
`/usr/local/bin/kdos-session-wrapper`, the user services in `/etc/kdos/user-services/`, the
environment script `/usr/share/kdos/xdg/plasma-workspace/env/kdos.sh`, and a handful of autostart
entries. The sections take them in the order a login meets them.

## Starting a session

A graphical login runs this chain. Each step is a different program, and each writes its own log:

```
SDDM (root, tty1)                     /var/log/sddm.log
  └─ sddm-helper: PAM session `sddm` or `sddm-autologin`
       │   pam_unix, pam_limits, pam_turnstile, pam_elogind, pam_kwallet5
       │   (turnstiled starts the user's services on the first login;
       │    elogind registers the session on seat0)
       └─ kdos-session-wrapper        ~/.local/share/sddm/wayland-session.log
            └─ plasma-dbus-run-session-if-needed
                 └─ startplasma-wayland
                      ├─ plasma-workspace/env/*.sh, sourced
                      └─ kwin_wayland_wrapper → kwin_wayland
                           └─ plasma_session
                                ├─ kded6, ksmserver
                                ├─ plasmashell
                                └─ XDG autostart entries, in three phases
```

1. **SDDM** shows the login screen on tty1 (see
   [Boot and init](boot-and-init.md#the-display-manager)). When a password is accepted, or the
   machine logs in by itself, `sddm-helper` opens the user's PAM session.
2. **The PAM stack** does the work a service manager does elsewhere. `/etc/pam.d/sddm` (or
   `sddm-autologin`) includes `system-login`, whose session phase runs `pam_limits` (the resource
   limits under `/etc/security/limits.d`), `pam_turnstile` and `pam_elogind`. `pam_turnstile`
   asks `turnstiled` for the user's runtime directory, `/run/user/<uid>`, and on the user's first
   login has it start the [user's services](#the-users-services), holding the login until they
   answer. `pam_elogind` registers the session with elogind, which then hands the seat's display
   and input devices to the session's compositor; it records the session's type and its desktop
   (`KDE`, from the `XDG_SESSION_DESKTOP` SDDM puts in the PAM environment), which
   `loginctl show-session -p Desktop` and `kdos status` read. The stacks are listed in
   [The security model](security-model.md#the-login-stacks).
3. **`kdos-session-wrapper`** is SDDM's `[Wayland] SessionCommand`
   (`/etc/sddm.conf.d/00-kdos.conf`), run as the user with the session's `Exec=` line as its one
   argument. What it does is in [the next section](#what-kdos-session-wrapper-does).
4. **`startplasma-wayland`** sources every `plasma-workspace/env/*.sh` it finds along
   `XDG_CONFIG_DIRS`, the user's `~/.config` last, and starts KWin under `kwin_wayland_wrapper`,
   which holds the Wayland socket and starts KWin again after a crash, up to ten in a row (see
   [KWin](../04-programs/kwin.md#how-kwin-runs)).
5. **`plasma_session`** starts kded6, ksmserver and plasmashell, then the autostart entries. See
   [How Plasma comes up](#how-plasma-comes-up).

The session is the same whichever way it began: an automatic login, a password at the greeter, or
the first-boot wizard's account on a machine installed for somebody else. A login on tty2 or the
serial line gives a shell, and still registers an elogind session and starts the user's services
(root has none), because util-linux's `login` runs the same `system-login` stack. An ssh login gives a shell with
neither: OpenSSH is built without PAM.

**Real-time limits come from PAM.** `pam_limits` reads two files at every login. PipeWire's
`/etc/security/limits.d/25-pw-rlimits.conf` gives the `audio` group, which every human account is
in, real-time priority 95, nice −19 and 4 MiB of locked memory, which PipeWire's threads need to
run at real-time priority; without them a cycle missed under load is heard as a gap. KDOS's
`30-kdos.conf` gives every login real-time priority 1, the lowest, which is what KWin asks for its
input and frame-commit threads. A group entry outranks `*` whatever the file order, so a member of
`audio` keeps 95. The limits reach PipeWire because the user's services run inside the PAM session
turnstiled opens for them, whose stack includes `pam_limits` too.

### What `kdos-session-wrapper` does

The wrapper is a short shell script, run once per graphical login. Its standard
error is the session log, `~/.local/share/sddm/wayland-session.log`; read the script to see exactly
what a login does. In order:

1. **It becomes a login shell.** It runs itself again under the user's shell with `--login` (bash
   and zsh), or sources `/etc/profile` and `~/.profile` itself for any other shell, so
   `/etc/profile.d` and the user's own profile reach Plasma and everything it starts, as they reach
   a console login.
2. **It refuses a login with no runtime directory of its own.** `XDG_RUNTIME_DIR` must name a
   directory owned by the user; `pam_turnstile` makes it. Without it KWin cannot start, so the
   wrapper exits and the login screen comes back, with the reason in the session log and in syslog
   under `kdos-session`.
3. **It insists on the one session bus.** It pings `unix:path=$XDG_RUNTIME_DIR/bus` for up to four
   seconds and refuses the session when nothing answers. When the login's
   `DBUS_SESSION_BUS_ADDRESS` names anything else, it logs the difference and sets the fixed
   address. `plasma-dbus-run-session-if-needed` starts a private bus whenever the variable is empty,
   and a private bus is one no box, user service or console login can reach: notifications,
   single-instance applications and the portals would all stop working with nothing on the screen
   to say why.
4. **It settles the desktop's environment** before the first program reads it:
   `XDG_CURRENT_DESKTOP=KDE`, and `XDG_CONFIG_DIRS` as described
   [below](#the-environment-the-session-starts-with). It unsets `QT_IM_MODULE`, `QT_IM_MODULES` and
   `GTK_IM_MODULE`, so each toolkit chooses its input-method route itself
   ([Input methods](#input-methods)) and a value a profile left behind cannot choose for it, and `TERM`, `LINES` and `COLUMNS`, which
   describe a terminal the session does not have: carried into Plasma, a terminal program Konsole
   runs for a launcher would draw at 80x24 in a larger window. A person who wants an input-method
   variable sets it in `~/.config/plasma-workspace/env/`, which Plasma reads after this.
5. **It warms the boxes** behind the pinned favourites: `kdos-appbox warmup`, in the background at
   `nice 10`. At 19 the warm-up loses every CPU slice to the starting desktop and is still busy
   minutes later, and a launch in that window waits for it. A second warm-up while one runs does
   nothing.
6. **It runs the `Exec=` line**, word-split as SDDM's own session script runs it and never
   globbed.

The Plasma session entry, `/usr/share/wayland-sessions/plasma.desktop`, is plasma-workspace's,
unedited. The pieces the wrapper and the environment script share are in
`/usr/local/lib/kdos/session-common.sh`, sourced by both: `kdos_session_open` (sets `$BROWSER` to
`xdg-open` when nothing has, so a program started by D-Bus has a link handler),
`kdos_session_xdg_dirs` and `kdos_session_boxes`.

### The environment the session starts with

Three sources set the environment, in the order a login meets them.

**`/etc/profile.d/10-wayland.sh`** runs in every login shell: a console, the serial line, ssh, and
the login shell the wrapper re-executes itself in. The runtime directory, the bus address, the
desktop's name and the configuration directories are filled in only where the session does not
already carry a value; the toolkit variables below them are set outright, so a value a login
carried in is replaced:

| Variable | Value, and what it does |
|---|---|
| `XDG_RUNTIME_DIR` | `/run/user/<uid>`, only when that directory exists: a shell no PAM session started (ssh) is pointed at the directory of a login that is still open, or at none |
| `DBUS_SESSION_BUS_ADDRESS` | `unix:path=$XDG_RUNTIME_DIR/bus`, only when the socket exists, so an ssh shell reaches a running session's bus |
| `XDG_CURRENT_DESKTOP`, `XDG_MENU_PREFIX` | `KDE` and `plasma-`, so a file opened from a console opens in the application the desktop opens it in: `kde-mimeapps.list`, where every KDOS default application is named, is read only by a program whose desktop names `KDE` |
| `XDG_CONFIG_DIRS` | `/usr/share/kdos/xdg` appended after `/etc/xdg` when missing |
| `QT_QPA_PLATFORM` | `wayland;xcb`: Qt uses Wayland and falls back to Xwayland only when its Wayland plugin cannot start, which is a Qt 5 program built without one |
| `QT_QPA_PLATFORMTHEME` | `kde`: a Qt 6 program finds plasma-integration's theme and the colours of `kdeglobals`; a Qt 5 program finds the `qt5ct` port's plugin under the same name, which reads the qt5ct files `kdos theme` writes |
| `GTK_USE_PORTAL` | `1`: GTK 3's native file chooser and print dialog go through the portals, so a GTK 3 program that uses them opens the KDE file dialog, and Firefox's file picker reads the same variable. GTK 4 uses the portal without it. A program that builds GTK's own file dialog widget, such as AbiWord, keeps GTK's dialog. GTK 3 also reads its theme, fonts and other settings from the Settings portal under the variable, and its `gtk-modules` from `org.gtk.Settings`, which the patched gtk3 opens beside the portal and kde-gtk-config's kded module serves whatever the variable says (see [Portals](#portals)) |
| `GDK_BACKEND` | Not set. GDK tries Wayland first by itself, and a value would override the `GDK_BACKEND=x11` an X11-only program sets for itself |
| `MOZ_ENABLE_WAYLAND`, `SDL_VIDEODRIVER`, `CLUTTER_BACKEND` | Wayland, for Firefox ESR, LibreWolf and Thunderbird, SDL and Clutter |

**`XDG_CONFIG_DIRS`** decides where every KDE program finds its defaults. `kdos_session_xdg_dirs`
builds it as `/etc/xdg:/usr/share/kdos/xdg`, with up to two overlays in front, and adds each
directory at most once:

| Directory | When | What it holds |
|---|---|---|
| `/usr/share/kdos/xdg-live` | The root is the live medium's overlay | No file index of a home that is gone at the next boot, no timed lock and no wallet for an account with no password, no update notice, the welcome tour on its live page |
| `/usr/share/kdos/xdg-vm` | `kdos in-vm` says this is a virtual machine | No idle dimming, blanking, timed lock or idle and lid suspend, which from outside the guest look like a hung one |
| `/etc/xdg` | Always | The machine owner's settings, and every package's autostart entries |
| `/usr/share/kdos/xdg` | Always | Every KDOS default: `kwinrc`, `kscreenlockerrc`, `powerdevilrc`, `startkderc`, `kde-mimeapps.list`, the environment scripts below, and the rest |

KDOS's defaults come after `/etc/xdg`, so a file the owner puts in `/etc/xdg` wins key by key and
an update never collides with an edited `/etc` file. A session whose list lacks
`/usr/share/kdos/xdg` reads Breeze's and upstream's defaults with nothing to say so; one that lacks
`/etc/xdg` loses every autostart entry, the polkit agent's among them. `~/.config` outranks all of
them, and `startplasma` puts `~/.config/kdedefaults` in front of the list it is handed. An automatic
login can carry one more directory first, `/etc/kdos/autologin-xdg`, which is how a machine that
logs in by itself starts behind the lock screen (see
[The security model](security-model.md#the-login-stacks)).

**`plasma-workspace/env/`** in `/usr/share/kdos/xdg` holds two scripts `startplasma` sources before
KWin starts, in one shell, keeping every variable they leave set:

- `kdos.sh` runs the same `kdos_session_xdg_dirs` and `kdos_session_open`, so a
  `startplasma-wayland` run by hand from a console settles the same names, exports
  `XDG_CURRENT_DESKTOP=KDE`, and sets `SSH_ASKPASS` to `ksshaskpass` when nothing has. A passphrase
  asked for with no terminal (ssh-add from an autostart entry, git or ssh run from an application)
  then comes up in a dialog whose answer the session's secret store can keep. `SSH_ASKPASS_REQUIRE`
  stays unset, so ssh and ssh-add in Konsole still ask on the terminal; git's own HTTPS prompts are
  the exception, because git prefers `SSH_ASKPASS` to the terminal when nothing else is set. It
  also exports `QSG_DISTANCEFIELD_ANTIALIASING=gray`: Qt Quick draws its distance-field text, the
  kind Plasma's labels and KSplash use, with subpixel antialiasing otherwise, since fontconfig's
  `rgba` and `kdeglobals`' `XftSubPixel` never reach it.
- `kdos-shortcuts.sh` copies the KDOS global shortcuts into the person's
  `~/.config/kglobalshortcutsrc`, which is the only file kglobalaccel reads. Each shipped entry is
  copied once, only when the person has no value of their own, and recorded in
  `$XDG_STATE_HOME/kdos/kglobalshortcutsrc.seeded`, so a shortcut reset to KDE's default (which
  kglobalaccel stores by deleting the entry) does not come back at the next login. See
  [KWin](../04-programs/kwin.md#shortcuts).

### How Plasma comes up

There is no systemd user instance, so the session is always Plasma's *classic* start:
`/usr/share/kdos/xdg/startkderc` sets `[General] systemdBoot=false`, and `plasma_session` starts
kded6, ksmserver, plasmashell and the autostart entries itself. Left unset, `startplasma` would
first ask the session bus for systemd's `plasma-workspace.target` and decide the same thing after
the question failed.

plasma_session starts plasmashell once and supervises nothing. plasmashell exits when KWin crashes
or is replaced, and KCrash restarts it only after a crash signal, so the KDOS kded module watches
`org.kde.plasmashell` and starts plasmashell again when the name has been gone for three seconds and
ksmserver says the session is not ending, at most five times a minute (see
[How plasmashell is kept running](../04-programs/plasma-shell.md#how-plasmashell-is-kept-running)).

Every daemon of the session that has no autostart entry is started by D-Bus activation from the
session bus: kactivitymanagerd, the portal front end and its KDE backend, the secret store and
the rest. The XDG autostart entries run in three phases;
`X-KDE-autostart-phase=2`, the last, comes after the shell. Upstream's entries start the polkit
agent (`polkit-kde-authentication-agent-1`), PowerDevil, kaccess, kwrited, `pam_kwallet_init` and
`xdg-user-dirs-update`, which writes `~/.config/user-dirs.dirs` and makes the directories at a
user's first login. KDOS ships these:

| Entry | Runs | What for |
|---|---|---|
| `kdos-session-good.desktop` | `kdos-notice --bless` | The second mark of a confirmed boot: once `org.kde.plasmashell` has answered for a minute, asks `kdos-sysupdated` to record that this boot reached a working desktop (see [Boot and init](boot-and-init.md#counting-and-confirming)) |
| `kdos-notice.desktop` | `kdos-notice` | Notices for updated programs that need a restart, known vulnerabilities, a trial or rolled-back system version, a program closed to free memory, and applications chosen at install that are not installed yet |
| `kdos-boxes-restore.desktop` | `kdos-appbox session restore` | The boxes of the last session, when the session is set to restore the previous one (see [Ending a session](#ending-a-session)) |
| `kdos-fwask.desktop` | `kdos-fwask` | The firewall's "Allow on this network?" question |
| `kdos-mpris.desktop` | `kdos-mpctl mpris` | Plasma's media controls for mpd, over MPRIS; only when mpd is installed |
| `kdos-speech-sync.desktop` | `kdos speech sync` | Read Aloud for the clipboard |
| `kdos-library-place.desktop` | `/usr/libexec/kdos/kdos-library-place` | The Library entry in Dolphin's Places |

The KDOS kded module, `kded-kdos`, is loaded by kded6 at start. It keeps the KDOS accent in step
with a colour scheme chosen inside Plasma, by running `kdos theme --from-plasma`, and offers the
application sets (`*.ktar`) found on a stick that has just been mounted. Its state is on the
session bus at `org.kde.kded6` `/modules/kdos`, interface `io.kdos.Kded1`.

`kdos toggle stay-awake` holds an inhibition of sleep and of the screen locker for as long as it is
on. The holder is a process of KDOS's, whose pid is in `$XDG_RUNTIME_DIR/kdos/stay-awake.pid`;
PowerDevil drops an inhibition the moment its holder leaves the bus, so the process is the state,
and the battery applet lists it as blocking.

### Applications chosen during installation

The installer offers two kinds of application. The native groups (see
[Packaging](packaging.md#groups)) are installed during the install, prebuilt from the release's
binhost; a group that cannot be, because the network or the binhost did not answer, is written to
the new disk's `/var/lib/kdos/groups-pending`, and `kdos update install --pending` installs it
later. Nothing in the session reads that file.

Boxed applications ticked in the installer are *offered* in the first session, never built in the
background. `kinstall` builds none of them: it writes the chosen ids to the new disk's
`/var/lib/kdos/apps-pending`. Discover's KDOS backend lists them under
"Selected during install", and `kdos-notice` says once per content of the file that they are
waiting, and withdraws the notice when the file names nothing. Building means podman and apt, which
can take twenty minutes or more on a machine booted for the first time, and a build that long with
nothing on screen leaves the machine busy with no visible cause.

## The user's services

A user's services run from that user's first login, of any kind, to the last logout. They are not
tied to a graphical session: logging out of Plasma to the login screen and back in keeps the same
bus, the same PipeWire and the same running boxes.

**turnstile** counts the logins. Its daemon, `turnstiled`, runs as root, started by
`/etc/init.d/65_turnstiled.sh` under `ksvc`; `pam_turnstile` in every login stack connects to its
socket, `/run/turnstiled/control.sock`. At a user's first login it creates `/run/user/<uid>` (mode
0700), runs the *backend* that starts the services, and exports `XDG_RUNTIME_DIR` and
`DBUS_SESSION_BUS_ADDRESS` into the login once the bus answers. At the last logout it stops them and
removes the directory. turnstile is the directory's only owner: elogind is built with its own
runtime-directory code removed (`no-runtime-dir.patch`), because two owners would delete each
other's directory, elogind's at its last session, under services and boxes still using it.
Configuration is `/etc/turnstile/turnstiled.conf`.

**The backend** is `/usr/lib/turnstile/ksvc`, a shell script that runs the table in
`/etc/kdos/user-services/` under `ksvc`, the same supervisor and the same `service_helper` as the
system's own `/etc/init.d` scripts:

| Script | Service | Notes |
|---|---|---|
| `10_dbus.sh` | `dbus` | The [session bus](#the-session-message-bus) |
| `15_ssh-agent.sh` | `ssh-agent` | The account's one SSH agent, at `$XDG_RUNTIME_DIR/ssh-agent.socket`; `ready` waits for the socket, so every login's `/etc/profile.d/50-ssh-agent.sh` finds it and exports `SSH_AUTH_SOCK` |
| `20_pipewire.sh` | `pipewire` | The media graph |
| `21_wireplumber.sh` | `wireplumber` | PipeWire's session manager; restarted whenever `pipewire` is |
| `22_pipewire-pulse.sh` | `pipewire-pulse` | The PulseAudio server every Pulse client finds, the panel's volume applet among them; restarted whenever `pipewire` is |
| `25_mpd.sh` | `mpd` | Only when a `mpd.conf` exists in one of the places mpd reads; the image ships none. It makes `$XDG_RUNTIME_DIR/mpd` first, where the socket [`kdos-mpctl`](../04-programs/kdos-command.md#kdos-mpctl) and every client `MPD_HOST` points at (`/etc/profile.d/45-mpd.sh`) reach mpd; mpd makes that directory itself only when it binds no address |
| `30_timers.sh` | `timers` | The user's periodic jobs: every row of `/etc/kdos/user-services/timers.d` and `~/.config/kdos/timers.d`, under `snooze`, in one supervised process. Its start also builds the first file index in the background (`kdos-updatedb --if-missing`) when there is none |
| `40_boxes.sh` | `boxes` | Starts nothing. Its `stop` runs first, because the table stops backwards, and stops every running container of the user's within thirty seconds, through `kdos-box stop`: `podman stop` signals nothing in a box that shares the host's PID namespace and has no cgroup of its own. It then ends podman's rootless pause process, by the pid in `$XDG_RUNTIME_DIR/libpod/tmp/pause.pid` |

Each script is asked `ready` before the next starts, so PipeWire's socket exists before WirePlumber
connects, and the backend reports ready only after the last. turnstile holds the login until then,
which is why the bus already answers when Plasma starts and `pipewire-pulse` already answers when
the volume applet loads. The waits are bounded at three seconds a script and ten in all, under
`login_timeout = 15`, past which turnstiled gives the login up, runtime directory and all. A login
that hangs for fifteen seconds and then has no bus is that.

Each service's pid, state and log are `kdos-svc.<name>.pid`, `.state` and `.log` in
`$XDG_RUNTIME_DIR/ksvc`, and the backend's own log is
`$XDG_RUNTIME_DIR/ksvc/kdos-user-services.log`. `service --user` works on the table the way
`service` works on the system's:

```sh
service --user list
service --user status dbus
service --user restart pipewire
service --user disable mpd     # a marker in ~/.config/kdos/user-services.disabled
```

**Which accounts get services.** Every account from uid 1000 up to `nobody`, plus `plasma-setup`,
the system account the first-boot wizard runs as in a whole Plasma session. `sddm`, the login
screen's account, gets `dbus` alone: it runs through `pam_turnstile` because nothing else makes
that account's runtime directory, and KWin will not start without one, and turnstile exports the
bus's address into its PAM environment, which SDDM hands to the greeter's KWin and the greeter.
The greeter's on-screen keyboard and keyboard-layout buttons are calls to that KWin over that bus;
it gets no PipeWire, timers or boxes. Any other system account gets a runtime directory and no
services. `root` gets no services (`root_session = no`); the system's services are rcS's.

**Lingering.** `loginctl enable-linger <user>` writes `/var/lib/elogind/linger/<user>`, and
turnstile, built to read the same directory, then starts that user's services at boot and keeps
them after the last logout, boxes included. elogind's `KillUserProcesses=no` is what lets a box or a
user service survive a logout at all.

**The last logout clears the runtime directory**, after the services have stopped. The clear
leaves every mount point in the directory alone, and every directory above one
(`rundir-mounts.patch`): a box's root is an overlay that `kdos-packd` composed under
`/run/user/<uid>/kdos/boxes/<box>/root`, and a clear that descended into it would write a whiteout
for every file of the box into its persistent state. A directory with a mount still in it is kept.

### The session message bus

One `dbus-daemon` per user, the `dbus` user service, listening at a fixed path:
`unix:path=$XDG_RUNTIME_DIR/bus`. Every login of the user, the Plasma session, the user's services
and every box's bus proxy are on it. A bus somewhere else would leave each box's proxy pointed at a
socket nobody answers, and with it single-instance applications, settings and notifications.

- **The address carries no GUID.** A bus restarted by the supervisor at the same path is then one
  every client can reconnect to; a strict D-Bus implementation (zbus, for one) refuses a
  reconnection whose GUID changed.
- **Its output, and that of every program it activates, goes to the supervisor's log**,
  `$XDG_RUNTIME_DIR/ksvc/kdos-svc.dbus.log`, and to syslog under `dbus`. That includes
  `plasma-shutdown` and the scripts it runs at logout.
- **polkit judges a program the bus started by the user's graphical session.** turnstile registers
  the user's services as an elogind session of their own, with no seat, and every program the bus
  activates lives in it, as does everything those programs start. polkit carries
  `turnstile-session.patch`: for a process in such a session it takes the user from the session
  and answers for that user's display session. So Plasma's Shut Down and Restart, which the bus
  activates, are the person at the machine and need no administrator's password, and a program
  launched from KRunner gets the same answers, and the same password dialog, as one launched from
  the application menu.

## Audio

PipeWire runs on the host as three user services, once per user rather than once per session:

1. `pipewire`, the media graph;
2. `wireplumber`, its session manager, which connects to the daemon's socket;
3. `pipewire-pulse`, the PulseAudio compatibility layer, which announces sinks that exist only once
   WirePlumber has created them.

Every login of the user, graphical or not, and every box share the sockets under
`$XDG_RUNTIME_DIR`. A second PipeWire would fight the first for the same devices, and one
supervisor per name makes a second impossible. WirePlumber and `pipewire-pulse` are clients of the
daemon that do not exit when it goes away, so both are supervised `--restart-with pipewire`: a
crash of the daemon restarts both once it is back. Plasma's volume applet and the Audio page of
System Settings talk to `pipewire-pulse`.

PipeWire is built with no session manager of its own (`-Dsession-managers=[]`). The daemon routes
nothing by itself: device discovery, which sink a stream lands on and a Bluetooth headset's profile
are all policy, and policy is WirePlumber's. `kdos-oomd` names `wireplumber` explicitly in the
processes it protects, because the `pipewire` prefix that covers `pipewire` and `pipewire-pulse`
does not match it, and killing it leaves a running graph with nothing connected.

**Bluetooth codecs are enabled by name**, not left to auto-detection: aptX, LDAC, AAC, LC3, Opus
and G.722, linked from `libfreeaptx`, `ldacbt`, `fdk-aac`, `liblc3` and `opus` (all in the port's
`depends`). SBC is mandatory in the profile and always present. Each codec is a meson feature
that silently disables itself when its library is missing, and a headset then falls back to SBC
with nothing saying why.

**LE Audio needs the Bluetooth daemon's half too.** bluez offers the LC3 endpoints of the Basic
Audio Profile (BAP, LE Audio's streaming profile) only when its experimental D-Bus interfaces and
the kernel's ISO socket feature (the isochronous channels LE Audio streams over) are both on, so
`/etc/init.d/60_bluetooth.sh` starts
`bluetoothd -n -E --kernel=6fbaf188-05e0-496a-9885-d6ddfdb4e03e`. Without them an LE Audio
headset or hearing aid falls back to A2DP (classic stereo playback) and HFP (classic headset
calls), or does not connect. They are command-line flags rather than a `main.conf`, so the file
the bluez package owns is not replaced.

### Reaching the graph from inside a box

A pack box or a store box sees a private runtime directory of its own as `/run/user/<uid>` (see
[What is shared into a box](#what-is-shared-into-a-box)). When the box's profile allows audio
(`audio = yes`, the default), `kdos-appbox` puts hard links to the session's `pipewire-0` and
`pulse/native` sockets in it. A socket's inode is what a `connect()` reaches, so a link is the same
socket, and the session's own directory stays out of the box. A restarted PipeWire binds a new
socket while the link still names the old one, so the box's keeper watches the session's runtime
directory and links again. A program that speaks PulseAudio then finds the server through the base
pack's `libpulse0`. With `audio = no` no link is made and the box has no sound server at all.

A plain ALSA program in a pack box needs one more file. `kdos-boxinit`, a pack box's first process,
writes `/etc/asound.conf` pointing ALSA's default PCM and mixer at PulseAudio
(`pcm.!default { type pulse }`, `ctl.!default { type pulse }`), using `libasound2-plugins` from
the Debian base. It writes the file only when that plugin is present, so a base without it
(Alpine) keeps ALSA's own default. A box created by `distrobox` from somebody else's image has no
`kdos-boxinit`, and so no redirect file.

Without the redirect, `default` inside the box is alsa-lib's built-in `plug → softvol → dmix`
chain, where dmix is alsa-lib's software mixer. When `/dev` is shared and the host `audio` group
survives the box's user namespace, that open succeeds and takes the sound card: the box has sound
and the rest of the machine has none until the box stops. The redirect moves `default` only;
`hw:0` and `sysdefault` still name the hardware.

### Reaching the graph from a plain ALSA program on the host

A plain ALSA program on the host reaches PipeWire through one file,
`/etc/alsa/conf.d/99-kdos-pipewire.conf`.
`alsa.conf`'s `@hooks` read `/var/lib/alsa/conf.d`, `/usr/etc/alsa/conf.d`, `/etc/alsa/conf.d`,
`/etc/asound.conf` and `~/.asoundrc`, and not `/usr/share/alsa/alsa.conf.d`, where PipeWire
installs its own drop-ins. Without a file in a directory that is actually read, every ALSA
program talks to the card directly, the first one to open it owns it, and PipeWire runs with no
device. The resulting stutter is hard to diagnose: a dmix client's underrun happens in userspace,
so it shows no xrun on the card and nothing in `dmesg`.

The file defines two devices and a default:

| Name | Is | Shown as |
|---|---|---|
| `kdos_pipewire` | PipeWire | "Default Audio Device (PipeWire)" |
| `kdos_card` | `empty`, a pass-through, onto `sysdefault`: the card directly | "Sound Card (no sound server)" |
| `pcm.!default` | Whichever PCM `$KDOS_ALSA_DEFAULT` names; `kdos_pipewire` when unset | |

A shell with no user services has no PipeWire: an ssh login, or root on the serial console. A
program run there with `KDOS_ALSA_DEFAULT=kdos_card` plays straight to the card:

```sh
KDOS_ALSA_DEFAULT=kdos_card kdos-bb
```

An undefined name fails with "Unknown PCM", and an absent PipeWire with "Host is down"; neither
failure is silent. `kdos_card` is exclusive in both directions: while it holds the card PipeWire
cannot open it, and the other way round. The mixer control (`ctl.default`) is left on the
hardware, so `alsamixer` still sets the card's levels when PipeWire is what went wrong.

Every redefinition in that file carries its `!`. The hooks run after the rest of `alsa.conf` is
parsed, so a plain `pcm.default { … }` aborts the whole configuration load and leaves every ALSA
program with no configuration at all. Check any edit with `aplay -L`.

`kdos_card` is a pass-through and not a `plug` of its own. `pcm.!default` is already a `plug`, and
`sysdefault` starts with one, so a third between them makes alsa-lib abort whatever program opens
`default` with `Assertion failed: !snd_interval_empty(i)` and exit status 134, while
`aplay -D kdos_card` on its own still plays. Check an edit to either device with
`KDOS_ALSA_DEFAULT=kdos_card aplay <file>.wav` as well as `aplay -L`.

### JACK programs on the host

There is no JACK server. The `pipewire` port builds PipeWire's own `libjack.so.0`, with its
headers and `jack.pc`, into `/usr/lib` (`-Dpipewire-jack=enabled -Djack-devel=true
-Dlibjack-path=/usr/lib`), so a JACK client such as Ardour, Hydrogen or LMMS loads it from the
default search path and appears as nodes in the same graph as every other stream. `pw-jack` is
not needed to start one. The SPA plugin that would make PipeWire a client of a separate `jackd`
(`-Djack`) is off.

Once `jack.pc` is installed, a consumer that leaves JACK on auto-detection gains it or not by
build order. Every port that can link JACK therefore names it, on or off. It is on, with
`pipewire` in the port's `depends`, in `portaudio`, `sdl3`, `mpv`, `mpd`, `openal-soft`,
`fluidsynth`, `obs-studio`, `ardour`, `hydrogen` and `lmms`; every other port names it off. A
port built before `pipewire` cannot have it, and neither can one `pipewire` itself depends on:
`pipewire` links `ffmpeg`, so ffmpeg's `--enable-libjack` (off unless named) stays off and its
`jack` capture device is not built. `portaudio`, and `lirc`, which reads audio through it, sit in
`42_graphics` rather than `41_system` so that they come after `pipewire`.

### The quantum, and why a virtual machine gets a bigger one

The *quantum* is how many audio frames PipeWire processes per cycle; it sets the latency. Under a
hypervisor the card gets a deeper buffer, because an emulated card has no clock of its own: the
emulated codec advances its DMA position from the emulator's main loop, the same loop that
updates the display, so the position moves in bursts. PipeWire keeps about one quantum in the
device and times its wake-ups from that position, so a wake-up later than one quantum is silence.
The underrun belongs to the *player*, not the card, and nothing appears in `dmesg`.

`/etc/pipewire/pipewire.conf.d/99-kdos-vm.conf` applies only when `cpu.vm.name` is set, which
PipeWire does only under a hypervisor:

| Setting | Value |
|---|---|
| `default.clock.quantum` | 4096 (85 ms at 48 kHz) |
| `default.clock.min-quantum` | 4096 |
| `default.clock.max-quantum` | 8192 |

PipeWire's own configuration already raises the floor to 1024 frames in a virtual machine, which
suffices for a device whose clock is steady. Measured on this image against a 1920×1080
accelerated display, the card held at least 42 ms of audio and typically 64 ms at that floor, and
at least 85 ms and typically 127 ms at 4096 frames. A host that is still late needs 8192 frames,
170 ms, the ceiling the rule allows. A machine on real hardware does not match the rule and keeps
PipeWire's defaults: a 1024-frame quantum, about 21 ms at 48 kHz. `pw-metadata -n settings`
shows which is in force, and `pw-top`'s `ERR` column counts underruns.

The quantum is also the granularity of every music clock on the machine: a program that follows
a song position sees it advance in quantum-sized steps, not smoothly, and has to tolerate one
quantum of flat step rather than chase it. [kdos-bb](../04-programs/kdos-bb.md#the-servo) has the
measured staircase and the correction that copes with it.

## Portals

A *portal* is how a sandboxed application asks the host to do something it cannot do itself: open
a file chooser, share the screen, open a link. The front end, `xdg-desktop-portal` (1.22.1), owns
the public D-Bus interfaces on the session bus; *backends* implement them. Both are started by
D-Bus activation the first time something calls them.

The front end picks backends from `<desktop>-portals.conf`, where `<desktop>` is the lowercased
first name in `XDG_CURRENT_DESKTOP`. The session's name is `KDE`, so the file is
plasma-workspace's `/usr/share/xdg-desktop-portal/kde-portals.conf`:

```ini
[preferred]
default=kde
org.freedesktop.impl.portal.Settings=kde;gtk;
org.freedesktop.impl.portal.Secret=kwallet
org.freedesktop.impl.portal.Notification=plasmanotify
```

`xdg-desktop-portal-kde` answers nearly every interface with KDE's own dialogs: the file chooser
(KIO's, which reaches `sftp:` and `smb:` locations through kio-fuse), the application chooser for
OpenURI, screenshots and screen casting through KWin, remote desktop and input capture, print,
e-mail, inhibit, background, global shortcuts, the clipboard, wallpaper, USB access, Access, and
Settings, which hands an application the session's colour scheme, accent and fonts under
`org.kde.*` and `org.freedesktop.appearance`. `kde-portals.conf` names `kde;gtk;` for Settings, and
`xdg-desktop-portal-gtk` answers the keys the KDE backend does not: every `org.gnome.desktop.*`
key (the GTK theme, icon and cursor themes, fonts, `gtk-im-module` and the window-button layout
under `org.gnome.desktop.wm.preferences`) and `org.gnome.fontconfig`, read from the GSettings
kde-gtk-config writes. GTK 4 on Wayland takes every one of those settings from the portal and
none from `settings.ini`; a key no backend returns takes GTK's built-in fallback, which for
`gtk-im-module` is `simple`, no input method, and for the button layout `menu:close`. GTK 3 on
Wayland does the same under `GTK_USE_PORTAL=1`, on the host and in every box, except for two
groups. `gtk-modules` and `gtk-fontconfig-timestamp` come only from kde-gtk-config's
`org.gtk.Settings`, and the host's gtk3 is patched to open that service beside the portal, so
`colorreload-gtk-module` loads and a running GTK 3 program repaints on a colour change; a box's own
GTK 3 is not patched, and its filtered bus does not reach the service. `gtk-im-module` GTK 3 sets
itself to `wayland` on a compositor that offers `text-input-v3`, above the portal's empty value
(see [Input methods](#input-methods)). Where both
backends answer a key, as for `org.freedesktop.appearance`, the front end takes the KDE one's.
kwallet answers Secret.

KWin hands its privileged interfaces only to the backend started from its own desktop entry: the
hidden `org.freedesktop.impl.portal.desktop.kde.desktop` names `ScreenShot2` and the fake-input,
window-management and screencast protocols, and its `Exec=` is the binary's path. A backend run
from any other path gets no screenshots, no screen cast and no remote input.

**A box reaches the portals and little else.** A box's session bus is its keeper's
`xdg-dbus-proxy`, which passes `org.freedesktop.portal.*` and a short list of scoped calls (see
[Sandboxed clients](security-model.md#sandboxed-clients)). A GTK application routes through the
portal only when it believes it is sandboxed, which it decides from a marker file
(`/.flatpak-info`) or from `GTK_USE_PORTAL=1`, and a container has neither, so `kdos-appbox` sets
the variable. Without it every boxed GTK application draws its own file dialog over a home the box
may not share. Firefox's default file-picker setting consults the same variable.

**The portal names a caller only from an application scope, and no caller is in one.** The front
end names a caller from the cgroup its process is in, the `app-<launcher>-<id>-<n>.scope` shape
systemd gives an application. Without libsystemd, `xdg-desktop-portal` carries
`cgroup-app-id.patch`, which reads the same name from `/proc/<pid>/cgroup`. A host application that
Plasma starts runs in the login's own cgroup, `/sys/fs/cgroup/<session>/session`, beside
`plasmashell`: KIO's launcher places a child in a scope only through a systemd user manager, which
KDOS does not run. Every call a box makes arrives from its proxy's pid, and the keeper moves the
proxy into a leaf under the box's own scope, `app-kdos-<box>-<n>.scope`, when that scope exists;
it is never made, because elogind leaves `/sys/fs/cgroup/<session>/cgroup.procs` root's (see
[Containers](security-model.md#containers)), so the proxy stays in the login's cgroup too. The
portal therefore sees every host application and every box as one unnamed program, and KDE's
dialogs call it "an unidentifiable application". See
[Known gaps](../06-reference/known-gaps.md#host-applications-and-boxes-reach-the-portals-unidentified).

`OpenURI` ("open this on the host for me") is what a boxed application asks when a link or a
downloaded file is clicked. The front end resolves the host's handlers itself and launches the
chosen one from the host's own desktop entries, so a link in a boxed application opens in the
host's browser. Inside a pack box, `kdos-boxinit` writes `/usr/local/bin/xdg-open` (with
`x-www-browser` and `sensible-browser` linked to it), a script that sends the URI to that portal
with `gdbus`; a relative or absolute path is turned into a `file://` URI first.

### Access, and what it unlocks

The Camera, Screenshot and Location portals each ask the Access backend before handing an
application what they guard, and the KDE backend asks the person in a dialog. Answers are kept in
the front end's permission store under `~/.local/share/flatpak/db/`, keyed by application id, so
each question is asked once per application id. Every caller has the empty id (see above), so one
remembered answer covers every host application and every box.

Location comes from the system GeoClue service, `geoclue` (port `geoclue`). D-Bus starts it on the
system bus, as its own `geoclue` account, when the first client asks, and it exits after a minute
with no clients. Every source is built in: the Wi-Fi and 3G sources ask beaconDB, a public
database of Wi-Fi and cell-tower locations, for a position from what `wpa_supplicant` and
ModemManager see, the modem sources read ModemManager, the NMEA source finds a phone's GPS feed
through Avahi, the compass comes from `iio-sensor-proxy`, and there are IP-address and static
sources. It runs with no consent agent: `/etc/geoclue/conf.d/90-kdos.conf`, from the port, empties
the agent whitelist and marks the Location portal a system component, and the port's
`no-agent.patch` treats an empty whitelist as leave to answer every client at once. Upstream would
hold every client until an agent registered, which on this image would be never. So GeoClue
answers whoever asks on the system bus. A boxed application cannot ask it directly, because the
box's bus is a proxy of the session bus, not the system bus; it asks the Location portal.

An icon or a sound an application hands a portal is decoded by
`xdg-desktop-portal-validate-icon` or `xdg-desktop-portal-validate-sound`, each of which re-runs
itself under `bwrap` with no network, no home directory and a read-only `/usr`, so an exploit in
an image or sound decoder lands in an empty namespace. `bwrap` is not setuid; it sandboxes
through an unprivileged user namespace.

## Screen capture and the clipboard

Screen capture belongs to KWin. Spectacle takes screenshots through KWin's `ScreenShot2` D-Bus
interface and records through the screencast plugin; a screen share in a browser or a video call
goes through the ScreenCast portal, whose KDE backend asks KWin for a stream over
`zkde_screencast_unstable_v1` and hands the application a PipeWire node through kpipewire.

The clipboard is Klipper's, part of the Plasma shell: it keeps the history and is what the
clipboard applet shows. A program on the host reaches the clipboard through the ordinary Wayland
data device, and a clipboard manager through `ext_data_control_manager_v1`.

A boxed application is kept off both, at two layers:

- **At the compositor**, every client from a box arrives on a socket tagged with the box's security
  context, and KWin refuses it the screencast, data-control, window-management and fake-input
  protocols, and the desktop-shell protocols that reach past its own windows.
- **On the bus**, the box's proxy hides `org.kde.KWin`, so `ScreenShot2`, which answers any caller
  that can reach it, is out of reach, and so is KWin's scripting interface.

That is what makes the portal the sanctioned route rather than a convenience: a boxed screen
recorder cannot bind the capture interfaces at all, so it asks the ScreenCast portal, which runs on
the host and asks which screen or window to share. An application in a box that records sound as
well (OBS, for example) takes its video through the same portal and its audio from PipeWire
through the socket linked into its runtime directory. Spectacle itself runs on the host, never in
a box.

A screenshot taken with Phosphor mode on shows the plain picture: KWin's CRT pass gives
screenshots, screen casts and the colour picker the desktop without the effect.

### Granting a box more than the allowlist

A box can be given named sets of the refused protocols with a `grant` line in its profile,
`~/.config/kdos/boxes/<name>.conf`:

```ini
grant = screencast, data-control
```

KWin reads the key once per client, for clients whose security-context engine is `io.kdos.appbox`
or starts with `io.kdos.appbox+` (`+private` and `+tagged`, the two `kdos-boxsock` registers), and
for no other:

| Name | Unlocks |
|---|---|
| `window-list` | `org_kde_plasma_window_management`, `org_kde_plasma_activation_feedback`, `org_kde_plasma_virtual_desktop_management` |
| `fake-input` | `org_kde_kwin_fake_input` |
| `screencast` | `zkde_screencast_unstable_v1` |
| `data-control` | `ext_data_control_manager_v1` |
| `layer-shell` | `zwlr_layer_shell_v1` |
| `output-management` | `kde_output_management_v2`, `kde_external_brightness_v1`, `wp_drm_lease_device_v1` |
| `dpms` | `org_kde_kwin_dpms_manager` |
| `plasma-shell` | `org_kde_plasma_shell`, `kde_screen_edge_manager_v1`, `org_kde_kwin_keystate` |
| `input-method` | Nothing: KWin offers the input-method interfaces only to the input method it started |
| `secrets` | Not a Wayland grant: the box's bus proxy lets it talk to `org.freedesktop.secrets`, the password store |

Any other name grants nothing, and `kdos-box profile <name>` says so beside the name. A grant takes
effect for the box's next client; a running client keeps what it already bound. What each name puts
at risk is in [the security model](security-model.md#granting-a-box-past-the-allowlist), and
System Settings' Boxes page edits the line.

## Input methods

An *input method* turns keystrokes into text that a keyboard cannot type directly: Chinese,
Japanese or Korean, for example. The engine and the application never speak to each other
directly; KWin is the wire between them:

```
fcitx5  ──input-method──▶  KWin  ──text-input──▶  the application
                                                  (host or boxed)
```

The engine is `fcitx5`, with the Chinese (`fcitx5-chinese-addons`), Anthy (Japanese) and Hangul
(Korean) add-ons. KWin starts it itself, as the session's input method:
`/usr/share/kdos/xdg/kwinrc` sets `[Wayland] InputMethod=/usr/share/applications/org.fcitx.Fcitx5.desktop`.
KWin offers the input-method protocol to that one process and to no other client, host or boxed. The
engines are set on System Settings' Input Method page (`fcitx5-configtool`), and the Virtual
Keyboard page chooses between fcitx5 and an on-screen keyboard.

An application reaches the engine one of two ways. Through KWin's text-input protocol: Qt 6, GTK 3
and GTK 4 through `text-input-v3`, Qt 5 through `text-input-v2`. Or through a toolkit module that
speaks to fcitx5's own D-Bus interface: `fcitx5-qt` for Qt 6, `fcitx5-gtk` for GTK 3 and GTK 4.
The session wrapper unsets `QT_IM_MODULE` and `GTK_IM_MODULE`, so a variable a profile left
behind does not choose for the toolkit, and each toolkit then chooses as follows:

| Toolkit | With both variables unset |
|---|---|
| Qt 6 | `text-input-v3` through KWin. `fcitx5-qt`'s plugin is loaded only for `QT_IM_MODULE=fcitx`; it is installed for a program that sets that itself and for the settings page, which links its libraries |
| GTK 4 | `text-input-v3` through KWin whenever KWin offers it: GTK's Wayland module registers at priority 100 and fcitx5's at 10, and the choice by priority is made only while the `gtk-im-module` setting is empty. GTK 4 reads that setting from the Settings portal, where `xdg-desktop-portal-gtk` answers GSettings' empty default; with no backend answering it GTK falls back to `simple` and takes no input method. Under Xwayland, where the Wayland module does not apply, fcitx5's module |
| GTK 3 | `text-input-v3` through KWin: on a compositor that offers `zwp_text_input_manager_v3`, GTK 3 sets `gtk-im-module` to `wayland` for the Wayland display, so its choice by locale, where fcitx5's module would win, never runs. Under Xwayland, fcitx5's module over D-Bus, loaded through `/usr/lib/gtk-3.0/3.0.0/immodules.cache`, which `fcitx5-gtk` installs |

Both routes are read from the toolkits' sources (GTK 3.24.52 `gtksettings.c` and 4.24.0
`gtkimmodule.c`) and measured on the rig with `GTK_USE_PORTAL=1` and the Settings portal's GTK
backend answering `org.gnome.desktop.interface`: `gtk-query-settings` prints `wayland` and
`gtk4-query-settings` an empty module, and pinyin typed into AbiWord, a GTK 3 program, and into a
GTK 4 `zenity --entry` shows fcitx5's candidates. A program that gives the input method no cursor
position gets its candidate window at the top-left of its window: AbiWord is one.

A boxed application uses KWin's route. Inside a box `QT_IM_MODULE=wayland` is set; the GTK
equivalent is deliberately not set at all, because GTK on Wayland chooses its route by itself
when it is unset, and setting it is how a working GTK application stops accepting input. A box
carries its own GTK, without the host's `fcitx5-gtk`, and its filtered session bus does not reach
fcitx5, so the D-Bus route is never a boxed program's. The text-input protocol is not refused to a
box, because denying it would deny input methods to the applications that need one most.

**An X11 application has an input method only through a toolkit module.** Xwayland passes no text
input to its X clients, and the route an X client has to an engine without a module is XIM, which
fcitx5 provides only when built with X11 support; the port is built without it (`ENABLE_X11=Off`).
A GTK 3 or GTK 4 program under Xwayland on the host loads `fcitx5-gtk`'s module and reaches fcitx5
over D-Bus; every other X client on the host, and every X client in a box, has none.

## The environment a box receives

A command run in a container with `podman exec` inherits nothing: not the container's own init
environment and not the caller's. So `kdos-appbox` states every variable a launch needs:

| Variable | Value, and why |
|---|---|
| `PATH` | `/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games`. Without `/usr/games`, Debian's games are all "not found" |
| `HOME`, `USER`, `LOGNAME` | The user's own. Some toolkits crash with no user name |
| `LANG` | `C.UTF-8`, whatever the host's: it is the one UTF-8 locale every Debian has without generating one, and GTK and others refuse a non-UTF-8 locale |
| `XDG_RUNTIME_DIR` | `/run/user/<uid>`, which in a pack box or a store box is the box's private directory |
| `XDG_SESSION_TYPE` | `wayland` |
| `WAYLAND_DISPLAY` | The launcher's own tagged socket from `kdos-boxsock`, by its absolute path inside the box. Empty when the profile says `wayland = no`, so a toolkit that would guess `wayland-0` finds no display to guess at |
| `DBUS_SESSION_BUS_ADDRESS` | The box's filtered bus, the keeper's proxy. Without it a single-instance application blocks with no window and no message |
| `XDG_CURRENT_DESKTOP` | `KDE`, so a toolkit that uses portals knows which desktop it is on |
| `XDG_CONFIG_DIRS` | `/etc/xdg:/usr/share/kdos/xdg`; KDOS's defaults are bound into the box read-only at the host's path, so a boxed KDE application reads the same `kdeglobals`, fonts and kiosk keys as one on the host |
| `GTK_USE_PORTAL` | `1`. See [Portals](#portals) |
| `GTK_THEME` | The theme the session's `~/.config/gtk-3.0/settings.ini` names, `:dark` added when it prefers the dark variant. The Settings portal answers a box no theme name and the box's GSettings holds none, so without it GTK draws Adwaita under the session's colours. A running program keeps the theme it started with |
| `GSETTINGS_BACKEND` | `keyfile`, because no settings daemon is reachable over the filtered bus |
| `QT_IM_MODULE` | `wayland`: input methods through the compositor |
| `QT_QPA_PLATFORMTHEME` | `kde` when the box's runtime has the KDE platform theme, which reads the `kdeglobals` Plasma writes into the shared home; `gtk3` otherwise. A pack box takes it, and any other per-runtime variable, from the `env =` lines of its packs' metadata (see [Packs and boxes](packs-and-boxes.md)) |
| `CUPS_SERVER` | `/run/cups/cups.sock`, when that socket exists on the host |
| `NO_AT_BRIDGE=1`, `GTK_A11Y=none` | Unless assistive technology is in use; see below |
| `QT_QPA_PLATFORM`, `SDL_VIDEODRIVER` | Wayland first and X11 as the fallback, so Qt 5 and SDL 2 do not take `DISPLAY` over the tagged Wayland socket; a catalogue or pack `env` row that forces X11 overrides them |
| `DISPLAY`, `XAUTHORITY` | Unless the profile says `x11 = no`: the display, and the box's copy of the cookie in its runtime directory. A box with `x11 = no` gets an empty `DISPLAY`, so a toolkit with an X11 fallback does not reach for one it would be refused |
| `LIBGL_ALWAYS_SOFTWARE` | `1` only when the profile's `render` key resolves to software. Nothing is exported for hardware rendering, because Mesa already picks the right driver |

**Accessibility follows the person, not a setting.** The accessibility bus is unfiltered: a client
on it can read the text of every other application on it. So a box is given it, as a hard link to
its socket in the box's runtime directory, only while assistive technology is in use: the screen
reader is on in System Settings, the running AT-SPI registry says an assistive tool is attached,
`~/.config/kdos/a11y` exists, or `KDOS_A11Y=1` is set for one launch (`KDOS_A11Y=0` forces it off).
Otherwise the GTK bridge is switched off, so the application does not look for the bus when it
starts.

## What is shared into a box

What a box sees depends on its *lane* (see
[Packs and boxes](packs-and-boxes.md#two-lanes-one-box)). `kdos-appbox` creates a pack box with
`podman create --rootfs` over the overlay `kdos-packd` composed, and a store box from the
catalogue's container image, and both receive the mounts below. A box `distrobox` made from
somebody else's image shares what distrobox shares instead, which includes the session's own
runtime directory with KWin's `wayland-1`, the bus and the X11 cookie in it.

| Host path | Inside a pack box or a store box |
|---|---|
| `$HOME` | Read-write, at the same path. With `home = private` in the profile the host home is not shared; the box's own `home` directory beside its writable layer is bound at the home path instead |
| `~/.config/kdos/boxes` | Read-only over the shared home, so a box cannot change its own profile |
| `$XDG_RUNTIME_DIR/kdos/box/<box>` | Read-write, as `/run/user/<uid>`: the box's tagged Wayland sockets, its bus proxy, the audio and accessibility links, and the X11 cookie for a box handed X11. Never the session's `wayland-1` or `bus` |
| `/tmp` | Read-write and shared. A throwaway box, the one "Open in a throwaway box" makes for a single file, gets a `/tmp` of its own instead, with only the X11 socket directory bound in, read-only, when it is handed X11 |
| `/dev`, `/sys` | Read-write, unless the profile says `devices = private`; then only `/dev/dri`, and only with `gpu = yes` |
| `/dev/shm` | Read-write |
| `/run/cups` | Read-write, when `/run/cups/cups.sock` exists at creation time |
| `/var/lib/kdos/packs/mnt` | Read-only, when it exists, so links into mounted data packs resolve |
| `/usr/share/kdos/xdg` | Read-only, KDOS's defaults |
| `/usr/libexec/kdos/kdos-boxinit` | Read-only, as `/usr/libexec/kdos-boxinit`, the box's first process |
| `/run`, `/run/lock` | Private temporary filesystems |

**Sharing is fixed when the box is created.** A box created before the print service was running
does not have the CUPS socket until the box is created again, while `CUPS_SERVER` is decided at
every launch. The two halves are asymmetric on purpose: the variable probes again each time, and
a volume can only be added when the box is created. The same holds for every profile key that
changes a namespace or a volume (`home`, `devices`, `network`, `ipc`, `gpu`, and the rest). The
contents of the private runtime directory are not fixed: a launch links the sockets again.

To apply a changed key, remove the box with `kdos-box remove <box>`; the next launch (or
`kdos-box create <box>`) creates it again with the current profile and host state. Removing a box
keeps its profile, `~/.config/kdos/boxes/<box>.conf`, and its writable layer,
`~/.local/share/kdos/boxes/<box>/upper`, and never touches the home directory. On a live session,
whose home is on the boot overlay, the writable layer is in `$XDG_RUNTIME_DIR/kdos/boxes/<box>/`
instead and does not survive the session, because the kernel refuses an overlay upper directory
on overlayfs.

## Ending a session

Log Out, Restart and Shut Down are Plasma's. ksmserver asks each application to save its state and
closes its windows, then `plasma-shutdown` runs every file in every `plasma-workspace/shutdown/`
directory along `XDG_CONFIG_DIRS`, in name order, waiting for each, before KWin quits. KDOS ships two
in `/usr/share/kdos/xdg/plasma-workspace/shutdown/`:

| Script | Does | Bound |
|---|---|---|
| `kdos-boxes.sh` | `kdos-appbox session save`: records every running box, and every boxed application that was open when the logout began, in `$XDG_STATE_HOME/kdos/session-boxes` | 10 s |
| `kdos.sh` | `kdos-box gc --session-end`: stops every box that has run past its profile's `autostop`, without asking the desktop which boxes still have a window, since every window is closed by now | 20 s |

Before those, when the session is set to restore the previous one (System Settings → Session →
Desktop Session, ksmserverrc `[General] loginMode`, whose default is `restorePreviousLogout`),
`plasma-shutdown` runs `plasma-fallback-session-save`. A Wayland application does not speak the X11
session protocol, so ksmserver saves none of them itself; the fallback records only the desktop
entry of each open window, in `~/.local/state/plasmasessionrestorestaterc`, and at the next login the
autostart entry `org.kde.plasma-fallback-session-restore.desktop` starts each of those entries once,
with no arguments. An application comes back with its own start-up state: no documents, tabs or
window positions, and one window however many it had. A window that matches no desktop entry is not
recorded, and does not come back (see
[Known gaps](../06-reference/known-gaps.md#session-restore-reopens-applications-not-their-documents)).

`kdos-boxes.sh` sorts first (`-` before `.`), so the record is taken while the boxes `gc` is about
to stop are still running. KDE's own session restore starts launchers, not boxes, so when the
session is set to restore the previous one, `kdos-boxes-restore.desktop` reads the record at the
next login and starts the boxes and boxed applications it lists. Both scripts are started by the
session bus, so their output is in `$XDG_RUNTIME_DIR/ksvc/kdos-svc.dbus.log`, not in the session
log.

When KWin has quit, the PAM session closes, elogind ends the session, and SDDM shows the login
screen again. A Restart or Shut Down then reaches elogind's `Reboot` or `PowerOff`, which runs
`/sbin/reboot` or `/sbin/poweroff`, and so sysvinit's runlevel 6 or 0 (see
[Boot and init](boot-and-init.md#shutdown)).

The user's services do not end with the session. If the user has no other login and does not
linger, turnstile stops them at this point: the `boxes` service first, so every container is
stopped while its sockets still exist, then the rest of the table backwards, and then it clears
the runtime directory. A box with no `autostop` that is still running at a Log Out is therefore
stopped by the last logout and not before.

A login that arrives while that stop is under way, such as SDDM's automatic login logging the
account straight back in after `loginctl terminate-user`, waits for it. turnstile starts a fresh
set of services once the old manager has exited and releases the login when they are ready, never
on the readiness of the set being stopped (`stopping-manager-readiness.patch`), so Plasma never
starts on a session bus that is about to go. The backend's `stop` ends the manager before it stops
the table, and waits for it: a manager stopped while it is still starting the table finishes the
service it is starting, starts nothing more, and leaves no service behind.

## See also

- [Architecture overview](overview.md) — where the session sits in the whole system
- [Boot and init](boot-and-init.md) — everything up to the login screen
- [KWin](../04-programs/kwin.md) — the compositor, its defaults and its patches
- [kdos-appbox](../04-programs/kdos-appbox.md) — launching boxes and box profiles
- [Packs and boxes](packs-and-boxes.md) — how a box is built and started
- [The security model](security-model.md) — the login stacks, the sandbox and what it denies
- [The daemons](../04-programs/daemons.md) — the root daemons a session talks to
- [The desktop](../02-user-guide/desktop.md) — using what this chapter starts
- [How KDOS differs](../01-philosophy/how-kdos-differs.md#the-desktop) — how this session compares
  with the desktops of other distributions

<!-- book-nav -->
---

*Part III — Architecture, chapter 14.* Previous: [13. Boot and init](boot-and-init.md) · [Contents](../README.md) · Next: [15. Packaging](packaging.md)
