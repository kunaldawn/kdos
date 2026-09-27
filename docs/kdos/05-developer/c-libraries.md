# The C libraries

KDOS's own C programs are built on seventeen libraries under `src/libs/`, each named `libk*`; almost
every one of those programs compiles some of them in, and the few exceptions are listed in [How the
libraries are built](#how-the-libraries-are-built). This chapter describes them: how they are
compiled into their programs, the rule about external dependencies that shapes the whole set, what
each library owns, which may call which, and the rules each one exists to enforce. It is written for
developers who change or extend KDOS's own C code. If you are building a new window, popup or
settings page, read [Writing desktop software](writing-desktop-software.md) first; it covers the
drawing libraries from the point of view of someone building a *surface* (one window or popup drawn
by KDOS; see the [glossary](../06-reference/glossary.md)) and sends you back here for the detail.
The libraries' tests are described in [Testing](testing.md).

After reading this chapter you should be able to find the library that owns a piece of behaviour,
change it without breaking the program that can least afford a new dependency, and add a library
of your own.

## How the libraries are built

A `libk*` library is a directory of C sources with one public header, and sometimes private
headers of its own (`kcell_priv.h`, `kicon_int.h`, `kwl_priv.h`, and several in `libkvt`). It is
not an installed archive or a shared object. Each program that uses a library compiles that
library's `.c` files straight into its own binary, from its own `build.sh`, and names the external
libraries it needs on its own link line. `kinstall`'s recipe, for example, compiles
`src/libs/libkbase/*.c`, `src/libs/libktui/*.c` and `src/libs/libkcolor/*.c` together with its
own sources.

`kdos-comp` differs only in shape. Meson takes extra objects through the environment, so its
`build.sh` compiles `libkbase`, `libkcolor` and `libkwm` into a local static archive, `libkdos.a`,
and passes that in `LDFLAGS`.

A few programs use a library's header without compiling any of its sources. `libkcolor`'s palette
is an X-macro (a macro that expands a table once per consumer at compile time; see
[libkcolor](#libkcolor)), so `kdos-splash` and `xdg-desktop-portal-kdos` include `kcolor.h` for
the colour table and link none of `libkcolor`'s code. Three programs use no `libk*` code or header
at all: `kdos-bb`, `kdos-boxsock` and `kdos-record`.

Two consequences follow from compiling the sources into each program:

- Nothing is installed under `/usr/lib` for these libraries, and nothing outside this tree can link
  them.
- A program's recipe has no upstream source, so the package manager's recipe hash for it covers
  the whole of `src/libs/` as well as the program's own directory (`kp_hash.c` in `libkpkg`). The
  package manager cannot tell which libraries a `build.sh` compiles without parsing shell, so
  editing any library rebuilds every KDOS program on the next build, not only the ones that use
  it. See [Developing](developing.md) for the narrow rebuild commands.

Some consumers are compiled outside a recipe, by a script that names its libraries explicitly:

| Script | Builds | Libraries |
|---|---|---|
| `script/01_phase1/12_kpkg.sh` | `kpkg`, the package manager, into the phase-1 sysroot | `libkbase`, `libkpkg`, `libksig` |
| `script/01_phase1/13_kinstall.sh` | `kinstall`, the installer, into the phase-1 sysroot | `libkbase`, `libktui`, `libkcolor` |
| `script/kdosbuild.sh` | `kdosbuild`, the build screen, on the build host | `libkbase`, `libkbuild`, `libktui`, `libkcolor` |
| `src_kpkg_ensure` in `ports/srclib.sh` | the host recipe reader `ports/fetch` and `ports/publish` use | `libkbase`, `libkpkg`, `libksig` |
| `ports/update` | `kdos-portup`, the upstream version checker | `libkbase`, `libkpkg`, `libkbuild` |
| `testing/selftest.sh` | `src/libs/selftest.c` and a compile of every consumer | all seventeen |

## The constraint

The libraries a terminal program needs link nothing but the C library.

The reason is phase 1 of the build (see [The build system](build-system.md), and [How KDOS is
built](how-kdos-is-built.md#phase-1-a-minimal-kdos) for the build told end to end). The installer,
`kinstall`, and the package manager, `kpkg`, are both cross-compiled there against musl and the
kernel headers alone, so that they exist on every tree from the first bootable image onward.
`kinstall` uses `libkbase`, `libktui` and `libkcolor`; `kpkg` uses `libkbase`, `libkpkg` and
`libksig`. If any of those five ever needed a real `-l` flag, the program using it would have to
move to a later phase, and the first bootable image would lose its installer or its package manager.

Twelve libraries keep the rule outright: `libkbase`, `libkcolor`, `libktui`, `libkxdg`, `libkpkg`,
`libksig`, `libkbuild`, `libkproc`, `libkpack`, `libkvt`, `libkwm` and `libkdisp`. None of them
links the maths library either. `libksig` carries its cryptography as vendored source rather than
linking it, and `libkproc` opens NVIDIA's management library with `dlopen` at run time, only when
one is installed, so neither adds a link flag. The root daemons depend on the same property: every
library a daemon links is code running as root, and these twelve link no external library, so the
only code a daemon runs as root is source in this tree, including the one vendored copy in
`libksig`.

The other five draw pixels, and pixels need real libraries, each of them a port of its own (see
[The ports catalogue](../06-reference/ports-catalogue.md)). Each of the five is a separate
directory so that the twelve above stay clean, and so that a program pays only for what it draws:

| Library | External libraries it needs |
|---|---|
| `libkcell` | fcft (font rasterising) and pixman |
| `libkicon` | pixman and libpng |
| `libkimg` | pixman, plus libpng, libjpeg, libwebp, libsixel and libnsgif, each optional and switched on by a `KIMG_HAVE_*` define |
| `libkwl` | wayland-client, xkbcommon, fontconfig, fcft and pixman |
| `libkchrome` | pixman directly (the pixel tile), plus everything `libkcell`, `libkicon` and `libkwl` need, since it is built on them |

`libkcell` and `libkwl` are split for the same reason one level down: a program that wants the
cell painter is not made to link a Wayland client library to get it.

## The set

The header column names the library's one public header, which is what a consumer includes. The
prefix column is the one every exported symbol of that library carries (a second prefix marked
"internal" is linker-visible but declared only in a private header). The "Built on" column lists
the other `libk*` libraries it needs, either by calling them or by using a type or macro from
their header.

| Library | Header | Prefix | Owns | Built on | Used by |
|---|---|---|---|---|---|
| `libkbase` | `kbase.h` | `kb_` | Allocation and its failure hook, fatal and warning output, strings, files, paths, locking, monotonic time, SHA-256 and MD5, base64, JSON string escaping, `file://` URIs, per-user toggles and state paths, group membership and the root-daemon authorisation gate, the argument-vector builder and process helpers, desktop notifications, Landlock self-sandboxing, the fuzzy matcher, and the freedesktop trash | — | Every KDOS C program except `kdos-splash`, `kdos-bb`, `kdos-boxsock`, `kdos-record` and `xdg-desktop-portal-kdos`; also `kpkg`, `kdosbuild` and `kdos-portup` |
| `libkcolor` | `kcolor.h` | `kcol_` | The palette table, colour-space conversion, mixing, contrast, the readable muted colour, the hue-family classifier, remapping and retinting | `libkbase` | Everything that links `libktui`, plus `kdos-comp`, `kdos-theme`, `kdos-tools` and `kdos-powerd`; its header alone is used by `kdos-splash` and `xdg-desktop-portal-kdos` |
| `libktui` | `ktui.h` | `ktui_` | Terminal ownership, the cell buffer and its diff, key and mouse decoding, the touch-gesture recogniser, character width, paste and drop, immediate-mode widgets, the draw-and-key views, menus, modals, the keys contract (the keys every surface answers; see [Writing desktop software](writing-desktop-software.md#the-keys-contract)), the selection rule every surface draws its rows with, the three glyph tiers, charts, the sprite table, offscreen rendering, and announcements | `libkcolor` (its palette macro), `libkbase` | `kinstall`, `kdosbuild`, `kdos-appbox`, `kdos-shell`, `kdos-res`, `kdos-term`, `kdos-lock` |
| `libkxdg` | `kxdg.h` | `kxdg_` | Desktop entries, the MIME glob table, the one correct way to turn a command line into an argument vector, places, recent files and file verbs | `libkbase` | `kdos-shell`, `kdos-res`, `kdos-term`, `kdos-appbox`, `kdos-tools` |
| `libkpkg` | `kpkg.h` | `kp_` | Configuration, the package database, the ports tree, dependency parsing and solving, version comparison, the recipe and build-config hashes | `libkbase` | `kpkg` (also compiled on the build host as the recipe reader), `kdos-portup`, `kdos-pack`, `kdos-packd`, `kdos-tools` |
| `libksig` | `ksig.h` | `ksig_` | Ed25519 signing and verification, key files, keyrings; the one library with vendored third-party source | `libkbase` | `kpkg`, `kdos-pack`, `kdos-packd`, `kdos-tools` |
| `libkbuild` | `kbuild.h` | `kbuild_`, `kj_` | Phase discovery, the phase metadata block, the build plan, the snapshot inventory, a read-only JSON scanner | `libkbase` | `kdosbuild`, `kdos-portup` |
| `libkproc` | `kproc.h` | `kpr_` | Every reading about the running machine, from a movable root: processes, uptime, box identity, processor, memory and pressure, block devices, network, power, sensors, graphics, sound PCMs, and the sample ring | `libkbase` | `kdos-res`, `kdos-shell`, `kdos-tools`, `kdos-oomd`, `kdos-energyd` |
| `libkpack` | `kpack.h` | `kpk_` | The pack format: the footer, the metadata blob, the requirement solve, the payload hash, the signature block, and the index | `libkbase`, `libksig`, `libkpkg` | `kdos-pack`, `kdos-packd`, `kdos-tools` |
| `libkvt` | `kvt.h` | `kvt_`, `screen_` (internal) | The terminal: the VT100–VT520 state machine, the screen, scrollback, selection, the pty, and one render boundary that turns it all into cells. A hard fork of libtsm 4.7.1 | `libktui`, `libkbase` | `kdos-term` |
| `libkimg` | `kimg.h` | `kimg_` | The only place untrusted image bytes are decoded: two entry points, five optional decoders, and a budget enforced from the header the format declares before any allocation | — | `kdos-shell`, `kdos-term` |
| `libkwm` | `kwm.h` | `kwm_` | The window model the compositor obeys: placement, the tiled-state transition and its geometry, the neighbour-edge arithmetic, the nearest occupied workspace, and the drag threshold | — | `kdos-comp`, `kdos-shell` |
| `libkdisp` | `kdisp.h` | `kdisp_` | Which display server, decided once: the surface configuration, the seven roles, the lifecycle every surface asks for, the font list, and the window list a panel manages | `libktui` (its `KRect` type) | `kdos-shell`, `kdos-res`, `kdos-term`, `kdos-lock` |
| `libkchrome` | `kchrome.h` | `kch_` | The window furniture: the header band, group headings, the button bar, the list and scrollbar rule, the tone ladder for plates, the pixel display list and the pixel tile | `libktui`, `libkcolor`, `libkicon`, `libkcell`, `libkdisp`, `libkwl` | `kdos-shell`, `kdos-res` |
| `libkicon` | `kicon.h` | `kicon_`, `ki_` (internal) | A name or a file path becomes a sprite slot, or −1 | `libktui`, `libkcolor`, `libkxdg` | `kdos-shell`, `kdos-res` |
| `libkcell` | `kcell.h` | `kcell_` | The glyph cache and the cell painter (a grid of cells into a pixel buffer), the character ramp built from it, the pixel canvas, and the one scale-and-cut of a decoded picture into sprite tiles | `libktui` | `kdos-shell`, `kdos-res`, `kdos-term`, `kdos-lock` |
| `libkwl` | `kwl.h` | `kwl_` | The toolkit's Wayland backend: surface roles, buffers, scale, the font in force, input, touch, clipboard, compose, cursors, frame throttling | `libkcell`, `libkdisp`, `libktui` | `kdos-shell`, `kdos-res`, `kdos-term`, `kdos-lock` |

## Dependency direction

Each arrow reads "is built on". The graph has no cycles, and it must stay that way: nothing lower
down may call anything higher up. An edge means a call into the other library, or a type or macro
taken from its header.

```
libkchrome → libkicon, libkcell, libkdisp, libkwl, libktui, libkcolor
libkwl     → libkcell, libkdisp, libktui
libkicon   → libkxdg, libktui, libkcolor
libkcell   → libktui
libkdisp   → libktui
libkvt     → libktui, libkbase
libktui    → libkcolor, libkbase
libkcolor  → libkbase
libkxdg    → libkbase
libkpkg    → libkbase
libksig    → libkbase
libkbuild  → libkbase
libkproc   → libkbase
libkpack   → libksig, libkpkg, libkbase

libkwm     (nothing)
libkimg    (no libk* library)
```

Some edges carry a rule of their own:

- `libktui` takes the palette from `libkcolor`'s X-macro at compile time. That is why
  `script/01_phase1/13_kinstall.sh` puts `libkcolor` on the phase-1 command line: leaving it out
  builds on a development host, where `testing/selftest.sh` supplies it, and fails only in phase 1.
- `libkdisp` uses `libktui`'s `KRect` type and calls none of its functions, so adding it to a
  program costs a table of function pointers (`KDispImpl`, see [libkdisp](#libkdisp)) and a
  structure rather than a font renderer.
- `libkvt` is a terminal's private screen and reaches `libktui` only at its render boundary, for
  character width, the nearest theme slot and the sprite table.
- `libkimg` decodes untrusted bytes and calls no other `libk*` library, so a decoder bug cannot
  reach into the toolkit.
- `libkwm` calls nothing at all. That lets the compositor take it without taking anything else,
  and lets the self-test replay it against a fixture with no display.

## libkbase

`libkbase` is the base library: allocation, strings, files, paths, locking, time, hashing and
process helpers. Every other library except `libkwm`, `libkimg`, `libkcell`, `libkdisp`,
`libkicon`, `libkchrome` and `libkwl` calls into it directly.

A library does not own the exit path. `kb_calloc`, `kb_realloc` and `kb_strdup` never return
NULL; on failure they call whatever handler the program registered with `kb_set_oom_handler()`
(dropping the terminal, closing a log) rather than knowing that a terminal exists or what the
program is called. `kb_set_progname()` sets the prefix for the out-of-memory message and for
`kb_die()` and `kb_warn()`.

`kb_copy_file` streams. It is what moves application packs, which run to hundreds of megabytes,
and the pack daemon copies one into the store as root on every install. Reading a file whole to
write it whole would ask for its size in anonymous memory for no reason.
`kb_write_file_atomic` replaces a state file by temporary file, `fsync`, rename and a directory
`fsync`, for any file where an empty result is a loss rather than a retry.

### Processes and secrets

There is no `system()` and no `popen()` in these libraries, and no KDOS program may use either.
Application names, package names, device paths and file arguments reach this code from desktop
entries, `/sys` and the command line; a shell in the middle would make each of them an injection
point. Everything is executed through a `KbArgv`, built one element at a time, by `kb_run` and its
variants.

The process helpers send a child's error output to `/dev/null` unless `kb_proc_verbose` is set, so
anything whose failure is diagnosed by the child's own message has to turn that on.

A secret reaches a child on a descriptor and never in `argv`, because `/proc/<pid>/cmdline` is
world-readable for the life of the process. `kb_run_feed` writes to the child's standard input and
sends its output to `/dev/null`, which suits a checker such as `kdos-checkpass`; `kb_run_feed_tty`
leaves the child the caller's output, which a pager needs. `kb_run_feed_env` additionally sets
names in the child's environment and captures its standard error, which is what a mount helper
needs: `mount.cifs` reads a password from the descriptor `$PASSWD_FD` names and reports its
refusal on standard error, and a caller without both would either put the secret in an argument
list or report a status with no reason. In the forms that feed and read back, the input must fit in
one pipe buffer, because nothing is read until the whole input has been written.

`kb_child_reset_signals()` belongs between `fork` and `exec` in any program that ignores a signal.
The terminal backend in `libktui` and `libkwl` both set `SIGPIPE` to `SIG_IGN`, an ignored
disposition survives `execve`, and a shell started without the reset never ends a pipeline:
`yes | head` runs until something else stops it.

### The argument-vector builder

A `KbArgv` holds at most `KB_MAX_ARGV` (256) pointers, including the terminating NULL, and
`kb_die()`s rather than truncating. `kb_argv_add()` stores the pointer it is given and does not
copy. Several arguments formatted one after another into a single reused buffer therefore all
point at the same bytes, and the child receives the last value repeatedly. Use `kb_argv_addf()`,
which measures the formatted result, allocates exactly that much and stores the copy; the copy
lives until the process exits. This is a property of the builder, not of any one caller.

### Hashing

Two hashes live here, for two different jobs. `kb_sha256_*` checks the `sha256 =` line of a recipe
before an archive is unpacked. It lives in the base library because `libkpkg`, `libksig`,
`libkpack` and the programs above them all hash, and none of them may link a crypto library. It
proves the bytes are the ones the recipe named and says nothing about who named them; signatures
are `libksig`'s job. `kb_md5_*` names thumbnail cache files, because the freedesktop thumbnail
standard names them by the MD5 of the source URI and a stronger hash would produce a cache no other
program could share. It is a file name, never a security check.

### Smaller helpers

- `kb_toggle_on()` and `kb_toggle_set()` read and set a per-user on/off flag, which is an empty file
  under `kdos/toggles/` in the state directory. They are the only two places the path is spelled, so
  a program cannot write a toggle where nothing reads it. `kb_state_path()` builds a path under
  `$XDG_STATE_HOME` (or `~/.local/state`) on every call, and returns 0 when there is no home.
- `kb_json_str()` is the escaper every `--json` output goes through.
- `kb_notify()` raises a desktop notification through `gdbus`, detached and best effort, so a
  program that emitted a notification escape is never blocked by it. Nothing here links a D-Bus
  library.
- `kb_terminal()` names the terminal emulator to open a terminal program in, and answers NULL where
  there is no session (a bare virtual terminal, a serial console, an ssh login).
- `kb_desktop_prefix()` gives the lowercased first name in `$XDG_CURRENT_DESKTOP`, which is how
  `kdos-mimeapps.list` is found.
- `kb_in_vm()` reads the DMI vendor string. The idle timers and the lid policy default to off in a
  virtual machine.

The freedesktop trash lives here too (`kb_trash_put`, `kb_trash_list`, `kb_trash_restore` and
their neighbours), so `kdos trash` at a prompt and the desktop's delete key are one
implementation. See [The kdos command](../04-programs/kdos-command.md#kdos-trash).

### Landlock

`kb_landlock_*` is unprivileged self-sandboxing through the kernel's Landlock interface: three
system calls and no library. A ruleset starts with nothing reachable, each `kb_landlock_allow()`
opens one directory tree back up (read-only or read-write), `kb_landlock_allow_tcp()` opens one TCP
port, and `kb_landlock_enforce()` makes the result permanent for the process and everything it
starts. The order is always new, allow, enforce, exec. `kdos sandbox` is built on it; see
[The kdos command](../04-programs/kdos-command.md#kdos-sandbox).

What the running kernel can police depends on its Landlock ABI version. `kb_landlock_new()` with
`net_off` also denies TCP bind and connect, which needs ABI 4; `kb_landlock_new_scoped()` with
`ipc_off` additionally scopes the abstract Unix socket namespace and same-uid signals to the
sandbox, which needs ABI 6. Below those versions the request is silently not applied, so a caller
that must not run without it checks `kb_landlock_abi()` first and refuses. An ABI of `-EOPNOTSUPP`
means Landlock is compiled into the kernel but not enabled in `CONFIG_LSM` or `lsm=`. That is the
quiet failure to report: without the check, everything runs with no sandbox and nothing says so.

### The root-daemon gate

`kb_uid_allowed(uid, group)` is the whole authorisation boundary of the five root daemons
(`kdos-mountd`, `kdos-packd`, `kdos-powerd`, `kdos-oomd` and `kdos-energyd`): is this uid root, or
a member of that group. Each daemon reads the peer's uid from `SO_PEERCRED`, which the kernel fills
in and the peer cannot forge, and asks this one function before acting as root on the peer's
behalf. The sockets themselves are world-writable, and an unauthorised peer is answered with a
refusal and a closed connection; a socket mode that looked like the authorisation would be a mode
somebody eventually loosens.

It is one function because five copies are a rule that gets tightened on one socket and stays
loose on the other four, with nothing in the tree to show which is which. A daemon that needs a
different rule states the difference beside its own call and still asks this for the rest.
`kdos-mountd`'s `--fixture-serve` mode is the only one that does, and it grants nothing, because
its paths are a scratch directory and every child it would spawn is printed instead of run.

A uid with no passwd entry is refused. Membership itself is `kb_user_in_group`, which reads
`/etc/group` whole rather than through `getgrnam()` and counts the group's own gid as well as the
member list. A user whose primary group is `wheel` never appears in that list, and that is the
account an installer creates.

### kb_fuzzy

`kb_fuzzy` is the desktop's only answer to "does this row match what was typed". It matches a
subsequence rather than a substring, so `sm` finds `System Monitor`, which no substring search can.
Matches are scored so that a prefix beats an acronym, an acronym beats a run and a run beats a
scatter. Higher is better and zero is no match; an empty needle matches everything with the same
small score, because a list filtered as somebody types starts with nothing typed.

Each needle character prefers a word start over an earlier occurrence inside a word, which is what
makes the acronym score reachable. That preference is greedy and can consume a character the rest
of the needle still needs, so a preferring pass that fails is redone taking the leftmost occurrence
of each character. Any needle that is a subsequence of the row therefore matches, and a row never
loses to a prefix of its own name.

It lives here rather than in a surface because several surfaces search the same things. The
command palette, Find and the Start menu search applications through `kdos-shell`'s one
application matcher, which ranks by `kb_fuzzy`; the palette ranks windows, routes and files with
it; and the keybind card filters its rows with it. One query ranked three ways teaches a person
that the desktop's search cannot be relied on.

A caller that sorts on it must sort descending. A sort written for a lower-is-better matcher ranks
a correct list backwards, which reads as bad ranking rather than as a bug.

## libkcolor

`libkcolor` holds the one palette table for the whole distribution and the only copy of the colour
arithmetic on it. The `kdos` command, the installer, the theme generators and the bootloader
configuration all take their numbers from here, so the boot menu, the installer and the desktop
cannot disagree about what an accent is.

The palette is the `KCOL_SCHEMES` X-macro in `kcolor.h`: eight schemes (phosphor, amber, ice,
bone, norton, borland, perfect and paper, with bone the default), each nine colours in the order
primary, dim, secondary, urgent, deep, text, variant, pdark and backdrop. Every consumer expands the
same literal values into its own table at compile time. `libktui` projects them onto its eight
colour slots; the theme generators expand them into stylesheets, vector artwork and cursor images.
Nobody keeps a second copy of the numbers. A scheme must clear two contrast floors, which the
self-test asserts for every scheme: 7:1 between `text` and `deep`, and 4.5:1 between the accent and
`deep`.

The HLS conversions (`kcol_to_hls`, `kcol_from_hls`) reproduce Python's `colorsys` exactly,
including its unusual `2.0 - maxc - minc` intermediate, its modulo on a negative hue, and Python's
round-half-to-even rounding. The vendored icon and cursor artwork is committed, so a generator that
rounded differently would produce a diff against files already in version control. Do not reorder
the arithmetic in `kcolor.c`. The self-test asserts that every scheme colour survives an HLS round
trip unchanged.

The library stays off the maths library for the same reason the toolkit does: a phase-1 consumer
cannot link one. The modulo is done in a loop, the rounding by hand, and the sRGB transfer function
behind `kcol_contrast` is a 256-entry table rather than a call to `pow`.

The two mixing functions are not interchangeable. `kcol_mix` takes an integer percentage and
integer arithmetic; `kcol_mixf` takes a fraction and rounds the way Python does. They disagree by a
unit on some inputs, and each generated file was written against exactly one of them. The self-test
asserts that they disagree, so that nobody unifies them.

Two derived colours exist, and confusing them is a legibility defect. The dim value is a fill and
measures below any text floor against the background; the muted colour from `kcol_muted` is the
readable one, and every text role goes through it. See
[the design language](../03-architecture/design-language.md#colour).

## libktui

The toolkit: terminal ownership, the cell buffer, the diff, input decoding, widgets, charts and the
sprite table. It links nothing but the C library, which is what lets the installer use it in
phase 1.

Several sections below refer to a *guest*: another program's graphical output shown inside a
surface's cells, an embedded client whose pixels the surface displays and whose input it
forwards.

### Capabilities and the frame bracket

Three glyph tiers are chosen from the terminal's capabilities, because the console font KDOS ships
holds 512 glyphs and a character it lacks renders as a blank. The table is in
[the design language](../03-architecture/design-language.md#the-glyph-tiers).

What a terminal can do is asked once, in one write, on entering the screen. Two facts cannot be
learned from a `TERM` value or a capability database entry: the kitty keyboard flags (`CSI ? u`),
which is what makes `Super` arrive at all, and synchronized output (`CSI ? 2026 $ p`), which is
what lets a frame be bracketed. Both queries go out together and their replies are told apart by
scanning the buffer. Asking in turn would pay the timeout twice on a terminal that answers neither,
and the second wait would take a slow answer to the first query as its own. A terminal that
reports kitty flags is asked for flag 1 only (`CSI > 1 u`, disambiguate escape codes). A DECRQM
value of `0` means "not recognised" and `4` means "permanently reset", so only `1`, `2` and `3` set
`KT_CAP_SYNC`. The replies are consumed there, or they would be typed into the desktop as stray
characters. The Linux VT answers neither and is skipped rather than waited on.

A frame is bracketed where that bit is set: `CSI ? 2026 h` before the diff and `CSI ? 2026 l`
after it. A diff frame is a scatter of cursor moves and single cells, and a terminal that draws
them as they arrive shows a menu before the one under it is erased. The close is written again when
a bounded flush dropped the frame, and once more when the terminal is handed back, because a
terminal left inside the block draws nothing further: an unclosed bracket freezes the screen.

`ktui_progress()` draws a solid whole-cell bar, and its output must not change: `kinstall` and the
on-screen display in `kdos-shell` call it. `ktui_progress_ex()` is the general form, with the
`KT_BAR_SOLID`, `KT_BAR_TIP` and `KT_BAR_SEGMENTED` styles and the `KT_BAR_PULSE` flag; the build
screen uses it. Change the general form freely and leave the plain wrapper's output alone.

### Resizing, the caret and the pointer

A resize is not applied until the consumer applies it. The backend sets `ktui_resized`; the cell
buffer follows only when the loop calls `ktui_draw_resize()` and `ktui_draw_invalidate()`. Any
loop that owns a surface owns this. A surface that was always a fixed size and starts being resized
will draw against stale dimensions and silently fail its own bounds checks.

The caret goes to the backend when the backend has one. `ktui_term_caret()` is the one call a
surface makes to say where it is typing, and it writes the terminal escape only when nothing else
claims the answer. A client drawing through a display server claims it: its standard output is not
the screen it appears on, and the position it knows is in its own cells. A backend that leaves the
`caret` entry NULL keeps the escape. The Wayland backend leaves it NULL, because a compositor's
surfaces draw their own caret.

The pointer follows the same rule for the same reason. `KtuiBackend.pointer` is handed the cell the
pointer is on and answers whether it drew a pointer itself; a backend that composites an arrow into
a framebuffer it owns says so. Everything else leaves the entry NULL, and `ktui_draw_flush()`
reverses the cell under the pointer, which is the pointer a `--tty` run, a `--dump` and `tty1` can
show. An arrow cannot be drawn in this library: it links nothing but musl and has to keep doing so,
and an arrow needs a pixel buffer and a colour in it. The hook is called on every flush, with a
negative `x` for no pointer, because that call is the only thing that tells a backend to take the
last arrow off the screen. Over a cell marked `KT_A_GUEST` the flush reports no pointer to either
path: the guest's own compositor has drawn a cursor into those pixels, and a second one a cell away
would not be the one anybody is aiming with.

Offscreen rendering takes a fixed size and writes the cell buffer out as plain text, with no
terminal at all. A geometry defect in this toolkit is invisible to the compiler and to a test suite
that cannot draw, so this is how one gets looked at; the committed reference frames under
`testing/goldens/` are made this way.

### Widgets

A widget is either an immediate-mode call or a draw-and-key pair, and its callers decide which.
`ktui_list`, the buttons, the checks and the input field read the frame's focus and return what
happened in one call, for a surface built around the frame. The four views in `ktui_view.c` (the
page strip, the column table, the dropdown and the text block) and the menu split drawing from input
instead: the strip, the table and the dropdown each have a `_draw`, a `_key` and a `_hit`, the text
block has `ktui_textarea_draw` and `ktui_textarea_key`, and the menu has `ktui_menu_draw` and
`ktui_menu_event`. They are split because every surface that wanted them runs its own event loop and
holds its own selection. An immediate-mode form would have meant rebuilding each of those surfaces
around the frame. A hit test takes the same rectangle its draw took, so it measures what is on the
screen rather than what the widget remembered from an earlier size.

No widget owns its selection. The caller holds it, because the caller is what persists it, dumps
it and restores it, and because a page strip and the body under it are one selection seen twice.

A table's heading row says whether the selection may land on it. Both readings exist in the tree:
a network device heading is the row `Enter` rescans from, and a device-section caption is
furniture. The row-kind callback answers per row rather than the widget choosing for both, and a
row the selection steps over never lights.

Three rules hold about the library's own state, each guarding a link or focus failure:

- Symbols are prefixed. Generic names collide with a consumer's own definitions of the same names
  with different semantics, which is enough to make two KDOS programs unlinkable together.
- The frame state is private, behind accessors. A public structure that applications assign to
  field by field is a second interface nobody can change.
- Chrome identifiers are the library's business. Chrome registers with caller-local identifiers in
  a reserved range that never joins the focus ring and never drags the page scroll.

### The sprite table

A *sprite* is a picture occupying whole cells: the table holds an opaque pointer per slot, and each
cell carries the slot and its sub-cell position inside its codepoint. The table has
`KTUI_MAX_SPRITES` (4096) slots, and `ktui_sprite_budget()` adds a byte budget on top for pictures
larger than icons.

A sprite table entry is a borrowed pointer, so eviction is whatever the owner told the table to do.
The table does no pixel work and cannot free a picture. An owner registers an evictor with
`ktui_sprite_evictor()`, and the table calls it whenever the table itself stops naming a picture: a
slot taken back under the byte budget, a slot reused for a different picture under the same key, or
a tile refused part-way through a tiled put.

`ktui_sprite_drop` is the one way a picture stops being named without the evictor. Dropping by key
is the owner's own call, and a callback there would be a free from inside that call, so the table
hands nothing back; the owner releases what it dropped and clears whatever it keeps beside the
slot.

Without an evictor a full table answers −1, which every consumer already handles by drawing its
glyph. That is right for icons, which are owned for the life of the session, and wrong for
photographs, which are megabytes each.

A refused put is a hole, and only the owner can see it. `ktui_sprite_put` answers −1 when the
budget cannot be made to fit: eviction skips every slot the cell grids still reference, so a table
whose pictures are all on screen has nothing it may take. A caller that stores that −1 as a slot
draws the background where a picture belongs, and the table can never repair it because it never
learned the picture existed. A consumer holding pictures that are not its own has to tell the side
that owns them.

There is one evictor per process, and every owner in it shares it. A program that draws
photographs and icons has a single function handing back pictures built by two different
libraries, so no owner may assume the evictor is its own free. A picture given to the table must
free its own pixels on the last unref (a pixman image carries a destroy function for that), and its
owner must hold a reference of its own across the handoff, or an eviction leaves that owner's cache
naming a freed image.

Whether a sprite is still on screen is asked of the cell buffer's own size, not of `ktui_w` and
`ktui_h`. A backend reports a new size the moment it is resized, and the buffer is reallocated only
when the consumer calls the resize function, so between those two points the globals describe a
grid larger than the allocation.

### Colour

One rule governs reducing a colour that came from outside the palette. A terminal's SGR colour and
a picture's average tint both land on the nearest of the theme's slots by squared distance, through
one function, `ktui_theme_nearest()`. A second implementation would drift, and a table saying "red
means the error slot" would be a second set of colour decisions beside the palette that stopped
following the accent, so `kdos theme amber` would move some colours and not others.

Night light is a transform over the slots, not a scheme. `ktui_theme_night()` warms whatever
`ktui_theme_set()` last loaded, scaling green to 93% and blue to 77% and leaving red untouched. That
is the shape of a lower colour temperature, and it is why a warmed accent still reads as itself.
It hands out a copy and keeps the chosen scheme beside it: eight-bit values do not divide back
exactly, so turning night light off returns to the table rather than undoing the arithmetic. The
caller reads the toggle, because this library holds no opinion about where a desktop keeps its
state, and the call returns whether the palette actually changed, so a caller can skip a repaint it
does not owe. The warmed copy is one buffer rewritten in place, so its address stays the same when
the scheme under it changes. Anything that caches work per palette (the terminal backend's SGR
table is one) compares the eight slot colours, never the pointer.

### Announcements

A widget states what it is, because it already knows. Every widget computes which item has focus
each frame from the same identifier its hit test uses, so a screen reader working that out again
from a grid of cells would be guessing at what the surface has in hand, and it would guess wrong
first on the controls that matter most: which cell of a table, which tab of a strip, which item of
how many. `ktui_announce()` takes a role, a label, a value and the item's position in its set, so
"3 of 9" is a fact the widget states rather than a count somebody has to make.

- It lives here, not in a surface. A record composed per surface would be composed once per
  surface; one set in this library is set once and every consumer reads it.
- The queue is per frame, fixed in size, and cleared at the start of every frame. Nothing on the
  draw path allocates, because a widget that allocated to say its own name would drop frames over
  a slow link, and a frame with more to say than the queue holds drops the rest.
- Silence is the failure mode, never a stale name. A widget that says nothing announces nothing; a
  reader told the wrong control is worse off than one told nothing, and last frame's record is the
  wrong control by default.
- A widget says what it knows and no more. A tab strip has its names and says them, and so does a
  menu, which holds its own rows; a list and a table take their rows from the caller's callback, so
  they state the position and leave the name to a surface that has it. A secret field announces
  that it is one and never its contents.
- A position counts what a caret can reach. A menu's separators and its rows hidden by the scope
  rules are drawn and cannot be selected, so neither the ordinal nor the total counts them: a
  person hearing "3 of 4" can count to the same row.
- A repeated record is the same control. Dropping the repeat is the reader's job; the widget's job
  is to be right every frame.

What a screen reader does with these records is described in
[Accessibility](../02-user-guide/accessibility.md).

### The cell's attribute byte

A `KtuiCell` holds a codepoint, a foreground and a background slot, a sixteen-bit attribute and
three literal colours (`fgc`, `bgc`, `ulc`). The low six bits of the attribute are styles: bold,
reverse, underline, italic, strikethrough and overline, one bit each in the byte the cell already
had, so a terminal's own text reaches a KDOS surface without the per-cell data growing for it. All
of them but reverse are dropped on a real VT, where an attribute bit selects a font page or a
colour the palette does not own rather than a style. The seventh bit, `KT_A_GUEST`, is not a
style: it says the cell holds an embedded guest's pixels with the guest's own cursor already
composited into them, and the flush is its only reader. It is set on a guest's content cells only,
so the pointer returns at the window border, where a window is grabbed, moved and resized.

Above the eighth bit nothing is part of the portable attribute. The low byte is what a consumer
drawing in slots alone honours. The high byte says which of the three literal colours mean anything
(`KT_A_FGRGB`, `KT_A_BGRGB`, `KT_A_ULCOLOR`) and, in three bits, what shape the underline is (plain,
single, double, curly, dotted or dashed, in SGR `4:0` to `4:5` order). A literal reaches a cell from
three places: the terminal's render boundary, the theme picker's swatches, and the toolkit's own
compositing calls `ktui_draw_blend()` and `ktui_draw_shadow()`, which mix the colours under a
translucent plate (a filled shape the pixel layer paints under the cells; see
[libkchrome](#libkchrome)) or a shadow into literals and leave every cell a slot to fall back on. A
consumer reading slots alone therefore cannot receive a cell claiming a colour it never got and draw
the black it was never given. There is one bit per colour rather than one for the pair: a program
that sets a foreground and leaves the background alone is the common case, and a single bit would
freeze the theme's background into the cell as a literal, after which a retint leaves a rectangle of
the old scheme behind.

A caller's own literal reaches a frame only through `ktui_draw_put()`, which copies whole cells;
terminal content is what uses it, since the two places that copy a terminal's grid into a frame
copy whole cells. `ktui_draw_blend()` and `ktui_draw_shadow()` compute their literals by mixing
colours already in the frame (for a blend, the buffer `ktui_draw_bg_take()` filled) rather than
taking a colour of the caller's choosing. Every other draw call takes slots and clears
the literals, because chrome that stopped following `kdos theme` would be a second palette nobody
can change.

### Two input queues

A guest, as defined at the start of this section, receives its input through the program hosting it.
The *session* is that program, which decides which input it keeps for itself (a desktop chord, for
example) and which it passes on; a *grabbed* guest is one that currently receives all pointer input.
The raw queue exists for that forwarding: a guest needs real key codes and pointer distances, not
characters and cells.

The same physical input travels twice. `poll_event` answers a character and a cell.
`KtuiBackend.poll_raw` answers a `KtuiRaw`: an evdev keycode with a separate press and release, the
xkb modifier mask and group, a pointer position in the backend's own pixels together with the cell
those pixels were measured in and both deltas the device reported, and a scroll with a second axis,
a `value120` and its source. `KtuiBackend.keymap` hands over the backend's compiled layout as xkb
text with a generation counter. A backend with no input device of its own leaves both NULL, which
is how a consumer knows not to claim the capability at all.

There are two queues because motion coalesces and a key must not. A thousand-hertz mouse in the
cooked queue evicts the oldest entry there, which can be the click that came before it. In the raw
queue consecutive bare motions merge into one at the newest position with their deltas summed (a
delta is a distance, and dropping one shortens the movement a grabbed guest sees), and nothing else
merges at all.

The cooked event for one physical input is queued before its raw partner, and `KtuiRaw.after` is
how a caller knows which belongs to which. It counts the cooked events that must be taken before
that raw one is sent, so a caller sends cooked events up to that number and only then the raw one;
the field carries the order, not the order in which the two queues were filled. Draining one queue
to empty and then the other loses it: two keys inside one poll arrive as two cooked messages and
then two raw ones, and a session that swallowed the first key's chord has no way left to say which
press it swallowed. A handler may queue a key switch before the character it resolves to, so a raw
event's number is raised by any cooked event queued before the caller drains it. The number is only
ever raised, because a raw event delivered early is a chord the session ate and the guest also saw.

## libkwm

`libkwm` holds the window model and nothing else. Placement, tiling and the workspace walk live here
and nowhere else, out of the compositor that obeys them, so that every one of them can be asserted
against a fixture with no display anywhere.

`kdos-comp` calls eight entry points: `kwm_place`, `kwm_tile_geom`, `kwm_tile_next`,
`kwm_ws_adjacent`, and four pieces of the neighbour-edge search. Of that search only the arithmetic
is shared (`kwm_clip_add`, `kwm_clip_sub`, `kwm_edge_best` and `kwm_edge_check`, the first three
through thin inline wrappers in `kdos-comp`'s `include/edges.h` because they sit on the pointer's
motion path), while the walk that finds the candidate edges is the compositor's, across its scene
graph. The desktop icons in `kdos-shell` call a ninth, `kwm_drag_threshold`, which starts a drag
once the pointer leaves the cell it went down in; the threshold is in cells because every other
geometry in this model is.

Every entry point a consumer is meant to call has a caller in a shipped program.
`kwm_edge_between`, the between-test `kwm_edge_check` applies to each candidate, is exported only
so the self-test can pin it. A rule kept in this library that nothing calls would be a second
answer to a question the compositor already answers, and the contract cannot arbitrate between two
copies when only one of them ships; a rule with no caller belongs in the one place that runs.

The library is handed rectangles and told what is being asked. What a window is, which output it is
on, whether a client accepted its size and whether it is maximised all stay with the caller. The
rectangle and border types match `libktui`'s `KRect` and `kdos-comp`'s `struct border` field for
field, and the `KWM_EDGE_*` bit values match the compositor's own edge enum, because the compositor
passes its values straight in; changing one side without the other is a silent geometry error.

The contract is `testing/fixtures/wm/geometry.txt`. Each section cites the source file and function
of `kdos-comp` its rows were derived from, and the self-test replays the file rather than asserting
anything of its own. Where the library and a row disagree, the row is right. Adding a case means
adding a row, under a section that cites where the behaviour lives.

The file pins three behaviours that each look like a defect and are not:

- A quarter snapped towards the edge it already occupies collapses to a half. The parallel
  component is then neither the inverse of the request nor absent, so no branch of the transition
  matches and the orthogonal component is discarded.
- The two halves of an axis come from different expressions, `(size + gap) / 2` and
  `(size - gap) / 2`, which puts a whole gap between two tiled windows rather than half a gap each.
  An odd dimension therefore gives the right or bottom half one extra pixel.
- Occupancy is an input, not a derivation. The compositor counts views that are not omnipresent;
  the panel counts windows that are not minimised, because the workspace protocol reports active,
  urgent and hidden but has no state for "there is something here". Those are two rules with two
  right answers, and this library picks neither.

There is no maths library here, the constraint `libkcolor` and `kcell_ascii.c` are also written
under. The placement grid's interval search compares doubles, which is plain arithmetic and calls
nothing.

## libkdisp

`libkdisp` decides in one place which display server a surface reaches.

`kdos-shell` alone opens a surface from 54 call sites (`kdisp_init` calls, counted under
`src/desktop/kdos-shell/`), and each then asks whether it should close, resizes itself, or hides its
panel. Branching on the server at every one of those would be the same decision written 54 times
in one program and again in the next. The lifecycle is therefore an interface, `KDispImpl`, and a
display server is an implementation of it.

A surface asks for one of seven roles: `KDISP_ROLE_TOPLEVEL` (an ordinary window),
`KDISP_ROLE_NONE` (connect and bind but create no surface, which is what `--dump` runs under),
`KDISP_ROLE_PANEL` (anchored, with an exclusive zone), `KDISP_ROLE_OVERLAY` (layer shell, no
exclusive zone), `KDISP_ROLE_BACKGROUND` (the desktop itself, below every window),
`KDISP_ROLE_LOCK` (a session-lock surface the compositor keeps on screen even if the process
dies) and `KDISP_ROLE_SAVER` (the whole screen, above everything, taking no input).

The consumer decides what it links. This library names no implementation and pulls in none; a
caller hands over the ones it compiled, in preference order, so a program that links no display
server still compiles:

```c
extern const KDispImpl kwl_impl;   /* libkwl, the Wayland backend */
static const KDispImpl *const have[] = { &kwl_impl };
kdisp_init(&cfg, have, 1);
```

Each of `kdos-shell`, `kdos-res`, `kdos-term` and `kdos-lock` states that list once, as
`kdos_disp[]`, and it is the single line that changes when a second server is added. One
implementation ships, and the indirection is also the test seam: `testing/fixtures/shell/dumpmain.c`
replaces the `kdisp_*` entry points so that `kdos-shell` draws one frame into `libktui`'s offscreen
buffer, and `testing/fixtures/term/kwlstub.c` supplies an implementation whose probe answers no.
Both let the reference frames be rendered on a host with no fcft, pixman or Wayland installed.

### Fonts

The screen's font is a display's to answer, not a surface's to load. A surface draws cells and
something else turns them into pixels, so `kdisp_font_count`, `kdisp_font_at`,
`kdisp_font_current` and `kdisp_font_set` take an index into the display's list and never a name,
and no fontconfig syntax crosses to a display that has never seen it.

A display lists only faces it can use. A cell grid gives every glyph one advance, so `libkwl` lists
fontconfig's monospaced families (`FC_SPACING == FC_MONO`), one row per family, sorted, and nothing
else, because a proportional family offered here would be a screen of smeared columns.

A count of 0 is a display with no face to offer: a machine carrying no monospace family, or a run
with no display installed at all, where `libktui`'s terminal backend draws and the font is the
terminal's. A current index of −1 is not an error. It says the screen is wearing something no row
names, which is what an alias such as `monospace`, or a `chrome_font` naming a face this machine
does not have, both come to.

`keep` says whether a choice survives the logout, so a picker's arrows pass 0 and only its `Enter`
passes 1. Each surface is its own process with its own face, so `keep` is the only thing that
reaches the others: `libkwl` writes the family into `chrome_font` and `panel_font` in
`~/.config/kdos/comp.conf`, keeping each key's own size, and the other surfaces read it at their
next start. Switching face never changes the size, because a chrome that fell back to fontconfig's
default size would resize every window on the desktop.

A caller re-reads the list each turn, the same rule the window list keeps. `kdisp_font_ask()`
starts a gathering, and a backend that collects its faces over a socket answers some pumps later,
so a caller that believed the first answer would draw an empty list for ever. A backend whose list
is local leaves `font_ask` NULL; fontconfig answers `libkwl` in-process, so its first `font_count`
is already the whole list. An empty slot rather than a stub is deliberate: a slot that did nothing
would read as one that had started something.

A server that cannot answer an entry leaves it NULL, and the forwarding function returns the
neutral answer rather than crashing. A `--tty` run has no server-side decoration to report and
nothing to hand out in place of a Wayland handle. `kwl_display()` is not in the vtable for that
reason: it hands out the Wayland connection itself, for a program such as `kdos-shell` that binds
protocols of its own on it, and nothing else can stand in for one.

### Other programs' windows

Five vtable entries cover other programs' windows, and a caller has to ask for them. `win_count`,
`win_at`, `win_activate`, `win_close` and `win_set_state` are what a panel, a task switcher and a
window menu need: enough to draw a row and act on the one that was clicked. A caller reaches them
as `kdisp_win_count()`, `kdisp_win_at()`, `kdisp_win_activate()`, `kdisp_win_close()`, and
`kdisp_win_minimise()`, `kdisp_win_maximise()` and `kdisp_win_fullscreen()`, the last three all
through `win_set_state`. `kdisp_win_supported()` says whether the display offers a window list at
all, which is how a caller tells a desktop with no windows open from a display that cannot say.

`manage` in the configuration is a declaration, not a gate. A compositor hands the list and the
verbs to whatever binds `wlr-foreign-toplevel-management` and cannot tell one client from another,
so the only thing that keeps a launcher away from somebody's editor is that it never calls
`kdisp_win_*`.

An identifier crosses the interface, never a handle. A caller draws a list in one frame and acts on
a row in a later one, and a stale handle is a request to a destroyed proxy, which kills the
connection.

Announcement order is list order, and a row is offered only once it is settled. A panel draws task
N at position N, so a removal closes the gap rather than filling it from the end; an unordered swap
would move the last button into the middle of the bar. A handle whose property batch has not been
closed by `done` is left out of both the count and the walk, compacted rather than left as a hole,
because a hole would hide every settled window behind it.

The Wayland side binds foreign-toplevel management on first use, not at start-up. A compositor
announces every window to whoever binds it, and a terminal, a lock screen and a resource monitor all
link this library and want none of that traffic.

### Two edge vocabularies

`KDISP_EDGE_*` is a sequence naming which edge a panel is anchored to. `KWM_EDGE_*` is a bitmask
whose values match the compositor's own enum, so that corners are combinations. They answer
different questions and must not be conflated.

## libkxdg

`libkxdg` reads desktop entries and the MIME glob table, splits command lines, and holds places,
recent files and file verbs.

The splitter, `kxdg_exec_split`, is the single implementation of turning a desktop entry's command
into an argument vector. It unquotes, substitutes the file-argument codes, drops the codes that
carry no argument, and, given a negative count, keeps every code verbatim for a tool that rewrites
a line rather than running one. Its inverse, `kxdg_exec_quote`, re-quotes, so a generator's output
round-trips. Every launch path in the system goes through it. See
[kdos-appbox](../04-programs/kdos-appbox.md#exec-lines).

`kxdg_launch_read()` is the single reader of the keys that decide how to start something: `Exec`,
`Name`, `Terminal`, `X-KDOS-Term`, `X-KDOS-Size` and `X-KDOS-Float`. Its callers are `kdos-shell`'s
application index, its lookup of an entry by id, its desktop icons, its menus, its *Open With*
chooser and `kdos-appbox open`. Each would otherwise hold a private copy of that list, and a key
added to one of them would make a row behave differently depending on which surface it was clicked
from. It is the "can this entry be started at all" test as well, so a caller's check and its read
are one call.

`NoDisplay` and `Hidden` are not among those keys. They say whether an entry belongs in a menu,
which is a question for whoever draws one: the MIME route opens a `NoDisplay` entry on purpose,
and a shared reader that refused one would break every default handler that carries it.

The `Exec` line is copied with its field codes intact. The splitter spends them on the documents a
launch carries, and reads the same codes to decide that a line carrying none takes its documents
appended instead, so a reader that stripped them here would make every entry look like
`Exec=xterm` and pass a file in the wrong position.

### Typing an argument

`kxdg_mime_for_arg()` says what a command-line argument is. It is one implementation because two
openers answering that question separately get a URL wrong in two different ways.

An argument carrying a scheme is typed `x-scheme-handler/<scheme>`; `file:` names a path, so the
path is unwrapped and typed like any other; a name that `stat()`s is a path whatever it looks like.
Deciding on the basename instead would match `mailto:a@b.c` against the `*.C` glob,
case-insensitively, and resolve it to C++ source.

It returns the argument the caller must pass on, read-only and never a copy, so a long path cannot
be silently truncated on the way to the handler. The pointer is into the argument, except for a
`file:` URL naming no path at all, where it is a static `/`. Percent-escapes are not decoded, and
this is visible: `file:///home/kdos/My%20Report.pdf`, which is what a conforming caller emits for a
name with a space, resolves to a path that does not exist and opens nothing.

### Places, recents and verbs

`kxdg_places()` is one reader for the whole desktop. The desktop folder, the Places menu, the file
chooser's `Ctrl+P` list, *Add to Places* and `kdos places` at a prompt all resolve through it,
because two readers disagreeing about where a user directory is would put icons in the folder one
names and open an empty one beside it, which reads as a broken menu rather than as two readers.

There is no `xdg-user-dirs` on this system. KDOS seeds `user-dirs.dirs` from `/etc/skel` and it is
the user's to edit. `$HOME` is the only expansion the reader understands, because it is the only
one that file's format defines; a reader that guessed at the rest would be a shell.

A place that is not there is not offered. The user directories are created on demand, and every row
is checked before it is returned: a row that opens an error is worse than a row that is not
offered.

`kxdg_recent_add()` is called from one place, `kdos-appbox open`, which every open on this desktop
passes through, so a recent-files store can be kept without a second copy of the rule. It is the
same scanner run backwards: the bookmark this URI already had is cut out whole and a fresh one
appended, so a file opened twice is one entry and it is the newest. The writer keeps at most 60
bookmarks and drops the oldest on the same pass, because nothing else on this system prunes
`recently-used.xbel`. The rewrite is by temporary file and rename, because the store is shared
with every other program on the machine that keeps recent files, and every value is XML-escaped on
the way in and unescaped on the way out.

The read half memoises the parse on the store's size and modification time, and never the
existence check. Find rebuilds its rows on every keystroke, so the whole-file read and the backward
walk are worth keeping; the deleted-file filter is not cacheable, because nothing about the store
changes when a file is removed and a cached row would go on offering a destination that opens
nothing. A memo hit therefore re-runs `access()` over its rows, and keeps the rows that failed, so a
file that comes back returns to the list.

`kxdg_verb_*` is one table of what can be done to a file, read by the desktop's icons and the file
chooser. It holds ten verbs, in the order they are offered:

| Verb | Offered on | Program that must exist |
|---|---|---|
| Open | files and directories | `kdos-appbox` |
| Peek | files | `kdos-peek` |
| Edit | files | — |
| Open Terminal Here | files and directories | — |
| Find Here | directories | `kdos-find` |
| Add to Places | directories | — |
| Share | files and directories | `kdos-share` |
| Git Status Here | files and directories | `lazygit` |
| Extract Here | files | `kdos-openarchive` |
| Move to Trash | files and directories | — |

Separate tables would mean a verb landing on one surface and not the others, which reads as a
surface being incomplete rather than as two lists. A row whose program is absent from `$PATH` is
not offered, so a verb turns on when its program ships, with no edit to any caller. The resolution
is repeated at most once a second per verb, so a program installed under a running surface turns
its verb on within a second, while a menu redrawing its rows several times a frame does not walk
`$PATH` per row. Every verb builds an argument vector, never a command line. `mc`'s `F2` menu is a
text file, `/etc/skel/.config/mc/menu`, and cannot ask this table, so it carries only the verbs
whose programs exist, and `testing/preflight.sh` refuses one that does not.

## libkpkg

`libkpkg` holds the package manager's configuration, the package database, the ports tree, the
solver, version comparison and the two package hashes.

Version comparison (`kp_vercmp`) and the version-shape filter (`kp_vershape`) live here rather than
in either consumer, because the package manager and the upstream version checker ask the same
questions about the same strings and two implementations would eventually disagree. Both carry
`libkpkg`'s `kp_` prefix even though `kdos-portup` is the shape filter's only caller: a symbol
exported under a consumer's prefix is one the next reader looks for in the wrong library, and two
libraries that each define a generic name cannot be linked into one program.

The recipe hash of a port under `src/` with no `source =` covers its own directory and all of
`src/libs/`, as described under [How the libraries are built](#how-the-libraries-are-built). The
hashes and their three states are in
[Packaging](../03-architecture/packaging.md#deciding-what-to-rebuild).

## libksig

`libksig` signs and verifies the package index and packs with Ed25519, and it is the one library
with vendored third-party source.

The vendored implementation is Monocypher 4.0.3: four files of C99, dual-licensed CC0 and
BSD-2-Clause, with no dependencies, no maths library and no allocation, which is the rule this set
is built under. Each alternative fails that rule: libsodium is a shared library, and a large one;
BearSSL cannot sign with Ed25519; TweetNaCl has been unmaintained since 2014; and OpenSSL is the
opposite of linking nothing but the C library.

Monocypher sits in `src/libs/libksig/monocypher/`, verbatim and never edited, with its version,
source URL and checksum recorded in `UPSTREAM`. Everything above it (the file formats, the keyring
and the policy) is written for KDOS. The library holds three rules of its own: a key that arrived
with the thing it signs is never trusted, since the signature's key identifier only selects a key
from the local keyring; `ksig_keygen` fails rather than fall back when the kernel will not provide
entropy; and a secret key is held only for the duration of the call that uses it.

## libkbuild

The deciding half of the build orchestrator: phase discovery, the phase metadata block, the build
plan, the snapshot inventory, and `kj_*`, a JSON reader that refuses a document that does not parse
whole. Covered in [The build system](build-system.md#libkbuild).

## libkproc

`libkproc` takes every reading about the running machine, from a root that can be moved.

The movable root (`kpr_root_set()`) is what makes the resource monitor, the stutter attribution
(`kdos stutter`), the memory daemon and the energy daemon testable against recorded system state
under `testing/fixtures/`. A tool whose readings cannot be replayed cannot be tested at all.

Elapsed time may only be computed against the system uptime. A process's start time and the uptime
are both seconds since boot; pairing the start time with a monotonic timestamp mixes two clocks,
and under a fixture, two machines.

The box identity walk (`kpr_box_of()`, `kpr_box_of_pid()`) turns a process id into a box name by
walking the parent chain up to podman's per-container supervisor, `conmon`, and reading the `-n`
name from its command line. It is used by `kdos-shell`, `kdos stutter`, `kdos-oomd` and
`kdos-energyd`, which is why it is here rather than in any of them, with one climb limit for all of
them. The conmon lookups are cached for one sample and emptied by `kpr_conmon_forget()` at the
start of the next.

Graphics readings come from the kernel's DRM interface (`kpr_drm_list()`). Only amdgpu and NVIDIA's
management library publish a utilisation figure; everywhere else `busy_percent` is −1, and a
renderer must show engine time and label it as such. The NVIDIA library is opened with `dlopen`
when `libnvidia-ml.so.1` is present, which it is not on a stock KDOS, so it adds no link
dependency.

A reading goes into a caller's buffer wherever a buffer bounds the file. A process walk on a busy
machine opens a few thousand files a tick, inside a draw loop, and a whole-file read pays an
`fstat`, a heap block and a second `read` to confirm end of file for each of them: three system
calls and an allocation to carry a few hundred bytes. `kpr_read_into_proc()` and
`kpr_read_into_sys()` are the form to use. The whole-file readers (`kpr_slurp_proc`,
`kpr_slurp_sys`) remain for `/proc/cpuinfo`, `/proc/stat` and a command line, which no buffer
bounds: a full buffer means the content may be cut, and a cut value parsed as though it were whole
is a wrong reading rather than a missing one, so that case is re-read on the heap.

What cannot change is latched, and every latch is keyed on `kpr_root_gen()`. A CPU topology, a
model string and a disk's rotational flag are constants that would otherwise cost two files per
logical CPU and a `/proc/cpuinfo` of 26 KB on sixteen cores to rederive on a tick that only wanted
the busy figure. Moving the root is moving to a different machine, so a cache that does not record
the generation it was filled at would describe this host's hardware under a recorded one.

What is not latched is equally deliberate. A device's size is re-read, because an optical drive
keeps its name across a disc change and an LVM volume across a resize. The existence of a sensor is
re-read, because a hwmon chip appears when its module loads. That is also why no reader of `hwmon`
or `thermal` probes an index range: the index comes from one system-wide counter and says nothing
about how many chips there are, so `kpr_sysfs_indices()` reads the directory and every reader shares
the answer.

`KprCpu.ncpu` is the highest CPU number plus one, not the count of running CPUs. `/proc/stat` lists
only the online CPUs and keeps their real numbers, so a four-CPU machine with `cpu1` offline has
arrays four long and `online[1]` clear. A per-core chart walks `ncpu` and skips the clear slots,
which shows the gap as a gap; sizing by the number of lines instead drops the highest CPU's times
altogether and leaves a phantom core reading zero. Anything that wants the count, such as rescaling
a per-core percentage to percent of the machine, calls `kpr_cpu_online()`, because a CPU taken
offline below the highest index leaves `ncpu` where it was.

`libkproc` links `libkbase` and nothing else, which is what lets a root daemon take it. Two root
daemons do, `kdos-oomd` and `kdos-energyd`, and every library they link is code running as root.

## libkpack

The pack format. It links the base, signing and package libraries and nothing else, so a root
daemon, `kdos-packd`, can take it.

The rules it exists to keep are stated in
[Packs and boxes](../03-architecture/packs-and-boxes.md#rules-the-format-keeps): a pack that does
not parse whole is absent rather than partial, each section has its own size limit, the signature
block ends exactly where the footer begins, and the payload hash is checked before the signature
means anything. The footer is packed byte by byte rather than written as a structure. Its format
number says which byte span the payload digest covers; the library reads every format from
`KPK_FORMAT_MIN` (1) to `KPK_FORMAT` (2) and writes `KPK_FORMAT`. Changing that span without raising
the format makes every released pack fail its hash check and stop mounting. Nothing in this library
mounts or executes a pack; that is the daemon's job.

The index (`kpk_index.c`) carries every pack's SHA-256, so one signature over the index covers a
whole catalogue. The solve (`kpk_solve.c`) orders what has to be mounted before what, lowest layer
first, and takes an array of pointers: a pack's metadata structure is large, and an array of them is
not something a function puts on its stack.

## libkvt

The terminal as a state machine: a hard fork of libtsm 4.7.1, kmscon's MIT-licensed VT100–VT520
state machine, pinned rather than tracked. The fork renames every public symbol into the `kvt_`
prefix (three internal helpers, `screen_cell_init`, `screen_line_at` and `screen_mark_last`, are
linker-visible under a `screen_` prefix and declared only in `kvt_int.h`), replaces upstream's LGPL
hash table with its own `kvt_htable.c`, and takes character width from `libktui` so a glyph and its
cell cannot disagree. The terminal's own cell type stays private and is converted to `KtuiCell` at
the render boundary, because `KtuiCell` cannot hold the per-cell age that drives damage or the
symbol reference that makes combining characters possible. A colour outside the sixteen named ones
(the 216-colour cube, the greys and any 24-bit SGR colour) keeps its exact RGB across the boundary
as the cell's literal colour, flagged `KT_A_FGRGB` or `KT_A_BGRGB`, and `libkcell` paints that
literal; the sixteen named colours reduce to theme slots, so they follow `kdos theme` (see
[The cell's attribute byte](#the-cells-attribute-byte)). The slot is set either way, so a consumer
that reads slots alone draws the nearest theme colour.

What a consumer touches is `struct kvt_term`: a screen, a state machine and a child on a pty as one
object, with a descriptor to poll and a grid to draw.

The child dies with the window and is collected there. `kvt_term_close()` closes the master (which
hangs up the pty's foreground group), signals the child `SIGHUP`, waits up to 100 ms for it, and
then sends `SIGKILL` and blocks on whatever is left. Nothing else can do this: the non-blocking reap
runs from `kvt_term_pump()` and there is no pump after a close, so a child left behind would be a
zombie for the life of the session, and a child that ignores `SIGHUP` would be a program still
running with no terminal. The consumer needs no `SIGCHLD` handler.

The bytes a key produces are decided in here, never by the caller. The escape an arrow sends
depends on application cursor mode, on keypad mode and on the modifier encoding, and all three are
state machine state. A caller hands over a `libktui` key and modifier set; `kvt_term_key` turns it
into a keysym and lets the machine answer. `kdos-term` goes through it, and so must any other
terminal built here, so that there is one implementation rather than two that drift.

### The files

The state machine proper (`kvt_vte.c`, `kvt_screen.c`, `kvt_render.c`) keeps upstream's shape and
reaches the toolkit nowhere. `libktui` is reached from four files only, which are the terminal's
front end and its width rule:

| File | What it does for the rest of KDOS |
|---|---|
| `kvt_grid.c` | The render boundary: a screen cell becomes a `KtuiCell` here and nowhere else, once per frame, over the runs the screen reports changed |
| `kvt_term.c` | The screen, state machine and child as one object; maps `libktui`'s `KT_K_*` key codes to keysyms, from which the state machine produces the escape bytes |
| `kvt_selection.c` | Selection, and `kvt_ui_mouse()`, which turns a `libktui` mouse event into a selection, a scroll or a mouse report |
| `kvt_unicode.c` | Character width, through `ktui_wcwidth()` |

`kvt_vte.c` calls `libkbase`'s `kb_b64_decode()` for the payload of an `OSC 52` clipboard write,
which is why `libkbase` is a dependency. `kvt_grid.c`, `kvt_term.c` and `kvt_htable.c` are written
for KDOS and carry no upstream copyright; every other `.c` file keeps its upstream notice (MIT for
the libtsm files, public domain for `kvt_pty.c` and `kvt_ring.c`).

### Links and prompts

A hyperlink is a 16-bit identifier on the screen's own cell, and the address is interned. `OSC 8`
names an address for a run of text; the cell keeps an identifier into a per-terminal table, so the
text scrolls into the scrollback still knowing what it points at, and two runs of the same address
are one link. The table is capped and is not freed on a reset, which is what makes an identifier in
the scrollback safe to resolve for ever. `KtuiCell` is not widened for it: a link is a property of a
terminal's buffer, not of every surface the toolkit draws. `kvt_ui_mouse()` takes coordinates in
the terminal's own grid for the same reason a link lookup does, so a caller whose terminal is a
window subtracts its origin first, or both the selection and the link land as far from the pointer
as the window is from the corner.

A prompt mark is on the line, and the exit status is found by walking back. `OSC 133` says where a
prompt starts and what the command typed at it exited with. The mark rides the line so it survives
into the scrollback as one thing; a mark per cell would be eighty copies of one fact, and a mark
kept beside the screen would be lost the moment the line scrolled off. The status is found by
walking back to the nearest marked line at or above the cursor rather than remembered in a pointer,
because a pointer to a line kept across a scroll is a pointer to a line the screen may have
recycled.

### The render boundary

The render boundary is where an attribute becomes a cell, and what the cell cannot carry is
dropped. Bold, underline (with its style and colour), inverse, italic, strikethrough and overline
each have a bit and each is drawn by the pixel painter. `blink` and `dim` are parsed but have no bit
in the cell, so both are drawn as plain text. They are dropped here, at the one place an attribute
becomes a cell, rather than approximated by another style: a blink drawn as bold would claim a
weight the text never asked for.

A picture is written into the screen as sprite cells. `kvt_term_place` names tiles the caller has
already registered in `libktui`'s table and writes their codepoints at the cursor. It writes into
the screen rather than an overlay beside it, because that is what makes a picture scroll with its
output, clear with it and reach the scrollback, three behaviours an overlay would have to
reimplement. A tile the table has since dropped becomes a blank, and the cells carry the state
machine's own default attribute, so a fallback is drawn in the terminal's text colour on the
terminal's background and follows `OSC 10`/`OSC 11` and the installed palette. A literal colour
index there would reduce to whatever theme slot is nearest its RGB, which for a foreground index
and a background index can be the same slot: a blank drawn on itself.

This library decodes nothing. The three image protocols (sixel in a DCS string, `OSC 1337` inline
images and the kitty graphics protocol in an APC string) are delimited by one collector, since they
differ only in framing, and the payload goes to a callback the consumer sets. A payload past the
consumer's size cap is dropped whole rather than truncated. Keeping decoders out matters because a
consumer that links no pixel code at all still links this library; `kdos-term` backs the callback
with `libkimg`.

The colour a cell reduces to is cached, and the cache is indexed by the high bits of a
multiplicative hash, for the reason given under [Repainting](#repainting); a low-bit index would
make a 256-colour program miss on nearly every cell. The cache is discarded when the palette in
force changes value, which is the only thing that can change an answer, and the palette is compared
by value for the reason given under [Colour](#colour).

A CSI final byte with a private marker or an intermediate is not the plain sequence. `r` is DECSTBM
only as `CSI Pt;Pb r`; `CSI ? Pm r` is XTRESTORE and `CSI ... $ r` is DECCARA, and either taken
for DECSTBM would rewrite the scroll region and home the cursor under a program that only asked for
its modes back. `m` is guarded the same way. A final that means different things under different
markers tests `csi_flags`, never the byte alone.

A word selection's end is bounded by the screen, not only by the line. A line only ever grows, so it
keeps the widest the terminal has ever been; after a shrink, a word crossing the old right edge
would set `sel_end.x` past `size_x`, the renderer would never reach the index that turns the
highlight off, and every row below it would draw inverted. The copy takes the same bound, so the
text that comes back is the text that was highlighted.

## libkimg

`libkimg` is the only place untrusted image bytes are decoded, and anything that can write to a
terminal can reach it.

Two entry points: `kimg_decode()` for one picture, and `kimg_decode_all()` for every frame of the
one format that has more than one. Five decoders are optional (PNG, JPEG, WebP, sixel and GIF),
each compiled in by its `KIMG_HAVE_*` define, and `kimg_formats()` reports which are present.
`kdos-term` enables all five; `kdos-shell` enables all but sixel, because a file on disk is not an
escape sequence. The byte budget is enforced from the dimensions the format's header declares,
checked for overflow first, before any allocation.

GIF goes through libnsgif's `nsgif_*` interface, and the byte stream is walked for its block
structure before the library scans it. That walk refuses a stream that ends before its trailer
(libnsgif alone reports a GIF cut off after a whole frame as a success with the frames that
arrived) and caps the file at 4097 frames, because libnsgif keeps a record for every image it scans
and the budget is charged only for frames actually decoded. Each frame returned is the whole canvas
with disposal and transparency already applied, read out of libnsgif's bitmap as byte-order RGBA.
The canvas is the logical screen grown to cover the first frame only, so a later frame that reaches
past it is clipped. libnsgif treats a logical screen of zero, of one of the common monitor sizes
such as 640x480, or of more than 2048 pixels on a side as unset, and then the first frame's extent
becomes the canvas. The delay after a frame is the file's own centiseconds times ten, with a delay
of zero read as 100 ms; a frame with no graphic control extension carries libnsgif's default of ten
centiseconds. The file's loop count is not returned.

A decoder must answer NULL or an image for any bytes at all, and must never read past the end of
them. `testing/fixtures/img/fuzz.c` is the committed check on both, and the same directory holds
the oversized, truncated and malformed images the self-test feeds each decoder.

## libkchrome

The window furniture: the header band, group headings, the button bar, the list and wheel rule, the
scrollbar and its drag, the tone ladder, and the pixel tile.

It exists so that there is one implementation of each. Two implementations of a button bar
diverge as each is edited separately. The rules it enforces are in
[the design language](../03-architecture/design-language.md#the-chrome-primitives).

Two terms recur below and in [libkwl](#libkwl). A *backdrop* is the painter `libkwl` runs under a
surface's cells each frame; it replays the pixel display list the surface records while it draws.
The pieces of that list, such as a raised panel or the highlight under a menu row, are *plates*.

The tone ladder (`kch_tone.c`) supplies what the eight slots cannot: the colour of a raised plate.
The dark end of every palette is compressed so far that the `variant` colour against the `backdrop`
colour (the palette's ninth entry, not the painter above) measures at most 1.05:1 in any accent, so
a panel painted in its own background colour is the same colour as the desktop behind it. The
ladder's tones are `kcol_mix()`es of two scheme colours, each paired with the alpha it is laid on
at, and the taskbar and the Start menu read the same table. It holds no literal colour, which is
what makes `kdos theme` retint it.

A tile is cut to the display's pixel cell, because that is the cell the caller laid its contents
out in; a canvas cut to anything else clips the word or mis-centres the mark. Where the display has
no pixels of its own the tile is cut to the nominal cell instead: such a display answers a cell of
one, and whatever presents the sprite rescales it, which is the same rule `libkicon` keeps for
icons. A change of cell size or output scale is a resize, and both canvases are discarded and cut
again.

A refused sprite put keeps the picture that is already up. The table refuses on a full table or a
spent byte budget, and a frame of the previous picture is better than a flash back to the glyph
layout mid-hover, so a tile remembers what it published separately from what it last tried;
otherwise "already showing this" would freeze it at a picture it claims is current. Attempts are
capped per content hash, so that a persistent refusal does not turn every frame into a full canvas
raster under the memory pressure being survived. The cap is re-armed one second
(`TILE_RETRY_MS`) after the last refusal, because what refuses a put is transient while a tile's
content hash (for the Start button: the label, the hover state and the cell size) may never change
again, and a spent cap that never came back would leave that button on its glyph layout for the
rest of the session.

The pixel display list is recorded while a surface draws and replayed by the backdrop `libkwl`
paints under its cells. Whether the plates moved is a question asked at flush time, not a flag set
while recording. A frame is committed on a cell diff and a plate is not a cell, so a highlight
following the pointer down a menu would never reach the screen; but a surface re-records its whole
list on every draw, so a flag set from recording cannot tell a change from a redescription and
would make every draw of a backdrop surface a full-surface upload. Installing a backdrop registers
a callback that `libkwl` asks once the list is complete, and the answer compares the key of what is
currently recorded against the key of the picture last painted.

`kch_px_live()` is the one answer to whether a recorded operation can reach a screen. The list is
replayed by a backdrop and a backdrop is painted by `libkwl`, so it reaches a screen only where one
is installed, which a `--dump` run never does. The cell size cannot stand in for the test: it is
asked of `libkwl` either way, and `libkwl` answers with a fallback rather than with nothing. A
control whose only state cue is a plate has to draw the cell form of the same fact where this is
false.

## libkicon

`libkicon` turns an icon name or a file path into a sprite slot, or answers −1.

Minus one is not a failure. It is a terminal, an install with no artwork, icons switched off, or a
name nothing on this machine has a picture for. Every caller then draws its glyph tier, exactly as
it would without this library, and the whole icon layer is built on that rule.

A cell under 4x4 pixels is not a pixel backend, and `kicon_init()` refuses it. A backend with no
pixels of its own answers a cell of one, and rasterising at that size would decode, tint and rescale
a PNG per name to produce a picture one pixel across, which draws as a blank cell.
Refused means `kicon_enabled()` stays false and every lookup answers −1, so the caller draws its
glyph. A consumer that ships pictures over a wire at a nominal cell size passes that size instead of
the backend's, which is how such a consumer keeps its icons.

Pictures come from two sources, and the split is by where a picture came from, never by guessing
from its name:

- The KDOS theme's own icons come from one memory-mapped atlas, `/usr/share/kdos/icons/atlas.kia`,
  generated from the theme's artwork by `src/packages/kdos-icons/genatlas.py`, committed as
  `atlas/atlas.kia`, and installed by the `kdos-icons` port. It holds every icon at every size it
  was rasterised at, sorted by name and size so a lookup is a binary search and the pager reads only
  the pages that are drawn. Every header field, directory entry and blob extent is checked against
  the mapped length before use, so a damaged atlas is absent rather than partial. The atlas is
  tinted into the accent like every other piece of KDOS artwork, so one atlas serves all eight
  accents.
- An application's own icon, found as a PNG under `/usr/share/icons/hicolor/<size>/apps/`, is drawn
  untinted.

A decoded picture outlives the sprite slot naming it. The table gives slots back under its byte
budget, so a lookup that misses re-registers the picture this library still holds rather than
paying a PNG decode, a tint of every pixel and a bilinear rescale to rebuild something already in
memory. The pictures free their own pixels on the last unref and this library keeps a reference
across the handoff, which is what makes an eviction by another owner's evictor safe.

A path is memoised as a slot, so whatever frees the slots drops the memo. `kicon_slot_for_path()`
remembers the sprite slot a file's type resolved to (the lookup is a `stat` and a walk of the glob
table, and would otherwise be asked once per drawn row per frame), and a memo outliving its slot
either draws nothing or draws whichever picture reclaims the index next. The consumer clears it
with `kicon_forget_paths()` when it re-reads the directory it is drawing, and `kicon_retint()`
clears it too, because a retint hands back every slot the memo can name. The name-miss and
application-id tables are not cleared by a retint: neither answer depends on the accent, and both
live until the program is next started.

The dump harness stubs this library to exactly "no picture", so a committed reference frame is the
character grid.

## libkcell

The glyph cache and the cell painter: a grid of cells into a pixel buffer, the character ramp built
from it (`kcell_ascii.c`), the pixel canvas a block of cells can be drawn as, and the scale-and-cut
of a decoded picture into sprite tiles (`kcell_tile.c`).

The canvas is what makes a pixel tile possible without a second renderer: a pixel image exactly
some number of cells across, with fills and text at an arbitrary pixel size, handed to the toolkit
as a sprite. See [kdos-shell](../04-programs/kdos-shell.md#the-start-button).

### Synthesised frame characters

The frame characters are drawn, not rasterised. The single and double box-drawing sets from
U+2500, and the whole of U+2580 to U+259F (the full block, the eighths, the halves, the quadrants
and the three shades), are painted as pixman rectangles derived from the cell, in the cell's own
foreground, and the face is never asked for them.

Rasterising them is exact only while the face's box glyphs are drawn to precisely the advance the
cell was measured from. A face whose full block spans slightly more than its advance leaves a
hairline between two cells at some pixel sizes, and a face drawing its box glyphs to another metric
breaks a border into dashes.

Nothing selects the synthesis (not an attribute, not a caller, not a tier), so the same character
is the same picture in the panel, the terminal, the installer and the build screen, under every
face and at every size.

A rule is `max(1, cell_h / 16)` pixels thick, capped at a third of the shorter side, and a double
rule is that stroke twice with one stroke of gap; the single rule sits exactly between the double's
pair, which is what makes `├` meet `─` and `╪` meet `║` with no step. Every block edge is
`floor(span * k / 8)` from the cell's top or left, so an eighth, a half and a quadrant in
neighbouring cells share a pixel row and a pixel column; rounding each shape from its own fraction
would put a seam down a bar chart. A synthesised character is one cell wide and puts no ink outside
its own cell, which is what the damage report and the wide-glyph clip both assume. Every rectangle
is clamped to the cell after it is computed, so a cell too small to hold three strokes draws a
thinner line rather than one that leaves the cell.

They are not cached as masks. A cached mask is composited OVER, per pixel, through a solid source,
while a fill of at most eight rectangles issued in one call is cheaper than the composite it
replaces, so a cache would spend memory to make the draw slower. The three shades are the exception,
because a quarter-tone dither at a 16x32 cell is 128 disjoint pixels. They are one repeating `a8`
tile per tone per scale, four by two cell pixels across, offset by the cell's absolute position so
that one pattern runs unbroken across a whole shaded area instead of changing phase at every cell
boundary.

The synthesised set is exactly what the VT tier's font carries, so a box character is the same
picture on a pixel display as on `tty1`, and the two tiers cannot drift apart. That includes the
two mixed single/double junctions `╪` and `╬`, which the VT font has. The heavy, dashed and rounded
variants are not in it and still come from whatever face carries them.

### Styles and companion faces

A cell's style is drawn here, and two of the styles need a second face. Underline, strikethrough
and overline are one horizontal rule each, differing only in the row they land on, and are drawn
after the glyph so a descender crossing a strike is cut by it. A broken rule (curly, dotted,
dashed) is built as an array of rectangles and issued as one pixman fill, because each fill call
starts with a region intersection and a curl is dozens of pieces.

Italic and bold each ask fontconfig for the loaded name with `:slant=italic` or `:weight=bold`, and
the answer is kept only if its advance and height match the upright face. fontconfig never fails a
match, so asking for an italic Terminus returns a different family at a different size, and a
companion that disagreed would draw a row out of step with the one above it. There are four faces,
indexed by the two style bits, and the face is part of the glyph cache's key, because the same
codepoint from two faces is two glyphs.

Where a companion is missing the style is synthesised, and the two styles are synthesised
differently. Bold with no bold face is the same mask struck twice, one scaled pixel apart, so it
shares the upright glyph's cache slot; the weight belongs to the blit. Italic with no italic face is
a shear and therefore a different mask: the upright coverage is leaned by 7/32 (a little over twelve
degrees, the angle a designed oblique carries and the one FreeType synthesises) into a slot of its
own, because a shear widens the box and moves the bearing, and neither can be done at the blit.

The shear is about the mask's vertical middle rather than the baseline, and it moves by whole
pixels, rounded rather than truncated. The painter clips a glyph to its own cell, so a baseline
shear, which leans the whole letter to the right, would cut the top off every tall one; shearing
about the middle spends half the displacement on each side, and what is still lost is a pixel at
each extreme of a glyph that already fills its cell. Whole pixels, because a fractional shift needs
the mask resampled, and an alpha mask resampled at a terminal's size is a blur rather than a slant.
A glyph under six pixels tall leans by nothing and keeps the upright mask, which is the one case
where the style is still lost.

### Missing codepoints

A codepoint is missing only when its glyph matches the sentinel. fcft cannot report an absent
codepoint: its fallback search ends by rasterising glyph index 0 of the primary face, so what comes
back for a character no font carries is a valid glyph (a replacement box, or a PCF font's default
character), and `NULL` means a FreeType load error and nothing else.

The load therefore rasterises a permanent Unicode noncharacter, which no font can carry, and keeps
its metrics and its pixels; `kcell_has()` reports a codepoint missing when its glyph is that same
picture at those same metrics.

The pictures are compared row by row over the meaningful bytes only. A pixman image's stride is
rounded up past the glyph's width and the rasteriser writes only each row's own bytes, so a
whole-buffer compare would read uninitialised heap and report two copies of the same `.notdef` as
different pictures, which fails open and reports every absent codepoint present. The character
ramp's candidate filter depends on that answer being real, and anything else drawing a fixed glyph
set should ask once at load time rather than discover the gap on somebody's screen.

### Font lifetime

A font load replaces everything measured against the old font. `kcell_font_load()` may be called
repeatedly and tears the previous faces down first, taking the glyph cache, the character ramp's
candidate table and the tiling scratch buffers with them, so every `KCellGlyph` handed out before it
and every cached `kcell_w()`, `kcell_h()` or `kcell_ascent()` value is invalid once it returns.
`kcell_font_free()` is that same teardown plus fcft's own, and is not a step in a font change.

fcft is reference-counted inside `libkcell`, so the cell font and the canvas are independent of
each other and of any ordering. `fcft_from_name()` answers NULL for every request until
`fcft_init()` has run, so a consumer that draws canvases and never calls `kcell_font_load()` would
measure every string as zero and draw none of them, with fcft's own complaint going to a standard
error nothing reads. Neither of fcft's own calls is idempotent: a second `fcft_init()` replaces
FreeType's handle and orphans every face resolved through the old one, and `fcft_fini()` destroys
FreeType whether or not anything still wants it.

`kcell_font.c` therefore owns fcft for the whole library and counts its holders, declaring the pair
of functions in `kcell_priv.h`. The cell font takes one reference and the canvas takes one of its
own; the library comes up on the first and goes down on the last, and either may be used first
without tearing fcft down under the other. A canvas drawing through a `kcell_font_free()` keeps its
faces, and a failed `kcell_font_load()` gives its reference back before it returns −1, so a caller
reading that as "no cell font" has nothing left to free. The pair is private: a reference taken
outside `libkcell` would match no face inside it, so nothing could ever release it.

### Repainting

The changed-span repaint steps back onto a wide glyph's lead cell. A row is repainted only between
its first and last changed cell, widened by one cell each way because a cell's pixels are not always
its own. That is not enough on its own: a double-width glyph is painted entirely by its lead cell,
so a span that began on the `KTUI_WIDE_CONT` marker beside it would fill the marker's pixels,
erasing the right half of the character, and then find nothing to redraw there. The span therefore
takes one more step left when it starts on a continuation cell.

A run of one picture's cells is composited in one call. A sprite cell names a picture and a
sub-cell coordinate inside it, so cells that are consecutive columns of the same picture on the same
sprite row are one contiguous rectangle of one image and go out as a single
`pixman_image_composite32`. pixman charges most of a small composite to its setup (choosing a
combiner, building the iterators, walking the clip), and an 8x16 cell is small enough that the setup
is the whole cost: a 16x16-cell picture is 256 composites painted cell by cell and 16 painted a row
at a time. An embedded guest publishes a screenful of pictures per frame, so the difference is most
of the frame budget.

The run breaks on anything else, and the per-cell path draws it: a different picture, a sprite row
that does not advance one column at a time, a glyph, the end of the changed span, and
`KT_A_REVERSE`, which is the fill the pointer puts under the cell it is over and therefore a cell
that has to be drawn on its own.

The cached foreground sources are keyed on the colours, not on the theme's address. A glyph is
composited through a solid-fill image, and those are kept per slot and per literal colour.
A cache keyed on the table's identity (see [Colour](#colour)) would keep painting the old
scheme's text after a retint while backgrounds and rules, which read the palette fresh, came up in
the new one. The literal table is indexed by the high bits of its multiplicative hash, because the
low bits of a product carry only the low bits of blue, so a low-bit index would put the whole
216-colour xterm cube into six buckets.

## libkwl

The toolkit's Wayland backend, and the library with the most external dependencies: the Wayland
client library, xkbcommon, fontconfig, fcft and pixman. It provides `kwl_impl`, an implementation of
`libkdisp`'s `KDispImpl`, and `libktui`'s backend, draws through `libkcell`, and carries touch
input: `wl_touch` feeds `libktui`'s gesture recogniser, which turns a touch into a gesture and into
the ordinary mouse events every widget already handles.

Two kinds of rule live here. The first set is what a surface author relies on or must do; each is
written up from the author's side in [Writing desktop software](writing-desktop-software.md). The
second set is for anyone changing `libkwl` itself.

Three terms recur below. A *stash* is the saved frame a throttled commit leaves behind, published by
the next frame callback. A *backdrop* and its *plates* are defined under
[libkchrome](#libkchrome). A *pixel guest* is a guest (see [libktui](#libktui)) whose pixels are
shown in cells.

### What a surface author relies on

- A toplevel must ask for its frame, or it gets no decoration at all.
- `libkwl` binds the layer shell at version 4 where the compositor offers it, because an older
  resource answers on-demand with exclusive and the surface holds the seat's keyboard against every
  window.
- A Ctrl chord is the letter plus `KT_MOD_CTRL`, never the control code xkb folds it into: the tty
  decoder delivers the letter, and a chord table has one vocabulary.
- The cell painter leaves a clip on the image it was handed, so anything drawn into that same image
  afterwards (the panel's frame rule, for example) must drop the clip first or pixman writes
  nothing.
- A surface with a backdrop installs `kwl_set_pixels_dirty_fn()` so `libkwl` can ask at flush time
  whether its pixels moved. A latched dirty flag would not do: a backdrop redescribes the same
  plates on every draw and cannot tell a change from a redescription, so latching from the
  description would make every frame a commit and defeat the unchanged-frame gate (the check that
  skips committing a frame identical to the last) for every surface that has a backdrop.
- `kwl_font_step()` owns its own copy of the font name: `KDispConfig.font` is the caller's pointer,
  and the surface outlives whatever the caller built it in.
- `kwl_init` ignores `SIGPIPE` process-wide, because every clipboard and drag payload is written
  into a descriptor the receiver owns, and the default disposition would kill the surface when that
  receiver closes early. Any consumer that forks and executes a program must therefore call
  `kb_child_reset_signals()` in the child, for the reason given under
  [Processes and secrets](#processes-and-secrets).

### Rules for anyone changing libkwl

Each rule guards a distinct failure. The input rules are explained at length in
[Writing desktop software](writing-desktop-software.md#input-the-backend-cleans), and the two
presentation rules in [Presenting a frame](writing-desktop-software.md#presenting-a-frame).

- The event queue is a ring, not one slot, because a batch of callbacks arrives from a single read,
  and one slot would lose a button press to the motion that followed it in the same batch.
- Key repeat is the client's job. The protocol sends a rate and a delay and never a repeated key,
  so a client that does not time its own repeats never repeats a held key.
- A wheel tick is not an axis event. A wheel is already quantised, one event per *detent* (the
  notch a wheel clicks through), and running it through the touchpad's accumulator leaves a
  remainder that makes the next notch move a list two rows; a touchpad is not quantised, and its
  ticks are synthesised from the accumulated values.
- On the discrete path (the wheel's, as `axis_source` reports it) one pointer frame is one detent.
  Honouring a frame's count of two moves a list twice as far as the one scroll that produced it.
- A motion that did not move is not a motion. A virtual machine's absolute pointer resends its
  position with every wheel event, and a handler that took that as motion would step the
  selection and put it straight back.
- A double buffer needs a damage shadow per buffer: a record of what that buffer holds. With one
  shared record, rows that changed while the other buffer was in flight are never redrawn in it.
- A serial (the number the compositor attaches to an input event) must be retained, because
  setting the selection, starting a drag and setting the cursor shape each have to present one.
  Without it the clipboard, drags and cursor changes fail, and nothing points at the input code.
- An enter event carries coordinates and they are not optional. Discarding them puts the first
  click after an enter at an impossible position and leaves hover stale until the pointer moves.
- The scale and the resized buffer must land in one commit, or for one frame the compositor sees a
  buffer whose size disagrees with its declared scale.
- A data source is destroyed on cancellation, never at set time, or the copy silently does nothing.
- The clipboard and the primary selection are separate stores, and an in-flight send keeps the
  payload it started with. A terminal sets the primary selection on every mouse release, and one
  shared payload would splice that text into a clipboard send that is still draining.
- A parked send is polled for writability, and its deadline counts from the last byte that moved.
  Anything past one pipe buffer parks, and a budget counted from the first write would close the
  pipe on a receiver that is still reading, which the receiver cannot tell from a clean end of
  file, so it would accept a truncated selection.
- A throttled frame counts as presented, because the stash carries its own `full` flag and is
  committed by the frame callback that follows. Answering otherwise makes the toolkit re-arm a full
  repaint that never clears, on every surface that draws at the display's rate.
- A surface is clamped against the output's logical, upright box (the mode divided by that output's
  scale, with the axes exchanged on a 90 or 270 degree transform) and never against a slot whose
  proxy is gone. Each omission lets a popup be sized for a screen that is not there.
- A stash is content for the grid it was taken from, so every resize drops it; publishing it would
  paint the old layout into the new geometry.
- A compose table that fails to build is absent, never partial.
- A font reload spoils every paint baseline, because it rewrites no cell. `kwl_font_step()` changes
  what a cell looks like and not what it says, so the damage diff, both buffer shadows and the
  unchanged-frame gate would all find nothing to do while every glyph on the screen is drawn at the
  old size.
- A lock surface must not receive the pre-configure commit, which is a protocol error there.
- A drop's offer is owned separately from the drag's, because the leave that follows a drop arrives
  while the payload is still draining and a second drag may enter before it ends. One slot for both
  would lose the first offer and destroy the second while it is live, so the next drag would land
  and do nothing.
- A withdrawn seat capability takes everything derived from it: the `wl_keyboard`'s repeat and
  focus state, and the `wp_cursor_shape_device_v1` made from the `wl_pointer`. The protocol makes
  that device inert with the capability, and the re-create tests only for NULL, so a device kept
  across an unplug would send every later shape request to a dead proxy.
- The raw queue is filled, with one number this protocol cannot carry. `wl_pointer` reports a
  position and no distance, and this client binds no relative-pointer protocol, so the delta is the
  step between two surface positions: already accelerated, already clamped to the surface, and the
  same number in both the accelerated and the unaccelerated pair. Everything else is real: evdev
  codes, the four xkb components the compositor sends, `value120` and the axis source, and the
  keymap the compositor handed over.
- A key held when the surface loses the keyboard is released into the raw stream here. The
  compositor sends no release for it, and the raw queue carries a key switch rather than a
  character, so a press with no release would be a key a pixel guest holds down for ever. Only codes
  this client reported down are released, which keeps the two halves symmetrical.

## Adding a library

1. Decide what it owns and what it must not link. A library is split where its link requirements
   differ (see [The constraint](#the-constraint)), not by topic: `libkbase` is broad because
   everything may link it, while `libkcell` and `libkwl` are separate so that the cell painter
   does not bring a Wayland client library with it.
2. Pick a prefix and use it on every exported symbol.
3. Place it in the dependency order and confirm nothing points back up.
4. Link nothing but the C library if a terminal program or a root daemon could ever want it. If it
   needs an external library, keep it out of the phase-1 set (`libkbase`, `libkcolor`, `libktui`,
   `libkpkg`, `libksig`) and out of everything they call, and add it to the table under
   [The constraint](#the-constraint).
5. Add its sources and include path to the `build.sh` of every program that uses it. There is no
   archive to link; a program compiles the library's `.c` files itself. Check the consumers built
   outside a recipe as well, each of which names its libraries explicitly (see
   [How the libraries are built](#how-the-libraries-are-built)):
   - `script/01_phase1/12_kpkg.sh` (`kpkg`) and `script/01_phase1/13_kinstall.sh` (`kinstall`),
     the phase-1 builds. A library added under either program and missing here breaks the
     bootstrap. `kinstall` is also built as a port by `src/packages/kdos-installer/build.sh`, and
     the two must compile the same sources.
   - `script/kdosbuild.sh`, which builds `kdosbuild` on the host.
   - `src_kpkg_ensure` in `ports/srclib.sh`, the host recipe reader. Its source list, the one in
     `src/tools/kdos-portup/main.c` and the one in `testing/selftest.sh` must agree.
   - `ports/update`, which builds `kdos-portup` on the host.
   - The `libkdos.a` line in `src/desktop/kdos-comp/build.sh`, which meson links into the
     compositor.
6. Add its assertions to `src/libs/selftest.c`, especially any invariant established by comparing
   against a reference implementation.
7. Add it to the consumer compile block in `testing/selftest.sh` (the step headed "every consumer
   still compiles against the libraries"), so a header change that breaks a consumer fails on a
   development host rather than hours into a build.
8. Keep the frame state and any global private, behind accessors.
9. Add a row to [The set](#the-set) and a section to this chapter.

## See also

- [Writing desktop software](writing-desktop-software.md) — building a surface on the drawing
  libraries
- [The design language](../03-architecture/design-language.md) — the rules the drawing libraries
  enforce
- [Testing](testing.md) — the shared test program, the fixtures and the sanitizer runs
- [The build system](build-system.md) — where `libkbuild` fits, and what phase 1 compiles
- [How KDOS is built](how-kdos-is-built.md) — the whole build, from the cross toolchain that
  compiles `kpkg` and `kinstall` to the desktop phase that compiles the rest
- [How KDOS differs](../01-philosophy/how-kdos-differs.md#the-c-library) — musl, the C library
  every one of these libraries is compiled against, and what choosing it costs
- [The ports catalogue](../06-reference/ports-catalogue.md) — every port, including the external
  libraries the five pixel libraries link
- [Packaging](../03-architecture/packaging.md) — where `libkpkg` and `libksig` fit
- [Packs and boxes](../03-architecture/packs-and-boxes.md) — the format `libkpack` implements
- [Glossary](../06-reference/glossary.md) — surface, sprite, slot, pack, box and the other terms
  used here

<!-- book-nav -->
---

*Part V — Building and developing, chapter 35.* Previous: [34. Build troubleshooting](build-troubleshooting.md) · [Contents](../README.md) · Next: [36. Writing desktop software](writing-desktop-software.md)
