# Principles

The rules every change to KDOS is judged against, why each holds, and what each costs. These are
not preferences. Each one has consequences elsewhere in the system, and a patch that breaks one
tends to fail somewhere far from where it was written — so each principle below is stated with
the failure it prevents.

## No systemd

Nothing named `systemd-*` runs on the host, and no component may depend on one. The replacements
are each a small program doing one job:

| Function | KDOS uses | Not |
|---|---|---|
| Seat management | `seatd` | `systemd-logind` |
| sd-bus API | `basu` | `libsystemd` |
| Device management | `eudev` | `systemd-udevd` |
| Message bus | `dbus` | `dbus-broker` |
| DNS | `dnsmasq` | `systemd-resolved` |
| Network | `wpa_supplicant`, `NetworkManager` | `systemd-networkd` |
| Service supervision | `ksvc` and `/etc/init.d` | units and targets |

**The cost is real and is paid in two places.** Rootless containers get no cgroup delegation from
an init that does not provide it, so a `--memory` limit passed to podman is accepted and ignored
— which is why [`kdos-oomd`](../04-programs/daemons.md) enforces a box's memory budget itself,
and why [`15_userdirs.sh`](../03-architecture/boot-and-init.md) delegates a cgroup2 subtree per
user by hand. And a service that expects socket activation or a user slice has to be given
neither.

## No Xorg server, and one carve-out

There is no `xorg-server` port, no display manager, and nothing X on the login path.
`fs/etc/X11/` does not exist and must not be created.

**Xwayland is the single exception**, run rootlessly by the compositor itself so that X11-only
applications inside boxes work. It pulls in a client-side chain that exists *only* to satisfy it:
`xorgproto`, `xtrans`, `libXau`, `libXdmcp`, `xcb-proto`, `libxcb`, `libX11`, `libxkbfile`,
`xkbcomp`, `libxshmfence`, `libfontenc`, `libXfont2` and the `xcb-util` family. A recipe that
wants any of those for a different reason gets pushed back.

Two consequences worth knowing before you plan work around them. Mesa is built
`-D glx=disabled -D platforms=wayland`, so Xwayland is built `-Dglx=false` and **X clients get no
OpenGL**; enabling it means rebuilding Mesa with GLX and the X11 platform plus five more X
libraries. And the X core fonts `font-misc-misc`, `font-adobe-75dpi` and `font-cursor-misc` are
on the *host*, because a boxed Xt or Motif program asks the host's Xwayland for `-misc-fixed` or
`-adobe-helvetica`, and a font directory inside the box is invisible to a server outside it.

## No GTK and no Qt on the host

Neither toolkit is a host port. The desktop is drawn as a grid of character cells by
[our own libraries](../05-developer/c-libraries.md), which need neither. Graphical applications
live in [boxes](../03-architecture/packs-and-boxes.md), where both toolkits are present and are
themed through the shared home directory.

This is what keeps the host small enough to compile from source in one sitting and to reason
about in full. A library that would drag in a toolkit builds without it — `libcanberra`, if it is
ever added, builds `--disable-gtk`.

## One implementation of an idea

Where two programs would answer the same question, one of them owns the answer and the other
asks. The rule exists because two implementations are both correct on the day they are written,
and the one nobody is looking at is the one that drifts.

Worked examples, each of which would otherwise be a place for two answers to diverge:

- **The palette** is a table in `libkcolor`, expanded at compile time by everything that draws.
  No program keeps a second copy of the colour values.
- **Deleting a file** is `libkbase`'s trash implementation, used by both the `kdos trash` command
  and the desktop's Delete key, so a prompt and a keypress cannot mean different things.
- **Turning an `Exec=` line into an argument vector** is one function in `libkxdg`. Every launch
  path goes through it, including the one that writes those lines back out.
- **What a pin is** — `~/.config/kdos/favorites` has exactly one writer, even though the pinning
  happens in a menu and the drawing happens in the panel, which are separate processes.
- **Which program a command line runs** is one function, used by launcher generation, by the
  dispatcher and by the warmup.

When you find yourself about to write the second one, make the first one reachable instead.

## A thing that does not parse whole is absent, never partial

Every parser in this system treats a malformed input as *nothing there*, never as *some of it*.
A short footer, a wrong magic number, a truncated manifest, a format from the future, an offset
past the end of a file — each answers "there is no such object".

The reason is that half a structure which looks complete is exactly how a machine acts
confidently on something that is not true. A half-read boot-state file names a slot that was
never installed. A half-read snapshot manifest loses a tree. A half-read pack mounts a filesystem
nobody verified.

The same rule appears as: an absent A/B state file means "use the `root=` on the command line",
which is what a single-root machine does anyway; a `{}` restore marker is not a marker; and a
JSON scanner that refuses trailing junk, because every caller treats a parse failure as absent
and a lenient parser turns a corrupt file into a confident wrong answer.

## Never invent a number

Where the machine does not publish a value, the interface says so. It does not show a zero.

