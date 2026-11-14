# The C libraries

KDOS's own C programs are built on nine libraries under `src/libs/`, each named `libk*`. The
package manager, the installer, the build screen, the root daemons, `kdos-appbox`, the `kdos`
command and the C programs of the desktop session (`kdos-runner`, `kdos-notice`) all compile some
of them in. This chapter describes them: how they are compiled into their programs, the rule about
external dependencies that shapes the whole set, what each library owns, which may call which, and
the rules each one exists to enforce. It is written for developers who change or extend KDOS's own
C code. KDOS's Qt and QML code (the settings modules, widgets, the Discover backend, the KWin
pieces and the graphical installer) uses none of these libraries, except where a small C helper of
its own does and where `kdos-discover` compiles `libkpkg`'s version comparator, `kp_vercmp.c`, into
its model so it orders versions as `kpkg` does; it is covered in [Writing desktop software](writing-desktop-software.md). The
libraries' tests are described in [Testing](testing.md).

After reading this chapter you should be able to find the library that owns a piece of behaviour,
change it without breaking the program that can least afford a new dependency, and add a library
of your own.

## How the libraries are built

A `libk*` library is a directory of C sources with one public header. It is not an installed
archive or a shared object. Each program that uses a library compiles that library's `.c` files
straight into its own binary, from its own `build.sh`, and names the external libraries it needs on
its own link line. `kdos-appbox`'s recipe, for example, compiles `src/libs/libkbase/*.c`,
`src/libs/libktui/*.c`, `src/libs/libkcolor/*.c` and `src/libs/libkxdg/*.c` together with its own
sources. The installer's recipe first compiles `src/system/kinstall-core` into a local archive,
`libkinstall-core.a`, and links that with the libraries.

A CMake port uses the same sources the same way: `kcm-kdos-firewall`'s `CMakeLists.txt` globs
`src/libs/libkbase/*.c` into its `kdos-fwask` helper, and `kdos-discover`'s `build.sh` compiles
`kdos-ports-helper` against `libkbase` with `gcc` beside its CMake build.

