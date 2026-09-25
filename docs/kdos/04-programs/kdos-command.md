# The kdos command

`kdos` is the front door to this distribution: a dispatcher whose subcommands answer the questions
a general-purpose system cannot. Each one is documented below with what it does, what it measures,
and what it refuses to claim.

The dispatch table in `src/packages/kdos-tools/kdos.c` is the authoritative list; this page
follows it.

## Synopsis

```
kdos <subcommand> [arguments...]
kdos help
kdos version
```

An unknown subcommand exits 1 and points at `kdos help`.

## Finding a subcommand

| Subcommand | Answers |
|---|---|
| [`app`](#kdos-app) | Install, remove, export and import containerised applications |
| [`appid`](#kdos-appid) | Do launcher icons match the windows they open? |
| [`clone`](#kdos-clone) | Copy this boot medium onto another, verified by read-back |
| [`cve`](#kdos-cve) | Offline vulnerability tracking against a vendored database |
| [`doctor`](#kdos-doctor) | Check the session for the breakage this system actually has |
| [`explain`](#kdos-why-and-kdos-explain) | Browse the recorded reasons behind decisions |
| [`hey`](#kdos-hey) | Ask the window manager about windows, outputs and boxes |
| [`help`](#kdos-help) | The command list and the keybinding sheet |
| [`march`](#kdos-march) | Measure whether a processor tuning flag is worth keeping |
| [`menu`](#kdos-menu) | Open the menu on a named route |
| [`notify`](#kdos-notify) | Raise a toast, or drive the notification daemon |
| [`oracle`](#kdos-oracle) | One recorded lesson, picked for today |
| [`panel`](#kdos-panel) | Put the bar away and bring it back |
| [`persist`](#kdos-persist) | Keep a live session's writes across a reboot |
| [`places`](#kdos-places) | The places column, and the way to keep one |
| [`rebuild`](#kdos-rebuild) | Rebuild KDOS from the sources on this machine, offline |
| [`remind`](#kdos-remind) | A toast, later |
| [`restarts`](#kdos-restarts) | Which supervised services are restarting |
| [`sandbox`](#kdos-sandbox) | Run a native program under a Landlock profile |
| [`settings`](#kdos-settings) | The control centre, or one of its pages |
| [`share`](#kdos-share) | Send a file to another machine over one code word |
| [`speech`](#kdos-speech) | Fetch and manage transcription models |
| [`status`](#kdos-status) | What this machine is and what it is running |
| [`stutter`](#kdos-stutter) | Why the desktop hiccuped, with the application's name |
| [`theme`](#kdos-theme) | Set, preview and audit the accent |
| [`thumb`](#kdos-thumb) | Put a thumbnail in the shared cache |
| [`toggle`](#kdos-toggle) | Stay-awake, night light and do-not-disturb |
| [`trash`](#kdos-trash) | The desktop's trash, from a prompt |
| [`update`](#kdos-update) | Check for and apply upgrades across the A/B slots |
| [`version`](#kdos-version) | Release, commit, and whether the tree was clean |
| [`why`](#kdos-why-and-kdos-explain) | What provides a path or a port, and why it is that way |

---

## kdos help

```sh
kdos help
```

The output opens with a `WHERE THINGS LIVE` block naming the three lanes — `kpkg` for the host,
`kdos app` for applications, `kdos-box` for environments — and then lists the commands and the
keybinding sheet.

That opening block is the point of the command. One list showing the whole system's verbs is the
only place a reader learns that those are three different questions rather than three tools that
overlap.

## kdos theme

```sh
kdos theme                    # print the accent in force
kdos theme <name>             # phosphor amber ice bone norton borland perfect paper
kdos theme list | next | prev
kdos theme style <file>
kdos theme --audit [accent]
kdos theme --preview <accent>
```

Eight schemes are compiled into `libkcolor`; `bone` is the default, and the default is derived from
the scheme's name rather than from its position in the table, so the table can be reordered freely.
Setting an accent regenerates every themed artefact and commits the change in one ordered
operation. The user-facing half is in [Theming](../02-user-guide/theming.md).

`--preview` writes the state file and sends the signal and does nothing else — no GTK stylesheet,
no icon theme, no cursors, none of the foreign configuration files. Those take seconds and are read
by programs that are not running, so a preview repaints every KDOS surface at once and leaves a
boxed application wearing the old accent. It is what `kdos-theme`'s arrow keys run, and it is why
that picker restores the accent it opened on unless it is told to keep one.

The commit order is load-bearing. The wallpaper cache and the accent state file are both inputs to
the signal that repaints the session, so both are written before it is sent: the state file because
a signal arriving first makes the desktop re-read the accent it already had, and the wallpaper
because the compositor re-decodes on that same signal.

The signal reaches every long-lived surface by name, and the match must be exact. The panel, the
desktop and the notification daemon are three names of one binary, so signalling only one retints
the panel and leaves the desktop icons and any live toast in the old accent. Exactness matters in
the other direction too: `kdos-comp` is a substring of `kdos-desktop-start`, the shell script that
owns the session, and an unhandled signal kills a shell.

`--audit` checks the palette claim. It does not try to recognise "palette colours" in the installed
files — that test would have to know which mixes are legal, and would drift from the generators.
It runs the same generators with the home and cache directories pointed at a scratch directory and
compares byte for byte, symlinks included. Anything that differs, differs from what this machine's
palette produces right now. It writes nothing outside its scratch directory and signals nothing: an
audit that repaired what it found would be a `kdos theme` with a misleading name. Exit 0 clean, 1
on drift, 2 if it could not run.

An accent may follow `--audit`, so `kdos theme --audit amber` asks what would have to change for
amber. That is how a switch is checked before it is made.

## kdos notify

```sh
kdos notify <summary> [body]
kdos notify --time | --battery
kdos notify --dismiss | --dismiss-all | --raise | --dnd
```

A toast, through `kb_notify()` — the same call a terminal makes for a child's OSC 9, so the two
cannot drift apart.

`--time` and `--battery` compute their own text, and that is why they are verbs. A chord runs a
static command: `rc.xml` binds a string, so a chord that wanted the time could not be a chord that
formatted it. `Super+Ctrl+Alt+t` and `Super+Ctrl+Alt+b` are the two questions a bar answers by
being on the screen, and this desktop puts its bar away.

`--dismiss`, `--dismiss-all`, `--raise` and `--dnd` are one line down the notification daemon's own
socket. `kdos-notifyd` already holds the toasts, the history and the Do Not Disturb flag and
already answers a socket in `$XDG_RUNTIME_DIR`; a chord needs a command, and this is it. There is
no second daemon and no second owner of what is on the screen. It is silent when nothing is
listening — a chord pressed on a machine with no notification daemon should do nothing, not print
an error into a session with nowhere to show it.

`--raise` takes the entry out of the history rather than copying it. A notification is on the
screen or it is in the centre and never both, or dismissing it twice files two copies of one
thing. It comes back without its buttons: the notification it came from is closed and its actions
belong to the program that sent it, so a button pressed here would fire a verb nothing is waiting
for.

The charge is read from the kernel and not from `kdos-energyd`. That daemon estimates what a
program is costing; it holds no battery state at all, and its socket answers `ping`, `report` and
`report-json` about nothing else. `libkproc` reads `/sys` for the resource monitor, and one reader
means one answer. The body names wear as well as charge: a battery reporting 90% can hold 70% of
what it held new, and a person deciding whether to unplug wants both.

## kdos toggle

```sh
kdos toggle                  # list them and their state
kdos toggle <name>           # flip it
kdos toggle <name> on|off    # set it
```

Three switches, kept as flag files under `~/.local/state/kdos/toggles/` where a file's presence
means on:

| Name | Does | Read by |
|---|---|---|
| `stay-awake` | Never save, lock or blank on idle | `kdos-comp`'s idle policy |
| `night-light` | Warm the palette | Every surface, on the retint signal |
| `dnd` | Hold notifications back | `kdos-notifyd` |

An unknown name is refused with the list rather than written. Which file each one is and who reads
it is also in [configuration](../06-reference/configuration.md#localstatekdostoggles).

A toggle whose consumer reads it on the retint signal sends that signal — the same one `kdos theme`
sends — after writing the file and never before. The surfaces re-read their state the moment it
lands, so a signal sent first is one they answer with the state it replaced. `night-light` is that
toggle; the other two are stat'd on a tick their reader already runs and need nothing.

A chord has nowhere to print, so it gets a toast. Typed at a prompt the state goes to standard
output; spawned by a keystroke it is a one-line notification naming the switch and where it now
stands. Two of the three change nothing visible, and a switch flipped by a keystroke and answered
by nothing cannot be told from a broken keystroke. `Super+Ctrl+i` is `stay-awake` and
`Super+Ctrl+Shift+n` is `night-light` — not `Super+Ctrl+n`, which `rc.xml` gives to `kdos-note`.

## kdos panel

```sh
kdos panel toggle
```

One verb, because `rc.xml` runs a command and cannot send a signal. The bar is `kdos-shell`, so
this signals it: `SIGUSR1`, by exact `comm`, which reaches the panel and not the desktop icons or
the notification daemon, the other `argv[0]`s of the same binary.

It walks `/proc` itself instead of running `pkill`, and this is the one signal in `kdos` that has
to. procps-ng's `pkill`, the one on this image, takes `-USR1` as a signal before it reads any
option, but toybox's takes `-U` for a user id and refuses `-USR1` as `-U SR1`. A signal spelled so
that it depends on which options a `pkill` happens to have stops being sent the day one is swapped,
and it stops silently, because the refusal goes to a standard error nobody reads.

It outranks autohide. While the bar is put away the pointer will not bring it back; without that
rule the bar returns the first time the mouse crosses the bottom row. Nothing is reported when no
panel is running: a desktop with no bar has already granted the request.

## kdos menu

```sh
kdos menu summon <route>     # open the menu on a named place
kdos menu toggle [<route>]   # close it if it is open, else open it
```

A route is a name for a place in the system, from `/etc/kdos/menu.conf` and the user's copy of it —
see [configuration](../06-reference/configuration.md#etckdosmenuconf). This is what a script holds
instead of a chord: a chord is rebindable and a menu row moves, and neither is something another
program can refer to. Summoning opens the menu with the route in its search field, which is the one
code path that finds a route.

The menu is `kdos-palette`, which is what `rc.xml` binds `Super+space` to. Taking somebody to a
named place is a search with the name already typed, and a command hardcoding `kdos-start` would be
a third answer to that question. The program name is split into an argument vector here, as the
session splits it: the first word is the program, which is what `pkill -x` matches, because that
match is against a name and never against a command line.

Toggle closes by signal and opens by spawn, in that order. `pkill` reports whether it signalled
anything, so one call answers "was it open" and closes it, with no pidfile and no round trip. The
match is exact, for the same reason the retint signal's is.

## kdos status

```sh
kdos status
kdos status --bar            # one line: boxes and exported applications
```

What this machine is and what it is running: the kernel, the accent in force, the installed package
count, the exported and alien application counts, the session and the boxes.

The alien application count is a read of the baked table rather than a container-engine call, so it
answers on a machine where no box has ever been created — which is every machine before the first
launch.

The session line names the session rather than saying "wayland", because "wayland" would hide the
case where the session is something else entirely. `session_name()` is the one place that decides,
so this line cannot disagree with `kdos version`.

## kdos doctor

```sh
kdos doctor [--json]
kdos doctor --cve [...]      # delegates to kdos cve
```

Checks the things that actually break on this distribution rather than running a generic health
sweep. Exit 0 with no warnings, 1 with any; the exit status carries the verdict in both text and
JSON mode, so a script does not have to choose between reading the text and parsing the structure.

There are three report levels, not two: `ok`, `warn`, and `skip` with a reason. Half of what it
asks cannot be answered in a virtual machine — no energy counter, no wireless, no discrete
graphics, no boot medium — and reporting those as ok would be a green line for something never
tested, while a warning would make every virtual machine look broken.

| Section | Checks include |
|---|---|
| Kernel | The Landlock ABI level, and whether the microcode in the boot image matches the revision the processor is running |
| Hardware | The regulatory database, audio firmware, graphics firmware, the boot medium, and **device present but unopenable** — it walks the attached devices and reports each one the calling user cannot open, naming the owning group |
| Boxes | Whether the pack filesystem is loadable, whether `kdos-packd` answers and by which mount route, whether the home directory's filesystem can host a container layer, and whether every mounted pack still has a file behind it |
| Session | Whether the compositor's **socket** exists rather than whether the variable is set, `XDG_RUNTIME_DIR`, and whether `kdos-comp`, `kdos-shell` and the wlroots portal are running |
| Containers | The mount-namespace root, and the `subuid` and `subgid` mappings rootless containers need |
| Desktop | The accent state file, the foot theme, the KDE bridge file, the portal configuration, `~/.local/bin` on `PATH`, Xwayland's socket, the session daemons and their sockets, and five setuid bits: `kdos-checkpass`, `kdos-resctl`, `newuidmap`, `newgidmap`, and `dbus-daemon-launch-helper` with its group, `messagebus` |
| Security | The default password. Root only — an ordinary user gets no section at all rather than a check that pretends it looked |

"Add yourself to this group" is an instruction; "permission denied" is not. That difference is why
the hardware check names the group.

The session check reads the socket rather than the variable because a session that died leaves
`WAYLAND_DISPLAY` behind in every shell that inherited it, and a doctor reporting that green sends
a person looking anywhere but at the real cause.

The box check reports, on a live session, that the home directory is on an overlay and therefore a
persistent box cannot exist. That is a real rule stated to the person it affects rather than a
failure.

Losing a setuid bit is silent and catastrophic in each of four different ways: `kdos-checkpass`
without it means every password at the lock screen is wrong, `kdos-resctl` without it means
`kdos-res` cannot end a process, a mapping helper without it means the container engine exits
125 and no box starts, and the bus's launch helper without its bit or its group means no D-Bus
system service is ever activated — Wi-Fi's supplicant, fingerprints and firmware updates among
them.

`--cve` is delegated rather than inlined. The vulnerability answer is a table of findings with its
own exit code and its own vintage to quote, and folding it into doctor's ok/warn lines would
flatten "17 packages are behind a recorded fix" into one warning that says nothing.

## kdos version

```sh
kdos version
kdos -V
```

The release, the commit it was built from, and whether that tree was clean. It names the session
from the same test `kdos status` uses, so it reports the desktop a person is sitting at rather than
the one the image happens to ship.

## kdos app

```sh
kdos app list [--all] | search <text> | info <id> | groups
kdos app install <id|group>... [--dry-run] | install --pending
kdos app launch <id> | remove <id|group>...
kdos app export <file.ktar> <id|group>... | import <file.ktar> [<id>...]
kdos app tui add <name> <command> [--float] [--size COLSxROWS] [--icon NAME] [--category X]
kdos app tui rm <slug> | ls
```

The application front end, covered from the user's side in
[Applications](../02-user-guide/applications.md).

It is the command-line half of the store, over the same catalogue the graphical surface reads.
Every verb that builds, removes, exports or imports runs `kdos-appbox` as a child — one
implementation, so a command line and a button cannot disagree about what installing means.

`list` is what is installed; `--all` is the catalogue. Printing all 180 applications whenever
somebody types `kdos app list` buries the handful they actually have.

A size is labelled an estimate wherever it is printed. What apt resolves on the day depends on the
snapshot, and shared runtime layers are counted once on disk however many applications name them. A
number presented as fact that the install then contradicts is worse than none.

There is no update verb. An application is a stack of images this machine built, so rebuilding it
*is* the update, and `install` over an existing one does that.

`install --pending` builds what the installer chose and could not install itself. The file is
removed only on a clean run, so a partial one leaves it and a second attempt does the rest.

`kdos-packd` is needed only by `import`, the one verb that mounts anything. Refusing to list what
this machine can build because a daemon is down would refuse the store on every machine that has
never imported a set. And `import` never names a path to the daemon: it hands a filename inside the
daemon's own staging directory and nothing else, which is what keeps something reachable from
`wheel` from being `mount /dev/sda2 /etc`.

### kdos app tui

`tui` makes a terminal program an application without anybody editing a file by hand. It writes
`~/.local/share/applications/kdos-tui-<slug>.desktop` with `Terminal=true`, the two KDOS keys that
say how the window should open, and an `Exec` quoted a field at a time by the library that reads
one — never concatenated, so a quoted path with a space in it survives the round trip through a
file two other programs parse.

`X-KDOS-TUI=true` is what `rm` checks, and it is the whole safety of the verb. A slug is a person's
word and the same word can name an entry the image shipped, so without the marker this would be a
way to delete somebody else's application. The slug is prefixed for the other half of that: a file
in this directory shadows the one in `/usr/share`, so a colliding name would take a shipped entry
off the menu rather than adding a row beside it.

The command is an argument vector, not an `Exec` line. Every `%` in it is a literal and is written
doubled, because a single one begins a field code in the file. An entry added this way opens its
program; it is not a file handler, and one that needs to be is a recipe's entry rather than this.

`tui` answers before the pack daemon is asked. It writes a file in this person's own data directory
and needs `kdos-packd` for nothing, so a verb refused because the daemon is down, or because the
caller is not in `wheel`, would be a verb refused for a reason that has nothing to do with adding a
menu row for `ncdu`.

## kdos why and kdos explain

```sh
kdos why <path|port>
kdos explain [topic]
```

`why` answers what provides a path or a port and why it is configured the way it is. For an
absolute path it prints the owning package first, out of the package database's own file list.

`explain` browses the recorded reasons — 78 of them, installed under `/usr/share/kdos/reasons` —
each one a constraint and its consequence. The match is a substring over the whole document rather
than over the filename alone, so `kdos explain musl` finds the reasons that mention it in passing.

## kdos sandbox

```sh
kdos sandbox [profile] [--read P] [--write P] [--no-network] [--tcp PORT] [--explain] -- <cmd> [args...]
```

Runs a native program under a Landlock profile, and `--explain` prints what the filter would refuse
without running anything. On a kernel with no Landlock it says so rather than running unconfined
and claiming otherwise.

## kdos appid

```sh
kdos appid [--quiet]
```

Does the launcher's file identifier match the identifier a real window presented?

A dock matches a running window to its launcher by the entry's file identifier, so a mismatch shows
a second generic icon beside the pinned one. This checks the left-hand side against the right.

The right-hand side is a ledger the compositor appends to the first time each window maps, falling
back to the live window list where no ledger has been recorded. The two answer different questions
and the report says which it used: the ledger is every window this machine has ever shown, and the
live list is what is on screen now.

## kdos restarts

```sh
kdos restarts [--quiet] [--json]
```

Which supervised services have been restarting, and how often. A crash loop is visible here before
it is visible anywhere else.

## kdos stutter

```sh
kdos stutter [--json] [--fixture DIR]
```

Watches the compositor's frame timing and, when a frame is late, says what the machine was doing at
that moment. Three sources, none of which is an answer alone, joined:

| Knows | Does not know |
|---|---|
| A frame was late, by how much, and what the compositor's own render cost | Who did it |
| The machine was starved, and of what | By whom |
| Who burned processor time and who sat blocked on I/O | That anyone cared |

The output is a sentence:

> 7 frames dropped on eDP-1 (133 ms) — the busiest just then: an indexer waiting on the disk, and
> an application at 92% of a core.

The closest prior art reads pressure statistics and says outright that it cannot identify which
process caused a frame miss. That sentence is what this finishes.

The render cost separates the two explanations. Over a large fraction of the frame budget and it
says the desktop itself was late — the one causal claim it makes, because there it has both halves.
Otherwise it reports what it measured and names who was busy, and it never says "X caused this":
attribution from a half-second sample window is circumstantial, and a tool claiming otherwise would
be wrong the first time two things were busy at once.

Two details that are not obvious:

- **Blocked before busy.** A process asleep in uninterruptible I/O shows almost no processor time
  while it is the thing holding the disk, so sorting on processor time alone hides exactly the case
  the I/O half exists for.
- **Container names come from the supervisor's command line, not from control groups.** With no
  cgroup delegation a rootless container frequently sits in the root group, which says nothing.
  Walking the parent chain to the supervising process and reading its name costs a few file reads
  and no engine call, which matters because this runs while the machine is already struggling.

`--fixture` points the whole sampler at a recorded system state — two `/proc` snapshots and an
event stream — which is what makes an attribution engine testable at all.

## kdos march

```sh
kdos march probe             # which instruction set levels this processor has
kdos march run <port>...     # build it twice and measure
kdos march report            # the ledger
kdos march decide <baseline> <optimised> <noise%>
```

```
$ kdos march run lz4
lz4  x86-64-v3  baseline 0.321s  -march=x86-64-v3 0.302s  +5.9% (noise 16.7%) -> reverted
       the win is inside the noise; that is not a win
```

Builds the port twice on this machine, runs the port's own benchmark against both, and keeps the
flags only where the win clears both a fixed floor and the machine's own measured noise.

Four rules:

- A port with no declared benchmark is unmeasurable, never a winner. Most ports have no meaningful
  benchmark, and assuming a win for them is the blind optimisation this replaces.
- The median of several runs, not the mean — which the worst outlier owns — and not one run, which
  measures the scheduler.
- The noise floor is measured, not assumed. It is the spread of the samples themselves, and a win
  smaller than it is the machine breathing.
- The benchmark's setup runs once and is not timed. A fixture belongs outside the stopwatch.

`report` is the ledger: kept, reverted, unmeasurable, with the summary line. A report listing only
winners would be a sales pitch; the reverts are the evidence that the measuring is real.

The argument for measuring rather than choosing a tier is in
[Decisions](../01-philosophy/decisions.md).

## kdos rebuild

```sh
make build KDOS_ISO_SOURCES=1     # a developer medium
# ...boot it...
kdos rebuild [--dry-run] [--iso-only] /mnt/disk/work
```

The medium rebuilds the medium, with no network at any point. Every leg of this is old — live
systems have shipped their sources for decades — and what none of them does is rebuild the medium
from the medium. KDOS can because of three properties it already has: the repository builds
offline, KDOS can build KDOS, and packages are reproducible, so a rebuild can be compared to what
it was built from rather than merely produced.

The sources go on the medium's outer filesystem beside the system image, not inside it, so they
cost the installed system nothing and are readable the moment the live image is up. It is opt-in,
because they are gigabytes of already-compressed archives and squashing them again buys nothing.

The command is checks plus the ordinary build, and the checks are the valuable half. A live
medium's root is an overlay whose upper layer is RAM: a rebuild started there reports gigabytes
free, eats memory, and dies hours in with the machine unusable. The work directory is therefore
refused when it is on a temporary or overlay filesystem — a free-space check cannot see that — and
again when it has too little space or a build tool is missing.

Everything after the checks is the same orchestrator the normal build runs, compiled on demand out
of the tree being built, so the build is driven by the sources on the machine rather than by a
binary from somewhere else.

## kdos persist

```sh
kdos persist                        # report
sudo kdos persist create [<device>] [--yes]
```

```
$ kdos persist
no persistence store

  This session's writes are in RAM and go when it is
  powered off. `kdos persist create` makes a store in
  the free space after the image on the boot medium.

$ sudo kdos persist create
  disk       /dev/sda  (32G)
  partitions 3 now; the store becomes number 4
  filesystem ext4, labelled KDOS_PERSIST
```

A live session's writes land in the overlay's upper layer, which is a tmpfs. This makes that upper
a real filesystem instead: one partition, labelled `KDOS_PERSIST`, in the free space after the
image.

The label is the whole interface. The initramfs asks `blkid` for it by name and uses whatever
answers, so the store can be on the boot stick, on a second stick or on an internal disk, and none
of them are recorded anywhere. Nothing writes a path into a configuration file, because a path is a
promise about enumeration order that USB does not keep.

It has to be a filesystem carrying xattrs, hardlinks and a `d_type`, which is why `create` makes
ext4 and why the initramfs refuses vfat, exfat and ntfs by name. overlayfs rejects such an upper
with `EINVAL` — the same answer it gives for every other bad mount — so a boot that tried it without
checking would fall back to a tmpfs having said nothing anyone could act on.

Two consequences of it being an overlay upper, both of which surprise people:

- A store is used from the *next* boot, not the one that created it. `kdos persist` reports
  `present, NOT in use by this session` for exactly this case, because saying "store: yes" would
  tell somebody their work was being saved when it was not.
- A change that stops the desktop coming up is still there at the next boot. The **KDOS Live (clean
  session)** entry passes `nopersist` on the kernel command line, which ignores the store for one
  boot without deleting it.

`create` appends a partition and never rewrites the table: the entries already on a written stick
describe the image the machine is running from. The new partition is registered with `partx -a`
rather than by re-reading the table, because a re-read is refused while a partition on that disk is
mounted — and on the boot medium, one always is.

## kdos clone

```sh
kdos clone                          # list what may be written to
sudo kdos clone /dev/sdb
```

```
$ kdos clone
source  /dev/sda  9.5G

  DEVICE            SIZE  MODEL
  /dev/sdb          32G   Ultra Fit
  /dev/sdc         8.0G   DataTraveler   (too small)
```

The medium writes the medium — the operation somebody standing in front of two sticks actually
wants. It is a raw copy and deliberately nothing cleverer: the boot arrangement is whatever the
medium already carries, so a copy boots exactly what the original boots and there is no second
opinion about how a KDOS stick is laid out.

| Flag | Does |
|---|---|
| `-y`, `--yes` | Skip the confirmation, which is otherwise typing the device name |
| `--no-probe` | Skip the counterfeit probe, saying what was given up |
| `--no-verify` | Skip the read-back, saying what was given up |
| `--extent` | Print the image's exact byte count and stop |
| `--source <path>` | An image file or device to clone instead |

The image's length comes from the image, and two records describe it. A small image written to a
large stick leaves the device reporting the large size, so copying the device copies whatever was
on it before. The obvious record is a trap:

| Record | Spans an appended partition? |
|---|---|
| The filesystem's own volume size | **No** |
| The partition table's alternate header | Yes |

Measured on an image built with an appended partition, the first record stops short by exactly the
size of the boot partition — and those bytes are what make the copy boot. Both are read and the
larger wins, which is right for an image carrying either or both.

A read-back that reads the page cache verifies nothing, and this is the whole reason the verify is
worth having. Everything just written is still in the block device's cache, so re-reading hands
back the bytes this process produced rather than the bytes the flash stored — which is exactly what
a counterfeit stick does and exactly what the verify exists to catch. The cache is dropped first.
Measured against a deliberately corrupting device, a build with the cache drop refuses with both
hashes printed and an identical build without it reports success.

Four refusals stand before a byte is written, and they are wider than the removable-media daemon's
on purpose: that daemon chooses something to mount and this chooses something to destroy. Refused
are the medium this system booted from, any disk with a filesystem mounted anywhere, anything named
in `fstab`, and anything smaller than the image. Each is matched on the parent disk, because the
target of a clone is a whole disk while everything identifying the running system names a
partition.

A counterfeit-device probe runs by default. Such a device reports a capacity it does not have and
wraps silently, so the copy succeeds and the verify fails somewhere in the middle, which reads as a
broken image rather than a bad stick.

## kdos cve

```sh
kdos cve [--json]
```

Offline vulnerability tracking against a vendored security database, covered in
[Packaging](../03-architecture/packaging.md#vulnerability-tracking).

A package the database does not carry is unknown, never clean, and the summary says how many are in
that state.

## kdos thumb

```sh
kdos thumb <file>...
kdos thumb --path <file>           # where its thumbnail would be
kdos thumb --ppm <file> <out.ppm>  # a small P6, for a caller with no image library
```

A small picture of a file, in the cache everything else reads.

The cache is shared and its name is a hash. A thumbnail lives at
`$XDG_CACHE_HOME/thumbnails/normal/<md5>.png`, where the hash is over the file's escaped `file://`
URI, so a file manager, an image viewer and this desktop find each other's work. That is the whole
value of the standard, and it is why the escaper and the hash are `libkbase`'s rather than each
caller's: one character escaped differently is a thumbnail nothing else can find.

`Thumb::URI` and `Thumb::MTime` are required rather than decorative. A reader checks them before
trusting the picture: without the mtime a thumbnail of an edited file is served forever, and
without the URI a hash collision is undetectable. libpng's simplified writer cannot attach a text
chunk, so the cache file goes out through the full one — mode `0600`, because a thumbnail can
reveal the content of a file whose own permissions hide it, and renamed into place, because a
reader sharing the cache must not find half a picture.

It decodes nothing it does not have to. A video is `ffmpegthumbnailer`, a PDF is `pdftoppm`, and
every other still — JPEG, GIF, WebP, a camera raw — is `magick`. Each is a program on this image
that does one thing well.

A helper must hand back a file at the name it was given, which is why `exiv2` is not one: `-ep1`
writes `<file>-preview1.<ext>` with an extension it picks, so a caller can neither name the result
nor read it. `dcraw_emu` is not on the image at all, because `libraw` builds with
`--disable-examples`.

`--ppm` is for a caller with no image library: `kdos-pick`'s preview pane is built without one so
its offscreen build stays dependency-free, and it already parses P6.

## kdos places

```sh
kdos places                  # the column, one per line
kdos places add DIR [NAME]   # keep one
```

The same reader the surfaces use. `kxdg_places()` answers the desktop's Places menu, the Start
menu's column and the chooser's `Ctrl+P`; a command that walked `user-dirs.dirs` itself would be a
fourth answer to where a person's directories are.

`add` is why the command exists. The desktop can keep a folder from its context menu and `mc`
cannot, so `F2` in the file manager has no other way to say *keep this one* — and the whole point
of the column is that it holds the places somebody said rather than the ones a program guessed. The
path is made absolute, because the row is read back by a program standing somewhere else.

## kdos speech

```sh
kdos speech list            # what there is, and which one is here
kdos speech get base.en     # fetch one; base.en is what a bare `get` takes
kdos speech where           # the directory, and the model that would be used
kdos speech remove NAME
```

The transcription model, which the image does not carry. No model ships: the smallest useful one is
32 MB and the one most people want is 148 MB, against an image measured in hundreds, and a speech
model is the most personal choice in the catalogue — language, size, and the trade between the two.
The image carries `whisper-cli`; this carries the way to get a model for it.

It writes to the user's data directory and never to `/usr`. `kdos-rec` searches
`$KDOS_WHISPER_MODEL`, then `$XDG_DATA_HOME/whisper.cpp/models`, then
`/usr/share/whisper.cpp/models`; this writes the second, which needs no privilege and is what a
per-user choice should be. A model in the third is a model somebody packaged.

The download is verified against a SHA256 in the source, and that is the whole of the trust.
Upstream serves these over TLS and signs nothing, so what the checksum buys is that the bytes are
the bytes this tree was written against — a substituted or truncated file is refused. It does not
make the upstream trustworthy; the rule in
[the security model](../03-architecture/security-model.md) on unsigned content still applies. The
magic is checked as well as the sum, because a file that passed one and failed the other is worth
saying out loud rather than leaving for the button to discover.

A name is looked up in the table and never interpolated into a URL. The argument arrives from a
command line and, through `kdos-rec`, from a button, and a name pasted into a URL is a name that
can name another host.

## kdos remind

```sh
kdos remind in 20m tea
kdos remind at 15:30 call back
kdos remind tomorrow 9 stand-up
kdos remind ls | clear
kdos remind fire ID
kdos remind --ask            # the chord's form
```

A toast, later.

The job is a row in the per-user timer table — `~/.config/kdos/timers.d/remind-<id>.timer` — so a
reminder survives a logout and comes back with the session, and `snooze` is what waits. Nothing
here is a scheduler.

It is armed twice and delivered once, and both halves are needed. The per-user table is read once,
at login: nothing watches the directory, so a file written now waits for the next login before
anything looks at it, and `kdos remind in 1m tea` would never fire. So `kdos remind` starts its own
`snooze` as well, and the next login starts a second one from the same file. They cannot both
deliver, because `kdos remind fire` removes the reminder before it returns and the other one then
finds nothing: the file's presence is the reminder, and its absence is the record that it has been
given.

A reminder with nowhere to appear is not spent. With no session and no `gdbus` when the slot comes,
nothing is delivered and the file stays — a machine that happens to have no session at the wrong
minute must not silently eat it.

The text is a comment line and not an argument. Both timer parsers split a row into words with no
quoting and no `eval`, so `-- kdos notify "make tea"` reaches the command as `"make` and `tea"`. A
comment is the one field a parser that skips comments cannot mangle, so it is where the text goes,
and `kdos remind ls` reads it back from there.

`-t` is what makes a missed one fire, not `-s`. A `snooze` that has just started begins looking one
second from now, so a slot already past is the same slot next time round whatever the slack says.
`-t` makes it start looking from a file's modification time instead — for a reminder, the moment it
was asked for — and the slack, a day, then covers the gap. The timefile is the reminder's own file,
so there is nothing else to keep in step.

`in` is capped at ten months, because a `snooze` pattern carries a month and a day and no year:
past twelve months the same pattern is a different reminder. `at` and `tomorrow` roll forward —
`at 09:00` typed at ten in the morning means tomorrow, because a time that has gone is not a time
anybody is asking to be reminded at.

`--ask` is the chord's form, and it exists because a chord runs one command and there is no shell on
that path: `kdos-prompt --input | kdos remind` cannot be a keybinding, so the pipe lives inside the
program that would have been on the right of it. The line a person types is the whole argument —
`in 20m tea` — because a dialog with a field for the time and a field for the text would be two
boxes for one sentence.

With no terminal, `ls` and `clear` answer with a toast. The chords that reach them have nowhere to
print, and a command writing to a standard output nobody is reading is a chord that appears to do
nothing.

## kdos share

```sh
kdos share FILE...
kdos share --clipboard
kdos share                   # asks kdos-pick for a file
```

A file to another machine, over one code word.

`croc` is the transfer and this is the desktop around it. No account, no server of ours and no
protocol written here: the code word is the whole of the pairing and the two ends derive a key from
it. The receiving half is `croc <code>` typed in a terminal, and the sending window says so. There
is no receive surface, because receiving is that one command and a surface around it would only
retype it.

It is `--local` on this image, and that is not a tuning knob. `/usr/bin/croc` is a wrapper that
pins the flag, so a transfer stays on this network and reaches no relay of anybody's. `croc-relay`
is the same binary with upstream's default, for when reaching the public relay is the intent. It is
also why the sending window offers the code word and not the `getcroc.com` link croc prints beside
it: that page is the public relay's, and on this image there is no public relay to reach.

croc is built relay-only, with upstream's `croc_no_tailcat` tag. The Tailscale transport and
`croc ssh`, the shared terminal built on it, run over Tailscale's public DERP servers and take no
`--local`, so neither is in the binary: `croc ssh` answers that it is not supported in this build,
and `croc-relay` has croc's own relay and nothing else.

The transfer stays in front of the person. croc runs in the foreground of a terminal window and
prints its own progress, and this program relays every byte of it untouched rather than
summarising — what a transfer is doing is croc's to say. Reading croc's output and running croc
where somebody can watch it are the same process or they are two, so the window is opened by
`kdos-share` re-entering itself, and it is held open at the end because `kdos-term` has no `--hold`
and a window that closed with the command would take the result with it. Typed at a prompt it runs
where it was typed and waits for nothing.

`--ignore-stdin` is not optional. croc reads standard input for a piped payload, so a `croc send`
whose standard input is anything but a closed file blocks before printing anything at all — no
code, no error, on either stream. It is a global flag and the wrapper's own arguments come first,
so it goes before `send`.

The code word becomes a QR in the window, in full blocks. `qrencode -t ASCIIi`, with `#` replaced by
`█`: `-t UTF8` draws with half blocks, the VT font is 512 glyphs and carries none of them, so that
rendering is not a worse QR on `tty1` — it is no QR. `ASCIIi`'s `#` is a *light* module, so the
substitution puts the light of the code on a dark screen: the printed convention in emitted light
rather than an inverted code. It is drawn whole or not at all, because a QR wider or taller than the
window is a picture no camera can read and half of one is worse than the words. The code goes into
`qrencode` on standard input, because it is the transfer's whole secret and `/proc/<pid>/cmdline`
is readable by every process on the machine for as long as the child lives.

`--clipboard` sends the clipboard as `clipboard.txt`. croc sends files and a clipboard is not one,
so it is written into the runtime directory — 0700 and this person's own — rather than beside
somebody's documents, where it would be a copy nobody knows they have. The name is what arrives at
the other end. It is read once, before the window opens: re-reading it inside would send whatever
had been copied by the time the window appeared.

`kdos-share` is the name, and the verb table is why. libkxdg offers the Share row on the desktop's
icons, the chooser's `Shift+F10` and `mc`'s `F2` only when a program called `kdos-share` is on
`PATH` — a filesystem test, not a dispatch one — so the link on this binary is what turns the row
on, and `kdos share` is the same program under the spelling a person types.

## kdos trash

```sh
kdos trash <file>...
kdos trash --list
kdos trash --restore <name>
kdos trash --rm <name>
kdos trash --empty [-y]
```

The freedesktop trash at `~/.local/share/Trash`, from a prompt.

This, the desktop's `Delete` key and `kdos-trash` are one implementation. Two copies of the
specification would be two answers to what deleting means, and one machine where removing at a
prompt and deleting on the desktop are different operations — one recoverable, one not.

The library keeps the whole of it: the trash directories, the record file, the unique name, restore
and empty. Each front end keeps only the question — the confirmation, and the two pinned places on
the desktop that are not files.

The way back is [`kdos-trash`](kdos-shell.md#kdos-trash), which the Trash icon opens. Its path is a
directory, but a file manager opened on it shows the escaped names in `files/` with no origin and
no deletion date; the record carrying both lives in `info/` beside it, and reading the pair is what
the surface is for.

Five things the shared implementation gets right that a second copy would have to relearn:

- **The record is written before the rename**, because a file in the trash with no record cannot be
  restored by anything, while a stale record with no file is the state every implementation
  ignores.
- **A name that cannot be made unique is a refusal, not an overwrite.** The suffix counter runs to
  `name.999` and then to a clock-derived name; a call that still finds the slot occupied fails with
  `EEXIST`, because renaming over an already-trashed file destroys that file and its only restore
  record in one step and reports success.
- **The recorded path is made absolute**, or trashing by relative name records something nothing can
  put back.
- **The path is escaped both ways**, and an unescape hitting a truncated escape copies it through
  rather than dropping a character.
- **A cross-filesystem move is named out loud.** A rename cannot cross a filesystem, and a
  copy-then-delete here would be a file operation with no undo of its own, so the caller is told
  which filesystem problem it has rather than "failed".

## kdos hey

```sh
kdos hey list [--json]
kdos hey run <action> <id>
kdos hey outputs [--json]
kdos hey boxes [--json]
```

The window manager answers questions from the command line, so a window is something a script can
find and act on. That is Haiku's shape, and it is what the box collector uses to ask whether a box
still has a window before stopping it.

`list` reports each window's identifier, title, geometry, state, workspace, box and instance.
`<action>` is a labwc action name — `Close`, `Focus`, `Iconify`, `ToggleMaximize`, `ToggleShade`,
`ToggleFullscreen`, `Raise` and the rest.

The box column is never truncated. A box is named after its pack, so a fixed narrow column cuts
every name past its width, and a sweep over the catalogue then scores half of it as having no
window.

## kdos oracle

```sh
kdos oracle [--plain]
```

The aphorism picker, over the same reasons `kdos explain` reads. It is keyed on the day combined
with the boot, so the same line can be quoted an hour later and a machine up for a month is not
showing one line for a month.

## kdos update

```sh
kdos update check [--json] [--out PATH]
kdos update apply [--dry-run]
kdos update theme
```

Orchestration of the binary host and the A/B slots. No new trust path: it drives `kpkg` and the
slot state machine, and the exit code is the answer.

| Verb | Does |
|---|---|
| `check` | What the ports tree pins that is not installed. `--json` for a surface, `--out PATH` to write that document atomically |
| `apply` | Take it — binhost first, source second, A/B aware. On an A/B machine it installs into the mounted inactive slot, then puts that slot's kernel in its ESP directory with `kdos-bootctl deploy`, then marks it the candidate; a deploy that fails leaves the slot untried |
| `theme` | Re-run the theme generators for `$HOME` after an art upgrade |

## kdos settings

```sh
kdos settings           # the grid
kdos settings hardware  # straight to a page
```

Execs `kdos-settings`, passing a page word through as `--page`. The page name is not checked here:
`kdos-settings` owns the list, and a second copy would be a second list to keep in step — whose
failure mode is a page that exists and cannot be reached from a prompt. With `kdos-settings` absent
the command exits 127 and says so.

## The other names on this binary

`kdos-tools` dispatches on its own basename, and is installed under every name it answers to. That
is the same technique `kdos-appbox` uses for the application shims, and for the same reason: one
binary, no shell wrapper anywhere in the chain. Invoked under its own name, the first argument
selects the tool, so `kdos-tools service list` works before the symlinks exist.

| Name | Is |
|---|---|
| `kdos` | This page |
| `ksvc` | The service supervisor |
| `service` | The same, under the conventional name |
| `kdos-getty` | Loads the console font and palette, then runs a getty |
| `kdos-bootctl` | `status`, `select`, `mark-good`, `try`, `set-slot`, `crypt` and `deploy` for the A/B slots and each slot's kernel on the ESP, which rewrite the `/KDOS` entries of `limine.conf` — `try` and `mark-good` also arm and clear the UEFI `BootNext` trial — plus `theme` and `palette`, which own its theme lines and `/etc/vtrgb` — also copied into the initramfs |
| `kdos-shot` | Screenshots: `region`, `screen`, `window`, `qr` |
| `kdos-banner` | The login banner |
| `kdos-fetch-app` | Install an alien application from a network |
| `kdos-fetch-static` | Fetch a single verified static binary |
| `kdos-sfx` | The machine's four noises: `login`, `notify`, `error`, `degauss` |
| `kdos-mpctl` | Music player control: `toggle`, `stop`, `next`, `prev` go to mpd over its socket or to an MPRIS player on the session bus, whichever is playing; `now` and `watch` read mpd |
| `kdos-share` | A file to another machine over `croc` — the name the Share verb resolves |

`ksvc` is a C supervisor rather than a shell one because a shell one is not correctable. A respawn
loop in a backgrounded subshell does not lead its own process group, so stopping addresses a group
nothing leads, falls through to a plain signal, and kills the supervisor while orphaning the
daemon — reporting success as it goes. The C supervisor creates its own session, so the group
signal reaches both. It also refuses a service name that is not a plain name, since a name
interpolated into a glob is a name that can match anything.

`ksvc supervise [--final-exit CODE]... <name> <command>...` restarts the daemon five seconds after
any exit, except an exit with a status named by `--final-exit`. That status is one restarting
cannot change — thermald on an Intel model it does not know, smartd with no disk to watch, mdadm with
no array — so the supervisor says so once, removes its pid file and exits. A death by signal is
always restarted. A daemon that finds out only after probing the hardware is otherwise restarted
every five seconds for as long as the machine is up.

`kdos-fetch-app` passes the application name as a positional argument, never interpolated into a
command string. A nested shell invocation would let the outer shell expand the name before the
inner one parses it, so a name containing a quote breaks out and runs as the box's root. The in-box
package-manager fallback is still shell, because that is what it is — but the name arrives as `$1`.

`kdos-mpctl` is what the transport keys run, and one player answers each key. mpd and every MPRIS
player are ranked by state — playing, then paused, then anything else — and the highest takes the
key, mpd on a tie. So a playing mpd always takes it, a paused mpd beats a paused MPRIS player, and
a stopped mpd yields to any MPRIS player that is `Playing` or `Paused` but keeps the key over a
stopped one; among MPRIS players the first at the best rank wins. `toggle` sends a stopped mpd
`play`, because mpd's bare `pause` does nothing while it is stopped. MPRIS reaches `mpv` (through the `mpv-mpris` plugin in
`/etc/mpv/scripts`), `cmus` and every boxed player, since a box shares the session bus. A player that
does not answer within two seconds costs that key and nothing else. `libbasu` is opened at run time
rather than linked, because this binary is also `ksvc`, `kdos-getty` and the `kdos-bootctl` the
initramfs copies with a hand-kept library list. With no mpd and no player on the bus the verb says
`no player on this login` and exits 1. `now` and `watch` read mpd alone: the panel reads MPRIS
itself.

## See also

- [Administration](../02-user-guide/administration.md) — these commands in the jobs they belong to
- [Command index](../06-reference/command-index.md) — every command on the system
- [kdos-comp](kdos-comp.md) — the sockets `hey` and `stutter` read
- [The daemons](daemons.md) — what `doctor` checks and `restarts` reports on
- [Theming](../02-user-guide/theming.md) — `kdos theme` in full