A `0` where a reading is unavailable reports a sensor that does not exist as a machine that is
idle, and that is a worse failure than an empty cell, because it looks like information. So
[`kdos-res`](../04-programs/kdos-res.md) renders `-` for anything unreadable, a GPU driver that
publishes no statistics gets **no column** rather than a column of zeroes, and a counter that
went backwards is drawn as a gap rather than as a spike.

The same rule governs what a measurement is allowed to claim.
[`kdos-energy`](../04-programs/daemons.md) reports relative shares of attributable CPU energy and
refuses to convert them into battery percentages, because RAPL cannot see the panel, the radio or
the disk. [`kdos stutter`](../04-programs/kdos-command.md) names who was busy during a dropped
frame and never says who caused it, because attribution from a half-second sample is
circumstantial. And [`kdos cve`](../04-programs/kdos-command.md) reports a package its database
does not carry as *unknown*, never as clean.

## Measure rather than assume

A claim about performance, contrast, timing or size is made only where it was measured, and the
measurement is stated with it.

[`kdos march`](../04-programs/kdos-command.md) is the clearest case: rather than shipping
`-march=x86-64-v3` on the theory that newer instructions are faster, it builds the port twice on
*your* machine, runs that port's own benchmark against both, and keeps the flags only where the
win clears both a fixed floor and the machine's own measured noise. A port with no declared
benchmark is *unmeasurable*, never a winner. The tool reports its reverts as prominently as its
wins, because a report listing only successes is a sales pitch.

The same habit applies to the interface: the contrast ratios in
[the design language](../03-architecture/design-language.md) are measured against the palette
table, not judged by eye.

## Offline by construction

The build runs with no network. This is enforced rather than intended — `make build` passes
`--network none` — so a dependency that reaches out fails immediately and visibly instead of
working on the machine that added it and failing everywhere else a year later.

The consequence for recipes is a whole class of build failure that must be fixed rather than
tolerated: a meson subproject wrap, a CMake `file(DOWNLOAD)`, a `FetchContent` git clone, or a
Python build backend resolving a system tool from PyPI. Each has a canonical fix in
[Build troubleshooting](../05-developer/build-troubleshooting.md).

## Reproducible by construction

A package built twice from the same tree is byte-identical. That is a property of one function —
the archive roller inside `kpkg` — rather than of 764 recipes, which is precisely why `kpkg`
rolls the archive itself instead of letting each recipe do it.

It is not decoration. Reproducibility is what makes a signed binhost meaningful, what lets a
delta reconstruct a package that still verifies against the original signature, and what lets a
rebuild be *compared* to what it was built from rather than merely produced. See
[Packaging](../03-architecture/packaging.md).

## Say what cannot be enforced

KDOS does not offer confinement it cannot deliver, and where a setting is advisory it says so.

A box profile reports what it enforced and names the mechanism behind each line — and it states
out loud what it could not. There is no podman flag that grants a box a speaker and denies it a
camera, so the profile does not pretend to have one. A base that names a container registry is an
**online** operation and announces itself as such before doing anything, because it fetches
unsigned content from someone else's server and pretending otherwise would be dishonest.

[The security model](../03-architecture/security-model.md) has a section devoted to what is *not*
protected, for the same reason.

## Documentation describes the present

No document, comment or shipped configuration file records history. Not what something used to
be, not which bug a line fixed, not how a lesson was learned.

Write the **constraint** — what the code does and what breaks if it changes. Do not write the
**changelog**. A reader has the file in front of them and needs to know what is true and what
they must not break; the story of how it got there is in the commit that made it so.

```c
/* WRONG — narrates a past defect */
/* This read the buffer size before the offset, which overflowed on a full
 * buffer and reported EOF. Fixed by clamping first. */

/* RIGHT — states the rule and its consequence */
/* Clamp before the read: a full buffer would otherwise ask for a
 * zero-length read and take the result for EOF. */
```

Both sentences carry the same warning. Only the second is still true in five years, and only the
second survives the surrounding code being rewritten.

## Every change updates its documentation, in the same change

A change in behaviour updates **every** page under `docs/kdos/`, **every** code comment and
**every** shipped configuration file that describes that behaviour — before the change is done,
not afterwards and not in a follow-up.

**The update replaces the old description.** It does not append to it, annotate it, or record what
the description was. There is no "previously" line and no note explaining the difference: the file
is rewritten to describe what is true now, and the difference is in the commit.

A comment that contradicts its code is worse than no comment: it is a claim the next reader will
act on. Stale-optimistic sends them past a bug; stale-pessimistic makes them re-verify something
that already works. Both cost more than the minute the update would have taken.

This applies to the two files that describe the whole system as much as to a comment above a
function.

## See also

- [Why KDOS](why-kdos.md) — the four properties these rules serve
- [Decisions](decisions.md) — the arguments where two principles pulled against each other
- [The design language](../03-architecture/design-language.md) — these principles applied to what you see
- [The security model](../03-architecture/security-model.md) — including what is not protected
- [CLAUDE.md](../../../CLAUDE.md) — the working rules for editing this tree
