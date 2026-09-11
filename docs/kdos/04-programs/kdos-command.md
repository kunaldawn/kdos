# The kdos command

`kdos` is the front door: one subcommand for each thing this distribution can answer that a
general-purpose system cannot. This page documents each one — what it does, what it measures, and
what it refuses to claim. No count is given here on purpose; a number in a sentence is a number
that goes wrong the next time a verb lands.

The dispatch table in the source is the authoritative list; this page follows it.

## help

```sh
kdos help
```

`kdos help` opens with a **`WHERE THINGS LIVE`** block naming the three lanes — `kpkg` for the
host, `kdos app` for applications, `kdos-box` for environments — and then lists the commands and
the keybinding cheat sheet.

That opening block is the point. One list showing the whole system's verbs is the only place a
reader learns that those are **three different questions**, rather than three tools that overlap.

## theme

```sh
kdos theme <phosphor|amber|ice|bone|norton|borland|perfect>
kdos theme list | next | prev
kdos theme style <file>
kdos theme --audit [accent]
kdos theme --preview <accent>
kdos background [<name>|list|next|prev|none]
```

Regenerates every themed artefact and commits the change in one ordered operation. Covered in
[Theming](../02-user-guide/theming.md).

**`--preview` is the state file and the signal and nothing else** — no GTK stylesheet, no icon
theme, no cursors, none of the eight foreign configuration files. Those take seconds and are read
by programs that are not running, so a preview repaints every KDOS surface at once and leaves a
boxed application wearing the old accent. It is what `kdos-theme`'s arrow keys run, and it is why
that picker restores the accent it opened on unless it is told to keep one.

**The commit order is load-bearing.** The wallpaper cache and the accent state file are both
*inputs* to the signal that repaints the session, so both are written before it is sent — the state
file because a signal arriving first makes the desktop re-read the accent it already had, and the
wallpaper because the compositor re-decodes on that same signal.

**The signal goes to every long-lived surface by name, and an exact match is required.** The panel,
the desktop and the notification daemon are three names of one binary, so signalling only one
retints the panel and leaves the desktop icons and any live toast in the old accent; both halves of
the console desktop are on the list because `kdos-con` holds the cells and `kdos-view` holds the
palette they are painted with. And the match must be exact: one of those names is a **substring**
of the two shell scripts that own the session, and an unhandled signal kills a shell.

**`--audit` is the palette claim, checked.** It does not try to recognise "palette colours" in the
installed files — that test would have to know which mixes are legal and would drift from the
generators. It runs **the same generators** with the home and cache directories pointed at a
scratch directory and compares byte for byte, symlinks included. Anything that differs, differs
from what this machine's palette produces right now.

It writes nothing outside its scratch directory and signals nothing: an audit that repaired what it
found would be a `kdos theme` with a misleading name. Exit 0 clean, 1 on drift, 2 if it could not
run.

## notify

`kdos notify <summary> [body]` raises a toast, through `kb_notify()` — the same call a terminal
makes for a child's OSC 9, so the two cannot drift apart.

**`--time` and `--battery` compute their own text, and that is why they are verbs.** A chord runs a
static command: `rc.xml` binds a string and `con.conf` names one, so a chord that wanted the time
could not be a chord that formatted it. `Super+Ctrl+Alt+t` and `Super+Ctrl+Alt+b` are the two
questions a bar answers by being on the screen — and this desktop puts its bar away.

**`--dismiss`, `--dismiss-all`, `--raise` and `--dnd` are one line down the daemon's own socket.**
`kdos-notifyd` already holds the toasts, the history and the Do Not Disturb flag and already answers
a socket in `$XDG_RUNTIME_DIR`; a chord needs a **command**, and this is it. There is no second
daemon and no second owner of what is on the screen. It is silent when nothing is listening — a
chord pressed on a machine with no notification daemon should do nothing, not print an error into a
session with nowhere to show it.

**`--raise` takes the entry OUT of the history**, rather than copying it: a notification is on the
screen or it is in the centre and never both, or dismissing it twice would file two copies of one
thing. **And it comes back without its buttons.** The notification it came from is closed and its
actions belong to the program that sent it — a button pressed here would fire a verb nothing is
waiting for.

**The charge is read from the kernel and not from `kdos-energyd`.** That daemon estimates what a
program is *costing*; it holds no battery state at all and its socket answers `ping`, `report` and
`report-json` about nothing else. `libkproc` reads `/sys` for the resource monitor, and one reader
means one answer. The body names wear as well as charge: a battery reporting 90% can hold 70% of
what it held new, and a person deciding whether to unplug wants both.

