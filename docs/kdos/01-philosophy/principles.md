# Principles

This chapter states the rules that every part of KDOS is built to, and against which every change
to it is judged. It is written for anyone who wants to understand why the system behaves as it
does, and for contributors, whose patches are reviewed against these rules. Read
[Why KDOS](why-kdos.md) first: it describes the ideas these rules serve.
[How KDOS differs](how-kdos-differs.md) sets several of them beside the choices other
distributions make, and is useful but not required first. The chapter after this one,
[Decisions](decisions.md), records the choices where a reasonable alternative existed, and why it
was not taken.

Each rule exists because something breaks without it, and the break usually appears far from the
change that caused it. Each principle is therefore stated with the failure it prevents and the
price it charges.

| Principle | What it prevents | What it costs |
|---|---|---|
| [No systemd](#no-systemd) | One component owning init, logins, devices, logging and the network | Container resource limits need a hand-built cgroup delegation |
| [No Xorg server](#no-xorg-server-and-one-carve-out) | A second display stack on the login path | X clients get no GLX |
| [No GTK and no Qt on the host](#no-gtk-and-no-qt-on-the-host) | A host too large to build in one sitting or read in full | The host has no widget toolkit |
| [Built from source](#everything-that-runs-on-the-host-is-built-from-source) | Binaries nobody can read or rebuild | Features that exist only as upstream binaries are absent |
| [Offline by construction](#offline-by-construction) | Builds that work once and fail a year later | Every dependency a build would download must be bundled with the sources in advance |
| [Reproducible by construction](#reproducible-by-construction) | Signatures and deltas over packages nobody can re-derive | A recipe never rolls its own archive |
| [One implementation of an idea](#one-implementation-of-an-idea) | Two answers to one question that drift apart | Indirection |
| [Absent, never partial](#a-thing-that-does-not-parse-whole-is-absent-never-partial) | Confident action on half-read data | No salvage of a damaged file |
| [Never invent a number](#never-invent-a-number) | A missing reading presented as a real one | The interface shows dashes where others show guesses |
| [Measure rather than assume](#measure-rather-than-assume) | Claims nobody checked | Measuring is slow |
| [Say what cannot be enforced](#say-what-cannot-be-enforced) | Confinement that exists only in a settings file | A shorter feature list |
| [Documentation describes the present](#documentation-describes-the-present) | Comments that narrate a past nobody can act on | History lives only in git |
| [Documentation changes with the code](#every-change-updates-its-documentation-in-the-same-change) | Pages that contradict the tree | No change is ever only a code change |

## No systemd

Nothing named `systemd-*` runs on the host, and no component depends on one. Each job that systemd
would do is done by a separate program that does only that job, and each of those programs is a
port (a directory holding the recipe that describes how to build one package) that can be read
and replaced on its own.

| Function | KDOS uses | Not |
|---|---|---|
| Init and service supervision | `/etc/init.d/rcS` running the numbered scripts in `/etc/init.d`, with daemons kept alive by `ksvc` | units and targets |
| Seat management | `seatd` | `systemd-logind` |
| sd-bus API for programs that want one | `basu` | `libsystemd` |
| Device management | `eudev` | `systemd-udevd` |
| Message bus | `dbus` | `dbus-broker` |
| Network | NetworkManager with `wpa_supplicant`; `dhcpcd` when NetworkManager is turned off | `systemd-networkd` |
| DNS | `dnsmasq`, which NetworkManager starts as a local caching resolver on `127.0.0.1` and `::1` | `systemd-resolved` |
| Time synchronisation | `chrony` | `systemd-timesyncd` |
| System log | `sysklogd` | `journald` |
| Scheduled jobs | one `snooze` per job, supervised by `ksvc` | timer units |

`ksvc` is part of `kdos-tools`. It makes each supervisor the leader of its own process group, with
the daemon inside it, so that stopping a service signals the whole group and reaches the daemon and
not only its supervisor, and it passes the daemon command as an argument vector rather than as a
string for a shell to split. The init scripts are shell, because a list of services and their
start order reads most clearly as a script, and `ksvc` supplies the process handling a script
cannot. See [Boot and init](../03-architecture/boot-and-init.md#rcs-and-the-service-scripts).

The price is paid around containers. Rootless podman applies a `--memory` or `--cpus` limit through
a cgroup2 subtree that the user owns, and on other distributions systemd's user slice is what hands
one out. Without a delegated subtree, podman accepts the flag and silently does nothing. KDOS covers
this in two ways:

1. At boot, `/etc/init.d/15_userdirs.sh` delegates a cgroup2 subtree to every user with a uid of
   1000 or above, by hand: it enables the `cpu`, `memory` and `pids` controllers at the root and
   in `/sys/fs/cgroup/user.slice`, creates `user.slice/user-<uid>` owned by that user, and creates
   a `session` leaf inside it that the login is moved into. The leaf is needed because a cgroup
   that holds processes cannot enable controllers for its children, and podman creates its
   container cgroups beside the caller's.
2. [`kdos-oomd`](../04-programs/daemons.md#kdos-oomd) watches memory pressure through
   `/proc/pressure/memory` and, when the machine stalls, kills the largest process that is neither
   the desktop nor its plumbing. A box over the `memory` budget its profile declares is chosen
   first, so the profile key still means something where the limit itself could not be applied.
   Otherwise the largest boxed process is preferred to a host one whenever it holds at least half
   the memory of the largest candidate.

A service that expects socket activation or a user slice of its own has to be run without them.

## No Xorg server, and one carve-out

There is no `xorg-server` port, no display manager, and nothing X on the login path. The system
files under `fs/` contain no `/etc/X11/` directory, and a change that creates one is rejected in
review.

One display server, the Wayland compositor, owns the screen, the input devices and the login
path. An Xorg server would be a second complete display stack, with its own input handling, its
own drivers and its own privileges, to build, secure and keep consistent with the first. See
[How KDOS differs](how-kdos-differs.md#the-display-stack) for how other distributions carry both.

Xwayland is the single exception. The compositor runs it rootlessly so that X11-only applications
inside boxes (the rootless containers graphical applications run in) work, and it pulls in a
client-side chain of X ports that exists only to satisfy it: `xorgproto`, `xtrans`, `libXau`,
`libXdmcp`, `xcb-proto`, `libxcb`, `libX11`, `libxkbfile`, `xkbcomp`, `libxshmfence`, `libfontenc`,
`libXfont2`, `libxcvt` and `libepoxy`. Two of the five `xcb-util` ports are built: `xcb-util-wm`,
which `script/05_desktop/packages.txt` names because Xwayland's window manager needs ICCCM and EWMH,
and `xcb-util-renderutil`, which arrives through the `depends` line of `wlroots`. `xcb-util`,
`xcb-util-image` and `xcb-util-cursor` are recipes that no phase list or dependency reaches, so
nothing builds them. A recipe that wants any of these libraries for a reason other than Xwayland is
rejected.

Two consequences follow, and both matter when planning work around X clients.

- **X clients get no GLX.** Mesa is built with `-D glx=disabled -D platforms=wayland`, and
  Xwayland with `-Dglx=false`. Enabling GLX means rebuilding Mesa with GLX and the X11 platform,
  and adding the X client libraries those need, none of which is a port. An X11 client in a box
  that draws through EGL uses the box's own Mesa over Xwayland's DRI3 and is unaffected.
- **The X core fonts are host ports.** `font-misc-misc`, `font-adobe-75dpi` and
  `font-cursor-misc` are in `script/04_phase4/packages.txt`, and the tools that build them
  (`bdftopcf`, `font-util`, `mkfontscale` and `encodings`) arrive through their `depends` lines.
  A boxed Xt or Motif program asks the host's Xwayland for `-misc-fixed` or `-adobe-helvetica`,
  and a font directory inside the box is invisible to a server running outside it.

## No GTK and no Qt on the host

Neither toolkit is a host port. Every surface KDOS draws is a grid of character cells produced by
[libraries written for it](../05-developer/c-libraries.md), which need neither: the panel and all
its surfaces, the file chooser, the resource monitor, the terminal, the lock screen, the installer
and `tty1`. `libktui` composes the cells and knows nothing about where they go. `libkwl` paints
them into a `wl_shm` buffer as an ordinary Wayland surface, and a terminal receives them as escape
sequences. The boot splash, `kdos-splash`, runs from the initramfs before any of that exists and
links none of the libraries: its own code writes PSF glyphs and pixels straight to `/dev/fb0`,
taking only the colour table from the `libkcolor` header.

The compositor is the one exception inside the desktop. `kdos-comp`, a fork of labwc, links `cairo`
and `pangocairo` and draws its own chrome (titlebars, the root menu and the on-screen window
switcher) with pango rather than with cells. It draws in `Terminus (TTF)` at a size that makes a
titlebar one cell tall, because pango cannot render the bitmap Terminus the cell surfaces use. See
[The design language](../03-architecture/design-language.md#the-compositors-decoration-is-part-of-the-set)
and [Theming](../02-user-guide/theming.md#fonts) for the fonts each part of the desktop draws in and
the keys that set them.

Graphical applications live in [boxes](../03-architecture/packs-and-boxes.md), where both toolkits
are present and themed through the shared home directory, and an application in a box draws
whatever its toolkit draws.

The rule reaches dependencies as well as applications. A library or tool that would pull a toolkit
in is built without it: `avahi` and `ghostscript` are configured with `--disable-gtk`, `gnuplot`
with `--without-qt`, `android-file-transfer` with `-DBUILD_QT_UI=OFF`, and `fontforge` without its
GTK editor window.

This keeps the host small enough to compile from source in one sitting and to reason about in full.
The cost is that the host has no widget toolkit: anything a KDOS surface draws has to be
expressible in cells, and anything that is not goes in a box.

## Everything that runs on the host is built from source

Every program, library and module that the host installs and runs on its own processor is compiled
in this tree from pinned source: 1,038 recipes, 1,014 under `ports/core` for upstream software,
each pinned by hash, and 24 under `src/` for the software kept in this repository, most of it
written for KDOS. [The ports catalogue](../06-reference/ports-catalogue.md) lists every one of
them by phase and group, and [How KDOS is built](../05-developer/how-kdos-is-built.md) follows the
build from the first source fetch to a bootable ISO. The application catalogue is outside the rule.
It is Debian's packaging, built by podman into a box on the machine that asks for it, and it is
never part of the host.

A binary taken on trust cannot be read, cannot be rebuilt from the source tree by
[`kdos rebuild`](../04-programs/kdos-command.md#kdos-rebuild), and carries whatever its builder put
in it. A single prebuilt binary on the host is enough to make end-to-end inspection impossible.

Four classes are exempt, and nothing outside them is:

- **Firmware and code for another processor.** `linux-firmware`, `intel-ucode`, `sof-firmware`,
  the GPU kernels in `intel-media-driver` and `libva-intel-driver`, the SOF coefficient files in
  `alsa-ucm-conf`, the device stubs uploaded by `espflash`, `probe-rs`, `python3-esptool` and
  `openfpgaloader`, and the riscv64 EDK2 image that `qemu` installs from its tarball. This code
  runs on a DSP, a GPU, a microcontroller or a virtual machine guest, and most of it has no
  published source.
- **Compiled font data.** `noto-fonts`, `noto-fonts-extra`, `noto-cjk`, `nerd-fonts-symbols`, and
  the fonts bundled inside `mupdf`, `matplotlib` and `seqkit`. Their sources compile through
  toolchains this tree does not carry. A face whose upstream build runs on ports is compiled here:
  `ttf-dejavu`, `terminus-ttf` and `noto-emoji`.
- **Compiler bootstrap seeds.** The `rust` stage-0 toolchain, the `go` bootstrap toolchain,
  `zig1.wasm` inside the `zig` source, and the upstream musl GHC that `ghc` builds with. A compiler
  written in its own language needs a working copy first; the seed is used by the build and never
  installed.
- **Data with no other source form.** The `tesseract` English model, the `perl-xml-parser`
  encoding maps, the JavaScript of `libkiwix`'s server skin, the `fcitx5-chinese-addons` tables,
  `john`'s `.chr` files, the RP2350 boot-ROM copies in `picotool`, the recorded voice samples
  in `alsa-utils`, and the Alpine security database that `kdos-tools` ships as
  `/usr/share/kdos/secdb.txt`.

Each exemption is pinned in the tree, by a hashed `source =` line or, for the `kdos-tools`
security database, as a file committed under `src/`, so the offline build holds for it. A
prebuilt object for the host that fits no class is deleted from the package or rebuilt through a
build flag: `go` deletes its race-detector runtime and BoringCrypto module, and `john` deletes
`run/ztex`. A new exemption is added to the
[inventory](why-kdos.md#what-is-not-built-from-source) in the same change that introduces it.
[Writing ports](../05-developer/writing-ports.md#what-is-built-from-source) has the rules a recipe
keeps.

The cost is capability. A feature whose only form is an upstream binary for the host, such as
`go build -race`, BoringCrypto, or a board whose loader is a prebuilt image, is absent rather than
shipped.

## Offline by construction

The build runs with no network, and this is enforced rather than intended: `make build` starts the
build container with `docker run --network none`. A dependency that reaches out fails immediately
and visibly, instead of working on the machine that added it and failing everywhere else a year
later.

The network is used in one step, `make fetch`, which runs `ports/fetch`. It places every source
file a recipe names in that port's directory and checks each against the recipe's `sha256 =` line,
taking each file from the first place that holds a copy that verifies:

1. the port directory;
2. the local cache, `ports/.srccache/`;
3. the KDOS source archive, located through the committed `ports/sources.idx`;
4. the upstream URL in the recipe's `source =` line;
5. for a port's own vendor bundle only, regeneration.

`make fetch-check` repeats the verification offline and reports what is missing or wrong. See
[Decisions](decisions.md#upstream-archives-are-content-addressed-release-assets) for why the
sources are held in an archive keyed by their hashes.

For recipes this creates a class of build failure that has to be fixed rather than tolerated: a
meson subproject wrap, a CMake `file(DOWNLOAD)`, a `FetchContent` git clone, a Python build backend
resolving a system tool from PyPI. Each has a standard fix in
[Build troubleshooting](../05-developer/build-troubleshooting.md#a-build-that-reaches-the-network).

The cost lands on whoever adds a port. A language ecosystem that downloads its dependencies at
build time needs a *vendor bundle*: a reproducible tarball of those dependencies, produced by
`make fetch` and then held in the archive like any other source. A recipe asks for one with
`vendoring =`, and 123 recipes in `ports/core` do: 60 Rust, 34 Go, 25 Python and 4 Haskell. One
more, `pdfium`, carries a bundle that no tool writes, assembled by hand from the Chromium
checkouts it needs, so 124 ports carry a vendor bundle in all. A build with network access would
fetch these dependencies itself. See [Writing ports](../05-developer/writing-ports.md#vendoring)
and [A bundle no tool writes](../05-developer/writing-ports.md#a-bundle-no-tool-writes).

## Reproducible by construction

A package built twice from the same tree is byte-identical. That is a property of one function
rather than of 1,038 recipes: `roll_package()` in `kpkg`, the package manager
(`src/packages/kdos-kpkg/build.c`), runs tar with `--sort=name`, `--format=gnu`,
`--owner=0 --group=0 --numeric-owner`, an `--mtime` taken from `SOURCE_DATE_EPOCH`, and
`xz -9 -T1` as a pinned compressor. The build also sets its umask to `022` before a recipe runs.
Every phase's environment file pins `SOURCE_DATE_EPOCH` to `1735689600`, and with the variable
unset the mtime is `0`, so the answer never depends on when the build ran. Keeping all of this in
one place is why `kpkg` rolls the package archive itself instead of letting each recipe do it.

Reproducibility is what gives the other package mechanisms their meaning. A signed
[binhost](../06-reference/glossary.md) (a directory of prebuilt packages with a signed index) is
worth trusting only if its packages can be rebuilt and compared. A delta (a binary difference from
one version of a package to the next) can be applied without any trust of its own, because the
package it produces is checked against the hash the signed index already carries. And
`kpkg verify --repro <port>` builds the same recipe twice and compares the two packages byte for
byte. See [Packaging](../03-architecture/packaging.md#reproducible-packages).

The constraint it imposes is that a recipe never rolls its own package archive. It installs files,
and `kpkg` packs them.

## One implementation of an idea

Where two programs would answer the same question, one of them owns the answer and the other asks
it. Two implementations are both correct on the day they are written; the one nobody is looking at
is the one that drifts.

Each of these examples marks a place where two answers could otherwise diverge:

- **The palette** is a table in `libkcolor`, written as an X-macro: a list of macro calls that each
  consumer expands at compile time with its own definition of the macro. `libktui` projects it onto
  its colour slots and the theme generators expand it into CSS, SVG and Xcursor, but no program
  keeps a second copy of the colour values.
- **Deleting a file** is `libkbase`'s trash implementation (`kb_trash_*`), used by both the
  `kdos trash` command and the desktop's Delete key, so a command and a keypress cannot mean
  different things.
- **Turning an `Exec=` line into an argument vector** is `kxdg_exec_split()` in `libkxdg`. Every
  launch path goes through it, in the panel, the terminal, `kdos-appbox` and the `kdos` command,
  including the path that writes those lines back out.
- **What a pin is**: `~/.config/kdos/favorites` has one writer, `sh_fav_set()` in `kdos-shell`'s
  `fav.c`, even though pinning happens in a menu and the quick-launch row is drawn by the panel,
  which is a separate process.
- **Which pack a command belongs to**, a pack being the single read-only filesystem image an
  imported application runs from (an application built from the catalogue has none), is
  `app_pack_by_exec()` in `kdos-appbox`, reading the `alien-apps`
  table (one row per boxed application: its name, its command and, where it has one, its pack) that
  the launcher generator writes. `kdos-appbox run` and the warmup that starts pinned applications'
  boxes at login both ask it rather than each deciding.

When you are about to write the second implementation, make the first one reachable instead. The
cost is indirection: reaching a library function from a place that would rather hold a local copy.

## A thing that does not parse whole is absent, never partial

Every parser in this system treats malformed input as nothing there, never as some of it. A short
footer, a wrong magic number, a truncated manifest, a format version newer than the reader
understands, an offset past the end of a file: each answers "there is no such object".

Half a structure that looks complete is how a machine acts confidently on something untrue. A
half-read boot-state file names a root slot (one of the two root filesystems an installed disk
holds) that was never installed. A half-read build snapshot's manifest loses a tree. A half-read
pack mounts a filesystem nobody verified.

The same rule appears in several places:

- **The A/B boot state.** When the initramfs cannot read a usable state from
  `/EFI/kdos/bootstate` on the EFI system partition (ESP), which records which of the two root
  slots to boot, it keeps the `root=` on the kernel command line, which is also how a machine with
  a single root partition boots. See
  [Boot and init](../03-architecture/boot-and-init.md#ab-slot-selection).
- **The build's JSON.** `libkbuild`'s JSON reader (`kb_json.c`) rejects anything after the one
  value a document holds, because every caller treats a parse failure as "the snapshot is absent",
  and a lenient parser would turn a corrupt file into a confident wrong answer.
- **The build's restore marker.** When the build restores a phase snapshot (a saved copy of the
  build tree at the end of a phase), it first writes a marker naming the target, so that an
  interrupted restore can be resumed. A marker that holds `{}` names no target, so it is read as
  no marker at all rather than as a restore to nothing. See
  [The build system](../05-developer/build-system.md#snapshots).
- **A pack's footer.** `libkpack` checks that the regions a footer declares lie in order inside
  the file before it believes any of them.

The cost is recoverability. A file with one bad byte is discarded whole, and there is no salvage
path.

## Never invent a number

Where the machine does not publish a value, the interface says so. It does not show a zero.

A `0` in place of an unavailable reading reports a sensor that does not exist as a machine that is
idle, which is worse than an empty cell because it looks like information.
[`kdos-res`](../04-programs/kdos-res.md#missing-and-discontinuous-readings) renders `-` for any reading
it cannot take, leaves out a GPU line (memory, clock, sensors) that the driver does not publish,
and draws a counter that went backwards as a gap rather than as a spike. Where a driver has no
utilisation counter, as `i915` and `xe` do not, it shows engine time labelled as such and does not
convert it into a percentage.

The same rule governs what a measurement is allowed to claim.
[`kdos-energy`](../04-programs/daemons.md#kdos-energyd) reports relative shares of attributable CPU
energy and states that it gives no watt-hours, because RAPL, the processor's energy counters, cannot
see the panel, the radio or the disk; a driver with no per-client GPU accounting gets no GPU column
at all. [`kdos stutter`](../04-programs/kdos-command.md#kdos-stutter) names what each process was
doing during a dropped frame and never says which one caused it, because attribution from a 500 ms
sample cannot prove causation. The one causal claim it makes is about the compositor's own render
time, where it has both halves of the evidence. [`kdos
cve`](../04-programs/kdos-command.md#kdos-cve) reports a package its database does not carry as
unknown, never as clean.

The cost is that KDOS looks less capable than a tool willing to guess. The interface shows `-`
where another tool would show an estimate.

## Measure rather than assume

A claim about performance, contrast, timing or size is made only where it was measured, and the
measurement is stated with it.

[`kdos march`](../04-programs/kdos-command.md#kdos-march) is the clearest case. Rather than shipping
a higher `-march` level on the theory that newer instructions are faster, it builds a port twice on
your machine, once at the highest x86-64 level the processor supports, and runs the port's own
benchmark against both builds, five runs each by default. It keeps the flags only where the median
win exceeds 3 per cent plus the noise floor, which it measures as the larger of the two builds'
spreads across their own runs. A port with no `bench =` line in its recipe is unmeasurable, never a
winner; six recipes declare one. The ledger in `/var/lib/kdos/march.ledger` records reverted and
unmeasurable ports as well as kept ones, so the report shows every port that was tried and not only
the ones that won.

The habit applies to the interface too. The legibility of the palette is computed rather than
judged by eye: the library self-test (`src/libs/selftest.c`) asserts, for every scheme in
`libkcolor`, a contrast of at least 7:1 between text and its ground and 4.5:1 between the accent
and the same ground. See [The design language](../03-architecture/design-language.md#colour).

Measuring is slower than assuming. `kdos march` spends two builds and ten benchmark runs per port
to answer a question that a flag would have answered instantly, and sometimes wrongly.

## Say what cannot be enforced

KDOS does not offer confinement it cannot deliver, and where a setting is advisory it says so.

A box profile, printed by `kdos-box profile <box>`, reports what was enforced rather than what the
file asked for, and names the podman flag or KDOS mechanism behind each line. What it cannot
enforce it marks with `!`:

- There is no podman flag that grants a box a speaker and denies it a camera. A box whose devices
  are shared has the host's whole `/dev`, so an `audio` or `gpu` key that would subtract one of
  them is reported as not enforceable separately.
- `wayland = no` cannot be enforced, because the box shares `$XDG_RUNTIME_DIR` and the session's
  socket is in it.
- A `persistence` other than `persistent` is recorded but not imposed, because a box's writes land
  wherever its container runtime puts them.

A box base that names a container registry is an online operation, and says so before it does
anything, because it fetches unsigned content from a third-party registry. `kdos sandbox` follows
the same rule for Landlock, the kernel's unprivileged file and network sandbox: it offers no option
that Landlock cannot enforce, and on a kernel too old for network rules its `--explain` says that
`--no-network` cannot be applied. [The security
model](../03-architecture/security-model.md#what-is-not-protected) gives a section to what is not
protected, for the same reason.

The cost is a less impressive feature list.

## Documentation describes the present

No document, comment or shipped configuration file records history: not what something was, not
which bug a line fixed, not how a lesson was learned.

Write the constraint, meaning what the code does and what breaks if it changes, and leave the
changelog to git. A reader has the file in front of them and needs to know what is true and what
they must not break; the story of how it came to be is in the commit that made it so.

```c
/* WRONG — narrates a past defect */
/* This read the buffer size before the offset, which overflowed on a full
 * buffer and reported EOF. Fixed by clamping first. */

/* RIGHT — states the rule and its consequence */
/* Clamp before the read: a full buffer would otherwise ask for a
 * zero-length read and take the result for EOF. */
```

Both comments carry the same warning. Only the second is still true in five years, and only the
second survives the surrounding code being rewritten.

`testing/docscheck.sh` checks the mechanical half of this rule across the book. It reports dead
relative links, common phrases that narrate the past, pages under `docs/kdos/` that lack a title or
a `See also` section, and, when run over the whole book, pages that the table of contents does not
reach. It cannot recognise a paragraph that tells a story in its own words.

## Every change updates its documentation, in the same change

A change in behaviour updates every page under `docs/kdos/`, every code comment and every shipped
configuration file that describes that behaviour, before the change is complete, and not in a
follow-up.

The update replaces the old description. It does not append to it, annotate it, or record what the
description was. The file is rewritten to describe what is true, and the difference lives in the
commit.

A comment that contradicts its code is worse than no comment, because it is a claim the next reader
will act on. A description that is too optimistic sends them past a bug; one that is too
pessimistic makes them verify again something that already works. Both cost more than the minute
the update would have taken.

The price is that no change is ever only a code change.

## See also

- [Why KDOS](why-kdos.md) — the four properties these rules serve, and the full inventory of what
  is not built from source
- [Decisions](decisions.md) — the close choices, the alternatives and what each costs
- [How KDOS differs](how-kdos-differs.md) — these rules set beside the choices other
  distributions make
- [How KDOS is built](../05-developer/how-kdos-is-built.md) — the offline, from-source build told
  from `git clone` to a bootable ISO
- [The ports catalogue](../06-reference/ports-catalogue.md) — every recipe the host is built from,
  by phase and group
- [The design language](../03-architecture/design-language.md) — these principles applied to what
  you see
- [The security model](../03-architecture/security-model.md) — the enforcement behind the box
  profile, including what is not protected
- [Packaging](../03-architecture/packaging.md) — reproducible packages, the binhost and deltas
- [Writing ports](../05-developer/writing-ports.md) — the recipe rules that follow from building
  offline and from source
- [Glossary](../06-reference/glossary.md) — the terms this book uses

<!-- book-nav -->
---

*Part I — Introduction, chapter 3.* Previous: [2. How KDOS differs](how-kdos-differs.md) · [Contents](../README.md) · Next: [4. Decisions](decisions.md)
