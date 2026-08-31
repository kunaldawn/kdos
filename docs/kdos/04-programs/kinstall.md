# kinstall

The installer's design. This page is about how it is built and why; for using it, see
[Installation](../02-user-guide/installation.md), which does not repeat any of this.

## Why it links almost nothing

`kinstall` links **libkbase, libktui and libkcolor and nothing else** — not even a terminal
library. That is what lets it be cross-compiled in phase 1 and exist on every tree from the first
bootable image onward.

Do not give it, or any of those three libraries, a new dependency without moving its build to a
later phase.

The colour library is on that build line because the toolkit's theme file includes the palette
header — the numbers live there and nowhere else. Leaving it out builds fine on a development host
and fails only in the cross build.

A recipe sits beside the sources, so a running KDOS can rebuild the installer natively.

## The file split

| File | Owns |
|---|---|
| `probe.c` | The `/sys` and superblock reader, the partition-table reader, and the medium's flat pack index — a small device identifier |
| `pages.c` | The eleven wizard pages |
| `install.c` | The forked install child and its line protocol |
| `conf.c` | The answer file, and the filesystem table |
| `dump.c` | The non-interactive dumps |
| `main.c` | Chrome, the poll loop, the command line |

Everything it once owned besides those — the terminal, the cell buffer, the input layer, the
widgets, the modals, the palette — is [libktui](../05-developer/c-libraries.md).

## Five decisions that carry the design

### Nothing is written before the summary

Every page fills the configuration structure and **only** the configuration structure. The install
step is the single point of no return, which is what makes `Back` mean something.

`Next` on the summary page is deliberately **refused**: the install starts from the button and only
from the button.

### The whole interface is eight colours

A 512-glyph console font makes the terminal steal the foreground intensity bit for the ninth glyph
bit, so the bright colours are **unreachable as a foreground** and the bold attribute changes the
*font page* rather than the weight. **Never emit bold on a console.**

Eight slots mean a console and a modern terminal emulator render the same picture. On a console the
installer saves the palette, installs its own, and restores exactly what was there; elsewhere the
same slots go out as true colour or indexed escapes.

### The mouse works on a bare console

The Linux console has no mouse reporting at all, so the input layer opens the input devices
directly, keeps its own pointer — relative devices scaled by the real cell size read from the
framebuffer's reported dimensions, absolute devices mapped straight through — and draws it as an
inverted cell. Under a terminal emulator it uses the ordinary reporting mode instead and never
touches the input devices.

### The install runs in a forked child

The work stays straight-line sequential code and the parent stays a single-threaded poll loop that
never blocks on a multi-gigabyte copy.

The child redirects its own standard streams to nothing and keeps the protocol on its own
descriptor, so **nothing but the emitter can reach the pipe**.

**The protocol is one letter per line:**

| Letter | Means |
|---|---|
| `S` | Step *n* has started |
| `K` | Step *n* is skipped |
| `P` | Progress within the current step, as a fraction |
| `N` | A note on the current step |
| `L` | A log line |
| `F` | Failed, with a message |
| `D` | Done |

**A step start closes out every earlier step, not just the previous one.** A skipped step sits
between them often enough — no repartition, no theme regeneration — that closing only the
predecessor leaves the real one spinning for the rest of the run.

**There is no shell and no command string anywhere.** Device paths and user names all arrive from
menus, and everything is executed through an argument vector.

### Chrome takes hit identifiers from a reserved range

The sidebar draws **before** the page, so claiming ordinary focus identifiers there pushed every
control on every page down the focus ring and left the caret parked on a decoration — typing did
nothing at all. Chrome registers in a reserved range that never joins the focus ring.

## The page model

Eleven pages in one table, each with an identifier, a title, an icon, an optional entry hook, a
draw function and input handlers. The identifiers are the only spelling.

```
welcome  keyboard  time  disk  layout  accounts  system  packs  summary  install  done
```

**Pages are found by identifier, never by index.** An unattended install jumps straight to the
install page, and a magic number there is right until a page is added in front of it — after which
the child is already forked and the wrong screen is on the display.

## The filesystem table

One row per filesystem, and **every consumer reads the same row**: the menu, the `mkfs` argument
vector, the `fstab` line and the swapfile step.

Three things the row encodes, each a way this goes wrong later:

- **The `fstab` pass number is non-zero only for the default filesystem.** A non-zero pass is an
  instruction to run a checker at boot, and there is no checker worth running for the others.
- **The swapfile method differs per filesystem.** Preallocation leaves unwritten extents, which two
  of the four refuse to swap on — so the failure is at the *next* boot's swap activation, with no
  swap and nothing saying why.
- **Two of the four are kernel modules**, so **every row must also appear in the initramfs module
  list**. A root on a module the initramfs cannot load installs perfectly and never boots again.

A filesystem whose `mkfs` is missing from the image is still listed, with the row saying so, and is
refused **before** anything is written — a control that snaps back under the cursor is worse than
one that explains itself.

## The applications step