## toggle

```sh
kdos toggle                  # list them and their state
kdos toggle <name>           # flip it
kdos toggle <name> on|off    # set it
```

The switches a desktop needs at hand, as **flag files** under `~/.local/state/kdos/toggles/` where
a file's presence means on. What exists and who reads each one is in
[configuration](../06-reference/configuration.md#localstatekdostoggles); an unknown name is
refused with the list rather than written.

**A toggle whose consumer reads it on the retint signal sends that signal** — the same one `kdos
theme` sends — after writing the file and never before: the surfaces re-read their state the
moment it lands, so a signal sent first is one they answer with the state it replaced.
`night-light` is that toggle. The other two are stat'd on a tick their reader already runs and
need nothing.

**A chord has nowhere to print, so it gets a toast.** Typed at a prompt the state goes to stdout;
spawned by a keystroke it is a one-line notification naming the switch and where it now stands. Two
of the three change nothing that is visible, and a switch flipped by a keystroke and answered by
nothing is a keystroke a person cannot tell from a broken one. **`Super+Ctrl+i` is `stay-awake` and
`Super+Ctrl+Shift+n` is `night-light`** — not `Super+Ctrl+n`, which is the scratch pad's on both
desktops and has been since either had accessories.

## menu

```sh
kdos menu summon <route>     # open the menu on a named place
kdos menu toggle [<route>]   # close it if it is open, else open it
```

**A route is a name for a place in the system**, from `/etc/kdos/menu.conf` and the user's copy of
it — see [configuration](../06-reference/configuration.md#etckdosmenuconf). This is what a script
holds instead of a chord: a chord is rebindable and a menu row moves, and neither is something
another program can refer to. Summoning opens the menu with the route in its search field, which is
the one code path that finds a route.

**The menu is whatever `con.conf` names.** Which key opens a thing is `keys.conf`'s and which
program is the thing is `con.conf`'s; a command that hardcoded `kdos-start` would be a third answer
to that question. The key may carry arguments and is split into an argument vector here, as the
session splits it — the first word is the program, which is what `pkill -x` matches, because that
match is against a name and never against a command line.

**Toggle closes by signal and opens by spawn, in that order.** `pkill` reports whether it signalled
anything, so one call answers "was it open" and closes it — no pidfile, no round trip. The match is
exact, for the same reason the retint signal's is.

## status

What this machine is and what it is running: the release, the kernel, the packages, the session,
the boxes.

## doctor

Checks the things that have **actually broken on this distribution**, rather than a generic health
sweep.

**It has a third report level: `skip`, with a reason.** Half of what it asks cannot be answered in
a virtual machine — no energy counter, no wireless, no discrete graphics, no boot medium — and
reporting those as ok would be a green line for something never tested, while a warning would make
every virtual machine look broken.

Sections and their most valuable checks:

| Section | Checks include |
|---|---|
| Boot | The root-switch trap: a namespace root that is the emptied initramfs; the microcode present in the boot image and the revision the processor is running |
| setuid | The password checker, the resource helper, and **both user-namespace mapping helpers** — losing any is silent and catastrophic |
| Hardware | **Device present but unopenable**: it walks the attached devices and reports each one the calling user cannot open, **naming the owning group** |
| Boxes | Whether the pack filesystem is loadable, whether the pack daemon answers and by which mount route, whether the home directory's filesystem can host a container layer, and whether every mounted pack still has a file behind it |
| Desktop | Whether the frame-reporting socket exists, and **the checks that belong to the session that is running**: a graphical session is asked about `kdos-comp`, its panel and the wlroots portal; a console session about `kdos-con` and whether a view is attached. `$KDOS_CON` decides, and reporting a missing compositor on a cell desktop would fail a working machine |

"Add yourself to this group" is an instruction; "permission denied" is not. That difference is why
the hardware check names the group.

The box check reports, on a live session, that the home directory is on an overlay **so a
persistent box cannot exist** — which is a real rule stated to the person it affects rather than a
failure.

`kdos version` names the session from the same test, so it reports the desktop a person is sitting
at rather than the one the image happens to ship.

## app

```sh
kdos app list | search | show | install | launch | remove | rollback | update | sources
kdos app tui add <name> <command> [--float] [--size COLSxROWS] [--icon N] [--category X]
kdos app tui rm <slug> | ls
```

The application front end, covered in [Applications](../02-user-guide/applications.md).

**`tui` makes a terminal program an application**, without anybody editing a file by hand:
`~/.local/share/applications/kdos-tui-<slug>.desktop` with `Terminal=true`, the two KDOS keys that
say how the window should open, and an `Exec` **quoted a field at a time** by the library that reads
one — never concatenated, so a quoted path with a space in it survives the round trip through a
file two other programs parse.

**`X-KDOS-TUI=true` is what `rm` checks**, and it is the whole safety of the verb. A slug is a
person's word and the same word can name an entry the image shipped, so without the marker this
would be a way to delete somebody else's application. **The slug is prefixed** for the other half of
that: a file in this directory *shadows* the one in `/usr/share`, so a name that happened to collide
would take a shipped entry off the menu rather than adding a row beside it.

**The command is an argument vector, not an `Exec` line.** Every `%` in it is a literal and is
written doubled, because a single one begins a field code in the file. An entry added this way opens
its program; it is not a file handler, and one that needs to be is a recipe's entry rather than
this.

**It answers before the pack daemon is asked.** `tui` writes a file in this person's own data
directory and needs `kdos-packd` for nothing — a verb refused because the daemon is down, or because
the caller is not in `wheel`, would be a verb refused for a reason that has nothing to do with
adding a menu row for `ncdu`.

**There is deliberately no application store.** On a distribution whose medium *is* the software
library, "where do I get this" is not a question anyone has; what remains is disposal, and that
belongs where the readings already are.

**It never names a path to the daemon.** Every verb hands an identifier out of the list the daemon
published; the one exception is installing a **file**, which copies into the daemon's own staging
directory and then names the filename there.

## version

The release, the commit it was built from, and whether that tree was clean.

## why / explain

Why something on this machine is the way it is — the reasoning behind a configuration or a
behaviour, from a shipped set of explanations.

## sandbox

What a box is allowed to do, and what the sandbox filter would refuse it.

## appid

**Does the launcher's file identifier match the identifier a real window presented?**

A dock matches a running window to its launcher by the entry's file identifier, so a mismatch shows
a second generic icon beside the pinned one. This checks the left-hand side against the right.

The right-hand side is a **ledger the compositor appends to** the first time each window maps,
falling back to the live window list when no ledger has been recorded yet. **The two answer
different questions and the report says which it used**: the ledger is every window this machine
has ever shown, the live list is what is on screen now.

## restarts

Which supervised services have been restarting, and how often. A crash loop is visible here before
it is visible anywhere else.

## stutter

**Why did that jerk?** Three sources, none of which is an answer alone, joined:

| Knows | Does not know |
|---|---|
| A frame was late, by how much, and what the compositor's own render cost | Who did it |
| The machine was starved, and of what | By whom |
| Who burned processor time and who sat blocked on I/O | That anyone cared |

The output is a sentence like:

> 7 frames dropped on eDP-1 (133 ms) — the busiest just then: an indexer waiting on the disk, and
> an application at 92% of a core.

The closest prior art reads pressure statistics and says outright that it cannot identify which
process caused a frame miss. That sentence is what this finishes.

**The render cost is what separates the two explanations.** Over a large fraction of the frame
budget and it says *the desktop itself was late* — the one causal claim it makes, because there it
has both halves. Otherwise it reports what it measured and names who was busy, and it **never says
"X caused this"**: attribution from a half-second sample window is circumstantial, and a tool that
claimed otherwise would be wrong the first time two things were busy at once.

Two details that are not obvious:

- **Blocked before busy.** A process asleep in uninterruptible I/O shows almost no processor time
  while it is the thing holding the disk, so sorting on processor time alone hides exactly the case
  the I/O half exists for.
- **Container names come from the supervisor's command line, not from control groups.** With no
  cgroup delegation a rootless container frequently sits in the root group, which says nothing.
  Walking the parent chain to the supervising process and reading its name costs a few file reads
  and no engine call — which matters, because this runs while the machine is already struggling.

`--fixture` points the whole sampler at a recorded system state, which is what makes an attribution
engine testable at all.

## march

```sh
kdos march probe          # which instruction set levels this processor has
kdos march run <port>     # build it twice and measure
kdos march report         # the ledger
```

```
$ kdos march run lz4
lz4  x86-64-v3  baseline 0.321s  -march=x86-64-v3 0.302s  +5.9% (noise 16.7%) -> reverted
       the win is inside the noise; that is not a win
```

Builds the port twice on **this** machine, runs the port's own benchmark against both, and keeps
the flags only where the win clears **both** a fixed floor and the machine's own measured noise.

Four rules:

- **A port with no declared benchmark is unmeasurable, never a winner.** Most ports have no
  meaningful benchmark, and assuming a win for them is the blind optimisation this replaces.
- **The median of several runs**, not the mean — which the worst outlier owns — and not one run,
  which measures the scheduler.
- **The noise floor is measured, not assumed.** It is the spread of the samples themselves, and a
  "win" smaller than it is the machine breathing.
- **The benchmark's setup runs once and is not timed.** A fixture belongs outside the stopwatch.

**`report` is the ledger**: kept, reverted, unmeasurable, with the summary line. A report listing
only winners would be a sales pitch; the reverts are the evidence that the measuring is real.

The argument for measuring rather than choosing a tier is in
[Decisions](../01-philosophy/decisions.md).

## rebuild

```sh
make build KDOS_ISO_SOURCES=1     # a developer medium
# ...boot it...
kdos rebuild /mnt/disk/work       # no network at any point
```

**The medium rebuilds the medium.** Every leg of this is old — live systems have shipped their
sources for decades — and what none of them does is rebuild *the medium from the medium*. KDOS can
because of three properties it already has: the repository builds offline, KDOS can build KDOS, and
packages are reproducible, so a rebuild can be **compared** to what it was built from rather than
merely produced.

The sources go on the medium's outer filesystem **beside** the system image, not inside it, so they
cost the installed system nothing and are readable the moment the live image is up. Opt-in, because
they are gigabytes of already-compressed archives and squashing them again buys nothing.

**It is checks plus the ordinary build, and the checks are the valuable half.** A live medium's
root is an overlay whose upper layer is RAM: a rebuild started there reports gigabytes free, eats
memory, and dies hours in with the machine unusable. So the work directory is **refused** when it
is on a temporary or overlay filesystem — a free-space check cannot see that — and again when it
has too little space or a build tool is missing.

Everything after the checks is the same orchestrator the normal build runs, compiled on demand out
of the tree being built, so the build is driven by the sources on the machine rather than by a
binary from somewhere else.

## clone

```sh
$ kdos clone
source  /dev/sda  9.5G

  DEVICE            SIZE  MODEL
  /dev/sdb          32G   Ultra Fit
  /dev/sdc         8.0G   DataTraveler   (too small)

$ sudo kdos clone /dev/sdb
```

**The medium writes the medium** — the operation somebody standing in front of two sticks actually
wants. It is a **raw copy and deliberately nothing cleverer**: the boot arrangement is whatever the
medium already carries, so a copy boots exactly what the original boots and there is no second
opinion about how a KDOS stick is laid out.

**The image's length comes from the image, and two records describe it.** A small image written to
a large stick leaves the device reporting the large size, so copying the *device* copies whatever
was on it before. The obvious record is a trap:

| Record | Spans an appended partition? |
|---|---|
| The filesystem's own volume size | **No** |
| The partition table's alternate header | Yes |

Measured on an image built with an appended partition, the first record stopped short by exactly
the size of the boot partition — and those bytes are what make the copy boot. **Both are read and
the larger wins**, which is right for an image carrying either or both.

**A read-back that reads the page cache verifies nothing**, and this is the whole reason the verify
is worth having. Everything just written is still in the block device's cache, so re-reading hands
back the bytes *this process produced* rather than the bytes the flash stored — which is exactly
what a counterfeit stick does and exactly what the verify exists to catch. The cache is dropped
first. Confirmed to matter by building without it: against a deliberately corrupting device, the
cache-dropping build refuses with both hashes printed and the identical build without it **reports
success**.

**Four refusals, before a byte is written**, and they are wider than the removable-media daemon's
on purpose — that daemon chooses something to *mount* and this chooses something to *destroy*: the
medium this system booted from, any disk with a filesystem mounted anywhere, anything named in
`fstab`, and anything smaller than the image. Each is matched on the **parent disk**, because the
target of a clone is a whole disk while everything identifying the running system names a
partition.

A counterfeit-device probe runs by default: such a device reports a capacity it does not have and
wraps silently, so the copy succeeds and the verify fails somewhere in the middle — which reads as
a broken image rather than a bad stick.

| Flag | Does |
|---|---|
| `--yes` | Skip confirmation, which is otherwise typing the device **name** |
| `--no-probe` | Skip the counterfeit probe, saying what was given up |
| `--no-verify` | Skip the read-back, saying what was given up |
| `--extent` | Print the image's exact byte count and stop |
| `--source` | Name the source explicitly |

## cve

Offline vulnerability tracking against a vendored security database. Covered in
[Packaging](../03-architecture/packaging.md#vulnerability-tracking).

**A package the database does not carry is unknown, never clean**, and the summary says how many
are in that state.

## thumb

A small picture of a file, in the cache everything else reads.

**The cache is shared and its name is a hash.** A thumbnail lives at
`$XDG_CACHE_HOME/thumbnails/normal/<md5>.png`, where the hash is over the file's escaped `file://`
URI — so a file manager, an image viewer and this desktop find each other's work. That is the whole
value of the standard, and it is why the escaper and the hash are `libkbase`'s rather than each
caller's: **one character escaped differently is a thumbnail nothing else can find.**

**`Thumb::URI` and `Thumb::MTime` are required, not decoration.** A reader checks them before
trusting the picture: without the mtime a thumbnail of an edited file is served forever, and without
the URI a hash collision is undetectable. libpng's simplified writer cannot attach a text chunk, so
the cache file goes out through the full one — mode `0600`, because a thumbnail can reveal the
content of a file whose own permissions hide it, and renamed into place, because a reader sharing
the cache must not find half a picture.

**It decodes nothing it does not have to.** A video is `ffmpegthumbnailer`, a PDF is `pdftoppm`, and
every other still — JPEG, GIF, WebP, a camera raw — is `magick`. Each is a program on this image
that does one thing well.

**A helper must hand back a file at the name it was given**, which is why `exiv2` is not one: `-ep1`
writes `<file>-preview1.<ext>` with an extension it picks, so a caller can neither name the result
nor read it. `dcraw_emu` is not on the image at all — `libraw` builds with `--disable-examples`.

`--ppm FILE OUT` writes a small P6 instead of touching the cache. That is for a caller with no image
library: `kdos-pick`'s preview pane is built without one so its offscreen build stays
dependency-free, and it already parses P6.

## places

The places column, from a prompt — and the way to keep one.

**The same reader the surfaces use.** `kxdg_places()` answers the desktop's Places menu, the Start
menu's column and the chooser's `Ctrl+P`; a command that walked `user-dirs.dirs` itself would be a
fourth answer to where a person's directories are.

`kdos places add DIR` is why it exists: the desktop can keep a folder from its context menu and
`mc` could not, so `F2` in the file manager had no way to say *keep this one* — and the whole point
of the column is that it holds the places somebody said rather than the ones a program guessed. The
path is made absolute, because the row is read back by a program standing somewhere else.

## remind

A toast, later. `kdos remind in 20m tea`, `kdos remind at 15:30 call back`,
`kdos remind tomorrow 9 stand-up`, then `kdos remind ls` and `kdos remind clear`.

**The job is a row in J.13's per-user table** — `~/.config/kdos/timers.d/remind-<id>.timer` — so a
reminder survives a logout and comes back with the session, and `snooze` is what waits. Nothing
here is a scheduler.

**It is armed twice and delivered once, and both halves are needed.** The per-user table is read
**once, at login**: nothing watches the directory, so a file written now would wait for the next
login before anything looked at it, and `kdos remind in 1m tea` would never fire. So `kdos remind`
starts its own `snooze` as well. The next login starts a second one from the same file. They cannot
both deliver, because **`kdos remind fire` removes the reminder before it returns** and the other
one then finds nothing: the file's presence *is* the reminder, and its absence is the record that
it has been given.

**A reminder with nowhere to appear is not spent.** If there is no session and no `gdbus` when the
slot comes, nothing is delivered and the file stays — a machine that happened to have no session at
the wrong minute must not silently eat it.

**The text is a comment line and not an argument.** Both timer parsers split a row into words with
no quoting and no `eval`, so `-- kdos notify "make tea"` reaches the command as `"make` and `tea"`.
A comment is the one field a parser that skips comments cannot mangle, so it is where the text
goes — and `kdos remind ls` reads it back from there.

**`-t` is what makes a missed one fire, not `-s`.** A `snooze` that has just started begins looking
one second from now, so a slot already past is the same slot next time round whatever the slack
says. `-t` makes it start looking from a file's modification time instead — for a reminder that is
the moment it was asked for — and the slack, a day, then covers the gap. The timefile is the
reminder's own file, so there is nothing else to keep in step.

**`in` is capped at ten months** because a `snooze` pattern carries a month and a day and no year:
past twelve months the same pattern is a different reminder. `at` and `tomorrow` roll forward — `at
09:00` typed at ten in the morning means tomorrow, because a time that has gone is not a time
anybody is asking to be reminded at.

**`kdos remind --ask` is the chord's form**, and it exists because a chord runs one command and
there is no shell on that path: `kdos-prompt --input | kdos remind` cannot be a keybinding, so the
pipe lives inside the program that would have been on the right of it. The line a person types is
the whole argument — `in 20m tea` — because a dialog with a field for the time and a field for the
text would be two boxes for one sentence.

**With no terminal, `ls` and `clear` answer with a toast.** The chords that reach them have nowhere
to print, and a command that wrote to a stdout nobody is reading is a chord that appears to do
nothing.

## share

A file to another machine, over one code word. `kdos share FILE…`, `kdos share --clipboard`, or
`kdos share` with nothing named — which asks `kdos-pick` for a file.

**`croc` is the transfer and this is the desktop around it.** No account, no server of ours and no
protocol written here: the code word is the whole of the pairing and the two ends derive a key from
it. The receiving half is `croc <code>` typed in a terminal, and the sending window says so — there
is no receive surface, because receiving is that one command and a surface around it would be a
surface that only retypes it.

**IT IS `--local` ON THIS IMAGE, AND THAT IS NOT A TUNING KNOB.** `/usr/bin/croc` is a wrapper that
pins the flag, so a transfer stays on this network and reaches no relay of anybody's. `croc-relay`
is the same binary with upstream's default for when reaching the public relay is the intent. It is
also why the sending window offers the code word and not the `getcroc.com` link croc prints beside
it: that page is the public relay's, and on this image there is no public relay to reach.

**The transfer stays in front of the person.** croc runs in the foreground of a terminal window and
prints its own progress, and this program relays every byte of it untouched rather than
summarising — what a transfer is doing is croc's to say. Reading croc's output and running croc
where somebody can watch it are the same process or they are two, so the window is opened by
`kdos-share` re-entering itself, and it is held open at the end because `kdos-term` has no
`--hold` and a window that closed with the command would take the result with it. Typed at a prompt
it runs where it was typed and waits for nothing.

**`--ignore-stdin` is not optional.** croc reads stdin for a piped payload, so a `croc send` whose
stdin is anything but a closed file blocks before printing anything at all — no code, no error, on
either stream. It is a global flag and the wrapper's own arguments come first, so it goes before
`send`.

**The code word becomes a QR in the window, in full blocks.** `qrencode -t ASCIIi`, with `#`
replaced by `█`: `-t UTF8` draws with half blocks and the console font is 512 glyphs and carries
none of them, so that rendering is not a worse QR on `tty1`, it is no QR. `ASCIIi`'s `#` is a
*light* module, so the substitution puts the light of the code on a dark screen — the printed
convention in emitted light rather than an inverted code. **Drawn whole or not at all**: a QR
wider or taller than the window is a picture no camera can read, and half of one is worse than the
words. The code goes into `qrencode` on **stdin**, because it is the transfer's whole secret and
`/proc/<pid>/cmdline` is readable by every process on the machine for as long as the child lives.

**`--clipboard` sends the clipboard as `clipboard.txt`.** croc sends files and a clipboard is not
one, so it is written into the runtime directory — 0700 and this person's own — rather than beside
somebody's documents, where it would be a copy nobody knows they have. The name is what arrives at
the other end. It is read **once, before the window opens**: re-reading it inside would send
whatever had been copied by the time the window appeared.

**`kdos-share` is the name, and the verb table is why.** libkxdg offers the Share row on the
desktop's icons, the chooser's `Shift+F10` and `mc`'s `F2` only when a program called `kdos-share`
is on `PATH` — a filesystem test, not a dispatch one — so the link on this binary is what turns the
row on, and `kdos share` is the same program under the spelling a person types.

## trash

The freedesktop trash, from a prompt.

**This, the desktop's `Delete` key and `kdos-trash` are one implementation.** The desktop has had a
trash icon since it had icons and the command line had no verb for it, so removing at a prompt and
deleting on the desktop were two different operations on one machine — one recoverable, one not.
Two copies of the specification would be two answers to what deleting means.

The library keeps the whole of it — the trash directories, the record file, the unique name,
restore and empty — and each front end keeps only the **question**: the confirmation, and the two
pinned places on the desktop that are not files.

**The way back is [`kdos-trash`](kdos-shell.md#kdos-trash)**, which the Trash icon opens. Its path
is a directory, but a file manager opened on it shows the escaped names in `files/` with no origin
and no deletion date — the record carrying both lives in `info/` beside it, and reading the pair is
what the surface is for.

Four things the shared implementation gets right that a second copy would have to re-learn:

- **The record is written before the rename**, because a file in the trash with no record cannot be
  restored by anything, while a stale record with no file is the state every implementation
  ignores.
- **The recorded path is made absolute**, or trashing by relative name records something nothing
  can put back.
- **The path is escaped both ways**, and an unescape hitting a truncated escape copies it through
  rather than dropping a character.
- **A cross-filesystem move is named out loud.** A rename cannot cross a filesystem, and a
  copy-then-delete here would be a file operation with no undo of its own — so the caller is told
  which filesystem problem it has rather than "failed".

## hey

```sh
kdos hey list
kdos hey outputs
kdos hey boxes
```

The window manager answers questions from the command line, so **a window is something a script
can find and act on**. That is Haiku's shape, and it is what the box collector uses to ask whether
a box still has a window before stopping it.

`list` reports each window's identifier, title, geometry, state, workspace, **box** and instance.

**The box column is never truncated.** A box is named after its pack, so a fixed narrow column cut
every name past its width — and a sweep over the catalogue scored half of it as having no window.

## oracle

The aphorism picker, keyed on the day combined with the boot, so the same line can be quoted an
hour later and a machine up for a month is not showing one line for a month.

## update

Orchestration of the binary host and the A/B slots. **No new trust path** — it drives `kpkg` and
the slot state machine, and the exit code is the answer.

## settings

```sh
kdos settings           # the grid
kdos settings hardware  # straight to a page
```

Execs `kdos-settings`, passing a page word through as `--page`. **The page name is not checked
here**: `kdos-settings` owns the list, and a second copy would be a second list to keep in step —
whose failure is a page that exists and cannot be reached from a prompt.

## con

```sh
kdos con ls
kdos con {new|attach|detach|kill} [session]
kdos con attach --observe [session]
kdos con capture [--window N] [session]
kdos con record FILE
kdos con replay FILE
kdos con forward <host> [session]
kdos con layout {save|load} <name>
kdos con run [--] CMD [ARG...]
```

**The session is a bare name, not a flag.** `kdos con new work` — not `-t work`, which names a
session `-t`. This front end execs `kdos-con` and supplies the `-t` itself; it is five verbs and a
name, deliberately not an argument tunnel. `capture` sits beside that table rather than in it,
because it is the one verb with a flag of its own to pass.

The console desktop's sessions — the verb that reaches the **default** session, since `tty1` runs
`kdos-con-login` and everything else is started from there.

| Verb | Does |
|---|---|
| `ls` | The sessions that exist, by name |
| `new [session]` | Start one. **It holds the session and does not return** — nothing is displayed until a view attaches |
| `attach [--observe] [session]` | Put a display on one. `--observe` watches without typing: the session drops that view's keys and pointer |
| `detach [session]` | Take every display off one, leaving it and its windows running |
| `kill [session]` | Ask one to end. It stops its listeners and drains its clients |
| `capture [--window N] [session]` | Print what is on a **running** session's screen as text; `--window N` narrows it to one window by its ring number |
| `record FILE` | Draw the session in this terminal and write everything it sends to `FILE` — KDOS's own format, not an asciicast |
| `replay FILE` | Draw a recording in this terminal. It attaches to no session |
| `forward` | Carry a session's view socket to another machine over `ssh` |
| `layout save <name>` | Write what the **running** session has open to `~/.config/kdos-con/layouts/<name>` |
| `layout load <name>` | Open every entry of that layout that is not already open. It closes nothing |

**A layout is the session record with a name.** Same rows, same columns, same reader as the file a
session leaves behind on a clean exit — so what a person arranged is what comes back, and there is
one format rather than two to keep in step. `save` and `load` both reach the session that is
running, for the reason a capture does: it is the half that holds the windows, and nothing on the
wire can move one.

**A row names what to open and never how.** `term` is `con.conf`'s `terminal`; a **role** — `files`,
`mail`, `writing` — is the `con.conf` key for that role, opened in a terminal; a `con.conf` command
key such as `monitor` or `notes` is one of this desktop's own surfaces; anything else is an app id
for the pack store. A file that named a command line would be a file that executes one, and it is
written by a program into a directory anything running as this person can write.

**Which is why a terminal's row names a role.** Every terminal window's app id is the literal
`terminal`, so a row carrying that says a window *was* a terminal and not which program was in it —
saved and reloaded, an arrangement would come back as a screen of bare shells. A window running the
file manager is written as `files`, and `con.conf` says what fills that.

**A load adds; it never takes away.** A row whose program this image does not carry opens nothing
and is not an error — no image carries all seven roles, and a layout that refused to load at all
would be one nobody could use. A row already open opens nothing either, so a layout route is safe
to press twice. A `term` row always opens: there is no name that separates one plain shell from
another, so two terminals in a layout mean two terminals.

**Three ship** under `/usr/share/kdos/layouts/`, and a person's own copy of a name replaces it
rather than merging with it: `work` is a terminal with the file manager and the monitor beside it,
`write` the editor full-screen with the note pad as the scratchpad, `talk` mail, the diary and chat
side by side. Each has a `layout.<name>` route in the palette. Routes are read from `menu.conf`
rather than found in a directory, so a layout somebody saves of their own gets its row in their own
copy of that file.

**A capture asks the session that is running; it is not `kdos-con --dump`.** The dump composites a
session of its own and **settles** it — runs every terminal until its child has exited — which is
right for one-shot commands and wrong for a live session, whose shell never exits: a capture that
settled would hold the session for the length of its own spin and answer nothing. It pumps once
instead, so what comes back is what the children have already written.

**Only a shell surface may ask**, like every other management verb. Reading back a whole session is
not something a program with a window in it, or a display that was handed cells, has any business
doing — and a window number naming nothing returns nothing rather than widening silently to the
whole screen.

**A session and a display are separate processes, and that is the whole design.** The session holds
every window and draws nothing; the view holds a screen and no window state. So a view that crashes
loses nothing, a detach leaves the work running, and a display at the far end of an `ssh` connection
is trusted with nothing but the cells it is sent.

**Two sockets, and only one may leave the machine.**

| Socket | Admits | May be forwarded |
|---|---|---|
| `<name>.sock` | Surfaces — programs that place windows | **never** |
| `<name>.view` | Views — a display | yes, with `forward` |

Which socket a client reached decides what it is allowed to be; the kind in its handshake is a claim
and is overridden. Forwarding a socket that admitted surfaces would hand the far end the right to
place windows in your session, which is a different thing entirely from showing you yours.

**`remote = no` is enforced where the tunnel is built, not where a connection arrives.** `kdos con
forward` refuses. It cannot be enforced at the far end: a forwarded socket's peer is the local `ssh`
process running as the same user, so it is indistinguishable from a local view by credentials. A
check at the accepting end would be a check that cannot tell the two apart, which is worse than
none because it reads as protection.

**`kill` asks; it does not unlink.** Removing the socket files would leave the session running on
listeners it still holds — every attached view keeps its display and the session is unreachable and
alive. There is no pid in a socket path either, so looking one up by name would end whichever
process happened to match.

## The other names on this binary

The same binary answers to several other names, dispatched on its own name:

| Name | Is |
|---|---|
| `ksvc` | The service supervisor |
| `service` | The same, under the conventional name |
| `kdos-getty` | Loads the console font and palette, then runs a getty |
| `kdos-bootctl` | `select`, `mark-good`, `status` for the A/B slots — also copied into the initramfs |
| `kdos-banner` | The login banner |
| `kdos-shot` | Screenshots |
| `kdos-share` | A file to another machine, over `croc` — what the Share verb resolves |
| `kdos-sfx` | Sound effects |
| `kdos-fetch-app`, `kdos-fetch-static` | Fetch helpers |

**`ksvc` exists because a shell supervisor was not correctable.** A respawn loop in a backgrounded
subshell does not lead its own process group, so stopping addressed a group nothing led, fell
through to a plain signal, and **killed the supervisor while orphaning the daemon** — while
reporting success. The C supervisor creates its own session, so the group signal reaches both. It
also **refuses a service name that is not a plain name**, since the old one interpolated its
argument straight into a glob.

**`kdos-fetch-app` passes the application name as a positional argument, never interpolated into
a command string.** A nested shell invocation would let the outer shell expand the name before the
inner one parses it, so a name containing a quote breaks out and runs as the box's root. The
in-box package-manager fallback is still shell, because that is what it is — but the name arrives
as `$1`.

## See also

- [Administration](../02-user-guide/administration.md) — these commands in the jobs they belong to
- [Command index](../06-reference/command-index.md) — every command on the system
- [kdos-comp](kdos-comp.md) — the sockets `hey` and `stutter` read
- [The daemons](daemons.md) — what `doctor` checks and `restarts` reports on
- [Theming](../02-user-guide/theming.md) — `kdos theme` in full
