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
| [No systemd init, units or service manager](#no-systemd-init-units-or-service-manager) | One component owning init, services, logging, devices and the network | elogind's systemd-derived C on the host; no socket activation and no per-application scopes |
| [No Xorg server](#no-xorg-server-and-one-carve-out) | A second display stack on the login path | X-only applications run through Xwayland, and there is no X11 session |
| [The desktop is KDE Plasma](#the-desktop-is-kde-plasma) | A desktop KDOS maintains alone, and a GUI stack under KDOS's code that runs as root or before the login | Qt and KDE Frameworks under the desktop; KDOS's Plasma plugins rebuilt with every Plasma release |
| [Built from source](#everything-that-runs-on-the-host-is-built-from-source) | Binaries nobody can read or rebuild | Features that exist only as upstream binaries are absent |
| [Offline by construction](#offline-by-construction) | Builds that work once and fail a year later, and a machine that needs the network to stay complete | Every dependency a build would download is bundled in advance; there is no Flathub and no "Get New…" |
| [Reproducible by construction](#reproducible-by-construction) | Signatures and deltas over packages nobody can re-derive | A recipe never rolls its own archive |
| [One implementation of an idea](#one-implementation-of-an-idea) | Two answers to one question that drift apart | Indirection |
| [Absent, never partial](#a-thing-that-does-not-parse-whole-is-absent-never-partial) | Confident action on half-read data | No salvage of a damaged file |
| [Never invent a number](#never-invent-a-number) | A missing reading presented as a real one | Reports leave out what other tools would guess |
| [Measure rather than assume](#measure-rather-than-assume) | Claims nobody checked | Measuring is slow |
| [Say what cannot be enforced](#say-what-cannot-be-enforced) | Confinement that exists only in a settings file | A shorter feature list |
| [Documentation describes the present](#documentation-describes-the-present) | Comments that narrate a past nobody can act on | History lives only in git |
| [Documentation changes with the code](#every-change-updates-its-documentation-in-the-same-change) | Pages that contradict the tree | No change is ever only a code change |

<a id="no-systemd"></a>

## No systemd init, units or service manager

PID 1 is sysvinit, the system's services are numbered shell scripts that `ksvc` supervises, and a
user's services run under `ksvc` too. No unit file is installed anywhere: `kpkg` refuses a package
that puts anything under `usr/lib/systemd` or `lib/systemd`, so a recipe whose upstream installs
units deletes them. Each job systemd would do is done by a separate program that does only that
job, and each of those programs is a port (a directory holding the recipe that describes how to
build one package) that can be read and replaced on its own.

| Function | KDOS uses | Not |
|---|---|---|
| Init and service supervision | sysvinit's `init` as PID 1, with runlevels in `/etc/inittab`; `/etc/init.d/rcS` running the numbered scripts in `/etc/init.d`, with daemons kept alive by `ksvc` | units and targets |
| Logins, seats and sessions | `elogind`, which serves `org.freedesktop.login1` | `systemd-logind` |
| Per-user services | `turnstile`, whose backend runs the scripts in `/etc/kdos/user-services/` under `ksvc`: the session bus at `$XDG_RUNTIME_DIR/bus`, PipeWire, WirePlumber, `pipewire-pulse` and the user's timers | `systemd --user` |
| sd-bus API for programs that want one | `basu` | `libsystemd` |
| Device management | `eudev` | `systemd-udevd` |
| Message bus | `dbus` | `dbus-broker` |
| Host name, time zone, locale and keyboard layout | `kdos-settingsd`, serving `org.freedesktop.hostname1`, `timedate1` and `locale1` | `systemd-hostnamed`, `-timedated`, `-localed` |
| Operating-system updates | `kdos-sysupdated`, serving `org.freedesktop.sysupdate1` over the two root slots | `systemd-sysupdate` |
| Network | NetworkManager with `wpa_supplicant`; `dhcpcd` when NetworkManager is turned off | `systemd-networkd` |
| DNS | `dnsmasq`, which NetworkManager starts as a local caching resolver on `127.0.0.1` and `::1` | `systemd-resolved` |
| Time synchronisation | `chrony` | `systemd-timesyncd` |
| System log | `sysklogd` | `journald` |
| Crash records | a core dump; KCrash runs with no crash handler | `systemd-coredump` and DrKonqi |
| Scheduled jobs | one `snooze` per job, supervised by `ksvc`: the system's started by `rcS`, a user's by their own services; `cronie` for the crontabs users write, which System Settings' Task Scheduler edits | timer units |

`ksvc` is part of `kdos-tools`. It makes each supervisor the leader of its own process group, with
the daemon inside it, so that stopping a service signals the whole group and reaches the daemon and
not only its supervisor, and it passes the daemon command as an argument vector rather than as a
string for a shell to split. The init scripts are shell, because a list of services and their
start order reads most clearly as a script, and `ksvc` supplies the process handling a script
cannot. Run by a user, it keeps its state under `$XDG_RUNTIME_DIR/ksvc` rather than `/run`. See
[Boot and init](../03-architecture/boot-and-init.md#rcs-and-the-service-scripts).

Three programs on the host come out of systemd's source tree and run on their own: `eudev` for
device events, `basu` as the sd-bus library, and `elogind` for logins. elogind is the large one,
about 226,000 lines of C, built with the patches Alpine carries for musl, and it is there because
Plasma is built on `login1`. SDDM records each session through it and switches users through it;
KWin opens the display and the input devices through the session elogind records, and has no other
session backend that works here; PowerDevil suspends the machine and owns the lid through it; and
the lock screen locks when elogind announces a sleep. elogind runs under `ksvc` like any
daemon, and its power-off and reboot paths are sysvinit's, so leaving Plasma with Shut Down still
goes through runlevel 0 and `rcK`.

turnstile does the part of `systemd --user` that Plasma needs. It counts a user's logins across
SDDM and the text consoles, creates `/run/user/<uid>` at the first and removes it at the last
(unless the user lingers), and has its backend start that user's services and report them ready
before the login continues, so the session bus and PipeWire answer before Plasma starts. Plasma
then takes its classic start-up path, `systemdBoot=false` in `startkderc`, and starts its own
processes, as it does wherever systemd's user manager is absent.

A KDOS daemon may serve a freedesktop D-Bus interface that systemd also serves, and it documents
the subset it answers. `kdos-settingsd` serves `hostname1`, `timedate1` and `locale1`, because the
Date & Time settings page saves only through `timedate1`, plasma-setup uses all three, and the login
screen's compositor reads the keyboard layout from `locale1`. `kdos-sysupdated` serves
`sysupdate1`, which is how Discover offers an update of the operating system. Owning such a name is
not running systemd: no unit, generator or service manager stands behind it.

Rootless podman applies a `--memory` or `--cpus` limit through a cgroup2 subtree that the user
owns, and without one it accepts the flag and silently does nothing. elogind hands that subtree out:
with `DelegateSessionCgroups=yes` each session's cgroup is `/sys/fs/cgroup/<session>`, and the user
owns the `session` and `delegated` cgroups inside it, so a box started from the session gets a
limit it can enforce. At boot, `/etc/init.d/15_cgroups.sh` enables the `cpu`, `memory` and `pids`
controllers at the root, so the delegated subtree has them to hand on.
[`kdos-oomd`](../04-programs/daemons.md#kdos-oomd) stays as the second line: it watches memory
pressure through `/proc/pressure/memory` and, when the machine stalls while `MemAvailable` is at
most a tenth of `MemTotal` ([how it decides](../04-programs/daemons.md#how-it-decides)), kills the
largest process that is neither the desktop nor its plumbing. A box over the `memory` budget its
profile declares is chosen first, so the profile key still means something where the limit itself
could not be applied. Otherwise the largest boxed process is preferred to a host one whenever it
holds at least half the memory of the largest candidate.

The price is elogind's systemd-derived code on the host, and what Plasma does without systemd's user
manager. There is no socket activation. The systemd user units KDE's packages install are deleted,
and KIO starts applications without systemd scopes, so System Monitor cannot group processes by
application. DrKonqi, KDE's crash reporter, needs `systemd-coredump` and is not ported, so a crashed
program leaves a core dump and no dialog. A service that expects socket activation or a unit of its
own has to be run without them.

## No Xorg server, and one carve-out

There is no `xorg-server` port, nothing X on the login path and no X11 session. The system files
under `fs/` contain no `/etc/X11/` directory, a change that creates one is rejected in review, and
`kpkg` refuses a port whose package installs anything under `etc/X11`.

The display manager is SDDM, built against elogind, and its greeter runs on `kwin_wayland`
(`DisplayServer=wayland` in `/etc/sddm.conf.d/00-kdos.conf`). The greeter's KWin is started without
`--xwayland`, so no X server runs before somebody logs in, and SDDM's X11 session directory is
empty, so the session list offers Plasma on Wayland and nothing else. SDDM itself links `libxcb`,
an X client library, which the rule allows.

One display server, KWin, owns the screen, the input devices and the login path. An Xorg server
would be a second complete display stack, with its own input handling, its own drivers and its own
privileges, to build, secure and keep consistent with the first. See
[How KDOS differs](how-kdos-differs.md#the-display-stack) for how other distributions carry both.

Xwayland is the single exception, and the one X server. KWin starts it in a user's session, and runs
it rootlessly for every X11 client: applications inside boxes (the rootless containers the
catalogue's applications run in) and host applications that have no Wayland path, such as Xastir,
VLC 3 and programs written in Motif, Tk or Java's Swing. Any application may link the X client
libraries it needs, and the host carries them: the `x11` shelf holds the X client libraries
(`libXext`, `libXi`, `libXrandr`, `libXt`, `libXft` and the rest), the `toolkits` shelf holds
`motif`, and the X11 backends of Mesa, GTK, Qt and mpv are built against them beside their Wayland
ones. Xwayland itself depends on `xorgproto`, `xtrans`, `libXau`, `libXdmcp`, `libX11`,
`libxkbfile`, `xkbcomp`, `libxshmfence`, `libXfont2`, `libxcvt`, `libepoxy` and `xkeyboard-config`.
All six `xcb-util` ports are built, and all six are named in the `41_system` phase's list, in its
`x11` shelf file. What stays out is a second X server, an X11 session and anything X on the login
path.

Two consequences follow for X clients.

- **GLX comes from Mesa and Xwayland together.** Mesa is built with `-D glx=dri` and
  `-D platforms=wayland,x11`, which installs `libGLX_mesa.so` behind libglvnd's `libGL`, and
  Xwayland with `-Dglx=true`, which is the server side of the extension. An X11 client that asks
  for GLX finds it, and a Wayland client never loads the GLX library. An X11 client in a box uses
  the box's own Mesa over Xwayland's DRI3.
- **The X core fonts are host ports.** `font-misc-misc`, `font-adobe-75dpi` and
  `font-cursor-misc`, and the tools that build them (`bdftopcf`, `font-util`, `mkfontscale` and
  `encodings`), are all named in the `41_system` phase's list, in its `fonts` shelf file.
  A Motif or Xt program, native or boxed, asks the host's Xwayland for `-misc-fixed` or
  `-adobe-helvetica`, and a font directory inside a box is invisible to a server running outside
  it.

## The desktop is KDE Plasma

The desktop is KDE Plasma on KWin, Wayland only, with KDE Frameworks under it and KDE Gear beside
it. KDOS does not write a desktop of its own. What it adds goes where Plasma already loads code
from: System Settings modules (KCMs), plasmoids, a KRunner runner, KWin effects and scripts, the
window decoration's settings, Discover backends, the look-and-feel, Plasma style and SDDM theme
packages, and the graphical front end of the installer. That code lives under `src/desktop/`, is
written in C++ and QML against Qt 6, KDE Frameworks 6 and Plasma's own libraries, and links no other
toolkit. [Decisions](decisions.md#kde-plasma-and-all-of-gear-as-the-desktop) says why Plasma and
not a desktop of KDOS's own.

The line runs between the desktop and what has to keep working without it. KDOS's own code under
`src/system/`, `src/daemons/` and `src/art/`, every `libk*` library, and every KDOS program that
runs as root or before the login links no toolkit at all: `kpkg`, the `kdos` command and `ksvc`,
the root daemons, `kdos-boxsock`, the boot splash and the installer's backend. These are what a
person repairs the machine with when the desktop is broken, from the text console on the second
virtual terminal or a serial line, so a broken Qt must not take them down with it; and a root
process that linked a toolkit would put that toolkit's parsers on the privileged side. The root
side of every KDOS settings page is one of those daemons, reached over its socket or D-Bus and
authorised through polkit, never a Qt helper running as root. The login screen and the helpers
KDE's own settings pages start as root are upstream's, and they do link Qt.

KDOS's terminal programs draw a grid of character cells with [libraries written for
it](../05-developer/c-libraries.md): the installer's text front end, used on a serial console and
for unattended installs, the build's start-up picker, `kdosbuild`, and `kdos-appbox`. `libktui`
composes the cells and knows nothing about where they go, and a terminal receives them as escape
sequences. The boot splash, `kdos-splash`, runs from the initramfs before any of that exists and
links none of the libraries: its own code writes PSF glyphs and pixels straight to `/dev/fb0`,
taking only the colour table from the `libkcolor` header.

Where Plasma has to behave differently on KDOS, its own configuration is used first. KDOS's
defaults are files in `/usr/share/kdos/xdg`, which comes after `/etc/xdg` in the configuration
search path, so the machine owner's files and each user's own override them. A `.patch` against a
KDE module is carried only where no option exists, under the rule for patches in [Writing
ports](../05-developer/writing-ports.md), and it is offered upstream at the same time; the port's
notes name the merge request. The CRT effect behind Phosphor mode is one: KWin refuses an effect
built outside it for any other KWin version, point releases included, so the effect is built into
KWin by a patch and can only fail to build, never fail to load.

The cost is weight and a release lock. Qt, KDE Frameworks and Plasma are more code than the rest
of the host, and they sit under the desktop. The KDOS code that uses Plasma's internal interfaces,
the Discover backend, the System Monitor plugin and the KWin effect, is rebuilt and tested again
for every Plasma minor release.

### Applications bring their own toolkit

A graphical application on the host is ported with the toolkit it is written in: GTK 3 or 4,
libadwaita, WebKitGTK, Qt 5 or 6, QtWebEngine, KDE Frameworks, wxWidgets, FLTK or Tk. Each toolkit
that has a Wayland backend is built with both it and its X11 backend, and Wayland is the run-time
default; Tk has only an X11 backend, so a Tk application always runs through Xwayland.
`/etc/profile.d/10-wayland.sh` sets `QT_QPA_PLATFORM="wayland;xcb"`, so Qt falls back to Xwayland
only when its Wayland plugin cannot start, and leaves `GDK_BACKEND` unset, because GTK already
tries Wayland first and a value there would override the `GDK_BACKEND=x11` that an X11-only
application sets for itself. A KDE application is at home. A GTK application takes the Breeze GTK
theme, which kde-gtk-config keeps in step with the Plasma colour scheme, and `GTK_USE_PORTAL=1`
sends its file chooser to xdg-desktop-portal-kde. A Qt 5 application has no KDE platform theme to
load, because no KDE Frameworks 5 are built, so it loads `qt5ct`, patched to answer to the `kde`
platform-theme name, with the colours [`kdos theme`](../04-programs/kdos-command.md#kdos-theme)
writes for the accent.

The ported applications are recipes under `ports/core` like any other. Most are built in `44_apps`,
the leaf applications over `43_toolkits`; one that something else depends on (VLC, Wireshark,
FontForge, GNU Radio) is built in `43_toolkits` or earlier. Each is filed on the shelf of what a
person does with it, never by its toolkit or the desktop project that wrote it, and KDE Gear's
applications are filed the same way: `browsers` (Firefox ESR, Chromium), `mail` (Thunderbird),
`office` (LibreOffice), `documents` (Okular), `editors` (Kate), `graphics` (GIMP, Inkscape, Krita,
darktable), and on through `studio`, `video-tools`, `cad`, `eda`, `education` and the `games-*`
shelves. The phase's list is split by the same shelves, one `packages.d/<shelf>.txt` each, and [The
ports catalogue](../06-reference/ports-catalogue.md) lists every port by shelf. They are ported because a
box is built from Debian packages over the network, and a machine that never sees a network has only
what its medium carries. The [boxes](../03-architecture/packs-and-boxes.md) remain for software KDOS
does not carry, and an application in a box draws whatever its toolkit draws.

A library's toolkit front end is built where a ported application uses it and switched off where
none does, so what a library installs does not depend on what happened to be built before it.
`fontforge`'s GTK editor window, `android-file-transfer`'s Qt window and `wireshark`'s Qt interface
are built. `ghostscript` is configured with `--disable-gtk`, and `avahi` with every toolkit off:
its GTK 3 front end cannot be built at all, because GTK 3 depends on CUPS, which depends on avahi.

## Everything that runs on the host is built from source

Every program, library and module that the host installs and runs on its own processor is compiled
in this tree from pinned source: about 2,400 recipes under `ports/core` for upstream software, KDE
Plasma, KDE Frameworks and KDE Gear among them, each pinned by hash, and a few dozen under `src/`
for the software kept in this repository, most of it written for KDOS. [The ports
catalogue](../06-reference/ports-catalogue.md) lists every one of them by shelf, with the phase
that builds it, and [How KDOS is built](../05-developer/how-kdos-is-built.md) follows the build from
the first source fetch to a bootable ISO. The application catalogue is outside the rule. It is
Debian's packaging, built by podman into a box on the machine that asks for it, and it is never part
of the host.

The binaries a machine installs after that are the ones this build produced. A KDOS release builds
every package from these recipes and publishes the result, as packages on a signed binhost and as
whole-slot images, signed with the release key. A machine installs a package or an image only once
its signature verifies against the release key its own image carries, and nothing in a release is
built anywhere else.
`kdos update apply --source` builds the same recipes on the machine instead of installing them, and
[`kdos rebuild`](../04-programs/kdos-command.md#kdos-rebuild) builds the whole system.

A binary taken on trust cannot be read, cannot be rebuilt from the source tree, and carries
whatever its builder put in it. A single prebuilt binary on the host is enough to make end-to-end
inspection impossible.

Five classes are exempt, and nothing outside them is:

- **Firmware and code for another processor.** `linux-firmware`, `intel-ucode`, `sof-firmware`,
  the GPU kernels in `intel-media-driver` and `libva-intel-driver`, the SOF coefficient files in
  `alsa-ucm-conf`, the device stubs uploaded by `espflash`, `probe-rs`, `python3-esptool` and
  `openfpgaloader`, the riscv64 EDK2 image that `qemu` installs from its tarball, the radio
  firmware in `meshtastic-firmware`, `rnode-firmware` and `libperseus-sdr`, the guest ROMs and
  replacement BIOSes the emulators carry, and `wine-mono` and `wine-gecko`. This code runs on a
  DSP, a GPU, a microcontroller, a radio or a guest (an emulated machine, or a Windows program
  under Wine), and most of it has no published source.
- **Compiled font data.** `noto-fonts`, `noto-fonts-extra`, `noto-cjk`, `nerd-fonts-symbols`,
  `font-carlito`, `font-caladea`, the fonts of `texlive`'s texmf tree, and the fonts bundled inside
  applications such as `mupdf`, `matplotlib`, `kodi` and `freecad`. Their sources compile through
  toolchains this tree does not carry. A face whose upstream build runs on ports is compiled here:
  `ttf-dejavu`, `terminus-ttf`, `noto-emoji`, `font-liberation` and the X.org bitmap fonts.
- **Bootstrap seeds.** The `rust` stage-0 toolchain, the `go` bootstrap toolchain, `zig1.wasm`
  inside the `zig` source, the upstream musl GHC that `ghc` builds with, the musl JDK that
  `openjdk` boots from, `ocaml`'s `boot/ocamlc` and `boot/ocamllex`, and the Groovy jars `kodi`'s binding generator
  runs on. A compiler written in its own language needs a working copy first; the seed is used by
  the build and never installed.
- **Data with no other source form.** The `tessdata-eng` OCR models, the Whisper and SoundFont
  data ports, the `perl-xml-parser` encoding maps, the `fcitx5-chinese-addons` tables, `john`'s
  `.chr` files, the RP2350 boot-ROM copies in `picotool`, the recorded voice samples in
  `alsa-utils`, trained network weights, map databases, game and learning content, and the Alpine
  security database that `kdos-tools` ships as `/usr/share/kdos/secdb.txt`.
- **Built JavaScript.** The JavaScript of `libkiwix`'s server skin, the web clients of
  `transmission`, `deluge`, `kolibri`, `pat` and `kodi`, and `yarn`'s release bundle, each the
  output of a JavaScript toolchain over an npm dependency tree that ships as upstream released it.

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

The build runs with no network, and this is enforced, not a convention: `make build` starts the
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
[Decisions](decisions.md#upstream-archives-are-hash-checked-release-assets) for why the
sources are held in an archive whose every file is checked by its hash.

For recipes this creates a class of build failure that has to be fixed rather than tolerated: a
meson subproject wrap, a CMake `file(DOWNLOAD)`, a `FetchContent` git clone, a Python build backend
resolving a system tool from PyPI. Each has a standard fix in
[Build troubleshooting](../05-developer/build-troubleshooting.md#a-build-that-reaches-the-network).

The cost lands on whoever adds a port. A language ecosystem that downloads its dependencies at
build time needs a *vendor bundle*: a reproducible tarball of those dependencies, produced by
`make fetch` and then held in the archive like any other source. A recipe asks for one with
`vendoring =`, and 168 recipes in `ports/core` do: 80 Rust, 39 Go, 43 Python, 4 Haskell and 2
Node. `python3-lsp-ruff` gets a Python bundle from its `pypackages` line alone, and four more
(`pdfium`, `libreoffice`, `librepcb` and `surfer`) carry bundles no tool writes, assembled by hand,
so 173 ports carry a vendor bundle in all. A build with network access would
fetch these dependencies itself. See [Writing ports](../05-developer/writing-ports.md#vendoring)
and [A bundle no tool writes](../05-developer/writing-ports.md#a-bundle-no-tool-writes).

The running machine keeps the same rule. Nothing KDOS ships downloads anything of its own accord:

- **No "Get New…".** KNewStuff, KDE's service for fetching themes, wallpapers and scripts from
  store.kde.org, is switched off for every program by `ghns=false` under `[KDE Action
  Restrictions]` in the distribution's `kdeglobals`, and Discover is built without its KNewStuff
  backend. What KDOS offers instead is a curated add-on pack on the medium: wallpapers, colour and
  Konsole schemes, KWin scripts and a few plasmoids.
- **No Flatpak.** There is no Flathub remote and no Flatpak backend in Discover, which lists the
  ports, packs and boxes this repository describes, and firmware. Discover fetches no ratings or
  reviews (`FetchOdrsRatings=false` in its shipped `discoverrc`), and its featured list is a file on
  the machine.
- **No browser extension store.** Plasma Browser Integration's extension for Firefox ESR and
  LibreWolf is installed from the medium with the browsers.
- **No telemetry.** KUserFeedback is not ported, and every KDE module that can use it is built
  without it.
- **No shipped timer touches the network.** The nightly update check compares what is installed
  with the tree, cache or medium already on the machine, and fwupd lists firmware from the LVFS
  metadata snapshot each release carries.

The network is used when the person at the machine asks for it: Discover's "Check for updates",
`kdos update fetch`, `kdos update apply`, `fwupdmgr refresh`, and installing a catalogue
application, which builds its box from Debian's archive. The cost is freshness: firmware metadata is
as new as the last release or the last refresh somebody ran, and what "Get New…" would have offered
is whatever the add-on pack carries.

## Reproducible by construction

A package built twice from the same tree is byte-identical. That is a property of one function
rather than of some 2,400 recipes: `roll_package()` in `kpkg`, the package manager
(`src/system/kdos-kpkg/build.c`), runs tar with `--sort=name`, `--format=gnu`, `--owner=0 --group=0
--numeric-owner`, an `--mtime` taken from `SOURCE_DATE_EPOCH`, and `xz -9 -T0 --block-size=32MiB
--no-adjust` as a pinned compressor with `XZ_OPT` and `XZ_DEFAULTS` removed (a package that
`kpkg install` deletes straight after installing it gets `xz -0` instead): xz's bytes follow its
preset and block size, not the number of threads, so the compressor uses every core and stays
reproducible. The build also sets its umask to `022` before a recipe runs. The environment every build
phase sources, `script/env/common.env`, pins `SOURCE_DATE_EPOCH` to `1735689600`, and with the
variable unset the mtime is `0`, so the answer never depends on when the build ran. Keeping all of
this in one place is why `kpkg` rolls the package archive itself instead of letting each recipe do
it.

Reproducibility is what gives the other package mechanisms their meaning. A signed
[binhost](../03-architecture/packaging.md#the-binary-host) (a directory of prebuilt packages with
a signed index) is worth trusting only if its packages can be rebuilt and compared. A
[delta](../03-architecture/packaging.md#deltas) (a binary difference from
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
  its colour slots, the boot splash and the text consoles take their colours from it, and the
  theme generator expands it into every KDE colour scheme, Konsole scheme, `qt5ct` palette and
  login-screen palette the image ships, but no program keeps a second copy of the colour values.
- **Turning an `Exec=` line into an argument vector** is `kxdg_exec_split()` in `libkxdg`. Every
  launch path of KDOS's own goes through it, in `kdos-appbox` and the `kdos` command, including the
  path that writes those lines back out.
- **What is out of date** is `/var/lib/kdos/update.json`, which `kdos update check` writes.
  `kdos-sysupdated` answers Discover from it rather than comparing versions itself, so the command
  line and the software centre cannot disagree about what is behind.
- **Which slot boots next** is `kdos-bootctl`'s to write. `kdos update` and `kdos-sysupdated`,
  which the settings page for system versions asks, both run it rather than touching the boot
  state on the EFI partition themselves.
- **Which pack a command belongs to**, a pack being the single read-only filesystem image an
  imported application runs from (an application built from the catalogue has none), is
  `app_pack_by_exec()` in `kdos-appbox`, reading the `alien-apps`
  table (one row per boxed application: its name, its command and, where it has one, its pack) that
  the launcher generator writes. `kdos-appbox run` and the warm-up that starts, at login, the boxes
  of the applications `~/.config/kdos/favorites` lists both ask it rather than each deciding.

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

The same rule governs what a measurement is allowed to claim.
[`kdos-energy`](../04-programs/daemons.md#kdos-energyd) reports relative shares of attributable CPU
energy and states that it gives no watt-hours, because RAPL, the processor's energy counters, cannot
see the panel, the radio or the disk. Its GPU column is engine time from the driver's per-client
counters, labelled as time and never turned into energy, and a driver with no per-client GPU
accounting gets no GPU column at all. [`kdos cve`](../04-programs/kdos-command.md#kdos-cve) reports
a package its database does not carry as unknown, never as clean.

The cost is that KDOS looks less capable than a tool willing to guess. A report leaves a column out
where another tool would fill it with an estimate.

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

A [box profile](../04-programs/kdos-appbox.md#box-profiles), printed by `kdos-box profile <box>`,
[reports](../04-programs/kdos-appbox.md#how-the-profile-reports-itself) what was enforced rather
than what the file asked for, and names the podman flag or KDOS mechanism behind each line. What it cannot
enforce it marks with `!`:

- There is no podman flag that grants a box a speaker and denies it a camera. A box whose devices
  are shared has the host's whole `/dev`, so an `audio` or `gpu` key that would subtract one of
  them is reported as not enforceable separately.
- A `persistence` other than `persistent` is recorded but not imposed, because a box's writes land
  wherever its container runtime puts them.

A box base that names a container registry is an online operation, and says so before it does
anything, because it fetches unsigned content from a third-party registry.
[`kdos sandbox`](../04-programs/kdos-command.md#kdos-sandbox) follows
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
relative links, common phrases that narrate the past, and pages in either book that lack a title or
a `See also` section. Run over a whole book, it also reports pages that the table of contents does
not reach and, for this book, any exact port count on the status page, the ports catalogue or
the packaging chapter that the tree contradicts. It cannot recognise a paragraph that tells a story in its own words.

## Every change updates its documentation, in the same change

A change in behaviour updates every page under `docs/kdos/` and `docs/kfs/`, every code comment
and every shipped configuration file that describes that behaviour, before the change is complete,
and not in a follow-up.

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
  by shelf and phase
- [The design language](../03-architecture/design-language.md) — these principles applied to what
  you see
- [Session](../03-architecture/session.md) — the login, elogind and the per-user services that
  stand in for systemd's
- [The security model](../03-architecture/security-model.md) — the enforcement behind the box
  profile, including what is not protected
- [Packaging](../03-architecture/packaging.md) — reproducible packages, the binhost and deltas
- [Writing ports](../05-developer/writing-ports.md) — the recipe rules that follow from building
  offline and from source
- [Glossary](../06-reference/glossary.md) — the terms this book uses

<!-- book-nav -->
---

*Part I — Introduction, chapter 3.* Previous: [2. How KDOS differs](how-kdos-differs.md) · [Contents](../README.md) · Next: [4. Decisions](decisions.md)