**It reads the flat index, not the pack library.** That is the property keeping the installer in
phase 1. The index carries a recommended flag and a human summary **for exactly this reader**, so
the installer and the application tool cannot disagree about what is suggested or what a pack is —
and a list of identifiers and byte counts is not a page anybody can choose from, which is why the
summary is in the file.

**The base and the runtimes are not a choice.** They are drawn as facts and carried always,
because leaving one out installs applications that cannot start — the one outcome a page of
checkboxes must not be able to produce.

**A delta stanza is dropped whole**, at the stanza boundary rather than by clearing a field when
the marker is seen — clearing depends on the marker arriving *after* the field it clears. A delta
is a route to a pack rather than a pack, and an installer that offered one would offer something
it cannot apply.

**The entry hook runs before the plan is computed** on every path that plans without walking the
wizard. Whether this step runs at all is a question about the medium, and planning first reports it
skipped on a machine that would carry gigabytes.

**The copy is a copy and nothing else.** The pack daemon verifies a pack where it *mounts* it, so
hashing here would do the work twice and drag the pack library into a program that links three. It
does set the staging directory's permissions, because a first boot that inherited restrictive ones
would refuse an install until the daemon had run once — which reads as the feature not working.

## Answer files

Flat `key = value`, written by a save flag and read by a config flag.

Two fallbacks keep an unattended run from failing over a spelling, and both are deliberate:
an unknown application falls back to the recommended set, and an unknown filesystem falls back to
the default. Both are read **before** the point of no return, and refusing there would leave a
machine with no operating system on it.

**An unattended run ends by itself, whichever way it went.** The event loop has two exits — a key,
and the reboot branch — and the reboot branch is gated on a key in the answer file. So an
unattended install told not to reboot did all its work and then spun on its own last screen
forever, which is a scripted install that never returns.

The test is **done or failed**, and the second half is not decoration: done is set only by a run
that reached the last step, so testing it alone leaves exactly the install that went wrong spinning
on its error screen. It **exits non-zero when the install failed**.

## Dumping

```sh
kinstall --dump probe [--json]
kinstall --dump plan  [--json]
```

Neither needs a terminal and neither runs anything.

`probe` is the machine as the prober sees it. `plan` calls **the same planner the wizard does**, so
the step list and its skips are the real ones. The structured form is a rendering of the same
traversal, not a second walk.

**No password reaches either form.** The configuration holds them in the clear because hashing is
the next thing that happens to them, and a dump is what ends up in a log. The test suite asserts
this with a sentinel value.

`--dump probe` is deliberately **not** in the test suite: it walks the root filesystem to measure
the payload, which is seconds on a live image and far longer on a development machine.

## Iterating without booting

```sh
kinstall --dry-run                      # log every command, execute none
kinstall --save answers.conf
kinstall --config answers.conf
kinstall --unattended --config answers.conf
kinstall --ascii                        # force the lowest glyph tier
kinstall --no-mouse
```

**Give it a terminal of its own when something else owns the console.** `kinstall < /dev/tty3 >
/dev/tty3` puts the interface where nothing competes for it — and pointing a full-screen redraw at
a serial console makes it the only thing on that wire for as long as the install runs.

The disk-install harness does exactly that, and prints a heartbeat, because a run that says nothing
until it finishes cannot be told from one that never will.

## What the rest of the tree had to provide

Each of these existed for the installer and would otherwise be decorative:

- `kdos-getty` loads the keymap the installer writes.
- `rcS` activates swap after mounting, or the swap option would do nothing.
- **`fstab` is appended to, never replaced** — the shipped file carries the temporary-filesystem
  entry every graphical application depends on.
- Renaming the user rewrites the account files, the primary group's own name, the home directory
  **and the autologin line**.
- The kernel and initramfs are copied onto the ESP, and the boot configuration points at those
  paths.

**The ESP is a FAT filesystem, so nothing is copied onto it with an archive-preserving copy.** FAT
has no ownership to preserve: such a copy calls the ownership change on every file, the kernel
refuses each, and the copy exits non-zero with a page of errors naming the **source** paths — which
reads as a problem with the boot loader rather than a filesystem that cannot hold what was asked of
it.

**The medium is bound out of the target's way before the target is mounted.** The target mountpoint
and the live medium's mountpoint are nested, so mounting the target hides the very directory the
packs are copied *from* — and every path into it then resolves inside the filesystem created empty
a moment earlier. It is a **bind**, not a second mount of the device: the device is not the
installer's to name, since the initramfs mounted it and moved it across the root switch, and a path
is the only handle anything has on it.

## See also

- [Installation](../02-user-guide/installation.md) — using the installer
- [Boot and init](../03-architecture/boot-and-init.md) — what it writes, and what boots it
- [Packs and boxes](../03-architecture/packs-and-boxes.md) — the index the applications page reads
- [The C libraries](../05-developer/c-libraries.md) — the three it links
- [Testing](../05-developer/testing.md) — the dumps and the disk-install harness
