# Diagnostics

KDOS ships its own diagnostic commands beside the usual process and log tools. This page covers
them: `kdos doctor`, which checks the faults KDOS is known to have; `kdos why` and `kdos explain`,
which say what owns a file and why it is the way it is; `kdos restarts`, `kdos stutter`,
`kdos cve`, `kdos hey` and `kdos appid`; `kpkg check`, which compares the root with what was
installed; and the resource monitor. Where each kind of log is written, and how to read it, is a separate page:
[Logs](logs.md). Each command is described in full in
[The kdos command](../../kdos/04-programs/kdos-command.md).

## Installation

| Port | Provides |
|:--|:--|
| [kdos-tools](#port-kdos-tools) | [`kdos`]{.idx term="kdos (command)"}, whose diagnostic subcommands are `status`, `version`, `doctor`, `why`, `explain`, `oracle`, `restarts`, `stutter`, `cve`, `appid` and `hey`; the reasons corpus under [`/usr/share/kdos/reasons`]{.idx}; the vulnerability database [`/usr/share/kdos/secdb.txt`]{.idx} and the list of bundled libraries [`/usr/share/kdos/bundled.txt`]{.idx} |
| [kdos-res](#port-kdos-res) | [`kdos-res`]{.idx}, the resource monitor, and its setuid helper [`kdos-resctl`]{.idx} |
| [kdos-comp](#port-kdos-comp) | The frames socket `kdos stutter` reads and the command socket `kdos hey` uses |
| [btop](#port-btop), [htop](#port-htop), [bottom](#port-bottom), [procs](#port-procs), [iotop](#port-iotop), [nvtop](#port-nvtop) | Terminal process, I/O and GPU monitors |
| [lsof](#port-lsof), [strace](#port-strace) | Open files, and system calls, of a running process |
| [util-linux](#port-util-linux) | `dmesg`, [`lsblk`]{.idx}, `findmnt` |
| [fastfetch](#port-fastfetch) | A one-screen summary of the machine, which the login banner also uses |

`kpkg check` is part of `kpkg` itself, which is not a port: the bootstrap step that builds it is
described in [kpkg](../03-bootstrap/120-kpkg.md), and the command in [kpkg](kpkg.md).

Every port above is named in a phase list, so all are on the image. No service runs for any of
them. `kdos` is installed as
`/usr/local/bin/kdos`, one of the names of `/usr/sbin/ksvc`.

## Configuration

### /usr/share/kdos/reasons

`kdos why`, `kdos explain` and `kdos oracle` read a corpus of short documents, each stating one
constraint in the system and its consequence. The [kdos-tools](#port-kdos-tools) port installs it,
with the vulnerability database, from `src/system/kdos-tools/build.sh`:

<!-- kfs:include src/system/kdos-tools/build.sh lines=40-44 pin=9fb84a77f3e2 -->

Each document begins with a header. One from `src/system/kdos-tools/reasons/`:

<!-- kfs:include src/system/kdos-tools/reasons/default-first-in-rc-xml.txt lines=1-4 pin=407096c976a5 -->

`path:` and `port:` are what `kdos why` matches. Three variables point the commands elsewhere:

| Variable | Replaces |
|:--|:--|
| `KDOS_REASONS` | `/usr/share/kdos/reasons` |
| `KDOS_SECDB` | `/usr/share/kdos/secdb.txt` |
| `KDOS_BUNDLED` | `/usr/share/kdos/bundled.txt` |

### The compositor's sockets

`kdos stutter` connects to `$XDG_RUNTIME_DIR/kdos-frames.sock` and `kdos hey` to
`$XDG_RUNTIME_DIR/kdos-cmd.sock`. Both exist only while the compositor runs, and only for the
account whose session it is. [The frames socket](../../kdos/04-programs/kdos-comp.md#the-frames-socket)
and [The command socket](../../kdos/04-programs/kdos-comp.md#the-command-socket) give the formats.

### ~/.local/share/kdos/observed-app-ids

The compositor appends every application identifier a window presents, once each. `kdos appid`
compares it with the installed launchers.

### ~/.config/kdos/res.conf

The resource monitor's settings, which ship absent; the keys are in
[Configuration](../../kdos/04-programs/kdos-res.md#configuration).

## Usage

Every command below has been run on the live medium, from a login shell and from the desktop's
own environment. The self-test also runs `doctor`, `cve` and `stutter` against fixtures.

### A first look at a machine

1. Say what the machine is and what it runs:

   ```bash
   kdos version
   kdos status
   ```

2. Check it for KDOS's known faults:

   ```bash
   kdos doctor
   ```

   The lines are grouped under `Kernel`, `Hardware`, `Boxes`, `Session`, `Containers` and
   `Desktop`. Each is `[ ok ]`, `[warn]` with the fix where there is one, or `[skip]` with the
   reason the check could not answer here. The exit status is 1 when anything warned, which on
   the live medium is always: its Boxes section warns that `$HOME is on overlayfs (a live
   session), so a persistent box cannot exist`.

3. Run it as root as well, for the checks only root can make: the Security section, which says
   whether the `kdos` password is still the shipped one, appears only when `/etc/shadow` is
   readable. The Containers section warns under root, because [boxes]{.idx term="box"} are meant to run as the
   desktop account.

   ```bash
   sudo kdos doctor
   ```

Run `kdos doctor` in a terminal inside the desktop for the Session and Desktop sections to mean
anything; from `tty2` or SSH they report a missing session.

### Doctor's answer in a script

```bash
kdos doctor --json | grep '"warn"'
kdos doctor >/dev/null || echo "kdos doctor found something"
```

`--json` prints one record per check line, such as
`{"section": "Boxes", "level": "warn", "message": "..."}`, with the same exit status.

### What owns a file, and why it is that way

1. Ask about a path:

   ```bash
   kdos why /etc/fonts/conf.d/56-noto-preferred.conf
   ```

   It prints the package, its version and its recipe, then every reason that names the path or a
   directory above it. `none — provided by fs/ or 10_bootstrap, or not installed` means the file
   came from the base filesystem tree, is `kpkg` or `kinstall` (which `10_bootstrap` compiles
   outside any package), or came from no package at all.

2. Ask about a port by name:

   ```bash
   kdos why kdos-comp
   ```

3. Search the reasons for a word:

   ```bash
   kdos explain              # every title
   kdos explain portal       # every reason that mentions it
   ```

   Each title is followed, on the next line, by the reason's file name in brackets, such as
   `(default-first-in-rc-xml)`; with a word, each reason's text follows its title. A word no
   reason contains prints `nothing recorded about '<word>'`, and a path or port that `kdos why`
   finds in no reason gets `no recorded reason`.

### After an upgrade

```bash
kdos restarts
```

It lists each running process that still maps a library or program an upgrade replaced, with the
packages concerned. Restart each one, or log out and back in for the desktop's own. Exit 0, with
`Nothing is running deleted code.`, means nothing is running old code, and exit 1 means something
is. A process whose memory map the caller cannot read is skipped, so run `sudo kdos restarts` to
cover the daemons and every other account's processes.

### Checking the installed files

```bash
kpkg check pipewire
sudo kpkg check --all
```

Each finding is a line such as `pipewire: changed <path>`, and the last line counts them:
`1 package checked: 0 missing, 0 changed, 0 mode, 0 config`. The exit status is 1 when anything
is missing, changed or has the wrong mode, and 2 for a package that is not installed. `config`
lines are edited files under `/etc` and do not count as faults; `unowned` lines, from `--all`, are files under `/usr` or `/opt`
that no package claims, such as `unowned ./usr/share/xdg-desktop-portal/kdos-portals.conf` for a
file of the base filesystem. `--all` reads every installed file, about ten minutes on the live
medium. The format is in
[Checking an installed root](../../kdos/03-architecture/packaging.md#checking-an-installed-root).

### Known vulnerabilities

```bash
kdos cve
kdos cve openssl
kdos doctor --cve
```

The comparison is offline, against the database shipped with the image. The report's first line
gives the database's date and age, as in `Alpine secdb of 2026-08-12 (<n> days old)`, and a
warning under the summary flags a database more than 180 days old. A package the database does not
carry is reported as unknown, never as clean, and the summary line counts each kind:
`<n> checked, <n> not in the database, <n> behind a recorded fix`. `kdos doctor --cve` prints the
same report as `kdos cve`. The exit status is 1 when any package is behind a recorded fix.

### A desktop that hiccups

1. In a terminal inside the session, start the watcher and leave it running:

   ```bash
   kdos stutter
   ```

   It prints `Watching for late frames. Ctrl-C to stop.` and then nothing until a frame is late.

2. Use the machine until the hiccup happens again. Each late frame is reported with its output, how
   late it was, the compositor's own render time, the pressure figures and the busiest processes.
3. Stop it with Ctrl+C.

A static screen produces no frames, so start something that moves before waiting for a hiccup.

### Windows and launchers

```bash
kdos hey list
kdos hey outputs
kdos appid
```

`kdos hey list` gives each window's id, workspace, state, application id, box and title, and
`(no windows)` when there are none; `kdos hey outputs` gives each screen's name, size, scale and
position; `kdos hey run Close <id>` acts on one window. `kdos appid` reports each application
identifier that matches no launcher file name, which is what puts a second icon beside a pinned
one on the panel. It reads the identifiers the compositor has recorded in this home; with none
recorded yet it reports on the windows open at that moment, and with no window open either it
says `No windows have been observed yet.`

### Processes and resources

1. Press Ctrl+Shift+Escape, or Super+Ctrl+T, for **Resources**, or run `kdos-res`. On `tty2` or
   over SSH it draws full screen in the terminal.
2. The **Processes** page lists everything; **Boxes** groups processes by box; **Energy** shows
   relative use by application. Its keys are in [Keys](../../kdos/04-programs/kdos-res.md#keys).

`btop`, `htop` and the others above work in any terminal. `sudo iotop` shows which process holds
the disk.

## Tips

- Most `kdos` subcommands share one exit-status convention: 0 nothing found, 1 something found or
  failed, 2 used wrongly, 127 the program it hands over to is missing. A subcommand that differs
  says so in its section of [The kdos command](../../kdos/04-programs/kdos-command.md).
- `kdos oracle` prints one reason, the same line the login banner shows. It stays the same all day
  and changes after a reboot.
- `kdos doctor` checks the setuid bit on the helpers that need one. A lost bit fails silently
  elsewhere: the lock screen refuses every password, or no box starts. Its Desktop section names
  the repair.
- `kdos status --bar` prints one line of running boxes and exported applications, for a script or
  a status bar.
- A skip from `kdos doctor` says the check could not answer, not that it passed.

## Troubleshooting

**`kdos doctor` warns `WAYLAND_DISPLAY=… but … does not exist`.**
The shell inherited the variable from a session that has ended. The compositor it names is gone;
see [Desktop troubleshooting](desktop-troubleshooting.md).

**`kdos stutter` exits at once with status 2.**
There is no frames socket: the compositor is not running, or the command was run from a shell whose
`XDG_RUNTIME_DIR` is not the session's. The message names the path it tried, as in
`nothing is reporting frame timing on /run/user/0/kdos-frames.sock` for root. A shell with no
`XDG_RUNTIME_DIR` at all, such as one reached through `sudo`, gets `no XDG_RUNTIME_DIR — this is
not a desktop session`.

**`kdos hey` exits 1.**
The compositor cannot be reached, or refused the request, for instance an id that names no window,
which prints `kdos hey: no view with that id`.
`kdos doctor` checks the command socket in its Desktop section.

**`kdos explain` prints `no reasons installed` and exits 1, and `kdos why` finds no reason for
anything.**
No reasons are installed at `/usr/share/kdos/reasons`, or `KDOS_REASONS` points at an empty
directory. `kpkg check kdos-tools` shows whether the files are missing.

**`kdos cve` exits 2.**
`/usr/share/kdos/secdb.txt` is missing, or `KDOS_SECDB` names a file that is not there; the message
is `kdos: no security database at <path>`.

**`kdos-res` cannot end or renice a process another account owns.**
`kdos-resctl` has lost its setuid bit. `kdos doctor` warns `kdos-resctl is not setuid root` and
prints the fix; see
[Acting on a process](../../kdos/04-programs/kdos-res.md#acting-on-a-process) for what the helper is
allowed to do.

**`lsblk` shows no filesystem type, label or UUID.**
Those columns are filled for root only; run `sudo lsblk -f`.

## Known gaps

- [kdos doctor cannot check hardware a machine does not have](../../kdos/06-reference/known-gaps.md#kdos-doctor-cannot-check-hardware-a-machine-does-not-have)
- [lsblk shows a filesystem's type, label and UUID to root only](../../kdos/06-reference/known-gaps.md#lsblk-shows-a-filesystems-type-label-and-uuid-to-root-only)
- [btop's GPU panel is empty on Intel graphics unless btop runs as root](../../kdos/06-reference/known-gaps.md#btops-gpu-panel-is-empty-on-intel-graphics-unless-btop-runs-as-root)
- [kdos-res draws its selected row in the accent colour](../../kdos/06-reference/known-gaps.md#kdos-res-draws-its-selected-row-in-the-accent-colour)

## See also

- [Diagnosis](../../kdos/04-programs/kdos-command.md#diagnosis) and
  [kdos doctor](../../kdos/04-programs/kdos-command.md#kdos-doctor): every diagnostic subcommand
- [Vulnerability tracking](../../kdos/03-architecture/packaging.md#vulnerability-tracking): how `kdos cve` compares versions
- [kdos-res](../../kdos/04-programs/kdos-res.md): the resource monitor
- [When a boot stops](../../kdos/03-architecture/boot-and-init.md#when-a-boot-stops): which file explains each boot stage
- [Logs](logs.md), [Services](services.md), [kpkg](kpkg.md), [Out of memory](out-of-memory.md),
  [Desktop troubleshooting](desktop-troubleshooting.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 419.* Previous: [418. Portals and screen sharing](portals-and-screen-sharing.md) · [Contents](../README.md) · Next: [420. Desktop troubleshooting](desktop-troubleshooting.md)
