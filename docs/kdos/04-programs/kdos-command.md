# The kdos command

`kdos` is the front door: nineteen subcommands covering the things this distribution can answer
that a general-purpose system cannot. This page documents each one — what it does, what it
measures, and what it refuses to claim.

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
kdos theme <phosphor|amber|ice|bone>
kdos theme list | next | prev
kdos theme style <file>
kdos theme --audit [accent]
```

Regenerates every themed artefact and commits the change in one ordered operation. Covered in
[Theming](../02-user-guide/theming.md).

**The commit order is load-bearing.** The wallpaper cache and the accent state file are both
*inputs* to the signal that repaints the session, so both are written before it is sent — the state
file because a signal arriving first makes the desktop re-read the accent it already had, and the
wallpaper because the compositor re-decodes on that same signal.

**The signal goes to four names, and an exact match is required.** The panel, the desktop and the
notification daemon are three names of one binary, so signalling only one retints the panel and
leaves the desktop icons and any live toast in the old accent. And the match must be exact: one of
those names is a **substring** of the two shell scripts that own the session, and an unhandled
signal kills a shell.

**`--audit` is the palette claim, checked.** It does not try to recognise "palette colours" in the
installed files — that test would have to know which mixes are legal and would drift from the
generators. It runs **the same generators** with the home and cache directories pointed at a
scratch directory and compares byte for byte, symlinks included. Anything that differs, differs
from what this machine's palette produces right now.

It writes nothing outside its scratch directory and signals nothing: an audit that repaired what it
found would be a `kdos theme` with a misleading name. Exit 0 clean, 1 on drift, 2 if it could not
run.

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
| Desktop | Whether the frame-reporting socket exists |

"Add yourself to this group" is an instruction; "permission denied" is not. That difference is why
the hardware check names the group.

The box check reports, on a live session, that the home directory is on an overlay **so a
persistent box cannot exist** — which is a real rule stated to the person it affects rather than a
failure.

## app

```sh
kdos app list | search | show | install | launch | remove | rollback | update | sources
```

The application front end, covered in [Applications](../02-user-guide/applications.md).

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

## trash

The freedesktop trash, from a prompt.

**This and the desktop's `Delete` key are one implementation.** The desktop has had a trash icon
since it had icons and the command line had no verb for it, so removing at a prompt and deleting on
the desktop were two different operations on one machine — one recoverable, one not. Two copies of
the specification would be two answers to what deleting means.

The library keeps the whole of it — the trash directories, the record file, the unique name,
restore and empty — and each front end keeps only the **question**: the confirmation, and the two
pinned places on the desktop that are not files.

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