A few programs use a library's header without compiling any of its sources. `libkcolor`'s palette
is an X-macro (a macro that expands a table once per consumer at compile time; see
[libkcolor](#libkcolor)), so `kdos-splash` includes `kcolor.h` for the colour table and links none
of `libkcolor`'s code, and `kdos-addons`' generator, `genaddons.py`, reads the same table out of the
header as text. Many of KDOS's own ports use no `libk*` code at all: `kdos-bb`, `kdos-boxsock`, the
artwork ports and every Qt unit under `src/desktop` that has no C helper.

Two consequences follow from compiling the sources into each program:

- Nothing is installed under `/usr/lib` for these libraries, and nothing outside this tree can link
  them.
- A program's recipe has no upstream source, so the package manager's recipe hash for it covers
  the whole of `src/libs/` as well as the program's own directory (`kp_hash.c` in `libkpkg`). The
  package manager cannot tell which libraries a `build.sh` compiles without parsing shell, so
  editing any library rebuilds every port of KDOS's own on the next build, the Qt ones included,
  not only the ones that use it. See [Developing](developing.md) for the narrow rebuild commands.

Some consumers are compiled outside a recipe, by a script that names its libraries explicitly:

| Script | Builds | Libraries |
|---|---|---|
| `script/phases/10_bootstrap/120_kpkg.sh` | `kpkg`, the package manager, into the bootstrap sysroot | `libkbase`, `libkpkg`, `libksig` |
| `script/phases/10_bootstrap/130_kinstall.sh` | `kinstall`, the installer, into the bootstrap sysroot | `libkbase`, `libktui`, `libkcolor` |
| `script/kdosbuild.sh` | `kdosbuild`, the build screen, on the build host | `libkbase`, `libkbuild`, `libkpkg`, `libktui`, `libkcolor` |
| `script/release-image.sh` | a scratch `kdos` command that writes a release's signed slot image | `libkbase`, `libkcolor`, `libkpkg`, `libkxdg`, `libkproc`, `libksig`, `libkpack`, and `libkbuild`'s JSON reader |
| `src_kpkg_ensure` in `ports/srclib.sh` | the host recipe reader `ports/fetch` and `ports/publish` use | `libkbase`, `libkpkg`, `libksig` |
| `ports/update` | `kdos-portup`, the upstream version checker | `libkbase`, `libkpkg`, `libkbuild` |
| `ensure_kpkg_bin` in `src/devtools/kdos-portup/main.c` | `kdos-portup`'s own copy of the recipe reader, in `ports/.portup-tools/` | `libkbase`, `libkpkg`, `libksig` |
| `testing/selftest.sh` | `src/libs/selftest.c` and a compile of every C consumer | all nine |

## The constraint

These libraries link nothing but the C library.

The reason is `10_bootstrap`, the second phase of the build (see
[The build system](build-system.md), and [How KDOS is built](how-kdos-is-built.md) for the build
told end to end). The installer, `kinstall`, and the package manager, `kpkg`, are both
cross-compiled there against musl and the kernel headers alone, so that they exist on every tree
from the first bootable image onward. `kinstall` uses `libkbase`, `libktui` and `libkcolor`; `kpkg`
uses `libkbase`, `libkpkg` and `libksig`. If any of those five ever needed a real `-l` flag, the
program using it would have to move to a later phase, and the first bootable image would lose its
installer or its package manager.

All nine keep the rule, and none of them links the maths library either. `libksig` carries its
cryptography as vendored source rather than linking it, and `libkproc` opens NVIDIA's management
library with `dlopen` at run time, only when one is installed, so neither adds a link flag. The
root daemons depend on the same property: every library a daemon links is code running as root,
and these nine link no external library, so the only `libk*` code a daemon runs as root is source
in this tree, including the one vendored copy in `libksig`. What a daemon links beyond them (the
`basu` D-Bus library for the ones with a bus face) is named on that daemon's own link line and
documented in [The daemons](../04-programs/daemons.md).

A program that needs an external library names it on its own link line, never inside a library:
`kdos-runner` links `basu` and `sqlite3`, and `kdos-theme` links `libpng`.
Each of those is a property of one program, and the libraries under it stay usable in
`10_bootstrap` and in a root daemon.

## The set

The header column names the library's one public header, which is what a consumer includes. The
prefix column is the one every exported symbol of that library carries (`libksig`'s `crypto_` is
the vendored Monocypher's own). The "Built on" column lists the other `libk*` libraries it needs,
either by calling them or by using a type or macro from their header.

| Library | Header | Prefix | Owns | Built on | Used by |
|---|---|---|---|---|---|
| `libkbase` | `kbase.h` | `kb_` | Allocation and its failure hook, fatal and warning output, strings, files, paths, locking, monotonic time, SHA-256 and MD5, JSON string escaping, `file://` URIs, state paths, group membership and the root-daemon authorisation gate, the argument-vector builder and process helpers, desktop notifications, Landlock self-sandboxing, the fuzzy matcher, and the freedesktop trash | — | Every C program of KDOS's own except `kdos-splash`, `kdos-bb` and `kdos-boxsock`; also `kpkg`, `kdosbuild` and `kdos-portup` |
| `libkcolor` | `kcolor.h` | `kcol_` | The palette table, colour-space conversion, mixing, contrast, the readable muted colour, a scheme's sixteen terminal colours, the hue-family classifier, the nearest of the 256 indexed colours, and the generated boot-menu look and console palette | `libkbase` | `kinstall`, `kdosbuild`, `kdos-appbox`, `kdos-tools`, `kdos-theme`, `kdos-powerd`, `kdos-runner`; its header alone is used by `kdos-splash` and read by `kdos-addons` |
| `libktui` | `ktui.h` | `ktui_` | Terminal ownership, the cell buffer and its diff, key and mouse decoding, character width, paste, immediate-mode widgets, the draw-and-key views, menus, modals, the selection rule rows are drawn with, the three glyph tiers, charts, the sprite table, offscreen rendering, and announcements | `libkcolor` (its palette macro and `kcol_theme_name()`), `libkbase` | `kinstall`, `kdosbuild`, `kdos-appbox` |
| `libkxdg` | `kxdg.h` | `kxdg_` | Desktop entries, the MIME glob table, what handles a type, the one correct way to turn a command line into an argument vector, and recent files | `libkbase` | `kdos-appbox`, `kdos-tools`, `kdos-runner`, `kdos-notice` |
| `libkpkg` | `kpkg.h` | `kp_` | Configuration, the package database and its merged-`/usr` path spellings, the ports tree, dependency parsing and solving, version comparison, the recipe and build-config hashes, and the content-keyed package store | `libkbase` | `kpkg` (also compiled on the build host as the recipe reader), `kdosbuild`, `kdos-portup`, `kdos-pack`, `kdos-packd`, `kdos-tools`; `kdos-discover` takes `kp_vercmp.c` alone |
| `libksig` | `ksig.h` | `ksig_`, `crypto_` (vendored) | Ed25519 signing and verification, key files, keyrings; the one library with vendored third-party source | `libkbase` | `kpkg`, `kdos-pack`, `kdos-packd`, `kdos-tools` |
| `libkbuild` | `kbuild.h` | `kbuild_`, `kj_` | Phase discovery, the phase metadata block, the build plan, the snapshot inventory and its chains, the layer diff, a read-only JSON scanner | `libkbase`, `libkpkg` | `kdosbuild`, `kdos-portup`; `kdos-tools` takes the JSON scanner alone |
| `libkproc` | `kproc.h` | `kpr_` | Every reading about the running machine, from a movable root: processes, uptime, box identity, processor, memory and pressure, block devices, network, power, sensors, graphics, sound PCMs, and the sample ring | `libkbase` | `kdos-oomd`, `kdos-energyd`, `kdos-tools` |
| `libkpack` | `kpack.h` | `kpk_` | The pack format: the footer, the metadata blob, the requirement solve, the payload hash, the signature block, and the index | `libkbase`, `libksig`, `libkpkg` | `kdos-pack`, `kdos-packd`, `kdos-tools` |

## Dependency direction

Each arrow reads "is built on". The graph has no cycles, and it must stay that way: nothing lower
down may call anything higher up. An edge means a call into the other library, or a type or macro
taken from its header.

```
libktui    → libkcolor, libkbase
libkcolor  → libkbase
libkxdg    → libkbase
libkpkg    → libkbase
libksig    → libkbase
libkbuild  → libkpkg, libkbase
libkproc   → libkbase
libkpack   → libksig, libkpkg, libkbase
```

One edge carries a rule of its own. `libktui` takes the palette from `libkcolor`'s X-macro at
compile time and reads the accent through `kcol_theme_name()`, so `libkcolor`'s header and its
sources go wherever `libktui` does. That is why `script/phases/10_bootstrap/130_kinstall.sh` puts
`libkcolor` on the bootstrap command line: leaving it out builds on a development host, where
`testing/selftest.sh` supplies it, and fails only in `10_bootstrap`.

## libkbase

`libkbase` is the base library: allocation, strings, files, paths, locking, time, hashing and
process helpers. Every other library calls into it directly.

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
variants. KDOS's Qt code keeps the same rule with `QProcess` and an argument list; see
[Writing desktop software](writing-desktop-software.md#starting-a-program).

The process helpers send a child's error output to `/dev/null` unless `kb_proc_verbose` is set, so
anything whose failure is diagnosed by the child's own message has to turn that on.

A secret reaches a child on a descriptor and never in `argv`, because `/proc/<pid>/cmdline` is
world-readable for the life of the process. The four feeding forms differ in what happens to the
child's output:

| Call | The child's input | The child's output |
|---|---|---|
| `kb_run_feed` | the bytes given | standard output and error to `/dev/null`: a child fed for what it does, not what it prints |
| `kb_run_feed_tty` | the bytes given | the caller's own, which a pager needs (`kdos help --pager`) |
| `kb_run_feed_capture` | the bytes given | standard output captured into a buffer: a filter |
| `kb_run_feed_env` | the bytes given, plus `NAME=VALUE` strings added to its environment | standard error captured, so a refusal can be reported with its reason |

`kb_run_feed_env` exists for a helper that takes a secret on the descriptor an environment variable
names and reports its refusal on standard error: a caller without both would either put the secret
in an argument list or report a status with no reason. In the forms that feed and read back, the
input must fit in one pipe buffer, because nothing is read until the whole input has been written.

`kb_child_reset_signals()` belongs between `fork` and `exec` in any program that ignores a signal.
The terminal backend in `libktui` sets `SIGPIPE` to `SIG_IGN`, an ignored disposition survives
`execve`, and a shell started without the reset never ends a pipeline: `yes | head` runs until
something else stops it. `kdos-settingsd`, `kdos-sysupdated`, `kdos-runner`, `kdos-appbox`,
`kdosbuild` and `kdos-tools` call it before the programs they start.

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
are `libksig`'s job. `kb_md5_*` names thumbnail cache files (`kdos-tools`' `thumb.c`), because the
freedesktop thumbnail standard names them by the MD5 of the source URI and a stronger hash would
produce a cache no other program could share. It is a file name, never a security check.

### Smaller helpers

- `kb_state_path()` builds a path under `$XDG_STATE_HOME` (or `~/.local/state`) on every call, and
  returns 0 when there is no home.
- `kb_json_str()` is the escaper every `--json` output goes through.
- `kb_notify()` raises a desktop notification through `gdbus`, detached and best effort, so a
  program is never blocked by it. `gdbus`'s reply, the notification's id, goes to `/dev/null`
  rather than to the caller's terminal. Nothing here links a D-Bus library.
- `kb_terminal()` names the terminal emulator a `Terminal=true` entry runs in: `konsole` in the
  Plasma session. It answers NULL where there is no session (a bare virtual terminal, a serial
  console, an ssh login), and the caller then runs the program where it already is.
  `/usr/local/bin/xdg-terminal-exec` gives GLib's launcher the same answer, and the two must agree.
- `kb_desktop_prefix()` gives the lowercased first name in `$XDG_CURRENT_DESKTOP`, which is how
  `kde-mimeapps.list` is found.
- `kb_in_vm()` reads the DMI vendor string, for the programs whose defaults differ in a virtual
  machine.

The freedesktop trash lives here too (`kb_trash_put`, `kb_trash_list`, `kb_trash_restore` and
their neighbours). It follows the specification Dolphin's Delete key follows, so `kdos trash` at a
prompt and the file manager share one trash. See
[The kdos command](../04-programs/kdos-command.md#kdos-trash).

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

`kb_uid_allowed(uid, group)` is the socket authorisation boundary of the four root daemons that
serve a Unix socket (`kdos-packd`, `kdos-powerd`, `kdos-oomd` and `kdos-energyd`): is this uid
root, or a member of that group. Each daemon reads the peer's uid from `SO_PEERCRED`, which the
kernel fills in and the peer cannot forge, and asks this one function before acting as root on the
peer's behalf. The sockets themselves are world-writable, and an unauthorised peer is answered with
a refusal and a closed connection; a socket mode that looked like the authorisation would be a mode
somebody eventually loosens.

It is one function because four copies are a rule that gets tightened on one socket and stays
loose on the others, with nothing in the tree to show which is which. A daemon that admits more
than root and the group states the difference beside its own call and still asks this for the
rest: `kdos-oomd` also admits the uid in the foreground of a seat, which it asks of elogind, and
`kdos-powerd` asks polkit about a caller outside `wheel`. Both are described in
[The daemons](../04-programs/daemons.md).

A uid with no passwd entry is refused. Membership itself is `kb_user_in_group`, which reads
`/etc/group` whole rather than through `getgrnam()` and counts the group's own gid as well as the
member list. A user whose primary group is `wheel` never appears in that list, and that is the
account an installer creates.

### kb_fuzzy

`kb_fuzzy` is a subsequence matcher: it matches a subsequence rather than a substring, so `sm`
finds `System Monitor`, which no substring search can. Matches are scored so that a prefix beats an
acronym, an acronym beats a run and a run beats a scatter. Higher is better and zero is no match;
an empty needle matches everything with the same small score, because a list filtered as somebody
types starts with nothing typed.

Each needle character prefers a word start over an earlier occurrence inside a word, which is what
makes the acronym score reachable. That preference is greedy and can consume a character the rest
of the needle still needs, so a preferring pass that fails is redone taking the leftmost occurrence
of each character. Any needle that is a subsequence of the row therefore matches, and a row never
loses to a prefix of its own name.

No program outside the self-test calls it; KRunner does the desktop's searching. A caller that
sorts on it must sort descending. A sort written for a lower-is-better matcher ranks a correct list
backwards, which reads as bad ranking rather than as a bug.

## libkcolor

`libkcolor` holds the one palette table for the whole distribution and the only copy of the colour
arithmetic on it. The `kdos` command, the installer, the theme generators and the bootloader
configuration all take their numbers from here, so the boot menu, the installer, the login screen
and the desktop cannot disagree about what an accent is.

The palette is the `KCOL_SCHEMES` X-macro in `kcolor.h`: eight schemes (phosphor, amber, ice,
bone, norton, borland, perfect and paper, with bone the default), each nine colours in the order
primary, dim, secondary, urgent, deep, text, variant, pdark and backdrop. Every consumer expands the
same literal values into its own table at compile time. `libktui` projects them onto its eight
colour slots; `kdos-theme`'s generators expand them into the KDE colour schemes, the Konsole
schemes, the qt5ct palettes, the wallpapers and the login screen's globals (see
[Theming](../02-user-guide/theming.md#how-the-theme-is-generated)). Nobody keeps a second copy of
the numbers. A scheme must clear two contrast floors, which the self-test asserts for every scheme:
7:1 between `text` and `deep`, and 4.5:1 between the accent and `deep`.

The HLS conversions (`kcol_to_hls`, `kcol_from_hls`) reproduce Python's `colorsys` exactly,
including its unusual `2.0 - maxc - minc` intermediate, its modulo on a negative hue, and Python's
round-half-to-even rounding, so generated output does not shift by a unit when the arithmetic is
moved between languages. Do not reorder the arithmetic in `kcolor.c`. The self-test asserts that
every scheme colour survives an HLS round trip unchanged.

The library stays off the maths library for the same reason the rest of the set does: a bootstrap
consumer cannot link one. The modulo is done in a loop, the rounding by hand, and the sRGB transfer
function behind `kcol_contrast` is a 256-entry table rather than a call to `pow`.

The two mixing functions are not interchangeable. `kcol_mix` takes an integer percentage and
integer arithmetic; `kcol_mixf` takes a fraction and rounds the way Python does. They disagree by a
unit on some inputs, and each generated file was written against exactly one of them. The self-test
asserts that they disagree, so that nobody unifies them.

Two derived colours exist, and confusing them is a legibility defect. The dim value is a fill and
measures below any text floor against the background; the muted colour from `kcol_muted` is the
readable one, and every text role goes through it. It starts as a 56% mix of the background towards
the text colour and moves one percent further at a time until it clears 4.5:1 against the
background, so every dark scheme keeps its 56% mix and `paper`, on its light ground, lands at 60%.
The self-test asserts the floor for every scheme. How the KDE colour roles are derived from these
two is in [the design language](../03-architecture/design-language.md#colour).

Some consumers cannot take the palette as numbers, so the library writes their format for them,
and each format is generated rather than kept as a second table that would go stale:

- `kcol_limine_conf()` writes the whole look of a `limine.conf`: the `interface_*` lines, the
  palettes, the margins, the wallpaper style and the font scale. It leaves out the two lines that
  name paths, `wallpaper` and `term_font`, because which artwork and face are installed is not a
  question an accent answers. The ISO step stamps the medium's boot menu with it through
  `kdos-bootctl theme --print`, the installer stamps a new machine's, and `kdos-bootctl theme`
  restamps an installed one.
- `kcol_vtrgb()` writes the Linux console's sixteen colours in `setvtrgb`'s format.
  `kdos-bootctl theme` writes the result to `/etc/vtrgb`, and `kdos-getty` loads that file before
  it clears the console, so the login prompt on `tty2` is already in the scheme. Blue, magenta and
  cyan are fixed hues rather than scheme colours, so `ls --color` and a diff can still tell their
  classes apart.
- `kcol_ansi()` derives a scheme's sixteen terminal colours, ANSI 0-7 and their bright variants.
  Konsole's colour schemes (written by `kdos-theme konsole`) and every terminal program `kdos theme`
  configures take them from here, so a status line drawn in one program's blue sits in a terminal
  whose blue is the same. A bright variant of a scheme slot moves the anchor's hue, lightness and
  saturation offset from the phosphor slot onto this scheme's slot; a hue the scheme cannot supply
  (blue, magenta, cyan) keeps the anchor's hue and follows only the scheme's text lightness, so an
  amber terminal gets a warm-dark blue rather than a green. For phosphor itself the bright slots
  are the anchors, byte for byte.
- `kcol_xterm256()` answers the nearest of the 256 indexed colours for a program that accepts only
  an index, such as newsboat. It searches the colour cube and the grey ramp and never the first
  sixteen, which are the terminal's own colours the scheme has already replaced.

`kcol_theme_name()` is the one read of the accent a person chose, from
`$XDG_CONFIG_HOME/kdos/accent`, which `kdos theme` writes (the `kded-kdos` module reaches it by
running `kdos theme --from-plasma`). An empty answer
means the default scheme, which is not an error.

`kcol_sem()` (the named semantic colours of a stylesheet), `kcol_remap()` and
`kcol_retint_text()` (recolouring every `#rrggbb` token in a text blob into a scheme) are in the
library and are called by nothing outside its self-test.

## libktui

The terminal toolkit: terminal ownership, the cell buffer, the diff, input decoding, widgets,
charts, the sprite table and motion. It links nothing but the C library, which is what lets the
installer use it in `10_bootstrap`.

Two programs draw with it. `kinstall`, the text installer, builds its pages from the immediate-mode
widgets, the layout cursor and the modals; `kdosbuild`, the build screen, draws directly into the
cell buffer and uses the progress bar, the gauge and the sparkline. `kdos-appbox` compiles the
library and calls none of its functions. Both drawing programs run on the built-in terminal
backend, at a prompt, on the Linux console or in Konsole. The parts of the library that serve a
backend other than the terminal (the pointer hook, the second input queue, the frame clock behind
motion, the sprite evictor) are exercised by the self-test, which installs a backend of its own.

### Capabilities and the frame bracket

Three glyph tiers are chosen from the terminal's capabilities, because the console font KDOS ships
holds 512 glyphs and a character it lacks renders as a blank. The table is in
[the design language](../03-architecture/design-language.md#the-glyph-tiers).

Colour depth is read from the environment. `COLORTERM` naming `truecolor` or `24bit` means 24-bit
colour. Konsole answers 24-bit SGR whatever `TERM` it was given and gives a plain
`xterm-256color`, so `KONSOLE_VERSION`, which only Konsole sets, means 24-bit colour too, except on
the Linux VT. A `TERM` naming a known truecolor terminal also counts; anything else that is a
terminal at all gets 256 colours.

What a terminal can do beyond that is asked once, in one write, on entering the screen. Two facts
cannot be learned from a `TERM` value or a capability database entry: the kitty keyboard flags
(`CSI ? u`), which is what makes `Super` arrive at all, and synchronized output (`CSI ? 2026 $ p`),
which is what lets a frame be bracketed. Both queries go out together and their replies are told
apart by scanning the buffer. Asking in turn would pay the timeout twice on a terminal that answers
neither, and the second wait would take a slow answer to the first query as its own. A terminal
that reports kitty flags is asked for flag 1 only (`CSI > 1 u`, disambiguate escape codes). A
DECRQM value of `0` means "not recognised" and `4` means "permanently reset", so only `1`, `2` and
`3` set `KT_CAP_SYNC`. The replies are consumed there, or they would be typed into the program as
stray characters. The Linux VT answers neither and is skipped rather than waited on.

A frame is bracketed where that bit is set: `CSI ? 2026 h` before the diff and `CSI ? 2026 l`
after it. A diff frame is a scatter of cursor moves and single cells, and a terminal that draws
them as they arrive shows a menu before the one under it is erased. The close is written again when
a bounded flush dropped the frame, and once more when the terminal is handed back, because a
terminal left inside the block draws nothing further: an unclosed bracket freezes the screen.

`ktui_progress()` draws a solid whole-cell bar, and its output must not change: `kinstall` calls
it. `ktui_progress_ex()` is the general form, with the `KT_BAR_SOLID`, `KT_BAR_TIP` and
`KT_BAR_SEGMENTED` styles and the `KT_BAR_PULSE` flag; the build screen uses it. Change the general
form freely and leave the plain wrapper's output alone.

### Resizing, the caret and the pointer

A resize is not applied until the consumer applies it. The backend sets `ktui_resized`; the cell
buffer follows only when the loop calls `ktui_draw_resize()` and `ktui_draw_invalidate()`. Any
loop that owns a screen owns this. A screen that was always a fixed size and starts being resized
will draw against stale dimensions and silently fail its own bounds checks.

The caret goes to the backend when the backend has one. `ktui_term_caret()` is the one call a
program makes to say where it is typing, and it writes the terminal escape only when nothing else
claims the answer: a backend that leaves the `caret` entry NULL keeps the escape, which is what the
terminal backend does.

The pointer follows the same rule. `KtuiBackend.pointer` is handed the cell the pointer is on and
answers whether it drew a pointer itself; a backend that composites an arrow into a framebuffer it
owns says so. Everything else leaves the entry NULL, and `ktui_draw_flush()` reverses the cell
under the pointer, which is the pointer a terminal, a `--dump` and the console can show. An arrow
cannot be drawn in this library: it links nothing but musl, and an arrow needs a pixel buffer. The
hook is called on every flush, with a negative `x` for no pointer, because that call is the only
thing that tells a backend to take the last arrow off the screen.

Offscreen rendering takes a fixed size and writes the cell buffer out as plain text, with no
terminal at all. A geometry defect is invisible to the compiler and to a test suite that cannot
draw, so this is how one gets looked at; `kdosbuild --dump` draws one screen this way.

### Widgets

A widget is either an immediate-mode call or a draw-and-key pair, and its callers decide which.
`ktui_list`, the buttons and the checks read the frame's focus and return what happened in one
call, for a screen built around the frame. The slider and the text field are both: a
`_draw`/`_key`/`_hit` trio for a screen with its own loop, and a frame call (`ktui_slider`,
`ktui_input`) that is the trio with the focus ring around it, so the two kinds of caller share one
implementation.

A frame control is known by an integer id, and there are three kinds in three ranges. `ktui_id()`
at the top of the id stack is *positional*: the n-th control of the frame is id n. A control drawn
only some of the time therefore renumbers every control after it, and the focus and anything kept
per id go with the numbers. Such a group is drawn between `ktui_id_push("name")` (or
`ktui_id_push_int(n)` for a row of data) and `ktui_id_pop()`. Inside the scope `ktui_id()` is a
hash of the scope and a counter local to it, from `KTUI_ID_HASHED` up, and the positional counter
outside does not move, so what follows keeps its ids whether the group was drawn or not.
`ktui_id_str()` names a single control outright. Chrome ids sit between the two ranges, from
`KTUI_ID_CHROME`. Tab walks the ids in the order they were claimed that frame, whatever their kind,
and a focus the frame did not draw goes to whatever now stands where it last stood.
`ktui_id_base() + k` names the k-th control of a group only at depth 0, since inside a scope the
next id is a hash. `ktui_state(id, n)` keeps a zeroed record of up to `KTUI_STATE_MAX` (64) bytes per
id in a fixed 256-slot table; a record unasked-for in 600 frames can be reclaimed, and a full table
answers NULL rather than growing, so a caller keeps a fallback. `ktui_input` keeps its caret there.

The text field is a `KtuiField`: the caller's buffer and capacity, a byte caret, a secret flag and
a placeholder. `ktui_field_key` owns Left and Right (by word with Ctrl), Home, End, Backspace,
Delete, `Ctrl+U` (the whole line), `Ctrl+W` and `Ctrl+Backspace` (the word before the caret) and
every printable key that is not a chord, and returns `KTUI_FIELD_USED` and `KTUI_FIELD_CHANGED`
bits; Enter, Esc, Tab, Up, Down, every Alt chord and every Ctrl chord it does not own come back
`KTUI_FIELD_PASS`, so a `Ctrl+S` meant for the program never types an `s` and `Alt+Left` still
turns the page. The caret is clamped onto the text and back to a UTF-8 boundary on every call, so
a caller that loads a new value says "the end" by setting any large caret. A paste is a queue and
not an event: the field takes it on its next `ktui_field_key`, whatever the event, so a loop offers
the focused field every wake (NULL for a wake with no event). A secret field draws one bullet per
codepoint and is one word to the word keys, because stopping at its spaces would show where they
are. `ktui_field_col` is the caret's display column, for a screen that draws its own line and
places `ktui_term_caret()` itself.

The four views in `ktui_view.c` (the page strip, the column table, the dropdown and the text
block) and the menu split drawing from input instead: the strip, the table and the dropdown each
have a `_draw`, a `_key` and a `_hit`, the text block has `ktui_textarea_draw` and
`ktui_textarea_key`, and the menu has `ktui_menu_draw` and `ktui_menu_event`. They are split so
that a program running its own event loop and holding its own selection can use them without
being rebuilt around the frame. A hit test takes the same rectangle its draw took, so it measures
what is on the screen rather than what the widget remembered from an earlier size.

No widget owns its selection. The caller holds it, because the caller is what persists it, dumps
it and restores it, and because a page strip and the body under it are one selection seen twice.

A table's heading row says whether the selection may land on it; the row-kind callback answers per
row, and a row the selection steps over never lights. A table column carries flags. `KT_COL_SORT`
makes a press on its title sort by it and a second press reverse the order; `ktui_table_event`
returns `KTUI_TABLE_SORT`, and the title shows a direction arrow (`KT_G_UP` or `KT_G_DOWN`, `^` or
`v` on the console). The table never reorders the caller's records: `ktui_table_sort()` puts an
index array in the table's order with the caller's comparison, records that compare equal keep the
order of their indices, and the selection follows its record. `KT_COL_RIGHT` right-aligns the
title and every cell drawn with `ktui_table_text()`. `KT_COL_RESIZE` makes the cell right of a
fixed-width column a drag handle, and returns `KTUI_TABLE_RESIZED`. The widths it sets live in
`KtuiTable.w[]`, and every draw holds them to the width it lays out, so no column and no edge
leaves the rect. Every field of `KtuiTable` is zero in a zeroed table, which draws exactly as a
table with none of them.

A page's rows come from a layout cursor, a `KtuiLay` (`ktui_layout.c`). `ktui_lay_begin()` starts
it at the top of a rect, and `ktui_lay_row()`, `ktui_lay_gap()`, `ktui_lay_section()`,
`ktui_lay_field()` and `ktui_lay_cols()` each take the next rows and move it on;
`ktui_lay_indent()` and `ktui_lay_unindent()` move the rows after them in and out (by
`KTUI_LAY_INDENT`, four cells, the width of a check box's mark, unless told), and
`ktui_lay_left()` is the room left below the cursor. A field is its label in `KT_MID` in a column
`KTUI_LAY_LABEL` (16) cells wide, the key column `ktui_kv()` draws, and the rect for its control
one cell past that column; a NULL label keeps the column and draws nothing, for a meter under a
field. The cursor claims no id and reads no frame, and it draws through the calls a page would have
made itself, so a page moved onto it draws the cells it drew before; the installer's pages are laid
out through it. A row past the bottom of the rect is still handed out at its real position, because
clipping is the draw calls' business.

The splitter and the fold are frame controls that are named rather than counted: each claims
`ktui_id_str()` of its name and keeps what the hand did in `ktui_state()` under that id, so a
control coming and going above one does not hand its state to the next. `ktui_split()` cuts a rect
side by side (`KT_SPLIT_SIDE`) or stacked (`KT_SPLIT_STACK`) with a one-cell divider, which a drag,
the arrows along its axis and `Home` move. `ktui_fold_begin()` draws a section heading with its
marker and returns whether it is open; while it is open its body is drawn inside an id scope the
fold pushed, closed by `ktui_fold_end()`, so the body's controls come and go without moving an id
after the fold.

Three rules hold about the library's own state, each guarding a link or focus failure:

- Symbols are prefixed. Generic names collide with a consumer's own definitions of the same names
  with different semantics, which is enough to make two KDOS programs unlinkable together.
- The frame state is private, behind accessors. A public structure that applications assign to
  field by field is a second interface nobody can change.
- Chrome identifiers are the library's business. Chrome registers with caller-local identifiers in
  a reserved range that never joins the focus ring and never drags the page scroll. The hashed
  range sits above it, so neither kind of id can be taken for the other.

### The sprite table

A *sprite* is a picture occupying whole cells: the table holds an opaque pointer per slot, and each
cell carries the slot and its sub-cell position inside its codepoint. The table has
`KTUI_MAX_SPRITES` (4096) slots, and `ktui_sprite_budget()` adds a byte budget on top. Only a
backend that can draw pixels shows a sprite; the terminal backend draws the cell's fallback glyph.

A sprite table entry is a borrowed pointer, so eviction is whatever the owner told the table to do.
The table does no pixel work and cannot free a picture. An owner registers an evictor with
`ktui_sprite_evictor()`, and the table calls it whenever the table itself stops naming a picture: a
slot taken back under the byte budget, a slot reused for a different picture under the same key, or
a tile refused part-way through a tiled put. `ktui_sprite_drop` is the one way a picture stops
being named without the evictor, and the owner releases what it dropped. Without an evictor a full
table answers −1, which every consumer handles by drawing its glyph. A refused put is a hole only
the owner can see, so a caller never stores −1 as a slot. There is one evictor per process, shared
by every owner in it.

Whether a sprite is still on screen is asked of the cell buffer's own size, not of `ktui_w` and
`ktui_h`, because between a backend reporting a resize and the consumer applying it the globals
describe a grid larger than the allocation.

### Colour

One rule governs reducing a colour that came from outside the palette. A terminal's SGR colour and
a picture's average tint both land on the nearest of the theme's slots by squared distance, through
one function, `ktui_theme_nearest()`. A second implementation would drift, and a table saying "red
means the error slot" would be a second set of colour decisions beside the palette.

A program that picks its own scheme, as the installer and the build screen do, calls
`ktui_theme_set()`. `ktui_theme_load()` is the call for a program that follows the person's
accent: it reads the accent through `kcol_theme_name()` and sets that scheme, or the default when
no accent is recorded. It never warms the palette, because Night Light is KWin's and already warms
the whole screen, and a palette warmed again inside it would be cast twice.

`ktui_theme_night()` is the warm transform itself, for a caller that asks for it: it scales green
to 93% and blue to 77% of whatever `ktui_theme_set()` last loaded and leaves red untouched, which is
the shape of a lower colour temperature, so a warmed accent still reads as itself. It hands out a
copy and keeps the chosen scheme beside it, because eight-bit values do not divide back exactly.
The warmed copy is one buffer rewritten in place, so its address stays the same when the scheme
under it changes; anything that caches work per palette (the terminal backend's SGR table is one)
compares the eight slot colours, never the pointer.

### Announcements

A widget states what it is, because it already knows. Every widget computes which item has focus
each frame from the same identifier its hit test uses, so a reader working that out again from a
grid of cells would be guessing at what the program has in hand, and it would guess wrong first on
the controls that matter most: which cell of a table, which tab of a strip, which item of how many.
`ktui_announce()` takes a role, a label, a value and the item's position in its set, so "3 of 9" is
a fact the widget states rather than a count somebody has to make.

- It lives here, not in a program. `ktui_announce_count()` and `ktui_announce_at()` are the one
  place to read it.
- The queue is per frame, holds `KTUI_A11Y_MAX` (16) records, and is cleared at the start of every
  frame. Nothing on the draw path allocates, and a frame with more to say than the queue holds
  drops the rest.
- Silence is the failure mode, never a stale name. A widget that says nothing announces nothing; a
  reader told the wrong control is worse off than one told nothing.
- A widget says what it knows and no more. A tab strip has its names and says them, and so does a
  menu; a list and a table take their rows from the caller's callback, so they state the position
  and leave the name to a program that has it. A secret field announces that it is one and never
  its contents.
- A position counts what a caret can reach, so a menu's separators and hidden rows are counted
  neither in the ordinal nor in the total.

No program reads the queue: the widgets fill it every frame, and the self-test is what pins its
contents. What a screen-reader user has in the text installer is described in
[Accessibility](../02-user-guide/accessibility.md).

### The cell's attribute byte

A `KtuiCell` holds a codepoint, a foreground and a background slot, a sixteen-bit attribute and
three literal colours (`fgc`, `bgc`, `ulc`). The low six bits of the attribute are styles: bold,
reverse, underline, italic, strikethrough and overline. All of them but reverse are dropped on a
real VT, where an attribute bit selects a font page or a colour the palette does not own rather
than a style. The seventh bit, `KT_A_GUEST`, marks a cell holding an embedded program's pixels with
that program's own cursor composited in, and the flush, its only reader, then reports no pointer
over it.

The high byte says which of the three literal colours mean anything (`KT_A_FGRGB`, `KT_A_BGRGB`,
`KT_A_ULCOLOR`) and, in three bits, what shape the underline is (plain, single, double, curly,
dotted or dashed, in SGR `4:0` to `4:5` order). A consumer drawing in slots alone honours only the
low byte, and every cell keeps a slot to fall back on, so it can never receive a cell claiming a
colour it never got. There is one bit per colour rather than one for the pair, because a single bit
would freeze the theme's background into a cell as a literal, after which a change of accent leaves
a rectangle of the old scheme behind.

A caller's own literal reaches a frame only through `ktui_draw_put()`, which copies whole cells.
`ktui_draw_blend()` and `ktui_draw_shadow()` compute their literals by mixing colours already in
the frame rather than taking a colour of the caller's choosing. Every other draw call takes slots
and clears the literals, because a frame that stopped following the scheme would be a second
palette nobody can change.

### Two input queues

The same physical input can travel twice. `poll_event` answers a character and a cell.
`KtuiBackend.poll_raw` answers a `KtuiRaw`: an evdev keycode with a separate press and release, the
xkb modifier mask and group, a pointer position in the backend's own pixels together with the cell
those pixels were measured in and both deltas the device reported, and a scroll with a second axis.
`KtuiBackend.keymap` hands over the backend's compiled layout as xkb text with a generation
counter. The raw queue exists for a host that forwards input to an embedded program, which needs
real key codes and pointer distances rather than characters and cells. A backend with no input
device of its own, the terminal backend among them, leaves both NULL, which is how a consumer knows
not to claim the capability.

There are two queues because motion coalesces and a key must not. In the raw queue consecutive bare
motions merge into one at the newest position with their deltas summed, and nothing else merges.
The cooked event for one physical input is queued before its raw partner, and `KtuiRaw.after`
counts the cooked events that must be taken before that raw one is sent; draining one queue to
empty and then the other loses that order.

### Motion

A `KtuiAnim` (`ktui_anim.c`) is a value that moves from one number to another over a fixed time:
a start time, a duration, a curve (`KT_EASE_OUT`, `KT_EASE_IN_OUT` or `KT_EASE_LINEAR`, each a
cubic or a straight line, so nothing links libm) and a count of beats, where a pulse goes there and
back that many times and ends where it began. `ktui_anim_start()` starts one and
`ktui_anim_value()` is what it is worth now; nothing keeps a list of them, so one that is dropped
mid-way costs nothing. `ktui_anim_running()` and `ktui_anim_left()` answer whether it is still
inside its time, and `ktui_anim_stop()` settles it.

An animation moves only where the backend keeps a frame clock (`KtuiBackend.animates`) and motion
is on. Everywhere else, which is the terminal and every `--dump`, it is worth its end value from
the moment it starts, so no frame from the middle of one reaches a dump or the console. Whether
motion is on is asked through `ktui_anim_set_motion_fn()` at each start; a host with a
reduced-motion setting registers its reader there, and with none registered it moves.
`ktui_anim_set_clock()` replaces the clock for a test. `ktui_anim_live()` answers whether the
latest end of any animation started while it could move is still ahead, which is the only question
a backend with a frame clock asks.

## libkxdg

`libkxdg` reads desktop entries and the MIME glob table, resolves what handles a type, splits
command lines, and keeps recent files. Its main consumer is `kdos-appbox`, whose `open` is the
`xdg-open` of the host and the opener of every box; `kdos-runner` and `kdos-notice` read desktop
entries through it, and `kdos app` writes launchers with it.

The splitter, `kxdg_exec_split`, is the single implementation of turning a desktop entry's command
into an argument vector. It unquotes, substitutes the file-argument codes, drops the codes that
carry no argument, and, given a negative count, keeps every code verbatim for a tool that rewrites
a line rather than running one. Its inverse, `kxdg_exec_quote`, re-quotes, so a generator's output
round-trips. Every launch path in KDOS's C code goes through it. See
[kdos-appbox](../04-programs/kdos-appbox.md#exec-lines).

`kxdg_launch_read()` is the single reader of the keys that decide how to start something: `Exec`,
`Name` and `Terminal`. It is the "can this entry be started at all" test as well (it answers −1
for anything that is not `Type=Application` with an `Exec`), so a caller's check and its read are
one call. `NoDisplay` and `Hidden` are not among those keys. They say whether an entry belongs in a
menu, which is a question for whoever draws one: the MIME route opens a `NoDisplay` entry on
purpose, and a shared reader that refused one would break every default handler that carries it.

The `Exec` line is copied with its field codes intact. The splitter spends them on the documents a
launch carries, and reads the same codes to decide that a line carrying none takes its documents
appended instead, the same decision KIO's launcher makes, so a reader that stripped them here would
make every entry look like `Exec=xterm` and pass a file in the wrong position.

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

### What opens a type

`kxdg_handlers()` answers who handles a MIME type, in freedesktop's order: the user's
`[Default Applications]`, then the user's `[Added Associations]`, then the shipped defaults in each
`$XDG_CONFIG_DIRS` directory in turn (`/etc/xdg` when the variable is unset), then each
`applications/mimeinfo.cache`. At each level the desktop-named list (`kde-mimeapps.list` under
Plasma, from `kb_desktop_prefix()`) is read ahead of the plain `mimeapps.list`. The shipped
defaults live in more than one of those directories, the administrator's `/etc/xdg` ahead of
KDOS's `/usr/share/kdos/xdg`, so a resolver that read `/etc/xdg` alone would miss the desktop's
whole default table. Only installed entries count, each once, so a list naming a removed
application falls through to the next. Every candidate carries the layer that named it, and
`kxdg_layer_name()` turns a layer into the words a person reads ("your default", "KDOS default",
"installed").

`kdos-appbox open` acts on the answer. A default, or a single candidate, opens without asking; two
or more with no default are offered as a choice. Plasma's own launcher reads the same files by the
same specification, so a document opened from a prompt and one opened from Dolphin go to the same
program.

### Recent files

`kxdg_recent_add()` is called from one place, `kdos-appbox open`, which every open through
`xdg-open` passes through. It is the same scanner as the reader run backwards: the bookmark this
URI already had is cut out whole and a fresh one appended, so a file opened twice is one entry and
it is the newest. The writer keeps at most 60 bookmarks and drops the oldest on the same pass. The
rewrite is by temporary file and rename, because `recently-used.xbel` is shared with every other
program on the machine that keeps recent files, and every value is XML-escaped on the way in and
unescaped on the way out.

`kxdg_recent()` reads one application's recent files, newest first, and `kxdg_recent_all()`
everyone's. Only paths that still exist are returned and duplicates are collapsed. The read memoises
the parse on the store's size and modification time, and never the existence check: nothing about
the store changes when a file is removed, so a cached row would go on offering a destination that
opens nothing, and a file that comes back returns to the list.

## libkpkg

`libkpkg` holds the package manager's configuration, the package database, the ports tree, the
solver, version comparison, the facet rules, the two package hashes and the content-keyed package store
(`kp_store.c`), which installs a port from a package built earlier from exactly the same inputs
instead of building it again (see
[Packaging](../03-architecture/packaging.md#the-package-store)).

It is the one place a port is found by name, and every tool that looks a port up goes through it:
`kpkg`, `kdos update`, `kdos cve`, `kdos why`, `kdos march`, `kdos-portup`, `kdosbuild` and the
build plan's port index. A repository on `PORT_REPO` holds each port at `<repo>/<name>/` or one
shelf down at `<repo>/<shelf>/<name>/`; `ports/core` is shelved and each `src/` area is flat (see
[Shelves, and how a port is found](writing-ports.md#shelves-and-how-a-port-is-found)).

| Function | Does |
|---|---|
| `kp_conf_set_repos(c, list)` | Replaces the repositories with a whitespace-separated list, lists each one's shelves once into the `KpConf`, and returns how many it kept. A list past `KP_MAX_REPOS` (8) warns and drops the rest, since a repository that is not kept is one whose every port reads as "no such port" |
| `kp_port_find(c, name, &dir, err, errcap)` | 1 with the port's directory, from the first repository that holds the name; 0 when none does; -1 when one repository holds it at two paths, with `err` naming both. A name containing `/` is tried only as a path under each repository |
| `kp_port_dir(c, name)` | `kp_port_find` for a caller that reads NULL as "no such port". A name filed twice dies with the message, because every answer a caller could make of NULL there (skip it, build without it, take one copy) builds something other than the tree says |
| `kp_ports_scan(c, &count, err, errcap)` | Every port of every repository, sorted, at both depths; the first repository wins a name two of them hold. NULL with `err` set when one repository holds a name twice or a port sits deeper than one shelf. A repository that exists and yields no port warns, since that is a walker and a tree that disagree about the layout |
| `kp_all_ports(c, &count)` | `kp_ports_scan` that dies on a malformed tree |
| `kp_depends_scan(portdir, out, max, err, errcap)` | The names on a recipe's first `depends` line, split on spaces only, and how many there are; 0 when the port has no recipe. -1, with `err` set to `<port>: depends names N ports; the limit is <max>`, when the line names more than `max` |
| `kp_depends(portdir, out, max)` | `kp_depends_scan` that dies with that message, because every caller reads its return as the whole closure, and a short one builds without the rest |
| `kp_owned_load(c)` | Every path an installed package claims, sorted, keyed on its canonical merged-`/usr` spelling, for `kp_owned_owner` and `kp_owned_other` to ask. A path several packages claim is sorted by package name, and `kp_owned_owner` names the first, so the owner does not depend on the C library's `qsort` |
| `kp_owned_load_some(c, rel, n)` | The same table holding only the claims on the `n` given paths, canonicalised the way a question is. Those two functions answer exactly as from the full table for those paths, and NULL for any other. It streams every manifest but keeps a handful of pairs, which is what an install, an upgrade's orphan sweep and a removal need |
| `kp_facet_of(path, link)` | The [facet](../03-architecture/packaging.md#facets) of a manifest path, `KP_FACET_RUN`, `KP_FACET_DEV` or `KP_FACET_DOC`; `link` is a symlink's target, NULL for anything else, and decides whether `lib<x>.so` is a development link. A directory is always run time. The one statement of the rules: `kpkgadd`, `kpkg facets --strip` and the self-test's table all go through it |
| `kp_facet_wanted(c, port)` | The installed packages a build of `port` compiles against whose `dev` facet is left out: its declared closure, `musl`, `linux` and `gcc`. `kpkg install` refuses a build while it names any, and `kdos update install --source` restores them. A database with no `.facets/` directory answers empty without walking the closure |
| `kp_record_facets`, `kp_installed_facets`, `kp_forget_facets`, `kp_facets_left` | Write, read, remove and count the facet record, `<db>/.facets/<name>` |
| `kp_needs_scan(c, name, body, extra, runtime)` | The [run-time needs](../03-architecture/packaging.md#run-time-needs) record of installed package `name` from its run-time files under the root: `body` its manifest, `extra` its recipe's `facet_dev =` line, `runtime` its `runtime =` line; sorted `<kind> <value>` lines |
| `kp_needs_where(c, edge, &pkg)` | The paths under the root, each present, that would provide one edge (`so libz.so.1`, `qml org.kde.kirigami`), and `pkg` set when a package name provides it instead (a `need` edge, a QML built-in) |
| `kp_record_needs`, `kp_installed_needs`, `kp_forget_needs` | Write, read and remove the needs record, `<db>/.needs/<name>` |
| `kp_elf_dynamic(path, &needed, &search)` | An ELF64 file's `DT_NEEDED` names and its `DT_RUNPATH` (else `DT_RPATH`) entries, read from its dynamic section with no process; -1 for anything that is not such a file |
| `kp_db_lock(c)` | Takes the database's writer lock, an exclusive `flock` on `<db>/.lock`, blocking, and returns the descriptor; closing it releases the lock. -1 when the database directory cannot hold the file |

A recipe's `depends` line is bounded by `KP_MAX_DEPS` (128), and a line naming more is refused,
never read short. Every name is counted, not only the ones that fit: the first 128 alone would be
a closure the build trusts and `testing/phaseclosure.py`, which reads every name, passes.
`testing/preflight.sh` fails the same recipe before a build reads it. A meta port that would
outgrow the limit is split into metas that each fit (see
[Writing ports](writing-ports.md#a-meta-port)).

One file can have two spellings on a merged-`/usr` root: a package built with `--exec-prefix=`
records `./bin/free` and one built with `--prefix=/usr` records `./usr/bin/free`. `kp_canon_load()`
reads which top-level names (`bin`, `sbin`, `lib`, `lib64`, `lib32`) are links into `usr/` on the
install root, and `kp_canon_path()` rewrites a path to its canonical spelling. Every ownership
question (the conflict scan, a declared takeover, an upgrade's orphan sweep and a removal)
compares canonical spellings, because otherwise a shared file is invisible to the scan, claimed
twice, and deleted by whichever package is upgraded or removed next. The aliases are read from the
root rather than assumed, because a root whose `bin` is a real directory holds two different files
under those two names. See [Who owns a file](../03-architecture/packaging.md#who-owns-a-file).

A shelf is any directory of a repository with no `kpkgbuild` of its own, other than a dot-name. The
shelf names are cached per repository when the repositories are set, so a shelf created afterwards
is not seen until they are set again. No shelf may be named `libs`: a source-less port's recipe
hash covers `<portdir>/../../libs`, which for a port under `ports/core` is `ports/core/libs`, and a
shelf by that name would be hashed into every such port.

Version comparison (`kp_vercmp`) and the version-shape filter (`kp_vershape`) live here rather than
in any one consumer, because several programs ask the same question of the same strings and two
implementations would eventually disagree: `kdos-portup` asks whether upstream is newer than the
pin, `kdos cve` whether the pin is older than the version that fixed a flaw, `kdos update` whether
a recipe is newer than what is installed and whether an offered slot image is newer than the
running release, and `libkpack`'s solve and `kdos-packd` which pack version is the newest. Both carry
`libkpkg`'s `kp_` prefix even though `kdos-portup` is the shape filter's only caller: a symbol
exported under a consumer's prefix is one the next reader looks for in the wrong library, and two
libraries that each define a generic name cannot be linked into one program.

The recipe hash of a port under `src/` with no `source =` covers its own directory and all of
`src/libs/`, as described under [How the libraries are built](#how-the-libraries-are-built). The
hashes and their three states are in
[Packaging](../03-architecture/packaging.md#deciding-what-to-rebuild).

## libksig

`libksig` signs and verifies the package index, packs and slot-image manifests with Ed25519, and
it is the one library with vendored third-party source.

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

A keyring is every `*.pub` in its directory, read in name order, and holds at most `KSIG_MAX_KEYS`
(16). `ksig_ring_load` names on stderr each `*.pub` past the cap and does not trust it, so the
owner of a key whose signatures fail is told why, and which keys miss out is the same on every run.

## libkbuild

The deciding half of the build orchestrator: phase discovery, the phase metadata block, the build
plan, the snapshot inventory with the chains of layers each restore reads, the index and diff that
decide what a layer holds, and `kj_*`, a JSON reader that refuses a document that does not parse
whole. `kdos-tools` and `script/release-image.sh` compile the JSON reader (`kb_json.c`) alone.
Covered in [The build system](build-system.md#libkbuild).

## libkproc

`libkproc` takes every reading about the running machine, from a root that can be moved.

The movable root (`kpr_root_set()`) is what makes the memory daemon and the energy daemon testable
against recorded system state: the self-test replays `testing/fixtures/res`, a recorded `/proc` and
`/sys`. A tool whose readings cannot be replayed cannot be tested at all.

User names are the one reading that does not move with the root. `kpr_user_of()` reads the file
`$KPR_PASSWD` names when it is set, and `/etc/passwd` otherwise; a test that moves the root without
setting it resolves names from the host it runs on.

Elapsed time may only be computed against the system uptime. A process's start time and the uptime
are both seconds since boot; pairing the start time with a monotonic timestamp mixes two clocks,
and under a fixture, two machines.

The box identity walk turns a process id into a box name by walking the parent chain up to
podman's per-container supervisor, `conmon`, and reading the `-n` name from its command line.
`kpr_box_of()` answers from a sample's process table and `kpr_conmon_name()` from a known `conmon`;
`kpr_box_of_pid()` reads `/proc` as it climbs, for a caller with no table of every process, which is
`kdos-oomd`'s case. `kdos-energyd` climbs with a walk of its own, because it also decides which
application a process rolls up to, and takes the boundary from `kpr_is_box_boundary()` and the box
name from `kpr_conmon_name()`. The boundary and the climb limit (`KPR_BOX_HOPS`) are here, so the
two daemons cannot disagree about where a box begins. The
conmon lookups are cached for one sample and emptied by `kpr_conmon_forget()` at the start of the
next.

Graphics readings come from the kernel's DRM interface (`kpr_drm_list()`). Only amdgpu and NVIDIA's
management library publish a utilisation figure; everywhere else `busy_percent` is −1, and a
reader must show engine time and label it as such. The NVIDIA library is opened with `dlopen`
when `libnvidia-ml.so.1` is present, which it is not on a stock KDOS, so it adds no link
dependency.

A reading goes into a caller's buffer wherever a buffer bounds the file. A process walk on a busy
machine opens a few thousand files a sample, and a whole-file read pays an `fstat`, a heap block and
a second `read` to confirm end of file for each of them. `kpr_read_into_proc()` and
`kpr_read_into_sys()` are the form to use. The whole-file readers (`kpr_slurp_proc`,
`kpr_slurp_sys`) remain for `/proc/cpuinfo`, `/proc/stat` and a command line, which no buffer
bounds: a full buffer means the content may be cut, and a cut value parsed as though it were whole
is a wrong reading rather than a missing one, so that case is re-read on the heap.

What cannot change is latched, and every latch is keyed on `kpr_root_gen()`. A CPU topology, a
model string and a disk's rotational flag are constants that would otherwise cost two files per
logical CPU and a `/proc/cpuinfo` of 26 KB on sixteen cores to rederive on a sample that only wanted
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
arrays four long and `online[1]` clear. A per-core reading walks `ncpu` and skips the clear slots;
sizing by the number of lines instead drops the highest CPU's times altogether and leaves a phantom
core reading zero. Anything that wants the count calls `kpr_cpu_online()`.

`libkproc` links `libkbase` and nothing else, which is what lets a root daemon take it. Two root
daemons do, `kdos-oomd` and `kdos-energyd`, and a daemon deciding under memory pressure must never
gain a dependency that can block. `kdos-tools` reads the power supplies through it
(`kpr_power_list()`). The rest of the library (the sample ring, the network, sensor, sound and
history readers) is called by nothing outside its self-test.

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

## Adding a library

1. Decide what it owns. A library is split where its consumers differ, not by topic: `libkbase` is
   broad because everything may link it, while `libkproc` is separate so that `kpkg` does not carry
   the machine readers and `libkpack` so that the installer does not carry the pack format.
2. Pick a prefix and use it on every exported symbol.
3. Place it in the dependency order and confirm nothing points back up.
4. Link nothing but the C library (see [The constraint](#the-constraint)). A program that needs an
   external library names it on its own link line, and the library stays usable in `10_bootstrap`
   and in a root daemon.
5. Add its sources and include path to the `build.sh` (or `CMakeLists.txt`) of every program that
   uses it. There is no archive to link; a program compiles the library's `.c` files itself. Check
   the consumers built outside a recipe as well, each of which names its libraries explicitly (see
   [How the libraries are built](#how-the-libraries-are-built)):
   - `script/phases/10_bootstrap/120_kpkg.sh` (`kpkg`) and
     `script/phases/10_bootstrap/130_kinstall.sh` (`kinstall`), the bootstrap builds. A library
     added under either program and missing here breaks the bootstrap. `kinstall` is also built as
     a port by `src/system/kdos-installer/build.sh`, and the two must compile the same sources.
   - `script/kdosbuild.sh`, which builds `kdosbuild` on the host.
   - `script/release-image.sh`, which builds a scratch `kdos` command on the release host.
   - `src_kpkg_ensure` in `ports/srclib.sh`, the host recipe reader. Its source list, the one in
     `src/devtools/kdos-portup/main.c` and the one in `testing/selftest.sh` must agree.
   - `ports/update`, which builds `kdos-portup` on the host.
6. Add its assertions to `src/libs/selftest.c`, especially any invariant established by comparing
   against a reference implementation.
7. Add it to the consumer compile block in `testing/selftest.sh` (the step headed "every consumer
   still compiles against the libraries"), so a header change that breaks a consumer fails on a
   development host rather than hours into a build.
8. Keep the frame state and any global private, behind accessors.
9. Add a row to [The set](#the-set) and a section to this chapter.

## See also

- [Writing desktop software](writing-desktop-software.md) — KDOS's Qt, QML and KWin code, and the
  C helpers it starts
- [The design language](../03-architecture/design-language.md) — the palette's roles and the glyph
  tiers the terminal programs draw with
- [Testing](testing.md) — the shared test program, the fixtures and the sanitizer runs
- [The build system](build-system.md) — where `libkbuild` fits, and what `10_bootstrap` compiles
- [How KDOS is built](how-kdos-is-built.md) — the whole build, from the cross toolchain that
  compiles `kpkg` and `kinstall` to the userland and desktop phases that compile the rest
- [How KDOS differs](../01-philosophy/how-kdos-differs.md#the-c-library) — musl, the C library
  every one of these libraries is compiled against, and what choosing it costs
- [The daemons](../04-programs/daemons.md) — the root daemons these libraries run inside
- [Packaging](../03-architecture/packaging.md) — where `libkpkg` and `libksig` fit
- [Packs and boxes](../03-architecture/packs-and-boxes.md) — the format `libkpack` implements
- [Glossary](../06-reference/glossary.md) — sprite, slot, pack, box and the other terms used here

<!-- book-nav -->
---

*Part V — Building and developing, chapter 35.* Previous: [34. Build troubleshooting](build-troubleshooting.md) · [Contents](../README.md) · Next: [36. Writing desktop software](writing-desktop-software.md)
