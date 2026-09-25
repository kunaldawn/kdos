# Principles

These are the rules every change to KDOS is judged against. Each one exists because something
breaks without it, and the break is usually far from where the offending patch was written, so
each principle below is stated together with the failure it prevents and the price it charges.

They are not preferences, and none of them is free. The cost is stated in every case.

## No systemd

Nothing named `systemd-*` runs on the host, and no component may depend on one. The replacements
are each a small program doing one job.

| Function | KDOS uses | Not |
|---|---|---|
| Seat management | `seatd` | `systemd-logind` |
| sd-bus API | `basu` | `libsystemd` |
| Device management | `eudev` | `systemd-udevd` |
| Message bus | `dbus` | `dbus-broker` |
| DNS | `dnsmasq` | `systemd-resolved` |
| Network | `wpa_supplicant`, NetworkManager | `systemd-networkd` |
| Service supervision | `ksvc` and `/etc/init.d` | units and targets |

The price is paid in two places, both around containers. An init that provides no cgroup
delegation gives rootless podman nothing to enforce a limit with, so a `--memory` limit passed to
podman is accepted and silently does nothing. KDOS answers that in two steps:
[`15_userdirs.sh`](../03-architecture/boot-and-init.md) delegates a cgroup2 subtree per user by
hand at boot, and [`kdos-oomd`](../04-programs/daemons.md) reads each box's declared budget and
prefers an over-budget box as a kill victim, so a profile key that cannot be enforced as a cap is
at least honest about what it does. Separately, any service expecting socket activation or a user
slice has to be given neither.

## No Xorg server, and one carve-out

There is no `xorg-server` port, no display manager, and nothing X on the login path. `fs/etc/X11/`
does not exist and must not be created.

Xwayland is the single exception. The compositor runs it rootlessly so that X11-only applications
inside boxes work, and it pulls in a client-side chain that exists only to satisfy it:
`xorgproto`, `xtrans`, `libXau`, `libXdmcp`, `xcb-proto`, `libxcb`, `libX11`, `libxkbfile`,
`xkbcomp`, `libxshmfence`, `libfontenc`, `libXfont2`, `libxcvt`, `libepoxy`, and two of the five
`xcb-util` ports: `xcb-util-wm`, which `script/05_desktop/packages.txt` names because Xwayland's
xwm needs ICCCM and EWMH, and `xcb-util-renderutil`, which arrives through wlroots's `depends`.
`xcb-util`, `xcb-util-image` and `xcb-util-cursor` are recipes nothing reaches, so nothing builds
them. A recipe that wants any of these for a different reason gets pushed back.

Two consequences are worth knowing before you plan work around them. Mesa is built with
`-D glx=disabled -D platforms=wayland` and Xwayland with `-Dglx=false`, so X clients get no
OpenGL; enabling it means rebuilding Mesa with GLX and the X11 platform, plus five more X
libraries. And the X core fonts `font-misc-misc`, `font-adobe-75dpi` and `font-cursor-misc` are
host ports, because a boxed Xt or Motif program asks the host's Xwayland for `-misc-fixed` or
`-adobe-helvetica`, and a font directory inside the box is invisible to a server outside it.

## No GTK and no Qt on the host

Neither toolkit is a host port. Every surface KDOS paints is a grid of character cells drawn by
[libraries written for it](../05-developer/c-libraries.md), which need neither: the panel and all
its surfaces, the file chooser, the resource monitor, the terminal, the lock screen, the installer,
the boot splash and `tty1`. `libktui` composes the cells and knows nothing about where they go:
`libkwl` paints them into a `wl_shm` buffer as an ordinary Wayland surface, a terminal gets them as
escape sequences, and the boot splash writes PSF glyphs straight to `/dev/fb0` before any of that
exists.

There is one carve-out inside the desktop and it is worth knowing before your first titlebar. The
compositor links `cairo` and `pangocairo` and draws its own chrome — titlebars, the root menu and
the window-switcher OSD — with pango rather than with cells, at a size matched to the grid so the
machine still looks like one machine. `~/.config/kdos-comp/rc.xml` states the rule and the size;
`comp.conf`'s `chrome_font` and `panel_font` are what pick the face.

Graphical applications live in [boxes](../03-architecture/packs-and-boxes.md), where both toolkits
are present and are themed through the shared home directory, and an application in a box draws
whatever its toolkit draws.

This is what keeps the host small enough to compile from source in one sitting and to reason about
in full. The rule reaches dependencies as well as applications: a library that would drag a
toolkit in builds without it, so `libcanberra`, were it ever added, builds `--disable-gtk`.

The cost is that the host has no widget toolkit. Anything a KDOS surface wants to draw has to be
expressible in cells, and anything that is not goes in a box.

## Everything that runs on the host is built from source

Every program, library and module the host installs and runs on its own processor is compiled in
this tree from a pinned upstream source. The application catalogue is outside the rule: it is
Debian's packaging, built by podman into a box on the machine that asks, and never part of the
host. A binary taken on trust cannot be read, cannot be rebuilt by `kdos rebuild` from the
medium, and carries whatever its builder put in it — so one of them quietly ends the claim that
the system is inspectable end to end.

Four classes are exempt, and nothing outside them is:

- **Firmware and code for another processor.** `linux-firmware`, `intel-ucode`, `sof-firmware`,
  the GPU kernels in `intel-media-driver` and `libva-intel-driver`, the SOF coefficient blobs in `alsa-ucm-conf`, the
  device stubs inside `espflash`, `probe-rs`, `python3-esptool` and `openfpgaloader`, and the
  riscv64 EDK2 image `qemu` installs from its tarball. It runs on a DSP, a GPU, a microcontroller or a guest,
  and most of it has no published source.
- **Compiled font data.** `noto-fonts`, `noto-fonts-extra`, `noto-cjk`, `nerd-fonts-symbols`, and
  the fonts bundled inside `mupdf`, `matplotlib` and `seqkit`. Their sources compile through
  toolchains this tree does not carry. A face whose upstream build runs on ports — `ttf-dejavu`,
  `terminus-ttf`, `noto-emoji` — is compiled here.
- **Compiler bootstrap seeds.** The `rust` stage-0 toolchain, the `go` bootstrap toolchain and
  `zig1.wasm`. A self-hosted compiler needs a working one first; the seed builds and never ships.
- **Data with no other source form.** The `tesseract` model, the `perl-xml-parser` encodings,
  `libkiwix`'s JavaScript, the `fcitx5` tables, `john`'s `.chr` files and recorded
  audio.

Each exemption is still a hashed `source =` line, so the offline build holds for it. A prebuilt
object for the host that fits no class is deleted from the package or rebuilt by a flag, and a new
exemption is added to the [inventory](why-kdos.md#what-is-not-built-from-source) in the same
change. [Writing ports](../05-developer/writing-ports.md#what-is-built-from-source) has the rules a
recipe keeps.

The cost is capability. A feature whose only form is an upstream binary for the host — `go build
-race`, BoringCrypto, a board whose loader is a prebuilt image — is absent rather than shipped.

## One implementation of an idea

Where two programs would answer the same question, one of them owns the answer and the other asks
it. Two implementations are both correct on the day they are written; the one nobody is looking at
is the one that drifts.

The worked examples each mark a place where two answers could otherwise diverge.

- The palette is a table in `libkcolor`, expanded at compile time by everything that draws. No
  program keeps a second copy of the colour values.
- Deleting a file is `libkbase`'s trash implementation, used by both the `kdos trash` command and
  the desktop's Delete key, so a prompt and a keypress cannot mean different things.
- Turning an `Exec=` line into an argument vector is `kxdg_exec_split()` in `libkxdg`. Every
  launch path goes through it, including the one that writes those lines back out.
- What a pin is: `~/.config/kdos/favorites` has exactly one writer, even though the pinning
  happens in a menu and the drawing happens in the panel, which are separate processes.
- Which pack a command belongs to is `app_pack_by_exec()` in `kdos-appbox`, reading the
  `alien-apps` table the launcher generator writes. `run` and the login warmup both ask it
  rather than each deciding.

When you find yourself about to write the second one, make the first one reachable instead. The
cost is indirection: reaching a library function from a place that would rather hold a local copy.

## A thing that does not parse whole is absent, never partial

Every parser in this system treats malformed input as nothing there, never as some of it. A short
footer, a wrong magic number, a truncated manifest, a format from the future, an offset past the
end of a file — each answers "there is no such object".

Half a structure that looks complete is precisely how a machine acts confidently on something
untrue. A half-read boot-state file names a slot that was never installed. A half-read snapshot
manifest loses a tree. A half-read pack mounts a filesystem nobody verified.

The same rule shows up in several disguises. An absent A/B state file means "use the `root=` on
the command line", which is what a single-root machine does anyway. A `{}` restore marker is not a
marker. The JSON scanner refuses trailing junk, because every caller treats a parse failure as
absent and a lenient parser turns a corrupt file into a confident wrong answer.

What this costs is recoverability. A file with one bad byte is discarded whole, and there is no
salvage path.

## Never invent a number

Where the machine does not publish a value, the interface says so. It does not show a zero.

A `0` in place of an unavailable reading reports a sensor that does not exist as a machine that is
idle, which is worse than an empty cell because it looks like information. So
[`kdos-res`](../04-programs/kdos-res.md) renders `-` for anything unreadable, a GPU driver that
publishes no statistics gets no column rather than a column of zeroes, and a counter that went
backwards is drawn as a gap rather than as a spike.

The same rule governs what a measurement is allowed to claim.
[`kdos-energy`](../04-programs/daemons.md) reports relative shares of attributable CPU energy and
refuses to convert them into battery percentages, because RAPL cannot see the panel, the radio or
the disk. [`kdos stutter`](../04-programs/kdos-command.md) names who was busy during a dropped
frame and never says who caused it, because attribution from a half-second sample is
circumstantial. [`kdos cve`](../04-programs/kdos-command.md) reports a package its database does
not carry as unknown, never as clean.

The cost is that KDOS looks less capable than a tool willing to guess. A dashboard of dashes is a
worse demonstration and a better instrument.

## Measure rather than assume

A claim about performance, contrast, timing or size is made only where it was measured, and the
measurement is stated with it.

[`kdos march`](../04-programs/kdos-command.md) is the clearest case. Rather than shipping
`-march=x86-64-v3` on the theory that newer instructions are faster, it builds the port twice on
your machine, runs that port's own benchmark against both, and keeps the flags only where the win
clears both a fixed floor and the machine's own measured noise. A port with no declared benchmark
is unmeasurable, never a winner. The tool reports its reverts as prominently as its wins, because
a report listing only successes is a sales pitch.

The habit applies to the interface too: the contrast ratios in
[the design language](../03-architecture/design-language.md) are computed against the palette
table rather than judged by eye.

Measuring is slower than assuming. `kdos march` spends two builds and a benchmark run per port to
answer a question a flag would have answered instantly and sometimes wrongly.

## Offline by construction

The build runs with no network, and this is enforced rather than intended: `make build` passes
`--network none` to the build container. A dependency that reaches out fails immediately and
visibly, instead of working on the machine that added it and failing everywhere else a year later.

For recipes this creates a whole class of build failure that has to be fixed rather than
tolerated — a meson subproject wrap, a CMake `file(DOWNLOAD)`, a `FetchContent` git clone, a
Python build backend resolving a system tool from PyPI. Each has a canonical fix in
[Build troubleshooting](../05-developer/build-troubleshooting.md).

The cost lands on whoever adds a port. Vendoring a dependency bundle by hand is work that a
network-enabled build would have done for you.

## Reproducible by construction

A package built twice from the same tree is byte-identical. That is a property of one function —
`roll_package()` in `kpkg`, which invokes tar with `--sort=name`, a pinned `--mtime` honouring
`SOURCE_DATE_EPOCH`, and `--owner=0` — rather than a property of 969 recipes. Concentrating it
there is precisely why `kpkg` rolls the archive itself instead of letting each recipe do it.

Reproducibility is not decoration. It is what makes a signed binhost meaningful, what lets a delta
reconstruct a package that still verifies against the original signature, and what lets a rebuild
be compared against what it was built from rather than merely produced. See
[Packaging](../03-architecture/packaging.md).

The constraint it imposes is that a recipe may not roll its own archive, however much it would
like to.

## Say what cannot be enforced

KDOS does not offer confinement it cannot deliver, and where a setting is advisory it says so.

A box profile reports what it enforced and names the mechanism behind each line, and it states out
loud what it could not. There is no podman flag that grants a box a speaker and denies it a
camera, so the profile does not pretend to have one. A base that names a container registry is an
online operation and announces itself as such before doing anything, because it fetches unsigned
content from someone else's server.

[The security model](../03-architecture/security-model.md) devotes a section to what is not
protected, for the same reason.

The cost is a less impressive feature list. A profile that claimed per-device permissions would
read better and mean less.

## Documentation describes the present

No document, comment or shipped configuration file records history. Not what something was, not
which bug a line fixed, not how a lesson was learned.

Write the constraint — what the code does and what breaks if it changes. Do not write the
changelog. A reader has the file in front of them and needs to know what is true and what they
must not break; the story of how it came to be is in the commit that made it so.

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

`testing/docscheck.sh` enforces the mechanical half of this across the book, alongside the page
contract and dead-link checks.

## Every change updates its documentation, in the same change

A change in behaviour updates every page under `docs/kdos/`, every code comment and every shipped
configuration file that describes that behaviour — before the change is done, not afterwards and
not in a follow-up.

The update replaces the old description. It does not append to it, annotate it, or record what the
description was. There is no explanatory note about the difference: the file is rewritten to
describe what is true now, and the difference lives in the commit.

A comment that contradicts its code is worse than no comment, because it is a claim the next
reader will act on. A stale-optimistic one sends them past a bug; a stale-pessimistic one makes
them re-verify something that already works. Both cost more than the minute the update would have
taken.

The price is that no change is ever only a code change.

## See also

- [Why KDOS](why-kdos.md) — the four properties these rules serve
- [Decisions](decisions.md) — the arguments where two principles pulled against each other
- [The design language](../03-architecture/design-language.md) — these principles applied to what you see
- [The security model](../03-architecture/security-model.md) — including what is not protected
- [CLAUDE.md](../../../CLAUDE.md) — the working rules for editing this tree
