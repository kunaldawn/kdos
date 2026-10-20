# The ports catalogue

This chapter lists every port KDOS can build: each of the 2,081 recipes under `ports/core` and the
25 recipes KDOS writes itself under `src/`, 2,106 in all, each exactly once. It is a reference for
anyone who needs to know whether a piece of software is in the tree, which version it is at, where
its recipe lives, and which phase of the build installs it. Read
[Packaging](../03-architecture/packaging.md) first for what a port and a package are, and
[How KDOS is built](../05-developer/how-kdos-is-built.md) for what the phases do;
[The build system](../05-developer/build-system.md#phases) covers how a phase runs.

## How the catalogue is organised

A **port** is a directory holding a recipe: a `kpkgbuild` file of declarative metadata (name,
version, sources, their hashes, a one-line description, dependencies) and a `build.sh` script that
compiles the unpacked source. `kpkg` turns a port into a **package**, the archive it installs.

**Shelves.** Upstream software lives one level down in `ports/core`, on a **shelf**: a directory
named for a subject, holding the ports that belong to it. `wlroots` is at `ports/core/wl/wlroots/`,
`ffmpeg` at `ports/core/media-frameworks/ffmpeg/`. The 102 shelves form a closed list,
`ports/shelves`, one line per shelf giving its name, its source-archive volume and what belongs
on it, and the order of that file is the order this chapter uses. A shelf is only a place to file a port. A port's identity is
its bare name: `depends =`, the package lists, `kpkg install` and the package database all use
`wlroots`, never `wl/wlroots`, and no name appears twice anywhere in the tree. `kpkg` finds a port
by looking for `<name>/` and then `<shelf>/<name>/` in each directory on `PORT_REPO`, and refuses a
name filed twice in one of them.

KDOS's own programs are not on a shelf. Their code sits in the port directory itself, under one of
the areas of `src/`: `src/system`, `src/art`, `src/desktop` and `src/daemons`
([Repositories and shelves](../03-architecture/packaging.md#repositories-and-shelves) explains the
repositories and the order they are searched in). A phase reaches an area only when that area is on
its `PORT_REPO`, which is why the desktop's programs can only be built by the desktop phase.

To find a port's recipe, look it up here: the heading above its table names its shelf or `src/`
area. On a running system, `kpkg info <name>` prints an installed package's version, release and
file count. Where the ports tree is present (inside the build chroot, or with `PORT_REPO` pointed at
a checkout's `ports/core`), it also prints an uninstalled port's recipe description and
dependencies.

**Phases and their lists.** The build installs ports in **phases**, the directories under
`script/phases/`, run in sorted order. A phase that installs packages carries a **package list**
naming the ports it installs, one per line: either one `packages.txt`, or a `packages.d/` directory
of `.txt` files read in byte order as one list
([Phases, package lists and shelves](../03-architecture/packaging.md#phases-package-lists-and-shelves)).
Both kinds are divided by shelf. A `packages.txt` has one comment block per shelf, titled
`<shelf> — <description>`, except the two shortest, `20_selfhost`'s and `60_kernel`'s, which have no
shelf blocks; a `packages.d/` has one file per shelf, `<shelf>.txt`. KDOS's own programs are divided
the same way by area, as `src-system`, `src-art`, `src-desktop` and `src-daemons`. A run of ports whose
install order matters stays together ahead of the shelves, with the comment that explains it:
at the head of `30_foundation`'s list, and in the `00-order.txt` file that sorts first in
`41_system`.

From `30_foundation` on, a list names every port its phase installs and nothing else: when a port's
`depends =` would pull in a port its phase's list does not name, `testing/phaseclosure.py` fails.
The one phase that installs more than it names is `20_selfhost`, the first to run `kpkg`: its
closure brings in 6 ports its list does not name. Of those, 4 are named by no list at all: `gmp`,
`mpc`, `mpfr`, `xxhash`.

**The `group =` key.** The recipe key `group =` has nothing to do with shelves or lists. It is read
only by `ports/update`, the upstream version checker, which offers the version bumps of ports in one
group together, all or none. Two kinds of recipe set it. A pair that builds one upstream source
twice, or a program and the data released beside it, shares a group: `glib` and `glib-introspection`
(`glib`), `python3` and `python3-tkinter` (`python3`), `gcc-arm-none-eabi` and
`libstdcxx-arm-none-eabi` (`gcc-arm-none-eabi`), `gcc-riscv64-unknown-elf` and
`libstdcxx-riscv64-unknown-elf` (`gcc-riscv64-unknown-elf`), `qca` and `qca-qt5` (`qca`), `qwt` and `qwt-qt5`
(`qwt`), `qscintilla` and `python3-qscintilla` (`qscintilla`), `webkitgtk` and `webkitgtk6`
(`webkitgtk`), `mgba` and `libretro-mgba` (`mgba`),
`supertuxkart` and `stk-assets` (`supertuxkart`), and four programs whose graphical front end is
built from the same tarball by a port of its own: `limesuite` and `limesuite-gui` (`limesuite`),
`nmap` and `zenmap` (`nmap`), `v4l-utils` and `qv4l2` (`v4l-utils`), and `stlink` and `stlink-gui`
(`stlink`). A family released together carries one key for all
its members, so that the checker does not split it by host: the 29 Qt 6 ports carry `group = qt6`
and the 11 Qt 5 modules `group = qt5`. All but `qt6-qtmqtt`, which comes from Qt's GitHub, are
fetched from `download.qt.io`, a host the derivation does not read. For any other port the checker derives a group
only when its first source is on GitHub, Codeberg, sr.ht or a GitLab host. That group is keyed on
the forge organisation and the version, so sibling projects that release together are offered
together. A port from any other host has no group and is reviewed on its own. See
[The group key](../03-architecture/packaging.md#the-group-key) and
[Checking for new versions](../05-developer/writing-ports.md#checking-for-new-versions).

**The tables.** The chapter goes:

1. [The phases](#the-phases): each phase, what its list names and what it installs.
2. [Ports per shelf](#ports-per-shelf): how many ports each shelf holds.
3. [The shelves](#the-shelves): one table per shelf, in `ports/shelves` order, then one per `src/`
   area. This is the catalogue proper.
4. [The application-developer platform](#the-application-developer-platform): the libraries no
   port depends on.
5. [Ports named in more than one list](#ports-named-in-more-than-one-list): the exception to one
   list per port.

Within each table, ports are in alphabetical order. The version is the recipe's `version =` value
and the description is its `description =` value, reworded where that line praises the software,
carries a typo or an aside, or does not plainly say what the software is. The phase column names the
phase that first installs the port, and any later phase whose list names it again. "As a dependency"
marks a port that phase installs without its list naming it.

## The phases

Counted from the lists under `script/phases/` and the `depends =` lines of the recipes. "Names"
counts the non-comment lines of a phase's list; "Installs" counts the ports the phase installs,
leaving out any an earlier phase installed.

| Phase | Title | List | Names | Installs |
|---|---|---|---|---|
| `00_cross` | Cross Toolchain | none: steps | — | — |
| `10_bootstrap` | Base Userland | none: steps | — | — |
| `20_selfhost` | Self-Hosting Bootstrap | `packages.txt` | 9 | 15 |
| `30_foundation` | Build Foundation | `packages.txt` | 138 | 127 |
| `31_compilers` | Compilers | `packages.txt` | 22 | 22 |
| `40_lang` | Languages | `packages.d/`, 13 files | 177 | 177 |
| `41_system` | System | `packages.d/`, 96 files | 1,012 | 1,012 |
| `42_graphics` | Graphics Stack | `packages.d/`, 54 files | 193 | 193 |
| `43_toolkits` | Toolkits | `packages.d/`, 46 files | 248 | 248 |
| `44_apps` | Applications | `packages.d/`, 59 files | 286 | 286 |
| `50_desktop` | Desktop | `packages.txt` | 22 | 22 |
| `60_kernel` | Kernel | `packages.txt` | 2 | 2 |
| `70_image` | Image | none: steps | — | — |
| Total | | | 2,109 | 2,104 |

The lists hold 2,109 lines naming 2,100 distinct ports; 9 of those ports are named by more than one
phase. Following `depends =` from the lists reaches 2,104 ports. With `kdos-installer`, which
`10_bootstrap` builds by script, that is every recipe but 1, listed in `ports/unbuilt`: `texlive-doc`,
a recipe kept for `kpkg install` that no phase builds, marked `not built` in its table.
`testing/bookcounts.py` fails on any other recipe no phase builds.

### Built by script

`00_cross` builds the cross binutils and gcc in the build container, and `10_bootstrap`
cross-compiles the base userland into the new root filesystem. Neither phase reads a package list or
runs `kpkg`: each step is a script that unpacks a port's source with `extract_port_source` and
builds it by hand. What those steps build is therefore not recorded as a package, and the same ports
are installed again as packages by a later phase, which is the phase the tables below give.

| Step | Sources used |
|---|---|
| `00_cross/00_binutils.sh` | `binutils` |
| `00_cross/01_gcc.sh` | `gcc`, `gmp`, `mpfr`, `mpc` |
| `10_bootstrap/010_linux_headers.sh` | `linux` |
| `10_bootstrap/020_musl_libc.sh` | `musl` |
| `10_bootstrap/030_libstdc++.sh` | `gcc` |
| `10_bootstrap/040_ncurses.sh` | `ncurses` |
| `10_bootstrap/050_xz.sh` | `xz` |
| `10_bootstrap/060_gzip.sh` | `gzip` |
| `10_bootstrap/061_tar.sh` | `tar` |
| `10_bootstrap/062_coreutils.sh` | `coreutils` |
| `10_bootstrap/063_diffutils.sh` | `diffutils` |
| `10_bootstrap/064_findutils.sh` | `findutils` |
| `10_bootstrap/065_gawk.sh` | `gawk` |
| `10_bootstrap/066_grep.sh` | `grep` |
| `10_bootstrap/067_patch.sh` | `patch` |
| `10_bootstrap/068_sed.sh` | `sed` |
| `10_bootstrap/070_readline.sh` | `readline` |
| `10_bootstrap/080_bash.sh` | `bash` |
| `10_bootstrap/090_binutils.sh` | `binutils` |
| `10_bootstrap/100_gcc.sh` | `gcc`, `gmp`, `mpfr`, `mpc` |
| `10_bootstrap/110_make.sh` | `make` |

Two of KDOS's own programs are built here from `src/system`, because nothing can read a recipe
before `kpkg` exists. `120_kpkg.sh` compiles the package manager from `src/system/kdos-kpkg`, which
has no recipe and is not catalogued. `130_kinstall.sh` compiles the installer from the sources of
the `kdos-installer` port, which no list names; it is catalogued under [src/system](#srcsystem).

## Ports per shelf

Every recipe, counted by where it lives.

| Shelf or area | Ports |
|---|---|
| [`base`](#base) | 32 |
| [`base-libs`](#base-libs) | 17 |
| [`auth`](#auth) | 14 |
| [`boot`](#boot) | 14 |
| [`toolchain`](#toolchain) | 39 |
| [`buildtools`](#buildtools) | 21 |
| [`lang`](#lang) | 27 |
| [`devtools`](#devtools) | 37 |
| [`vcs`](#vcs) | 10 |
| [`editors`](#editors) | 12 |
| [`doctools`](#doctools) | 28 |
| [`devlibs`](#devlibs) | 36 |
| [`formats`](#formats) | 35 |
| [`database`](#database) | 15 |
| [`archiver`](#archiver) | 31 |
| [`shells`](#shells) | 10 |
| [`cli`](#cli) | 19 |
| [`files`](#files) | 11 |
| [`sysmon`](#sysmon) | 8 |
| [`hardware`](#hardware) | 29 |
| [`input`](#input) | 16 |
| [`mobile`](#mobile) | 14 |
| [`disk`](#disk) | 30 |
| [`filesystem`](#filesystem) | 20 |
| [`optical`](#optical) | 13 |
| [`crypto`](#crypto) | 31 |
| [`security`](#security) | 26 |
| [`containers`](#containers) | 18 |
| [`virt`](#virt) | 25 |
| [`net-libs`](#net-libs) | 38 |
| [`network`](#network) | 28 |
| [`net-diag`](#net-diag) | 16 |
| [`servers`](#servers) | 10 |
| [`transfer`](#transfer) | 14 |
| [`mail`](#mail) | 13 |
| [`chat`](#chat) | 16 |
| [`browsers`](#browsers) | 10 |
| [`web-engines`](#web-engines) | 14 |
| [`x11`](#x11) | 43 |
| [`wl`](#wl) | 17 |
| [`gpu`](#gpu) | 26 |
| [`graphics-libs`](#graphics-libs) | 27 |
| [`image-libs`](#image-libs) | 47 |
| [`graphics`](#graphics) | 23 |
| [`3d`](#3d) | 17 |
| [`cad`](#cad) | 17 |
| [`fabrication`](#fabrication) | 13 |
| [`fonts`](#fonts) | 25 |
| [`themes`](#themes) | 7 |
| [`xdg`](#xdg) | 16 |
| [`accessibility`](#accessibility) | 9 |
| [`i18n`](#i18n) | 15 |
| [`spelling`](#spelling) | 10 |
| [`gtk`](#gtk) | 28 |
| [`qt6`](#qt6) | 29 |
| [`qt5`](#qt5) | 11 |
| [`qt-extra`](#qt-extra) | 21 |
| [`kf6`](#kf6) | 67 |
| [`kde`](#kde) | 9 |
| [`toolkits`](#toolkits) | 4 |
| [`audio-io`](#audio-io) | 15 |
| [`audio-codecs`](#audio-codecs) | 28 |
| [`audio-dsp`](#audio-dsp) | 27 |
| [`music-libs`](#music-libs) | 18 |
| [`studio`](#studio) | 12 |
| [`music-players`](#music-players) | 10 |
| [`video-libs`](#video-libs) | 23 |
| [`media-frameworks`](#media-frameworks) | 18 |
| [`video-players`](#video-players) | 8 |
| [`video-tools`](#video-tools) | 10 |
| [`game-libs`](#game-libs) | 18 |
| [`games-action`](#games-action) | 9 |
| [`games-shooter`](#games-shooter) | 11 |
| [`games-strategy`](#games-strategy) | 12 |
| [`games-rpg`](#games-rpg) | 8 |
| [`games-board`](#games-board) | 18 |
| [`emulators`](#emulators) | 18 |
| [`libretro`](#libretro) | 12 |
| [`education`](#education) | 16 |
| [`office`](#office) | 23 |
| [`documents`](#documents) | 22 |
| [`pim`](#pim) | 12 |
| [`printing`](#printing) | 26 |
| [`math`](#math) | 27 |
| [`sci-libs`](#sci-libs) | 21 |
| [`astronomy`](#astronomy) | 14 |
| [`bioscience`](#bioscience) | 12 |
| [`gis`](#gis) | 20 |
| [`eda`](#eda) | 29 |
| [`embedded`](#embedded) | 30 |
| [`sdr-hw`](#sdr-hw) | 19 |
| [`sdr`](#sdr) | 24 |
| [`hamradio`](#hamradio) | 26 |
| [`ai`](#ai) | 7 |
| [`python`](#python) | 36 |
| [`python-libs`](#python-libs) | 67 |
| [`python-net`](#python-net) | 22 |
| [`python-sci`](#python-sci) | 13 |
| [`python-gui`](#python-gui) | 16 |
| [`python-dev`](#python-dev) | 14 |
| [`python-hw`](#python-hw) | 12 |
| [`perl-cpan`](#perl-cpan) | 20 |
| [`src/system`](#srcsystem) | 6 |
| [`src/art`](#srcart) | 6 |
| [`src/desktop`](#srcdesktop) | 8 |
| [`src/daemons`](#srcdaemons) | 5 |
| Total | 2,106 |

## The shelves

Each table lists one shelf's ports in alphabetical order. The line under each heading is the shelf's
own description from `ports/shelves`.

### base

The minimal userland and system daemons that every KDOS image boots with. 32 ports, under
`ports/core/base/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `bash` | 5.3 | The Bourne-Again SHell | `30_foundation` |
| `bc` | 1.08.2 | An arbitrary precision calculator language | `30_foundation` |
| `ca-certificates` | 3.130 | Bundle of CA Root Certificates from Mozilla | `30_foundation` |
| `coreutils` | 9.12 | The GNU core utilities — the file, text and shell commands, one program each | `30_foundation` |
| `dbus` | 1.16.2 | Message bus system for communication between processes | `41_system` |
| `diffutils` | 3.12 | Utility programs for comparing files | `20_selfhost`, named again by `30_foundation` |
| `eudev` | 3.2.14 | Programs for dynamic creation of device nodes | `30_foundation` |
| `execline` | 2.9.9.2 | A non-interactive scripting language of chained programs, and the library s6 links | `41_system` |
| `file` | 5.48 | Identify a file by its content, and the magic database behind it | `30_foundation` |
| `findutils` | 4.11.0 | GNU utilities to locate files | `30_foundation` |
| `gawk` | 5.4.1 | GNU awk pattern scanning and processing language | `20_selfhost`, named again by `30_foundation` |
| `grep` | 3.12 | Searches text for patterns — grep, egrep and fgrep, with PCRE2 for -P | `30_foundation` |
| `gzip` | 1.15 | GNU compression utility | `30_foundation` |
| `iana-etc` | 20260911 | /etc/services, /etc/protocols and /etc/rpc, generated from IANA's registries | `41_system` |
| `kbd` | 2.10.0 | Key-table files, console utilities and the vlock console lock | `42_graphics` |
| `kmod` | 34.2 | Libraries and utilities for loading kernel modules | `30_foundation` |
| `less` | 710 | A text file viewer | `30_foundation` |
| `lsof` | 4.99.7 | Lists open files held by running processes | `41_system` |
| `musl-utils` | 20261006 | getconf, getent and iconv for musl, from Alpine's musl package | `30_foundation` |
| `patch` | 2.8 | Applies a diff file to an original | `30_foundation` |
| `procps-ng` | 4.0.7 | Programs for monitoring processes | `41_system` |
| `psmisc` | 23.7 | Programs for displaying information about running processes | `41_system` |
| `s6` | 2.15.1.0 | skarnet's process supervision suite — s6-ipcserver and s6-setuidgid run utmpd and wtmpd | `41_system` |
| `seatd` | 0.9.3 | Minimal seat management daemon for non-systemd Wayland setups | `41_system` |
| `sed` | 4.10 | GNU stream editor, with the extensions upstream build systems assume | `30_foundation` |
| `snooze` | 0.6 | Runs a command at a given time — one process per job in place of a cron daemon | `41_system` |
| `sysklogd` | 2.7.2 | System logging daemon with built-in rotation | `41_system` |
| `sysvinit` | 3.18 | System V init — PID 1, runlevels, telinit, shutdown, halt, reboot and poweroff | `30_foundation` |
| `tar` | 1.35 | Creates, lists and extracts tar archives | `20_selfhost`, named again by `30_foundation` |
| `tzdata` | 2026d | IANA time zone database — musl reads /usr/share/zoneinfo natively | `41_system` |
| `util-linux` | 2.42.4 | Miscellaneous system utilities for Linux | `30_foundation` |
| `which` | 2.25 | Shows the full path of a command | `30_foundation` |

Notes:

- `ca-certificates`: the converter drops every certificate already expired when it runs, so a
  rebuild after a root's expiry ships one certificate fewer. `/etc/ssl/cert.pem` is written by
  `postinstall.sh` from the bundle and `/etc/ca-certificates/trust-source/anchors/`, never
  packaged, or an upgrade would drop the local roots.
- `findutils`: its `locate` and `updatedb` are not installed. plocate's are, and neither pair reads
  the other's database or takes the other's options.
- `gawk`: the same recipe builds in `20_selfhost`, so every optional library is declared and pinned
  on, or pinned off; one left to detection makes the binary depend on build order.
- `kmod`: `--without-openssl` keeps `libcrypto` out of `libkmod`. The initramfs does not carry
  `libcrypto` for it, so a linked `libkmod` stops modprobe and udev loading there.
- `tar`: ACLs stay off, because tar builds in `20_selfhost`, where `acl` and `attr` do not exist.
- `tzdata`: the source is upstream's git tag archive, because kpkg strips the first path component
  and IANA's tarballs are flat. Zones are compiled `-b fat`, which the boxes' glibc reads as well as
  musl. `/etc/localtime` is never packaged, or every upgrade would reset the zone.
- `util-linux`: the shipped build is the `30_foundation` one, so every probe is pinned against what
  that phase holds. `--without-udev`, because eudev depends on util-linux.

### base-libs

Low-level C libraries that the base system links. 17 ports, under `ports/core/base-libs/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `acl` | 2.4.0 | Utilities to administer Access Control Lists, which are used to define more fine-grained discretionary access rights for files and directories | `30_foundation` |
| `attr` | 2.6.0 | Utilities to administer the extended attributes on filesystem objects | `30_foundation` |
| `basu` | 0.2.1 | sd-bus library extracted from systemd (no-systemd sd-bus provider) | `41_system` |
| `keyutils` | 1.6.3 | Tools to control the Linux key management system | `41_system` |
| `libbsd` | 0.12.2 | Functions commonly found on BSD systems, such as strlcpy() | `41_system` |
| `libcap` | 2.78 | Implements the user-space interfaces to the POSIX 1003.1e capabilities | `30_foundation` |
| `libcap-ng` | 0.9.6 | Alternate POSIX capabilities library (required by openvpn 2.6+) | `30_foundation` |
| `libedit` | 20260512 | NetBSD line editor with history, key bindings and tab completion | `41_system` |
| `libffi` | 3.8.0 | Portable foreign function interface library | `30_foundation` |
| `libmd` | 1.2.0 | Message Digest functions from BSD systems | `41_system` |
| `libxdg-basedir` | 1.2.3 | A small C library for the XDG Base Directory specification | `41_system` |
| `ncurses` | 6.6 | System V Release 4.0 curses emulation library | `20_selfhost` (as a dependency), named again by `30_foundation` |
| `newt` | 0.52.25 | Programming library for color text-mode widget UIs (provides libnewt for nmtui) | `41_system` |
| `popt` | 1.19 | Popt libraries which are used by some programs to parse command-line options | `41_system` |
| `readline` | 8.3 | GNU readline library | `20_selfhost` (as a dependency), named again by `30_foundation` |
| `skalibs` | 2.15.1.0 | skarnet's general-purpose C library, under utmps, execline and s6 | `30_foundation` |
| `slang` | 2.3.3 | S-Lang library — multi-platform programmer's library for text-mode UIs (newt dep) | `41_system` |

Notes:

- `readline`: the termcap cache variable names `ncursesw`. Without it `readline.pc` names the
  `libcurses` link, and every consumer that does not name ncurses itself fails on an undefined
  `tgetent`.

### auth

Authentication, authorisation, accounts and directory services. 14 ports, under `ports/core/auth/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `cracklib` | 2.10.3 | Password-strength checking library, with its small English word dictionary packed | `41_system` |
| `cyrus-sasl` | 2.1.28 | SASL authentication library — the mechanism loader mbsync authenticates through | `30_foundation` |
| `cyrus-sasl-xoauth2` | 0.2 | The XOAUTH2 SASL mechanism — what mbsync authenticates with where a provider has withdrawn app passwords | `41_system` |
| `fprintd` | 1.94.5 | The fingerprint daemon and its PAM module, so a reader can unlock a session | `41_system` |
| `krb5` | 1.22.2 | Kerberos 5 authentication libraries and tools | `41_system` |
| `libfprint` | 1.94.100 | Fingerprint reader drivers | `41_system` |
| `libpwquality` | 1.4.5 | Password quality checking and random password generation library, over cracklib | `41_system` |
| `oath-toolkit` | 2.6.14 | One-time passwords from the command line — HOTP and TOTP | `41_system` |
| `openldap` | 2.6.15 | The LDAP client library and the ldapsearch family of tools | `30_foundation` |
| `pam` | 1.7.3 | Linux Pluggable Authentication Modules (libpam + pam_unix against /etc/shadow) | `41_system` |
| `polkit` | 128 | Authorization framework for unprivileged processes (NetworkManager, fwupd, bolt, fprintd) | `41_system` |
| `shadow` | 4.20.3 | Password and user-account management programs | `30_foundation` |
| `sudo` | 1.9.17p2 | Runs commands as root or another user by policy, logging each command and its arguments | `41_system` |
| `utmps` | 0.1.3.4 | utmp and wtmp for musl — the utmpx library and the utmpd and wtmpd daemons behind it | `30_foundation` |

Notes:

- `cyrus-sasl`: every unshipped mechanism and database backend is off by flag, not by a missing
  library, so a build root carrying Kerberos, OpenLDAP or Berkeley DB adds no plugin. The build is
  serial: `include/` generates headers that a parallel make reaches before they exist.
  `-std=gnu17` keeps the K&R definitions in `lib/md5.c` compiling.
- `fprintd`: the sd-bus provider is `basu`; the default asks for libsystemd and fails at setup.
- `krb5`: a client only. No configure switch builds one, so the KDC, kadmind, the database tools,
  the demonstration clients, `ksu` and their manual pages are removed after the install.
  `--disable-nls`, because the probe finds gettext's `libintl.h` and then links without `-lintl`.
- `pam`: `unix_chkpwd` is installed setuid root, or pam_unix cannot read `/etc/shadow` for a caller
  that is not root. New hashes are sha512, because musl's `crypt()` has no yescrypt.
- `polkit`: `os_type=lfs` and `pam_prefix=/etc/pam.d` give `polkit-1` a service file that includes
  the pam port's stacks; the default prefix is a directory Linux-PAM never reads.
- `shadow`: no yescrypt, because musl's `crypt()` reads a `$y$` salt as DES. `newuidmap` and
  `newgidmap` stay setuid root, without which rootless podman, and so every box, cannot start.

### boot

Everything before userspace: the kernel, microcode, firmware for the host, bootloader and EFI
tooling. 14 ports, under `ports/core/boot/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `acpica` | 20260408 | ACPI Component Architecture tools — the iasl ASL compiler, acpidump and acpixtract | `41_system` |
| `dtc` | 1.8.1 | The device tree compiler, and libfdt — what qemu needs for arm and riscv | `41_system` |
| `efibootmgr` | 18 | Lists, adds and reorders EFI boot entries | `41_system` |
| `efivar` | 39 | Read and write EFI variables — the library efibootmgr needs | `41_system` |
| `fwupd` | 2.1.7 | Daemon and client for updating device firmware | `42_graphics` |
| `fwupd-efi` | 1.8 | The EFI program fwupd boots into to hand a firmware capsule to the machine's firmware | `41_system` |
| `gnu-efi` | 4.0.4 | GNU EFI library | `41_system` |
| `intel-ucode` | 20260812 | Intel processor microcode, bundled for early loading | `41_system` |
| `limine` | 12.9.0 | BIOS and UEFI bootloader and boot manager | `41_system` |
| `linux` | 7.2.11 | Linux kernel | `60_kernel` |
| `linux-firmware` | 20260916 | Device firmware blobs — upstream's tree, less the firmware no x86 kernel loads | `41_system` |
| `memtest86plus` | 8.10 | Standalone memory tester that runs outside the operating system | `41_system` |
| `sof-firmware` | 2026.09.1 | Intel Sound Open Firmware DSP firmware and topologies | `41_system` |
| `wireless-regdb` | 2026.09.03 | Signed wireless regulatory database — without it the kernel applies the restrictive world domain | `41_system` |

Notes:

- `fwupd-efi`: its libexecdir stays meson's default, `/usr/libexec`, the directory fwupd looks for
  `fwupdx64.efi` under; moving one without the other leaves the capsule plugin reporting the
  loader missing.
- `fwupd`: `esp-mount-path.patch` describes the ESP from the FAT filesystem mounted at
  `EspLocation`; without it fwupd finds the ESP only through udisks, and the capsule plugin
  stages nothing where udisks does not answer. `-Dvendor_ids_dir` must name hwdata's directory:
  left empty, meson probes fixed paths for `usb.ids` and fails the configure when none holds one.
- `gnu-efi`: `-Wl,` is stripped from `LDFLAGS` because its makefiles link with `ld` itself, which
  rejects the compiler-driver spelling.
- `intel-ucode`: the build walks the bundle record by record and fails unless the last record ends
  on its last byte. The kernel loads nothing from a bundle with one stray file in it, and says
  nothing.
- `memtest86plus`: `make` takes no variables. The Makefile assigns its own `CFLAGS`, and one given
  on the command line drops `-DARCH_BITS=64` with it and fails the build.
- `sof-firmware`: `sof-ipc4-tplg` is copied before `sof-ace-tplg`, which is a symlink to it
  upstream; in the other order the link dangles.

### toolchain

The native compilers, the linker, the C library, and the LLVM family. 39 ports, under
`ports/core/toolchain/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `argp-standalone` | 1.4.1 | Standalone version of arguments parsing functions from GLIBC | `30_foundation` |
| `bindgen` | 0.73.2 | Generates Rust FFI bindings from C and C++ headers | `31_compilers` |
| `binutils` | 2.47 | A collection of binary tools | `20_selfhost`, named again by `30_foundation` |
| `bison` | 3.8.2 | The GNU Bison parser generator | `30_foundation` |
| `bmake` | 20260912 | NetBSD make, portable — the make that BSD-style makefiles need | `41_system` |
| `cbindgen` | 0.29.4 | A project for generating C bindings from Rust code | `31_compilers` |
| `clang` | 23.1.2 | The C/C++ compiler, plus clangd, clang-tidy and clang-format | `31_compilers` |
| `clang21` | 21.1.8 | Clang 21 libraries and compiler, installed under /usr/lib/llvm21 for zig | `31_compilers` |
| `compiler-rt` | 23.1.2 | clang's runtime libraries — builtins, the profile runtime and UndefinedBehaviorSanitizer | `31_compilers` |
| `crubit` | 0.20260811 | Crubit's cc_bindings_from_rs, which writes C++ headers for calling Rust crates, and its C++ support library | `40_lang` |
| `elfutils` | 0.196 | utilities and libraries for handling ELF files | `30_foundation` |
| `flex` | 2.6.4 | Lexical analyser generator | `30_foundation` |
| `fortify-headers` | 3.0.2 | Standalone _FORTIFY_SOURCE implementation for musl, as overlay headers | `20_selfhost`, named again by `30_foundation` |
| `gcc` | 16.2.0 | The GNU compiler collection | `20_selfhost`, named again by `30_foundation` |
| `gperf` | 3.3 | Generates a perfect hash function from a key set | `30_foundation` |
| `ispc` | 1.32.0 | Intel SPMD Program Compiler — C-like SIMD kernels compiled for SSE, AVX and AVX-512, with a runtime dispatcher | `41_system` |
| `libclc` | 23.1.2 | Library requirements of the OpenCL C programming language | `40_lang` |
| `libintl` | 1.0 | GNU libintl — the gettext runtime musl does not carry in libc | `30_foundation` |
| `libunwind` | 23.1.2 | LLVM libunwind, installed under /usr/lib/llvm-libunwind beside the system libunwind | `31_compilers` |
| `libunwind-nongnu` | 1.8.3 | The libunwind library — call-chain unwinding of this process, another one over ptrace, or a core file | `40_lang` |
| `lld` | 23.1.2 | Linker from the LLVM project | `31_compilers` |
| `lld21` | 21.1.8 | LLD 21 libraries and linker, installed under /usr/lib/llvm21 for zig | `31_compilers` |
| `lldb` | 23.1.2 | Debugger from the LLVM project, for C, C++, Rust and Zig | `41_system` |
| `llvm` | 23.1.2 | Compiler infrastructure libraries and tools from the LLVM project | `31_compilers` |
| `llvm21` | 21.1.8 | LLVM 21, installed under /usr/lib/llvm21 beside the system LLVM for zig | `31_compilers` |
| `m4` | 1.4.21 | GNU implementation of the traditional Unix macro processor | `20_selfhost`, named again by `30_foundation` |
| `make` | 4.4.1 | GNU make utility | `30_foundation` |
| `musl` | 1.2.6 | musl C standard library and dynamic linker | `20_selfhost`, named again by `30_foundation` |
| `musl-fts` | 1.2.7 | Implementation of fts(3) for musl libc | `30_foundation` |
| `musl-ldd` | 1.2.5 | LDD script for Musl | `40_lang` |
| `musl-locales` | 20260425 | A locale command and message catalogues for musl | `40_lang` |
| `musl-obstack` | 1.2.3 | Obstack standalone library | `30_foundation` |
| `musl-rpmatch` | 1.0 | Implementation of rpmatch(3) for musl libc | `30_foundation` |
| `nasm` | 3.02 | The Netwide Assembler, a portable 80x86 assembler | `30_foundation` |
| `openmp` | 23.1.2 | LLVM's OpenMP runtime, libomp — what clang -fopenmp links | `31_compilers` |
| `patchelf` | 0.19.1 | Utility to modify ELF executables | `30_foundation` |
| `spirv-llvm-translator` | 23.1.1 | Tool and library for translation between LLVM IR and SPIR-V | `41_system` |
| `swig` | 4.5.1 | Generate scripting interfaces to C/C++ code | `30_foundation` |
| `yasm` | 1.3.0 | Assembler for the x86 and AMD64 instruction sets, a rewrite of NASM | `30_foundation` |

Notes:

- `binutils`: zstd stays off (`--without-zstd`): binutils builds in `20_selfhost`, where zstd does
  not exist, so it is off in every phase and `ld` is the same everywhere.
- `bison`: gettext is in `depends` because coloured diagnostics come from its libtextstyle, which
  configure probes with no switch; undeclared, the probe would follow build order.
- `bmake`: `BROKEN_TESTS` names `cmd-interrupt`, which races a signal against a sleep and fails at
  random under load; the install runs the unit tests first and stops on a failure.
- `clang`: `LLVM_EXTERNAL_CLANG_TOOLS_EXTRA_SOURCE_DIR` is what builds clangd and clang-tidy in a
  standalone build; without it the build succeeds with neither. `CLANG_DEFAULT_OPENMP_RUNTIME` stays
  libomp: the driver passes `-fopenmp` to the frontend only for libomp and libiomp5, so with libgomp
  every OpenMP pragma would be dropped. Both `*_INCLUDE_DOCS` switches are named, or the man-page
  targets do not exist.
- `compiler-rt`: UBSan is the only sanitizer built: the others intercept glibc internals and do not
  support musl.
- `crubit`: the commit is the `CRUBIT_REVISION` Chromium's `update_rust.py` names, because the
  chromium port compiles against the support library installed here. `cc_bindings_from_rs` links
  `librustc_driver`, so it runs only with the rust port it was built against.
- `gcc`: libstdc++'s `-gdb.py` hook is moved under `/usr/share/gdb/auto-load`: gdb loads it only
  from its safe path, and beside the library it is declined and every STL container prints as its
  internals.
- `libclc`: upstream's install writes one directory per triple and no `libclc.pc`, so the build adds
  the pkg-config file and the `spirv-mesa3d-.spv` and `spirv64-mesa3d-.spv` links Mesa's rusticl
  reads from its libexecdir; Mesa stops at setup without both.
- `libintl`: `--with-included-libintl` is required: musl's `gettext()` is an identity passthrough,
  so configure otherwise builds no library and the first consumer fails on `-lintl`. Only `intl/` is
  built and installed, after the whole runtime is configured, because its Makefile needs
  gettext-runtime's `config.h`; the header belongs to the gettext port.
- `libunwind`: it installs under `/usr/lib/llvm-libunwind` because `/usr/include/libunwind.h` and
  `libunwind.so` are libunwind-nongnu's. `LIBUNWIND_ENABLE_ASSERTIONS=OFF` is required: on, it adds
  `-UNDEBUG` after the Release flags.
- `lldb`: it is built standalone against the installed llvm and clang packages.
  `LLVM_TARGETS_TO_BUILD` stays unset, because it arrives from `LLVMConfig` and decides the ABI and
  disassembler plugins. `LLDB_INCLUDE_TESTS` must be set, or the test suite builds.
  `LLVM_MAIN_SRC_DIR` must name this tree for the man-page build to find `llvm_sphinx`.
- `llvm`: `LLVM_HOST_TRIPLE` and `LLVM_DEFAULT_TARGET_TRIPLE` are gcc's triple; LLVM's own guess is
  `x86_64-unknown-linux-gnu`, under which clang finds no gcc installation and no C++ headers.
  `LLVM_ENABLE_LIBEDIT=OFF`, because libedit builds in a later phase.
  `LLVM_TOOL_LLVM_IR2VEC_BUILD=OFF`: its helper libraries are never installed, so the binary cannot
  start. The man pages are the single `docs-llvm-man` target; `LLVM_BUILD_DOCS` would build several
  targets into one directory at once.
- `llvm21`: the triples are gcc's, for the same reason as llvm's.
- `openmp`: `LIBOMP_INSTALL_ALIASES=OFF`: the aliases include `libgomp.so`, which is gcc's.
- `spirv-llvm-translator`: the headers directory is a source-tree root to which the library adds
  `/include`, so `/usr` is the value that reaches `/usr/include/spirv/unified1`.
- `yasm`: `--enable-python` is required: the tag archive has no pregenerated `x86insns.c`. `XMLTO=:`
  keeps the prebuilt man pages, which xmlto would regenerate against a DocBook catalog.

### buildtools

Build systems and build helpers. 21 ports, under `ports/core/buildtools/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `autoconf` | 2.73 | Programs for producing shell scripts that can automatically configure source code | `30_foundation` |
| `autoconf-archive` | 2024.10.16 | The AX_* m4 macros a configure.ac reaches for and autoconf does not carry | `40_lang` |
| `automake` | 1.19 | Programs for generating Makefiles for use with Autoconf | `30_foundation` |
| `buildsystem` | 1.10 | NetSurf shared build framework (Makefile fragments used by libnsgif and friends) | `40_lang` |
| `cargo-c` | 0.10.25 | Cargo subcommand to build and install C-ABI-compatible dynamic and static libraries | `31_compilers` |
| `ccache` | 4.14 | Compiler cache | `30_foundation` |
| `ccache-manual` | 4.14 | The ccache manual page | `31_compilers` |
| `cmake` | 4.4.3 | Cross-platform build-system generator | `30_foundation` |
| `corrosion` | 0.6.1 | The CMake bridge that builds a cargo crate as a CMake target | `40_lang` |
| `gn` | 0.2540 | Chromium's meta-build system, which writes ninja files from BUILD.gn | `40_lang` |
| `intltool` | 0.51.0 | An internationalization tool used for extracting translatable strings from source files | `30_foundation` |
| `itstool` | 2.0.7 | Translates XML documents with PO files | `30_foundation` |
| `libtool` | 2.6.2 | The GNU generic library support script | `30_foundation` |
| `meson` | 1.12.1 | Build system that generates Ninja build files | `30_foundation` |
| `muon` | 0.7.0 | An implementation of the meson build system in C99, with a built-in ninja — the build system pkgconf is built with | `30_foundation` |
| `ninja` | 1.13.2 | Low-level build system that runs generated build files | `30_foundation` |
| `pkgconf` | 3.0.9 | Package compiler and linker metadata toolkit | `30_foundation` |
| `scons` | 4.11.1 | Python-based build tool (gpsd builds with it) | `40_lang` |
| `setconf` | 0.7.7 | Utility for changing settings in configuration files | `40_lang` |
| `unifdef` | 2.12 | Selectively remove C preprocessor conditionals | `40_lang` |
| `util-macros` | 1.20.2 | X.Org Autotools macros | `40_lang` |

Notes:

- `autoconf-archive`: a missing `AX_*` macro does not fail at `aclocal` time; it survives into
  `configure` as a literal word and fails there as `command not found`.
- `ccache`: `DEP_*=BUNDLED` pins the fmt, BLAKE3, cpp-httplib and tl-expected copies ccache
  carries: each finder otherwise takes a system copy whenever one is installed, so a
  `30_foundation` build and a later rebuild would differ.
- `ccache-manual`: a port of its own because asciidoctor is `31_compilers`' and ccache
  `30_foundation`'s, and `ENABLE_DOCUMENTATION` has no man-only form. It runs upstream's own
  generator with the arguments the doc target gives it.
- `gn`: the source is a snapshot of the commit Chromium's `DEPS` pins as `gn_version`:
  `git archive --prefix=gn/` of that commit plus `out/last_commit_position.h`, holding the
  position that is the version's minor number. A gn older than the pin rejects variables
  Chromium's BUILD files use.

### lang

Language implementations, plus the modules of any language that has no module shelf of its own. 27
ports, under `ports/core/lang/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `R` | 4.6.1 | Language and environment for statistics, with the recommended packages | `42_graphics` |
| `bwidget` | 1.10.1 | High-level Tk widgets written in pure Tcl: trees, notebooks, combo boxes, dialogs | `42_graphics` |
| `cabal-install` | 3.18.1.0 | The cabal command — builds and installs Haskell packages | `31_compilers` |
| `duktape` | 2.7.0 | Embeddable JavaScript engine (used by polkit for rule processing) | `40_lang` |
| `esbuild` | 0.28.2 | JavaScript and TypeScript bundler and minifier | `40_lang` |
| `ghc` | 9.12.4 | The Glasgow Haskell Compiler, its libraries and GHCi | `31_compilers` |
| `go` | 1.27.2 | The Go programming language toolchain (with bootstrap) | `31_compilers` |
| `guile` | 3.0.11 | Scheme implementation GnuCash and Aisleriot embed, with guild | `41_system` |
| `lua` | 5.5.1 | Lightweight programming language designed for extending applications | `30_foundation` |
| `lua54` | 5.4.9 | Lua 5.4, installed beside Lua 5.5 for software that requires 5.4 | `30_foundation` |
| `lua54-luaexpat` | 1.5.2 | Expat XML parser binding for Lua 5.4 — Prosody reads XMPP stanzas with it | `30_foundation` |
| `lua54-luafilesystem` | 1.9.0 | stat, mkdir and readdir for Lua 5.4 | `30_foundation` |
| `lua54-luasec` | 1.3.2 | TLS for LuaSocket on Lua 5.4 — Prosody encrypts its connections with it | `30_foundation` |
| `lua54-luasocket` | 3.1.0 | TCP, UDP and DNS for Lua 5.4 — Prosody's transport | `30_foundation` |
| `luajit` | 20260914 | Just-in-time compiler and drop-in replacement for Lua | `40_lang` |
| `nodejs` | 26.10.0 | JavaScript runtime built on Chrome's V8 JavaScript engine | `31_compilers` |
| `ocaml` | 5.5.1 | The OCaml native and bytecode compilers, runtime and standard library | `40_lang` |
| `ocaml-facile` | 1.1.4 | Constraint programming library for OCaml, the solver behind Kalzium's equation balancer | `40_lang` |
| `openjdk` | 25.0.4.1 | OpenJDK, the Java Development Kit: the JVM, the compiler and the class library (LTS) | `42_graphics` |
| `ruby` | 4.0.7 | The Ruby programming language | `31_compilers` |
| `rust` | 1.98.1 | The Rust compiler, with cargo, clippy, rustdoc and rustfmt | `31_compilers` |
| `tcl` | 9.0.4 | The Tcl scripting language — the interpreter yosys and weechat embed | `41_system` |
| `tk` | 9.0.4 | Tk, the GUI toolkit for Tcl — wish and libtcl9tk, drawn through X11 under Xwayland | `42_graphics` |
| `typescript-go` | 7.0.2 | TypeScript 7, the TypeScript compiler written in Go | `40_lang` |
| `vala` | 0.56.19 | Vala compiler and vapigen — C#-like language compiled to GObject C | `41_system` |
| `yarn` | 4.18.1 | JavaScript package manager, the Berry line | `40_lang` |
| `zig` | 0.16.0 | General-purpose programming language and toolchain | `31_compilers` |

Notes:

- `R`: `-D__MUSL__` in `CXXFLAGS` is recorded in `Makeconf`, which every later package build reads;
  Rcpp and the packages built on it choose their C library code paths by it.
- `cabal-install`: `_ghc` names the GHC the bootstrap plan was solved against, and the plan checks
  every GHC-shipped package by version, so `_ghc` moves with the ghc port.
- `guile`: libguile needs the base flags' `-std=gnu11`: under C23 its empty-parameter
  `scm_t_subr` means `(void)` and every subr call fails to compile.
- `lua`: `lua.pc` is written by two heredocs. The first expands `V` and `R`; the second is quoted,
  so every `${…}` reaches the file as a pkg-config variable. A single quoted heredoc leaves
  `Version:` unexpanded, and every consumer that asks for a minimum version finds no Lua. The
  `lua5.5.pc` link is what a consumer naming its Lua by version finds.
- `lua54`: every installed path is versioned (`lua5.4`, `include/lua5.4`, `liblua5.4`,
  `lua5.4.pc`), so nothing collides with the lua port. Its `lua5.4.pc` takes the same two-heredoc
  split.
- `lua54-luaexpat`, `lua54-luafilesystem`, `lua54-luasec`, `lua54-luasocket`: every path and
  version variable goes on the `make` command line. Upstream defaults to Lua 5.1, and a `make`
  argument beats the makefile's own assignment where an exported variable does not. luasocket's
  `CFLAGS_linux` is given whole, because upstream's carries `-ggdb3` after `MYCFLAGS`.
- `nodejs`: `--shared-sqlite` links `node:sqlite` against the system library, which must keep its
  session extension and column metadata. The Temporal patch compiles the time-zone rules in, from
  the icu port's `zoneinfo64.res`, copied over the vendored crate's.
- `openjdk`: the boot JDK must be the same feature release or the one before, so a bump past a
  feature release bumps `_seed` with it.
- `rust`: `src` and `rust-analyzer-proc-macro-srv` stay in `tools`: the rust-analyzer port resolves
  nothing in `std` without the first and expands no macro without the second. `rustc-dev` comes
  from its dist tarball, since `x.py install` has no step for it; rustc_private tools such as
  chromium's crubit link it.
- `tcl`: `--with-system-libtommath` puts `TCL_WITH_EXTERNAL_TOMMATH` in `tcl.pc`, which yosys needs
  to compile against tcl's public headers. `make` runs with `LD_LIBRARY_PATH` at the build
  directory, because the bundled packages' configure runs the uninstalled `tclsh`.
- `tk`: `--disable-zipfs` installs the script library as files. Appended to the shared library as a
  zip, it is lost to any tool that rewrites the ELF file.
- `typescript-go`: the `noembed` build reads `lib.*.d.ts` from the directory of the binary's
  resolved path, so they are installed beside it; a caller with its own edited set (chromium)
  places a copy of the binary beside those.
- `yarn`: `/usr/bin/yarn` exports `YARN_ENABLE_TELEMETRY=0` unless the caller set it, or every run
  reports to Yarn's servers.

### devtools

Debuggers, tracers, profilers, analysers, linters, language servers and developer utilities. 37
ports, under `ports/core/devtools/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `antlr2` | 2.7.7 | ANTLR 2 parser generator — the antlr tool and the C++ runtime library | `42_graphics` |
| `bcc` | 0.37.0 | The BPF Compiler Collection library, USDT probe support and the libbpf-tools tracers | `40_lang` |
| `bpftool` | 7.7.0 | Inspect and manage BPF programs and maps, and generate BPF skeletons | `40_lang` |
| `bpftrace` | 0.27.0 | High-level tracing language for eBPF, for tracing inside the kernel | `41_system` |
| `capstone` | 5.0.9 | Disassembly engine library (rizin disassembles with it) | `40_lang` |
| `castxml` | 0.8.0 | C++ declarations as XML, read by Clang — the parser behind pygccxml | `40_lang` |
| `cppcheck` | 2.22.0 | Static analyser for C and C++ that looks for undefined behaviour and bugs the compiler does not report | `41_system` |
| `delta` | 0.19.2 | A syntax-highlighting pager for git, diff, and grep output | `41_system` |
| `difftastic` | 0.71.0 | Syntax-aware structural diff | `40_lang` |
| `dwarves` | 1.32 | pahole and the DWARF tools — encodes the kernel's BTF type information | `60_kernel` |
| `gdb` | 17.2 | GNU Debugger | `41_system` |
| `gef` | 2026.01 | GDB extension with context, heap and ROP views | `41_system` |
| `gopls` | 0.23.0 | The Go language server | `40_lang` |
| `hurl` | 8.0.1 | Runs HTTP requests and their assertions from a plain-text file | `40_lang` |
| `hyperfine` | 1.20.0 | Statistical command-line benchmarking with warmup and outliers | `40_lang` |
| `imhex` | 1.38.1 | Hex editor for reverse engineering — pattern language, disassembler, diffing and data inspectors | `42_graphics` |
| `just` | 1.58.0 | A command runner for project-specific recipes | `40_lang` |
| `kdevelop-pg-qt` | 2.4.0 | LL(1) parser generator, for KDevelop's QMake project manager | `43_toolkits` |
| `libbpf` | 1.7.0 | The userspace side of BPF — load a program, read a map | `40_lang` |
| `libtraceevent` | 1.9.0 | Parser for the kernel's ftrace event format | `40_lang` |
| `libtracefs` | 1.8.3 | API for the tracefs filesystem | `40_lang` |
| `ltrace` | 0.8.1 | Library call tracer | `40_lang` |
| `perf` | 7.2.11 | The kernel's own profiler: sampling, counters, tracepoints and BPF | `41_system` |
| `rizin` | 0.9.1 | Reverse-engineering framework — disassembler and binary analysis in the terminal | `41_system` |
| `ruff` | 0.16.9 | The Python linter and formatter, as the ruff command and the Python module that runs it | `40_lang` |
| `rust-analyzer` | 2026.09.21 | The Rust language server | `40_lang` |
| `shellcheck` | 0.11.0 | Static analysis for sh and bash scripts — quoting, word splitting and portability | `41_system` |
| `shfmt` | 3.14.1 | Shell parser, formatter and linter | `40_lang` |
| `strace` | 7.2 | System call tracer | `40_lang` |
| `tinyxxd` | 1.3.17 | xxd — hex dump and its reverse, as a small standalone program | `40_lang` |
| `tokei` | 15.0.0 | Counts lines of code by language | `40_lang` |
| `tree-sitter` | 0.27.0 | The parser library and CLI — grammars compiled here, never fetched | `40_lang` |
| `universal-ctags` | 6.2.1 | Source-code tag index generator | `41_system` |
| `valgrind` | 3.27.1 | Instrumentation framework for memory debugging and profiling | `40_lang` |
| `xh` | 0.26.2 | Command-line HTTP client | `41_system` |
| `zeal` | 0.9.1 | Offline documentation browser — searchable API docsets for languages and libraries | `44_apps` |
| `zls` | 0.16.0 | The Zig language server | `40_lang` |

Notes:

- `bcc`: the source is upstream's `bcc-src-with-submodule` release asset; a tag archive lacks the
  libbpf and blazesym submodules and fails deep in a header. `ENABLE_LLVM_SHARED=OFF`, because
  the llvm port ships one library per component and no `libLLVM.so`. `REVISION` is passed because
  a tarball has no `.git`, and a missing revision ends up in the library's soname.
  `USE_BLAZESYM=0` keeps the libbpf-tools Makefile from running a networked cargo.
- `bpftool`: `VMLINUX_BTF` is named, so a build kernel without BTF fails the build instead of
  silently dropping the profiler and PID-iterator skeletons.
- `bpftrace`: `USE_SYSTEM_LIBBPF=ON` is required, because the tag archive's `libbpf/` submodule is
  empty. `llvm-config --libs` ends every C++ link: with LLVM split into one library per
  component, `--as-needed` otherwise leaves `bpftrace-aotrt` with unresolved LLVM symbols.
- `capstone`: the port follows the stable 5.x line that rizin builds against, never a 6.0
  prerelease.
- `delta`: the syntax and theme sets are the bat port's, copied over the vendored bat crate's, so
  delta and bat move together: the sets are bincode dumps of that bat version's types, and a set
  from another one aborts delta at the first highlighted line.
- `gopls`: the module is the `gopls/` subdirectory of the x/tools tag, named by `vendordir`;
  vendored at the root, the bundle is the parent module's and builds something else.
- `imhex`: the pattern library is moved beside the source as `ImHex-Patterns`, or the package
  carries no patterns, magic or encodings and the build does not say so. The plugins carry an
  `$ORIGIN` rpath: musl does not resolve a bare-name dependency against a library loaded by path,
  so without it the builtin plugin does not load and every command-line run aborts.
- `libbpf`: `LIBDIR` must be absolute. The install rules join `$(DESTDIR)$(LIBDIR)` with no
  separator, so a relative value packages headers and nothing else; the default is `lib64`.
- `perf`: the source is the linux port's tarball, so the two versions move together. `prefix`
  goes on both make lines and the install reuses the build's argument list, or the install
  re-probes, relinks and stages outside `/usr`. `LIBPERL=1` needs perl's `libperl.so`
  (`-Duseshrplib`). The manuals go through Asciidoctor, because the xmlto route passes libxslt's
  template limit on `perf-c2c`.
- `rizin`: the source is the `rizin-src` release, which carries the vendored subprojects. The
  bundled tree-sitter runtime stays: against the system one the shell parser is regenerated into
  the source tree and the build fails on a missing `parser.c`.
- `rust-analyzer`: it needs the rust port's standard-library source and its
  `rust-analyzer-proc-macro-srv`; without them it reports nothing for `std` and leaves every
  derive unresolved.
- `zeal`: the desktop entry is replaced to carry `StartupWMClass=org.zealdocs.zeal`, the Wayland
  app_id Zeal sets.
- `zls`: its version follows the zig port's. The dependency hashes in `build.sh` follow
  `build.zig.zon` on every bump, and `zig build --system` is what keeps the build offline.

### vcs

Version control and diff/merge tools. 10 ports, under `ports/core/vcs/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `gh` | 2.101.0 | Official GitHub command-line client | `40_lang` |
| `git` | 2.55.0 | Distributed version control system | `30_foundation` |
| `git-cola` | 4.19.0 | Git GUI for staging, committing and reviewing, with the git-dag history viewer | `44_apps` |
| `git-lfs` | 3.8.0 | Git extension that stores large files out of band | `40_lang` |
| `jujutsu` | 0.45.1 | Git-compatible version control system with an operation log | `40_lang` |
| `kdiff3` | 1.12.6 | Two- and three-way file and folder comparison and merge | `44_apps` |
| `lazygit` | 0.65.1 | Terminal UI for git commands | `40_lang` |
| `libgit2` | 1.9.7 | Portable, linkable C implementation of the Git core methods | `30_foundation` |
| `libkomparediff2` | 26.08.1 | Diff parsing and models, for KDevelop's patch review | `43_toolkits` |
| `tig` | 2.6.1 | ncurses interface for reading a git repository | `40_lang` |

Notes:

- `git`: `git-completion.zsh` is not installed. As `site-functions/_git` it comes first in `fpath`
  and shadows zsh's own, fuller `_git`.
- `git-lfs`: the package owns the `[filter "lfs"]` block in `/etc/gitconfig` that `git lfs install
  --system` writes. With no filter configured, a clone of an LFS repository succeeds and checks out
  pointer files.
- `libgit2`: every provider is named, so a missing library stops the configure rather than dropping
  HTTPS or SSH transport.
- `tig`: `--with-ncursesw` makes a missing wide ncurses a configure error; a narrow one draws UTF-8
  wrong. pcre2 and readline have no such switch, so the recipe checks `config.h` for all three.

### editors

Text editors, IDEs, notebooks and REPLs. 12 ports, under `ports/core/editors/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `geany` | 2.1 | GTK programmer's editor — syntax highlighting, symbols, build commands | `44_apps` |
| `helix` | 25.07.1 | Modal terminal text editor with tree-sitter syntax and language-server support (Rust) | `41_system` |
| `ipython` | 9.17.1 | Interactive Python shell | `41_system` |
| `jupyterlab` | 4.6.4 | JupyterLab and Jupyter Notebook — computational notebooks served locally to the web browser | `44_apps` |
| `kate` | 26.08.1 | KDE advanced text editor — tabs, split views, LSP, projects, search, terminal | `44_apps` |
| `kdevelop` | 26.08.1 | The KDE IDE for C, C++ and more, with libclang code analysis | `44_apps` |
| `lite-xl` | 2.1.8 | Text editor written in Lua, drawn with SDL3 and no toolkit | `44_apps` |
| `micro` | 2.0.15 | Terminal text editor with familiar keybindings and mouse support (Go) | `41_system` |
| `nano` | 9.2 | Small terminal text editor in the manner of Pico, with syntax colouring | `41_system` |
| `neovim` | 0.12.5 | Extensible Vim-based text editor with Lua plugins and tree-sitter | `41_system` |
| `qt-creator` | 20.0.2 | The Qt IDE for C++, QML and Python, with a clangd code model | `44_apps` |
| `spyder` | 6.1.7 | The scientific Python IDE: editor, IPython console, variable explorer and plots | `44_apps` |

Notes:

- `geany`: the terminal tab opens libvte-2.91 with `dlopen` and configure never looks for it;
  `vte3` in `depends` is the only thing that puts the tab there.
- `kdevelop`: `CMAKE_C_STANDARD=23` stays. The CMake plugin's lexer is C that includes `QtGlobal`,
  which needs C23's `static_assert`, and KDE's compiler settings otherwise make C strict C99.
- `nano`: `make install` writes no `/etc/nanorc`, and nano loads only the syntax files a nanorc
  includes; without the recipe's file every buffer is uncoloured and the libmagic matching is idle.
- `neovim`: the bundled dependencies are laid into CMake's download directory, where their
  `URL_HASH` matches and nothing is fetched; a bump that changes one of neovim's pins changes its
  `source` and `sha256` too.
- `qt-creator`: the Code Paster plugin stays built: the VCS plugin names it as a recommended
  plugin, and configure stops without its target.
- `spyder`: Spyder removes `PYTHONPATH` from itself and from every kernel and language server it
  starts, so `/usr/lib/spyder` is a virtual environment over the system Python, and spyder-kernels,
  wurlitzer and pyls-spyder sit in the system `site-packages`.

### doctools

Documentation toolchains, man pages, markup converters and TeX typesetting. 28 ports, under
`ports/core/doctools/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `asciidoc` | 10.2.1 | Text document format for short documents, articles, books and UNIX man pages | `30_foundation` |
| `asciidoctor` | 2.0.26 | AsciiDoc processor in Ruby — HTML, DocBook and manual pages from .adoc | `31_compilers` |
| `cmark` | 0.31.2 | CommonMark reference implementation: libcmark and the cmark converter | `40_lang` |
| `discount` | 3.0.2.0 | Markdown to HTML library and converter — libmarkdown and markdown | `40_lang` |
| `docbook-xml` | 4.5 | Document type definitions for verification of XML data files against the DocBook rule set | `30_foundation` |
| `docbook-xsl` | 1.79.2 | XML stylesheets for Docbook-xml transformations | `30_foundation` |
| `doxygen` | 1.18.0 | Documentation system for C++, C, Java, Objective-C, Python, IDL, PHP, C# | `30_foundation` |
| `fig2dev` | 3.2.9a | Converts Xfig drawings to EPS, PDF, SVG, PNG, LaTeX, PSTricks, TikZ and other formats | `41_system` |
| `go-md2man` | 2.0.7 | Converts Markdown to roff manual pages | `40_lang` |
| `groff` | 1.24.1 | The roff typesetting system — troff, nroff, eqn, tbl, pic and the PDF, PostScript and HTML output devices | `41_system` |
| `gtk-doc` | 1.36.1 | The m4 macro and gtk-doc.make that GNOME-lineage autotools projects include | `40_lang` |
| `help2man` | 1.49.3 | Turns a program's own --help into a man page | `30_foundation` |
| `lowdown` | 3.2.1 | Markdown translator to roff manual pages, HTML and LaTeX | `41_system` |
| `lyx` | 2.5.3 | Document processor that writes LaTeX — structure-first editing with typeset output | `44_apps` |
| `man-pages` | 6.19 | Linux kernel and C library manual pages — system calls, library functions, devices, file formats, overviews | `40_lang` |
| `mandoc` | 1.14.6 | Compact suite of tools for BSD mdoc and man formatting | `30_foundation` |
| `opensp` | 1.5.2 | SGML and XML parser library and the onsgmls, osx, ospam, ospcat, ospent and osgmlnorm tools | `41_system` |
| `pandoc` | 3.11 | Converts between markup formats — Markdown, HTML, LaTeX, DOCX, ODT, EPUB and more | `31_compilers` |
| `scdoc` | 1.11.5 | Simple man page generator for POSIX systems written in C99 | `30_foundation` |
| `sgml-common` | 0.6.3 | Creating and maintaining centralized SGML catalogs | `30_foundation` |
| `smu` | 1.5 | Markdown to HTML in one C file | `40_lang` |
| `texinfo` | 7.3 | Programs for reading, writing, and converting info pages | `30_foundation` |
| `texlive` | 20260301 | TeX, LaTeX, pdfTeX, XeTeX, LuaTeX, MetaPost, BibTeX and dvips from TeX Live 2026, with a curated texmf tree | `42_graphics` |
| `texlive-doc` | 20260301 | The English manuals and examples of every TeX Live 2026 package the texlive port ships, for texdoc | not built |
| `texstudio` | 4.9.8 | LaTeX editor — completion, live PDF preview with SyncTeX, spelling and grammar checking | `44_apps` |
| `typst` | 0.15.1 | Markup-based typesetting system | `40_lang` |
| `xmlto` | 0.0.29 | Front-end to an XSL toolchain | `30_foundation` |
| `xmltoman` | 0.6 | Convert manual pages written in XML to groff or HTML | `40_lang` |

Notes:

- `asciidoctor`: the gem is built with `GIT_DIR` naming a directory that does not exist: the gemspec
  lists its files with `git ls-files` and falls back to the tree only when that prints nothing.
- `docbook-xml`: each DTD's public identifier maps straight to its file: libxml2 does not pass a
  public lookup through the `rewriteSystem` entries, so a public entry naming the http URL sends a
  document with a relative system identifier to the network, and `xmllint --nonet` and `xmlto` fail.
- `docbook-xsl`: the nons and namespaced releases unpack into separate directories (the namespaced
  tarball's top directory is the source directory itself), and each gets `rewriteSystem` and
  `rewriteURI` entries for every base it is named by — sourceforge and cdn, http and https,
  versioned and `current`. A URI no rewrite matches is fetched, and `xsltproc --nonet` fails.
- `gtk-doc`: the port exists for its m4 macro and `gtk-doc.make`, which `autotools_support=true`
  installs. A missing `GTK_DOC_CHECK` survives into a generated configure as a literal and fails
  there with a shell syntax error.
- `groff`: `--without-urw-fonts` keeps the URW font search off: it takes ghostscript's font
  directory for the URW set, finds no AFM metrics there, and the install stops on font descriptions
  that were never generated. `soelim` and `roff(7)` are removed because mandoc owns those paths.
- `man-pages`: a page another port installs is removed with `rm` without `-f`, so a page upstream
  drops or renames fails the build. The subsection directories (`man3type`, …) fold into their base
  section, because mandoc's `man FILE` never looks in them.
- `mandoc`: configure is driven by `configure.local` alone and empties `CFLAGS` and `LDFLAGS` before
  reading it, so the exported flags reach the build only through that file. The `mandoc.db` index is
  written by `70_image`'s `080_whatis.sh`; without it `apropos` and `whatis` print nothing.
- `pandoc`: the port directory's `cabal.project.freeze` pins `pandoc-cli +lua +server` and
  `pandoc +embed_data_files`; a version bump regenerates it with
  `cabal freeze --constraint="pandoc-cli +lua +server" --constraint="pandoc +embed_data_files"`.
  Without `embed_data_files` every DOCX, ODT and EPUB conversion fails on the installed system.
- `smu`: `config.mk` assigns `CFLAGS` and `LDFLAGS`, so the flags go on make's command line; an
  exported value is ignored.
- `texlive`: `fmtutil.cnf`, `updmap.cfg` and the language
  files are written from the cut packages alone, because an entry for a package that is not shipped
  makes fmtutil and updmap fail.
- `texlive-doc`: listed in `ports/unbuilt`, so no phase builds it and the image does not carry it;
  `kpkg install texlive-doc` builds it on a machine. It cuts the same texmf tarball and
  `texlive.tlpdb` as `texlive`, and its `TL_ROOTS` must equal `texlive`'s, or `texdoc` lists
  packages whose manuals are missing and the package ships manuals for packages that are not
  installed. The two share `group = texlive`, so they are bumped together. The package-time prune
  removes PDF manuals under `usr/share/texmf-dist/doc`, so built as the tree stands the package
  holds the examples and sources without the PDFs.

### devlibs

General-purpose C and C++ utility libraries with no domain. 36 ports, under `ports/core/devlibs/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `abseil-cpp` | 20260817.0 | Google's C++ common libraries (strings, containers, synchronization, time) | `41_system` |
| `bdwgc` | 8.2.12 | The Boehm conservative garbage collector — w3m allocates through it | `41_system` |
| `boost` | 1.92.0 | Boost C++ libraries — the headers and the compiled components the catalogue links | `41_system` |
| `coeurl` | 0.3.2 | Asynchronous C++ wrapper around libcurl on a libevent loop | `41_system` |
| `docopt.cpp` | 0.6.3 | Command-line parser that derives the interface from the usage text | `41_system` |
| `double-conversion` | 3.4.0 | Binary-decimal and decimal-binary routines for IEEE doubles (Qt6 dep) | `41_system` |
| `fast-double-parser` | 0.8.1 | Header-only C++ parser from decimal strings to doubles, several times faster than strtod | `41_system` |
| `fmt` | 12.2.0 | The C++ formatting library std::format was standardised from | `41_system` |
| `gflags` | 2.3.1 | Google's C++ command-line flags library | `41_system` |
| `glog` | 0.7.1 | Google's C++ logging library, with gflags for its command-line switches | `41_system` |
| `gtest` | 1.18.0 | GoogleTest and GoogleMock — the C++ unit-test and mocking frameworks | `41_system` |
| `highway` | 1.4.0 | Performance-portable SIMD library with runtime CPU dispatch | `41_system` |
| `immer` | 0.9.1 | Immutable and persistent data structures for C++ (headers) | `41_system` |
| `jemalloc` | 5.4.0 | A malloc that holds fragmentation down under many threads | `41_system` |
| `lager` | 0.1.3 | Value-oriented unidirectional data-flow architecture for C++ (headers) | `41_system` |
| `libdaemon` | 0.14 | C library for writing UNIX daemons (avahi dependency) | `41_system` |
| `libtommath` | 1.3.0 | Multiple-precision integers — tcl bundles a renamed copy and yosys needs the real one | `41_system` |
| `liburcu` | 0.15.7 | Userspace RCU — required by xfsprogs, no --disable option exists | `41_system` |
| `mustache` | 4.1 | Kainjow Mustache, a header-only Mustache template engine for C++11 | `41_system` |
| `onetbb` | 2023.1.0 | oneAPI Threading Building Blocks: task-parallel C++ runtime | `42_graphics` |
| `pcre2` | 10.49 | Perl Compatible Regular Expressions library, version 2 | `30_foundation` |
| `pystring` | 1.2.0 | C++ functions with the behaviour of Python's string methods | `41_system` |
| `range-v3` | 0.12.0 | The header-only C++ ranges library C++20 ranges grew from | `41_system` |
| `re2` | 2025.11.05 | Google's linear-time regular expression library, with full Unicode properties from ICU | `41_system` |
| `rinutils` | 0.10.3 | Header-only C utility macros shared by Shlomi Fish's solvers | `41_system` |
| `rttr` | 0.9.6 | Run-time type reflection library for C++ | `41_system` |
| `sfsexp` | 1.4.1 | Small, fast S-expression parsing and printing library in C | `41_system` |
| `simde` | 0.8.2 | Portable implementations of SIMD intrinsics, header-only | `41_system` |
| `sparsehash` | 2.0.4 | Memory-efficient C++ hash map and set templates | `41_system` |
| `spdlog` | 1.17.0 | C++ logging library, over the system fmt | `41_system` |
| `talloc` | 2.5.0 | Hierarchical reference-counted memory pool library from Samba | `41_system` |
| `tevent` | 0.17.2 | Samba's event loop library on talloc — samba-util and the ndr libraries build on it | `41_system` |
| `tllist` | 1.1.0 | C header-only typed linked list library (foot/fcft dep) | `41_system` |
| `uthash` | 2.4.0 | A hash table for C structures (header-only) | `41_system` |
| `xxhash` | 0.8.4 | Non-cryptographic hash algorithm library | `20_selfhost` (as a dependency) |
| `zug` | 0.1.2 | Transducers for C++ (headers) | `41_system` |

Notes:

- `abseil-cpp`: built as C++17. `absl/base/options.h` is written from the standard the build used,
  and a C++17 build matches the ABI of consumers on every standard from 17 up, the ones that force
  C++17 included.
- `boost`: the compiled components are those the catalogue's consumers require through
  `find_package(... REQUIRED)`. The list goes to both `bootstrap.sh` and `b2`; on bootstrap alone
  one component is built. Changing it forces a libime rebuild. b2 reads no flags from the
  environment, so they ride on the `using gcc` line of `user-config.jam`. `icu` and `numpy` are in
  `depends` because bootstrap turns on Boost.Regex's ICU side and `boost_numpy` whenever it finds
  them.
- `gflags`: the library directory stays at its default: gflags declares it a `PATH` variable, so a
  value on the command line is taken relative to the build directory.
- `libdaemon`: `--disable-examples` is required on musl, because `examples/testd.c` includes
  `<sys/unistd.h>`.
- `libtommath`: tcl is built `--with-system-libtommath`; with tcl's own renamed copy, yosys fails to
  compile against tcl's headers.
- `onetbb`: `tbbmalloc_proxy` is not built: it hooks glibc-internal allocator entry points musl
  does not have.
- `sparsehash`: only `install-data` runs, after the one generated header is made, because `make`
  builds tests that do not compile as C++20.

### formats

Data-format and serialisation libraries (XML, JSON, YAML, TOML, INI, RDF, protobuf, thrift), plus
data query and convert tools. 35 ports, under `ports/core/formats/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `cereal` | 1.3.2 | Header-only C++ serialisation — bpftrace's BTF cache format | `41_system` |
| `cjson` | 1.7.19 | cJSON, a small ANSI C JSON parser (mosquitto links it) | `41_system` |
| `dotconf` | 1.4.1 | Configuration file parser (speech-dispatcher dependency) | `41_system` |
| `expat` | 2.9.0 | A stream oriented C library for parsing XML | `30_foundation` |
| `fx` | 39.2.0 | Interactive viewer and processor for JSON and YAML | `41_system` |
| `gumbo-parser` | 0.14.1 | HTML5 parsing library in C | `41_system` |
| `inih` | r62 | Tiny C library for parsing INI files (used by xdg-desktop-portal-wlr config) | `41_system` |
| `iniparser` | 4.3.0 | Small C library for parsing INI files | `41_system` |
| `jansson` | 2.15.1 | C library for encoding/decoding/manipulating JSON (NetworkManager dep) | `41_system` |
| `jq` | 1.8.2 | Command-line JSON processor | `41_system` |
| `json-c` | 0.19 | JSON implementation in C | `41_system` |
| `json-glib` | 1.10.8 | JSON parser/serializer library built on GLib (used by xdg-desktop-portal) | `41_system` |
| `jsoncpp` | 1.9.8 | JSON for C++ — recoll stores its index metadata through it | `41_system` |
| `libconfig` | 1.8.2 | Structured configuration file library for C and C++ | `41_system` |
| `libfyaml` | 0.9.6 | Complete YAML 1.2 parser and emitter library, and the fy-tool command | `41_system` |
| `libxml2` | 2.15.4 | Contains libraries and utilities used for parsing XML files | `30_foundation` |
| `libxmlb` | 0.3.29 | Library to help create and query binary XML blobs | `41_system` |
| `libxslt` | 1.1.45 | XSLT libraries used for extending libxml2 libraries to support XSLT files | `30_foundation` |
| `miller` | 6.21.0 | awk for CSV, TSV and JSON — named fields instead of column numbers | `41_system` |
| `nlohmann-json` | 3.12.0 | JSON for Modern C++, header-only | `41_system` |
| `protobuf` | 36.2 | Protocol Buffers — Google's language-neutral data interchange format | `41_system` |
| `protobuf-c` | 1.5.2 | Protocol Buffers for C — the runtime library and the protoc-gen-c code generator | `41_system` |
| `pugixml` | 1.16 | Light XML parser — libkiwix reads its OPDS catalogues with it | `41_system` |
| `rapidjson` | 1.1.0 | Header-only JSON parser and generator for C++ | `41_system` |
| `raptor2` | 2.0.16 | Raptor RDF syntax library — RDF/XML, Turtle and N-Triples parsers and serialisers | `41_system` |
| `thrift` | 0.25.0 | Apache Thrift — the IDL compiler, the C++ library and the Python module; GNU Radio's ControlPort transport | `41_system` |
| `tinyxml` | 2.6.2 | Small C++ XML parser (TinyXML 1) | `41_system` |
| `tinyxml2` | 11.0.0 | Small C++ XML parser (TinyXML-2) | `41_system` |
| `tomlplusplus` | 3.4.0 | TOML parser and serializer for C++17 | `41_system` |
| `visidata` | 3.4 | Terminal spreadsheet for tabular files | `42_graphics` |
| `xerces-c` | 3.3.0 | Validating XML parser library in C++ (DOM, SAX, schema) | `41_system` |
| `yajl` | 2.1.0 | Yet Another JSON Library — small, event-driven C JSON parser | `41_system` |
| `yaml` | 0.2.5 | C library for parsing and emitting YAML | `30_foundation` |
| `yaml-cpp` | 0.9.0 | YAML 1.2 parser and emitter for C++ | `41_system` |
| `yq` | 4.53.6 | Portable YAML / JSON / TOML / XML processor (Go, Mike Farah) | `41_system` |

Notes:

- `inih`: `with_INIReader=true` builds the C++ `libINIReader` beside `libinih`; exiv2 requires it
  by name.
- `json-c`: `DISABLE_EXTRA_LIBS=ON` keeps the seed probe from linking libbsd whenever it is
  installed; the hash seed comes from `getrandom()`.
- `jsoncpp`: built with meson, the build that writes the `jsoncpp.pc` recoll looks up; the cmake
  build writes only a CMake package.
- `libfyaml`: `ac_cv_search_trunc=` is required on musl. Without it configure writes `none
  required` into `libfyaml.pc`'s `Libs`, and every consumer links two libraries named `none` and
  `required`.
- `tinyxml`: upstream's makefile builds no library, so `build.sh` compiles `libtinyxml.so.0`
  itself. `define-stl.patch` makes the installed header always select the `std::string` API, so a
  consumer and the library agree on the class layout.
- `visidata`: `standard-mailcap` comes from the vendor bundle, since Python's standard library has
  no `mailcap` module. Each loader imports its library only when that format is opened, so a
  missing optional dependency still builds.

### database

Database engines, embedded key-value stores, database servers, clients and full-text indexes. 15
ports, under `ports/core/database/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `db` | 5.3.28 | Berkeley DB — the embedded key/value database library, with its C++ binding and db_* utilities | `41_system` |
| `duckdb` | 1.5.5 | In-process SQL database that queries CSV, JSON and Parquet files directly | `41_system` |
| `gdbm` | 1.26 | The GNU Database Manager | `30_foundation` |
| `libspatialite` | 5.1.0 | SpatiaLite — spatial SQL for SQLite, as a library and as mod_spatialite | `41_system` |
| `lmdb` | 0.9.36 | Lightning Memory-Mapped Database: an embedded B+tree key-value store, and its mdb_* tools | `41_system` |
| `lmdbxx` | 1.0.2 | C++17 header-only wrapper for the LMDB database library | `41_system` |
| `postgresql` | 18.6 | PostgreSQL relational database server | `41_system` |
| `recoll` | 1.44.1 | One full-text index across PDFs, ODT, EPUB, mail and source, with a search window | `44_apps` |
| `sqlite` | 3530400 | Self-contained, serverless, transactional SQL database engine | `30_foundation` |
| `sqlitebrowser` | 3.13.1 | DB Browser for SQLite — create, browse, edit and query SQLite databases | `44_apps` |
| `tdb` | 1.4.15 | Trivial database library and tdbtool — Samba's databases, Rhythmbox's metadata cache, libcanberra's sound cache | `41_system` |
| `unixodbc` | 2.3.14 | The ODBC driver manager KiCad and LibreOffice Base connect to databases through | `41_system` |
| `usql` | 0.21.6 | Command-line SQL client for PostgreSQL, SQLite and other databases | `41_system` |
| `valkey` | 9.1.2 | In-memory key-value store — the BSD continuation of Redis | `41_system` |
| `xapian-core` | 2.1.0 | The search engine library recoll indexes into | `41_system` |

Notes:

- `db`: `-fpermissive` keeps configure's probes, written with implicit declarations and implicit
  `int`, compiling; without it a failed probe reads as a missing feature.
- `duckdb`: extension autoloading and autoinstall are off, so the extensions in `BUILD_EXTENSIONS`
  are all there are; with them on, a query that needs another one downloads it from duckdb.org and
  fails offline.
- `postgresql`: `--with-icu` is what makes collation agree with other machines; musl's collation is
  byte order. `--without-llvm` stays until the JIT provider's version guards accept the llvm port's
  major version.
- `recoll`: poppler is in `depends` because PDFs are indexed through `pdftotext`; without it
  `recollindex` reports success and indexes no PDF. The GUI is built by qmake through `$QMAKE`,
  which names Qt 6's `qmake6`, or the first `qmake` on `PATH` builds it.
- `sqlite`: R*Tree and Geopoly stay on: a GeoPackage keeps its spatial index in an R*Tree table,
  and gdal refuses to build its driver without them.
- `sqlitebrowser`: it is Qt 5 only, so QScintilla, QCustomPlot and QHexEdit are the copies under
  `libs/`; the qscintilla port is the Qt 6 build, which it cannot link.
- `usql`: the `libsqlite3` and `duckdb_use_lib` tags link the sqlite and duckdb ports; without the
  second the bindings link a prebuilt glibc archive that does not link against musl.
- `valkey`: `MALLOC=libc` stays, because the bundled jemalloc builds against musl and aborts under
  load.
- `xapian-core`: `sed` in `depends` is GNU sed. configure filters `include/xapian/version.h`
  with a `0,/re/` address, a GNU extension; a sed without it writes the header empty and fails
  the compile on `XAPIAN_DOCID_BASE_TYPE`.

### archiver

Compression libraries and archive tools. 31 ports, under `ports/core/archiver/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `7zip` | 26.03 | Packs and unpacks 7z, zip, rar, tar, iso, cab, deb, rpm and disk images, with 7-Zip's 7zz | `41_system` |
| `ark` | 26.08.1 | Archive manager — browse, extract and create tar, zip, 7z and compressed archives | `44_apps` |
| `brotli` | 1.2.0 | Brotli compression library | `30_foundation` |
| `bzip2` | 1.0.8 | Block-sorting compressor: libbz2 and the bzip2, bunzip2 and bzcat commands | `30_foundation` |
| `c-blosc` | 1.21.6 | Blosc: a blocking, shuffling compressor for binary data — OpenVDB's grid codec | `41_system` |
| `cabextract` | 1.11 | Extract Microsoft cabinet (.cab) files | `41_system` |
| `cpio` | 2.15 | Copies files into and out of cpio and tar archives | `41_system` |
| `lhasa` | 0.6.0 | Free LHA/LZH archive library and lha extractor — MilkyTracker's .lha module archives | `41_system` |
| `libaec` | 1.1.7 | Adaptive Entropy Coding library and its libsz drop-in replacement for SZIP | `41_system` |
| `libarchive` | 3.8.10 | Multi-format archive and compression library, with bsdtar, bsdcpio, bsdcat and bsdunzip | `30_foundation` |
| `libdeflate` | 1.26 | DEFLATE/zlib/gzip compression library | `41_system` |
| `libmspack` | 0.11alpha | Library for Microsoft compression formats — CAB, CHM, HLP, LIT, KWAJ, SZDD | `41_system` |
| `libzip` | 1.11.4 | C library for reading and writing zip archives | `41_system` |
| `lz4` | 1.10.0 | LZ4 compression library and command-line tool | `30_foundation` |
| `lzip` | 1.26 | Lossless data compressor for the .lz format | `41_system` |
| `lzo` | 2.10 | LZO real-time data compression library | `30_foundation` |
| `minizip` | 1.3.2 | Read and write ZIP archives: the minizip library from zlib's contrib | `41_system` |
| `minizip-ng` | 4.2.2 | Zip archive manipulation library, the zlib-ng fork of minizip | `41_system` |
| `ouch` | 0.8.3 | Compresses and decompresses many archive formats with one command | `41_system` |
| `par2cmdline-turbo` | 1.5.0 | PAR2 recovery files with SIMD-accelerated Reed-Solomon coding | `41_system` |
| `snappy` | 1.3.1 | Google's fast compressor and decompressor, trading ratio for speed | `41_system` |
| `unshield` | 1.6.2 | Extract InstallShield cabinet (.cab) archives | `41_system` |
| `unzip` | 6.0 | ZIP extraction utilities | `41_system` |
| `wimlib` | 1.14.5 | Library and wimlib-imagex tool for Windows Imaging (WIM) archives | `41_system` |
| `xz` | 5.8.4 | XZ compression utilities | `30_foundation` |
| `zip` | 3.0 | Info-ZIP archive creation utilities | `41_system` |
| `zlib` | 1.3.2 | Compression library implementing the deflate compression method | `20_selfhost`, named again by `30_foundation` |
| `zlib-ng` | 2.3.3 | zlib data compression with SIMD, under its own zng_ names beside the system zlib | `41_system` |
| `zopfli` | 1.0.3 | Deflate compressor that trades time for size, with zopflipng for PNG files | `41_system` |
| `zstd` | 1.5.7 | Zstandard compression library and tools | `30_foundation` |
| `zziplib` | 0.13.80 | Read files inside ZIP archives through a stdio-like API — MilkyTracker's zipped modules and TeX Live's LuaTeX | `41_system` |

Notes:

- `7zip`: the makefile assigns `CFLAGS`, `CXXFLAGS` and `LDFLAGS` itself, so the tree's flags go in
  through `CFLAGS_BASE2`, `CXXFLAGS_BASE2` and `LDFLAGS_STATIC_3`.
- `bzip2`: both makefiles build the same object names, so the `make clean` between them is what
  makes `libbz2.a` recompile instead of archiving the `-fPIC` objects. The tarball ships no
  `bzip2.pc`; the recipe writes the one meson's `dependency('bzip2')` reads.
- `libdeflate`: OpenEXR finds it through pkg-config as its DEFLATE backend; without it OpenEXR's
  CMake downloads a copy, which the offline build cannot do.
- `lz4`: LTO carries `-frandom-seed`: the static archive keeps fat LTO objects, whose section names
  are random without a seed, so the archive would differ on every build.
- `lzip`: `configure` is hand-written and rejects `--disable-static` and the other autoconf flags.
- `minizip`: `install-ints-header.patch` edits `Makefile.am`, so `autoreconf` must follow the
  patches; without the patch a consumer's `#include <minizip/ioapi.h>` fails on the missing `ints.h`.
- `minizip-ng`: CMake clones PPMd's sources whatever `MZ_FETCH_LIBS` says;
  `FETCHCONTENT_SOURCE_DIR_PPMD` points it at the 7-Zip source the recipe carries.
- `ouch`: bzip3's bindings are generated by bindgen, which dlopens libclang, so `crt-static` is off
  for the build; the shipped binary links musl dynamically either way.
- `unzip`: the patch series beside the recipe is Alpine's, applied in its numbered order. The
  `unzips` target runs no configure, so ZIP64, large files, UTF-8 names and bzip2 exist only because
  `LOCAL_UNZIP` and the `*_BZ2` variables name them.
- `zip`: `unix/configure` compiles its probes with `$CC` alone, so
  `-Wno-implicit-function-declaration` rides on `CC`; without it every probe answers missing and the
  build stops on conflicting prototypes. `LDFLAGS` reach the link through `BIND`, and the build
  stops when the generated `flags` lack `-DBZIP2_SUPPORT`.
- `zlib-ng`: `ZLIB_COMPAT` stays off; on, the package installs `libz.so` and `zlib.h` over the zlib
  port's.
- `zstd`: LTO carries `-frandom-seed` for the same reason as lz4's. `ZSTD_LEGACY_FILES` names the
  legacy decoders, which the makefile otherwise collects from `ls` output, and must match
  `ZSTD_LEGACY_SUPPORT`. `HAVE_*=1` skips the programs' probes, so a missing zlib, xz or lz4 is a
  link error rather than a narrower `zstd`.

### shells

Interactive shells and their add-ons, terminal multiplexers, and terminal emulators. 10 ports, under
`ports/core/shells/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `atuin` | 18.23.0 | Shell history in a searchable SQLite database, local only | `41_system` |
| `bash-completion` | 2.18.0 | Programmable completion for the bash shell | `41_system` |
| `byobu` | 7.19 | Text-based window manager and terminal multiplexer | `41_system` |
| `direnv` | 2.37.1 | Loads and unloads environment variables per directory | `41_system` |
| `foot` | 1.28.0 | Wayland terminal emulator | `42_graphics` |
| `konsole` | 26.08.1 | KDE terminal emulator — tabs, split views, profiles, SSH manager | `43_toolkits` |
| `starship` | 1.26.0 | Configurable shell prompt | `41_system` |
| `tmux` | 3.7c | Terminal multiplexer | `41_system` |
| `zoxide` | 0.10.0 | A cd command that ranks directories by use | `41_system` |
| `zsh` | 5.9.2 | Z shell, with an extensive completion system | `41_system` |

Notes:

- `atuin`: the crates are unpacked beside the tree, not into its `vendor/`, which holds axoasset as
  a path crate without `.cargo-checksum.json`; a directory source over it refuses the whole
  registry.
- `konsole`: `INSTALL_ICONS` puts Konsole's own `utilities-terminal` PNGs in hicolor; the icon atlas
  lacks that name, and without them the Start menu row has no picture.
- `zsh`: `/etc/zsh/zprofile` sources `/etc/profile` under `emulate sh`: zsh never reads it, and the
  session's `PATH`, XDG directories and `/etc/profile.d` live there. `--enable-pcre` only asks, so
  `config.modules` is checked after configure.

### cli

Command-line productivity utilities: search, view, navigation and text wrangling in the terminal. 19
ports, under `ports/core/cli/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `bat` | 0.26.1 | Cat clone with syntax highlighting and Git integration | `41_system` |
| `bitwise` | 0.70 | Bit-level manipulation and base conversion, watched live in the terminal | `41_system` |
| `chezmoi` | 2.72.2 | Manage dotfiles across machines from one source of truth | `41_system` |
| `entr` | 5.8 | Run an arbitrary command when files change | `41_system` |
| `eza` | 0.23.5 | Replacement for ls, with colours and Git status | `41_system` |
| `fd` | 10.5.0 | File finder, an alternative to find | `41_system` |
| `fzf` | 0.74.4 | General-purpose command-line fuzzy finder | `41_system` |
| `glow` | 3.0.0 | Render markdown on the command line | `41_system` |
| `hexyl` | 0.17.0 | Hex viewer that colours bytes by class | `41_system` |
| `lesspipe` | 2.28 | An input filter for less that shows what is inside a file | `41_system` |
| `lnav` | 0.14.1 | Log navigator with SQL over log lines | `41_system` |
| `parallel` | 20260922 | Run shell command lines in parallel across cores and hosts | `41_system` |
| `pv` | 1.12.0 | Pipe viewer — progress, throughput and ETA for any stream | `41_system` |
| `ripgrep` | 15.2.0 | Recursively search directories for a regex pattern | `41_system` |
| `ripgrep-all` | 0.10.10 | ripgrep that also searches inside PDFs, archives and spreadsheets | `44_apps` |
| `sd` | 1.1.0 | Find-and-replace, a sed alternative for the common case | `41_system` |
| `tealdeer` | 1.9.0 | Example-first help — the tldr pages, pre-seeded and never fetched | `41_system` |
| `tree` | 2.3.2 | Display directory contents as a tree in a terminal | `41_system` |
| `ttyper` | 1.6.0 | Terminal touch-typing practice with per-key statistics | `41_system` |

Notes:

- `bat`: the syntax and theme sets are compiled in the build from `bat-assets-<version>.tar.xz`,
  the 92 submodules under `assets/` at the tag's pinned commits, which the release archive leaves
  empty; the bundle is remade for each version. `--source=assets` stays relative: an absolute
  source writes the build directory into the set. `delta` and `presenterm` take the sets from
  `/usr/share/bat/assets`.
- `bat`: `/usr/libexec/bat/man-pager` is the `MANPAGER` `/etc/bash.bashrc` names. mandoc's `man`
  splits `MANPAGER` on spaces and execs it with no shell, so the value is a script, never a
  `sh -c` line.
- `entr`: `RELEASE=$version` is passed to `make`: the Makefile carries the previous release, and
  `entr -h` would report it.
- `fd`: the `use-jemalloc` feature is on: the parallel walker allocates from every thread, which
  musl's allocator serialises behind one lock.
- `lnav`: the vendor bundle is unpacked into `src/`, not beside the Rust extension's manifest:
  the Makefile runs cargo from `src/`, and cargo finds `.cargo/config` only by walking up from the
  working directory, so a bundle elsewhere leaves a git dependency to be cloned.
- `parallel`: `/etc/parallel/will-cite` ships empty; without it the first interactive run blocks
  on the citation prompt, which in a script is a hang with no output.
- `ripgrep`: `PCRE2_SYS_STATIC=0` and a binary that is not crt-static link `rg -P` to the pcre2
  port; otherwise pcre2-sys compiles its bundled copy on musl.
- `ripgrep-all`: its adapters are programs it runs (pdftotext, pandoc, ffprobe, the decompressors);
  a missing one is a file type searched as nothing, so each is in `depends`.
- `tealdeer`: the pages ship in the layout of a downloaded cache,
  `/usr/share/tldr/tldr-pages/pages.en/`, which is the only path a lookup reads. kpkg unpacks the
  `tldr-pages` source into `$SRC_ROOT`; the build copies from there.

### files

File managers, disk-usage analysers, duplicate finders and file locators. 11 ports, under
`ports/core/files/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `dolphin` | 26.08.1 | File manager — tabs, split views, places, previews, a terminal panel and devices | `44_apps` |
| `duf` | 0.9.1 | Disk usage and free space, in the manner of df | `41_system` |
| `dust` | 1.2.6 | Disk usage as a tree, a du alternative (Rust) | `41_system` |
| `filelight` | 26.08.1 | Disk usage — folders drawn as concentric rings, sized by what they hold | `44_apps` |
| `lf` | 42 | Terminal file manager (Go, single binary) | `41_system` |
| `mc` | 4.8.33 | Norton-style two-panel terminal file manager (GNU Midnight Commander) | `41_system` |
| `ncdu` | 2.11.1 | Disk usage analyser with an ncurses interface | `41_system` |
| `plocate` | 1.1.25 | Finds a file by name in a prebuilt index, per user and without a setgid bit | `41_system` |
| `qdirstat` | 2.0 | Disk usage as a tree and a treemap, with cleanup actions | `44_apps` |
| `rdfind` | 1.8.0 | Find duplicate files and replace them with hardlinks or symlinks | `41_system` |
| `yazi` | 26.9.1 | Asynchronous terminal file manager with image previews | `44_apps` |

Notes:

- `plocate`: the first source has no wrapping directory, and kpkg strips one level from a first
  source, so the recipe unpacks it itself; preflight checks the line.
- `qdirstat`: built in the source tree: the manual page rule gzips its pages from the directory
  it runs in and finds nothing from a separate build directory.
- `yazi`: `VERGEN_GIT_SHA` is set to the release tag. A tarball has no `.git`, and the build then
  fails on the crate's `env!("VERGEN_GIT_SHA")`.

### sysmon

Process, resource and system-information monitors. 8 ports, under `ports/core/sysmon/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `bottom` | 0.14.9 | Graphical process and system monitor for the terminal, as btm | `41_system` |
| `btop` | 1.4.7 | A monitor for system resources | `41_system` |
| `fastfetch` | 2.68.1 | System information tool, with the KDOS banner config | `44_apps` |
| `htop` | 3.5.3 | Interactive process viewer | `41_system` |
| `iotop` | 1.31 | A top-like utility for I/O (C rewrite) | `41_system` |
| `nvtop` | 3.3.2 | GPU utilisation from DRM fdinfo — top for every vendor's GPU | `42_graphics` |
| `procs` | 0.14.12 | A ps replacement | `41_system` |
| `resources` | 1.10.2 | System monitor for CPU, memory, GPU, NPU, disks, network, battery and processes (GTK 4) | `44_apps` |

Notes:

- `btop`: the `statvfs64` define is exported in `CXXFLAGS`; passed to make it would replace the
  phase's flags and the prefix map with them.
- `fastfetch`: `ENABLE_XRANDR`, `ENABLE_XCB_RANDR`, `ENABLE_VAX11` and `ENABLE_GLX` stay off, or
  libX11 and libxcb join the host. Every library switch is named, because an `ON` whose library is
  missing builds without it. Lua is lua54: fastfetch opens `liblua5.<minor>.so`, a name only lua54
  installs.
- `resources`: meson runs cargo twice with `CARGO_HOME` under the build tree, so the vendor bundle's
  `.cargo/config.toml` is found by cargo's upward search from `build/`; one bundle serves both lock
  files (`vendorsync`).

### hardware

Device access, bus and sensor tools, power management, and Bluetooth. 29 ports, under
`ports/core/hardware/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `bluez` | 5.87 | Bluetooth protocol stack for Linux | `41_system` |
| `bolt` | 0.9.11 | Thunderbolt device manager, so a dock behind firmware security is authorised | `41_system` |
| `ddcutil` | 3.0.2 | Brightness, contrast and input source of an external monitor over DDC/CI | `42_graphics` |
| `dmidecode` | 3.7 | Prints the SMBIOS table in readable form | `41_system` |
| `freeipmi` | 1.6.19 | IPMI tools and libraries — sensors, event log, power control and serial-over-LAN for server BMCs and IPMI power supplies | `41_system` |
| `gusb` | 0.4.9 | GObject wrapper over libusb — libfprint's device layer | `41_system` |
| `hidapi` | 0.15.0 | HID device access — CMSIS-DAP debuggers and instruments speak it | `41_system` |
| `hwdata` | 0.411 | Hardware identification and configuration data | `41_system` |
| `hwloc` | 2.15.0 | Portable hardware locality — the CPU, cache, NUMA and device topology, with lstopo | `42_graphics` |
| `i2c-tools` | 4.4 | i2cdetect, i2cget, i2cset and the SMBus library, over /dev/i2c-* | `41_system` |
| `libcec` | 8.1.7 | HDMI-CEC control library for Pulse-Eight adapters and the kernel CEC framework, with cec-client | `41_system` |
| `libftdi` | 1.5 | Library for FTDI USB serial and JTAG bridge chips | `41_system` |
| `libgpiod` | 2.3.1 | The chardev GPIO ABI — gpioset, gpioget and gpiomon | `41_system` |
| `libpciaccess` | 0.19 | Generic PCI device access library | `41_system` |
| `libserialport` | 0.1.2 | Enumerate and open a serial port, portably | `41_system` |
| `libusb` | 1.0.30 | User-space USB device access library | `41_system` |
| `libusb-compat` | 0.1.9 | The libusb-0.1 API over libusb-1.0, for programs still written against usb.h | `41_system` |
| `lirc` | 0.10.2 | Infrared remote control daemon, client library and tools — lircd, irexec, irw, liblirc_client and the Python bindings | `42_graphics` |
| `lm-sensors` | 3.6.2 | Hardware sensor readings — temperatures, fans and voltages from hwmon | `41_system` |
| `numactl` | 2.0.19 | NUMA policy library (libnuma) and the numactl, numastat and migratepages tools | `41_system` |
| `nut` | 2.8.5 | UPS and inverter monitoring that shuts the machine down before the battery runs out | `41_system` |
| `pciutils` | 3.15.0 | Set of programs for listing PCI devices | `41_system` |
| `powerman` | 2.4.4 | Controls remote power distribution units and BMCs over telnet, serial, HTTP, Redfish and SNMP | `41_system` |
| `powertop` | 2.16 | Power consumption diagnosis and tuning tool | `41_system` |
| `switcheroo-control` | 3.0 | D-Bus service that tells launchers which GPU is discrete, so an application can be started on it | `42_graphics` |
| `thermald` | 2.5.13 | Intel thermal daemon — keeps a laptop off its throttle point under load | `41_system` |
| `tlp` | 1.10.2 | Laptop power saving — pure shell over /sys, no daemon | `41_system` |
| `upower` | 1.91.4 | D-Bus daemon for power-related stats and policy (battery, AC, suspend hints) | `41_system` |
| `usbutils` | 019 | Utilities used to display information about USB devices | `41_system` |

Notes:

- `bluez`: `--enable-cups` installs the Bluetooth printer backend only when configure can read cups'
  `serverbin` from pkg-config, so cups is in `depends`.
- `bolt`: `--localstatedir=/var` is compiled into boltd as the device database path; under any other
  prefix the enrolled device keys leave `/var/lib/boltd`.
- `ddcutil`: `--disable-x11` is required, because configure stops on a tree with no `xrandr.pc`.
  Upstream's udev rule is deleted: it opens every I2C bus, the SMBus and the DIMMs' SPD EEPROMs
  included, to an `i2c` group. `fs/etc/udev/rules.d/70-kdos-i2c.rules` is the scoped replacement.
- `dmidecode`: `install-completion` copies only into a completion directory the build root already
  has, so the recipe installs each built program's completion itself.
- `freeipmi`: the tools' worker threads overflow musl's 128 KiB default thread stack; the `-z
  stack-size=0x200000` linker note raises it to 2 MiB.
- `hidapi`: only the hidraw backend is built. The libusb backend detaches the kernel driver and
  bypasses the `dialout` access that `70-kdos-debug.rules` grants.
- `libftdi`: `libftdipp1.pc` is installed even with the C++ wrapper off, and is removed; a configure
  that finds it links `-lftdipp1` and fails.
- `lirc`: configure installs systemd units whenever the build host's `/proc/version` names Ubuntu,
  whatever the flag says; the recipe removes them.
- `lm-sensors`: every make variable goes to both the build and the install, or the library looks for
  `sensors.conf` where it was not installed. `sensors-detect` is removed from the package: it probes
  the SMBus by writing to it.
- `powertop`: pciutils and libnl are optional upstream with no switch, so `depends` is what keeps
  device names and the wireless tunables in the build.
- `thermald`: the tarball has no `m4/` directory, and autoreconf's gtkdocize fails without one, so
  the recipe creates it.

### input

Input-device libraries, keyboard data, input methods and on-screen keyboards. 16 ports, under
`ports/core/input/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `anthy-unicode` | 1.0.0.20260213 | Japanese kana-kanji conversion engine, the maintained Anthy fork | `50_desktop` |
| `fcitx5` | 5.1.22 | Input method framework, Wayland only on KDOS | `50_desktop` |
| `fcitx5-anthy` | 5.1.11 | Japanese input for fcitx5, via anthy-unicode | `50_desktop` |
| `fcitx5-chinese-addons` | 5.1.14 | Pinyin, shuangpin and table input for fcitx5 | `50_desktop` |
| `fcitx5-hangul` | 5.1.11 | Korean input for fcitx5, via libhangul | `50_desktop` |
| `libei` | 1.6.0 | Emulated input: libei for clients, libeis for compositors, liboeffis for the RemoteDesktop portal | `42_graphics` |
| `libevdev` | 1.13.7 | Wrapper library for kernel evdev input devices | `41_system` |
| `libhangul` | 0.2.0 | Hangul input processing library | `50_desktop` |
| `libime` | 1.1.16 | Pinyin and table input method engine library for fcitx5 | `50_desktop` |
| `libinput` | 1.32.0 | Input device handling for Wayland compositors: pointer acceleration, gestures, touchpads and tablets | `41_system` |
| `libwacom` | 2.20.0 | The tablet database libinput reads to pair a stylus with its pad and tell a pen display from a tablet | `41_system` |
| `libxkbcommon` | 1.13.2 | Keymap compiler and support library, with its Wayland tools and xkbcommon-x11 | `42_graphics` |
| `mtdev` | 1.1.7 | Translates every kernel multitouch event variant to the slotted type B protocol | `41_system` |
| `spacenavd` | 1.3.1 | The 6-DoF 3D mouse daemon — serves 3Dconnexion devices to libspnav clients over /run/spnav.sock | `41_system` |
| `wvkbd` | 0.20 | On-screen keyboard for Wayland — layer-shell and virtual-keyboard, drawn with cairo and pango | `42_graphics` |
| `xkeyboard-config` | 2.48 | Keyboard configuration database (keymap data, used by libxkbcommon) | `41_system` |

Notes:

- `anthy-unicode`: configure runs with `EMACS=no`, or an emacs on the build path compiles and
  installs the elisp front end.
- `fcitx5`: `ENABLE_TESTING_ADDONS` stays on: fcitx5-anthy, fcitx5-hangul and fcitx5-chinese-addons
  each require its TestFrontend module and fail to configure without it. `en_dict` is a recipe
  source copied into `src/modules/spell/`, where the spell module finds it instead of downloading
  it during a build that has no network.
- `fcitx5-chinese-addons`: the pinyin and stroke tables are recipe sources copied, not extracted,
  into `modules/pinyinhelper/`, where the download step finds them and checks their hashes. Cloud
  pinyin stays off: it sends what the user types to a remote service.
- `libime`: the language model, dictionary and table set are recipe sources copied into `data/`;
  without them the build reaches for 49 MB of downloads.
- `libinput`: `autoload-plugins` stays on: wlroots never asks libinput to load plugins, so without
  it a Lua plugin in `/etc/libinput/plugins` is never read.
- `libwacom`: the udev directory is `/usr/lib/udev`, where kpkg rebuilds the hwdb from; a hwdb file
  elsewhere never tags a tablet `ID_INPUT_TABLET`. `libwacom-show-stylus` is removed, since the
  Python modules it imports have no port.
- `libxkbcommon`: `enable-x11` builds libxkbcommon-x11, which Qt's xcb platform plugin needs for
  every Qt program under Xwayland.
- `wvkbd`: the compiler flags ride in `CC`, because `config.mk` assigns `CFLAGS`, and a `CFLAGS` on
  the `make` line would drop the Makefile's own pkg-config flags.

### mobile

Phones, media players and cameras over USB or the network. 14 ports, under `ports/core/mobile/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `android-file-transfer` | 4.5 | An Android phone's storage in a window, as a FUSE directory (aft-mtp-mount) and in the aft-mtp-cli shell | `44_apps` |
| `android-tools` | 37.0.0 | Android platform tools — adb, fastboot, and the sparse-image and boot-image utilities | `41_system` |
| `gphoto2` | 2.5.32 | Tethered capture and card download from a prompt | `44_apps` |
| `ifuse` | 1.2.1 | FUSE filesystem that mounts an iPhone's or iPad's media and app documents | `41_system` |
| `kdeconnect` | 26.08.1 | Files, clipboard, notifications and media control between this machine and a phone, over the LAN or Bluetooth | `44_apps` |
| `libgphoto2` | 2.5.34 | Drive a camera over PTP — the udev rules already grant the class | `42_graphics` |
| `libgpod` | 0.8.3 | Library to read and write the music database of an iPod classic, nano and shuffle | `41_system` |
| `libimobiledevice` | 1.4.0 | Library and idevice* tools that speak the iPhone and iPad protocols: pairing, backup, files, syslog | `41_system` |
| `libimobiledevice-glue` | 1.3.2 | Common socket, thread and utility code shared by the libimobiledevice libraries | `41_system` |
| `libmtp` | 1.1.23 | Talk MTP to a phone or a player — the class gphoto2 cannot reach | `41_system` |
| `libplist` | 2.7.0 | Apple property list library and plistutil — binary, XML, JSON and OpenStep plists | `41_system` |
| `libtatsu` | 1.0.5 | Client for Apple's Tatsu Signing Server, as used to restore and personalise iOS devices | `41_system` |
| `libusbmuxd` | 2.1.1 | Client library for usbmuxd, with iproxy and inetcat to tunnel TCP to an iOS device | `41_system` |
| `usbmuxd` | 1.1.1.20251206 | USB multiplexing daemon that carries every connection to an iPhone or iPad over its cable | `41_system` |

Notes:

- `libgphoto2`: a `--with-<lib>` value other than `no` or `auto` is read as an install prefix and
  links a library name that does not exist, so the library checks stay on `auto` and the build
  fails when `config.h` lacks one of them.
- `libusbmuxd`: `--with-inotify` is never passed, in either spelling. configure turns inotify off
  whenever the option is given, and inotify is how the library sees usbmuxd's socket appear.
- `usbmuxd`: `--without-systemd` makes the udev rule start the daemon itself. That rule is the only
  thing that starts usbmuxd, and `postinstall.sh` creates the `usbmux` account it drops to.

### disk

Block devices: partitioning, RAID, LVM, block encryption, SMART, NVMe, recovery, image writing and
async I/O. 30 ports, under `ports/core/disk/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `caligula` | 0.5.0 | Writes an image to a removable disk, with a progress display and a verify pass | `41_system` |
| `cryptsetup` | 2.8.8 | Used to set up transparent encryption of block devices using the kernel crypto API | `41_system` |
| `ddrescue` | 1.30 | Image a failing disk with a resumable mapfile | `41_system` |
| `f3` | 10.0 | Tests a flash drive's real capacity, detecting counterfeit drives | `41_system` |
| `gnome-disk-utility` | 46.1 | Partition, format, encrypt, image and benchmark drives over udisks2 (GTK 3) | `44_apps` |
| `gparted` | 1.8.1 | Create, resize, move, copy and check partitions and filesystems | `44_apps` |
| `gptfdisk` | 1.0.10 | A text-mode partitioning tool that works on GUID Partition Table (GPT) disks | `41_system` |
| `hdparm` | 9.65 | Set and read ATA power management, spindown and secure erase | `41_system` |
| `impression` | 3.8.0 | Write a disk image to a USB drive or memory card | `44_apps` |
| `libaio` | 0.3.113 | The Linux-native asynchronous I/O facility (aio) library | `41_system` |
| `libatasmart` | 0.19 | ATA S.M.A.R.T. reading and parsing library | `41_system` |
| `libblockdev` | 3.5.0 | Library for manipulating block devices: partitions, filesystems, LVM, MD RAID, crypto, NVMe | `43_toolkits` |
| `libbytesize` | 2.12 | Library for working with arbitrarily large sizes in bytes | `41_system` |
| `libiscsi` | 1.20.3 | Userspace iSCSI initiator library and client tools (iscsi-ls, iscsi-inq, iscsi-swp) | `41_system` |
| `libnvme` | 1.16.2 | C library for NVM Express on Linux | `41_system` |
| `libstoragemgmt` | 1.11.0 | Storage array and local disk management library, the lsmd plugin daemon and lsmcli | `41_system` |
| `liburing` | 2.15 | Linux kernel io_uring access library | `41_system` |
| `lvm2` | 2.03.42 | Logical volume management tools | `41_system` |
| `mdadm` | 4.6 | Linux software RAID management | `41_system` |
| `ndctl` | 85 | Userspace tools and libraries for NVDIMM, DAX and CXL devices (ndctl, daxctl, cxl) | `41_system` |
| `nvme-cli` | 3.1 | NVMe management — the drive's log pages, SMART data and self-tests | `41_system` |
| `partclone` | 0.3.50 | Filesystem-aware imaging — copies the used blocks and skips the rest | `41_system` |
| `parted` | 3.8 | Disk partitioning and partition resizing tool | `41_system` |
| `smartmontools` | 7.5 | SMART attribute reporting and disk self-tests | `41_system` |
| `testdisk` | 7.3pre20260819 | Recover lost partitions and, as photorec and the QPhotoRec window, deleted files | `44_apps` |
| `thin-provisioning-tools` | 1.3.4 | thin_check and its family, without which LVM refuses to activate a thin pool | `41_system` |
| `ubdsrv` | 1.8 | ublk userspace block devices — libublksrv, and the ublk tool with its loop, null, NBD and NFS targets | `41_system` |
| `udisks2` | 2.11.2 | Disk management daemon and D-Bus API for toolkit applications | `43_toolkits` |
| `veracrypt` | 1.26.29 | Create and mount encrypted volumes and containers, TrueCrypt-compatible | `44_apps` |
| `volume_key` | 0.3.12 | Library for manipulating storage volume encryption keys and storing them separately from volumes to handle forgotten passphrases | `43_toolkits` |

Notes:

- `f3`: `-largp` goes in `LDFLAGS`, the only variable f3's link lines read; musl has no argp, and
  the Makefile adds `-largp` only off Linux.
- `gparted`: `gpartedbin` runs as root through pkexec, which drops `WAYLAND_DISPLAY`, so the window
  is on X11 under Xwayland and the entry's `StartupWMClass` is `Gpartedbin`.
- `impression`: the vendor bundle is unpacked at the source root: meson runs cargo under `build/`,
  and cargo finds `.cargo/config.toml` only by searching upward from there.
- `liburing`: configure is given no `--prefix` (its default is `/usr`): it resolves one with
  `realpath -s`, and make re-runs a failed configure with no options.
- `lvm2`: the thin and cache tool paths are named and `THIN_CONFIGURE_WARN`/`CACHE_CONFIGURE_WARN`
  set, because thin-provisioning-tools builds after lvm2 and a probe that finds no tool turns
  `--needs-check` off. `69-dm-lvm.rules` is removed: it activates volume groups through
  `systemd-run`; `/etc/init.d/03_lvm.sh` and the initramfs activate them.
- `mdadm`: the Makefile assigns `CFLAGS` with `=`, so extra flags go in `CXFLAGS`; `CWFLAGS` stays
  as it is, carrying the `-fwrapv` and `-fno-delete-null-pointer-checks` the code relies on.
- `partclone`: a filesystem whose `--enable-*` flag or library is missing is not built, and
  configure only warns. The man pages name their stylesheet by URL, so `MAN_STYLESHEET` points at
  the installed docbook-xsl.
- `thin-provisioning-tools`: the tools install to `/usr/sbin`, the absolute path the shipped
  `lvm.conf` names; LVM refuses a thin pool when `thin_check` cannot run.

### filesystem

Filesystem utilities, FUSE, and network or encrypted filesystems. 20 ports, under
`ports/core/filesystem/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `btrfs-progs` | 7.1 | Btrfs filesystem utilities | `41_system` |
| `cifs-utils` | 7.7 | mount.cifs and the SMB/CIFS userspace helpers | `41_system` |
| `dosfstools` | 4.2 | Various utilities for use with the FAT family of file systems | `41_system` |
| `e2fsprogs` | 1.47.4 | Utilities for the ext2, ext3 and ext4 filesystems | `41_system` |
| `erofs-utils` | 1.9.4 | Tools for the EROFS read-only filesystem KDOS packs are built as | `41_system` |
| `exfatprogs` | 1.4.3 | exFAT filesystem utilities (mkfs, fsck, label) | `41_system` |
| `f2fs-tools` | 1.16.0 | Tools to create, check and resize an F2FS filesystem | `41_system` |
| `fuse` | 3.18.3 | The Filesystem in Userspace library (libfuse 3) and the setuid fusermount3 mount helper | `41_system` |
| `gocryptfs` | 2.6.1 | An encrypted directory that syncs as ordinary files | `41_system` |
| `hfsprogs` | 540.1.3 | Apple HFS+ filesystem tools — mkfs.hfsplus and fsck.hfsplus, from Apple's diskdev_cmds | `41_system` |
| `jfsutils` | 1.1.15 | JFS filesystem utilities — mkfs.jfs, fsck.jfs, jfs_tune and the debugger | `41_system` |
| `libnfs` | 8.0.0 | Userspace NFS v3/v4 client library — Kodi's nfs:// sources, and the nfs-ls, nfs-cp and nfs-cat tools | `41_system` |
| `mtools` | 4.0.49 | Collection of utilities for DOS disks in Unix | `41_system` |
| `nfs-utils` | 3.1.1 | NFS client and server utilities | `41_system` |
| `nilfs-utils` | 2.3.1 | NILFS2 filesystem utilities — mkfs.nilfs2, the cleaner daemon, nilfs-resize and snapshot tools | `41_system` |
| `ntfs-3g` | 2026.9.18 | NTFS driver and the ntfsprogs filesystem utilities | `41_system` |
| `squashfs-tools` | 4.7.5 | Tools to create and extract squashfs filesystems | `41_system` |
| `sshfs` | 3.7.6 | Mount a remote directory over SSH with FUSE | `41_system` |
| `udftools` | 2.3 | UDF filesystem tools — mkudffs, udffsck, udflabel, wrudf and the packet-writing helpers | `41_system` |
| `xfsprogs` | 7.2.0 | XFS filesystem utilities | `41_system` |

Notes:

- `f2fs-tools`: the libuuid probe's cache answer is preset. Without libuuid `uuid_generate()`
  compiles to nothing and every `mkfs.f2fs` writes the same all-zero UUID; the preset makes a
  missing library a link failure.
- `fuse`: the `/dev/fuse` node the install creates is removed from the package, since the build
  chroot's `/dev` is the container's. `fusermount` links to `fusermount3`, the name restic's FUSE
  client runs.
- `gocryptfs`: built with cgo against OpenSSL for AES-GCM, unlike every other Go port, so it links
  `libcrypto`; `-tags without_openssl` gives a slower pure-Go binary. The binaries are built into
  `out/`: `gocryptfs-xray` is also a package directory, and `-o gocryptfs-xray` writes inside it.
- `hfsprogs`: built with clang: `fsck_hfs` is written with C blocks (`-fblocks`), which gcc does
  not compile.
- `nfs-utils`: `nfsv4.exportd` links only with optimisation on: `exportd.c` defines a C99 `inline`
  function with no external definition, which is an undefined reference at `-O0`.

### optical

CD, DVD and Blu-ray burning, mastering and CD reading or ripping. 13 ports, under
`ports/core/optical/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `brasero` | 3.12.3 | GNOME disc burner and copier — data, audio and video CDs and DVDs | `43_toolkits` |
| `cdrdao` | 1.2.6 | Disc-at-once CD writer — audio CDs, CD-TEXT and exact copies | `41_system` |
| `dvd+rw-tools` | 7.1 | growisofs and friends — DVD and Blu-ray writing, formatting and media inspection | `41_system` |
| `k3b` | 26.08.1 | KDE disc burner — data, audio and video CDs, DVDs and Blu-ray, copies and rips | `44_apps` |
| `libburn` | 1.5.8 | Library for writing data to optical media | `41_system` |
| `libcdio` | 2.4.0 | CD-ROM and ISO 9660 access — audio CDs for mpv, mpd and GStreamer | `41_system` |
| `libcdio-paranoia` | 10.2+2.0.2 | Audio CD reading that verifies and repairs — cd-paranoia, and CD audio in mpv and mpd | `41_system` |
| `libcue` | 2.3.0 | CUE sheet parser library | `41_system` |
| `libdiscid` | 0.7.0 | MusicBrainz disc ID library — reads an audio CD's table of contents | `41_system` |
| `libisoburn` | 1.5.8.pl02 | Frontend for libburn and libisofs which enables creation and expansion of ISO-9660 filesystems | `41_system` |
| `libisofs` | 1.5.8.pl02 | Library for creating and manipulating ISO 9660 filesystem images | `41_system` |
| `libkcddb` | 26.08.1 | KDE CDDB client library — audio CD track names for K3b | `43_toolkits` |
| `xfburn` | 0.8.0 | GTK 3 disc burner over libburn — data and audio CDs, DVDs and ISO images | `44_apps` |

Notes:

- `brasero`: its desktop entry starts it with `GDK_BACKEND=x11`, because its dialogs and video
  preview take X11 window ids.
- `k3b`: writing runs `cdrskin`, `growisofs`, `cdrdao` and `mkisofs` from `$PATH`, so libburn,
  dvd+rw-tools, cdrdao and libisoburn stay in `depends` although the build links none of them.
- `libcdio-paranoia`: `-std=gnu17` stays while the bundled `getopt.h` declares `int getopt ()`,
  which C23 reads as a function taking no arguments.
- `libisoburn`: `mkisofs` and `genisoimage` are links to `xorriso`, which K3b and growisofs run to
  build a data image. readline, acl and zlib are probes that fall back silently, so `depends` is
  what keeps line editing, ACLs and zisofs.
- `libisofs`: each `--enable` only permits its probe, and a missing library turns the feature off
  without an error, so acl and zlib in `depends` are what keep ACLs and zisofs in the image.

### crypto

Cryptographic libraries, PKCS#11, smartcards and TPM. 31 ports, under `ports/core/crypto/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `argon2` | 20190702 | Argon2 password hashing library and command-line tool | `41_system` |
| `botan` | 3.13.0 | C++ cryptography and TLS library, version 3 | `41_system` |
| `ccid` | 1.8.4 | USB CCID and ICCD smart card reader driver for pcsc-lite | `41_system` |
| `gnutls` | 3.8.13 | Libraries and userspace tools which provide a secure layer over a reliable transport layer | `41_system` |
| `gpgme` | 2.2.0 | C library that allows cryptography support to be added to a program | `43_toolkits` |
| `gpgmepp` | 2.2.0 | The C++ binding to GnuPG Made Easy | `43_toolkits` |
| `libassuan` | 3.0.2 | Inter process communication library used by some of the other GnuPG related packages | `41_system` |
| `libcbor` | 0.14.0 | CBOR parser — libfido2's wire format for talking to a security key | `41_system` |
| `libfido2` | 1.17.0 | FIDO2/U2F over USB and NFC — what an ed25519-sk SSH key talks to | `41_system` |
| `libgcrypt` | 1.12.4 | A general purpose crypto library based on the code used in GnuPG | `41_system` |
| `libgpg-error` | 1.61 | Library that defines common error values for all GnuPG components | `41_system` |
| `libksba` | 1.8.1 | Library for X.509 certificates and CMS (Cryptographic Message Syntax) | `41_system` |
| `libsodium` | 1.0.22 | Cryptography library with NaCl's API, portable | `41_system` |
| `libtasn1` | 4.21.0 | Portable C library that encodes and decodes DER/BER data following an ASN.1 schema | `41_system` |
| `libtpms` | 0.10.2 | A software TPM 1.2 and 2.0 as a library — the TPM inside swtpm | `41_system` |
| `mbedtls` | 3.6.7 | Mbed TLS: a small TLS and cryptography library, the 3.6 long-term line | `41_system` |
| `nettle` | 4.0 | Low-level cryptographic library | `41_system` |
| `npth` | 1.8 | Portable POSIX/ANSI-C based library for Unix platforms which provides non-preemptive priority-based scheduling for multiple threads of execution (multithreading) inside event-driven applications | `41_system` |
| `nspr` | 4.40 | Netscape Portable Runtime — the platform layer under NSS | `41_system` |
| `nss` | 3.130 | Mozilla Network Security Services — TLS, PKCS #11 and certificate libraries | `41_system` |
| `opensc` | 0.27.1 | The PKCS#11 module for PIV, CAC, OpenPGP and national eID smart cards, and the tools to manage them | `41_system` |
| `openssl` | 4.0.3 | Management tools and libraries relating to cryptography | `30_foundation` |
| `openssl3` | 3.5.9 | OpenSSL 3 runtime libraries for programs built against libssl.so.3 | `30_foundation` |
| `p11-kit` | 0.26.5 | Provides a way to load and enumerate PKCS #11 (a Cryptographic Token Interface Standard) modules | `41_system` |
| `pcsc-lite` | 2.5.2 | PC/SC smart card middleware — the pcscd daemon and libpcsclite | `41_system` |
| `qca` | 2.3.12 | Qt Cryptographic Architecture for Qt 6, with the OpenSSL, GnuPG and SASL providers | `43_toolkits` |
| `qca-qt5` | 2.3.12 | Qt Cryptographic Architecture for Qt 5, with the OpenSSL, GnuPG and SASL providers | `43_toolkits` |
| `qgpgme` | 2.2.0 | The Qt 6 binding to GnuPG Made Easy | `43_toolkits` |
| `swtpm` | 0.10.2 | A software TPM for virtual machines — qemu's -tpmdev emulator backend | `41_system` |
| `tpm2-tools` | 5.8 | The tpm2_* commands — seal a LUKS key to the chip and unseal it at boot | `41_system` |
| `tpm2-tss` | 4.2.0 | The TPM2 software stack — what talks to the chip that can hold a disk key | `41_system` |

Notes:

- `ccid`: its udev rule is installed by hand into `/usr/lib/udev/rules.d`; meson would take
  `/lib/udev` from eudev's `udev.pc`.
- `gpgme`: `--disable-linux-getdents` is required on musl: that path declares its `dirent64` with
  `ino64_t` and `off64_t`, which musl exposes only under `_LARGEFILE64_SOURCE`.
- `libfido2`: upstream's `udev/70-u2f.rules` is not installed. It grants a `plugdev` group KDOS
  does not have and sorts after `fs/etc/udev/rules.d/70-kdos-fido.rules`, so it would take back
  that file's grant. `MANDOC_PATH` is pinned `OFF`, not to a `-NOTFOUND` value, which
  `find_program` searches again; with mandoc found, the package gains HTML pages and follows build
  order.
- `libsodium`: `--enable-opt` stays off. It compiles for the build machine's CPU, and the library
  picks its AVX2 and AES-NI paths at run time.
- `mbedtls`: the port follows the 3.6 long-term line, because OBS Studio and RetroArch are written
  to the 3.x API. Threading is turned on by `threading-pthread.patch` in the installed
  `mbedtls_config.h`, never by a define: `MBEDTLS_THREADING_C` changes the size of the RNG and SSL
  contexts, so a consumer built without it would disagree with the library.
- `nspr`: `_linux.h` picks libc features by testing `__GLIBC__`, so the `-D_PR_*` flags name poll,
  `off64_t`, IPv6 and `getaddrinfo` for musl; without them the library falls back to `select()`
  and IPv4-only name lookup.
- `nss`: the `.chk` files are installed with the libraries, because FIPS mode refuses to load a
  library whose self-test signature is missing.
- `opensc`: `openssl4.patch` is upstream's commit `8ad96adc5f`, which reads `ASN1_STRING` through
  OpenSSL 4's accessors; a release that contains it builds without the patch. The docbook-xsl
  directory is named by path, or configure finds no stylesheets and builds no manual pages.
- `p11-kit`: `trust_paths` is gnutls's only source of anchors, because gnutls is built
  `--with-default-trust-store-pkcs11`. A path the image does not ship leaves every gnutls consumer
  rejecting every peer.
- `pcsc-lite`: with polkit on, pcscd refuses a client polkit does not grant
  `org.debian.pcsc-lite.access_pcsc` and `access_card`.
  `fs/etc/polkit-1/rules.d/50-kdos.rules` grants both to `wheel`, so a user outside it reaches no
  reader.
- `tpm2-tss`: its udev rules give `/dev/tpmrm0` to the `tss` group, whose account is
  `fs/etc/passwd`'s; without the rules every call fails as a permission error. There is no
  `tpm2-abrmd`: the kernel's resource manager at `/dev/tpmrm0` serialises access.

### security

Security applications: GnuPG and front-ends, password and secret managers, certificates, scanners,
forensics and audit. 26 ports, under `ports/core/security/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `afflib` | 3.7.22 | Advanced Forensic Format (AFF) image library, the aff tools and affuse | `41_system` |
| `age` | 1.3.2 | File encryption tool | `41_system` |
| `aircrack-ng` | 1.7 | 802.11 Wi-Fi auditing and diagnosis tools | `42_graphics` |
| `clamav` | 1.5.4 | Anti-virus toolkit — clamscan, the clamd daemon, sigtool and the libclamav scanning engine | `41_system` |
| `gnupg` | 2.5.24 | GNU Privacy Guard — OpenPGP encryption and signing | `43_toolkits` |
| `john` | 1.9.0.jumbo1 | John the Ripper password hash cracker | `41_system` |
| `keepassxc` | 2.7.12 | Password manager — KeePass databases, TOTP, SSH agent, browser integration and the Secret Service | `44_apps` |
| `kleopatra` | 26.08.1 | Certificate manager and graphical front end to GnuPG — OpenPGP and S/MIME keys, signing and encryption | `44_apps` |
| `libbfio` | 20260623 | libyal basic file input/output abstraction library — the I/O layer of the libyal image and volume readers | `41_system` |
| `libewf` | 20240506 | Expert Witness Format (E01, Ex01) forensic image library, the ewf tools and ewfmount | `41_system` |
| `libkleo` | 26.08.1 | KDE PIM library for key management and cryptography widgets over GnuPG | `43_toolkits` |
| `libsecret` | 0.21.8.2 | Secret Service client library — stores and looks up passwords over D-Bus | `41_system` |
| `libvhdi` | 20260901 | Virtual Hard Disk (VHD, VHDX) image library, vhdiinfo and vhdimount | `41_system` |
| `libvmdk` | 20260714 | VMware Virtual Disk (VMDK) image library, vmdkinfo and vmdkmount | `41_system` |
| `libvslvm` | 20260901 | Linux LVM2 volume-system library, vslvminfo and vslvmmount — reads logical volumes inside an image | `41_system` |
| `lynis` | 3.1.7 | Host security auditing tool | `41_system` |
| `nmap` | 7.991 | Network exploration tool and security scanner | `41_system` |
| `pass` | 1.7.4 | The git-versioned, gpg-encrypted password store | `43_toolkits` |
| `pass-otp` | 1.2.0 | A one-time password in the password store — pass otp | `44_apps` |
| `pinentry` | 1.3.3 | Collection of simple PIN or pass-phrase entry dialogs which utilize the Assuan protocol | `43_toolkits` |
| `qtkeychain` | 0.17.0 | Qt 6 API for storing passwords in the Secret Service, KWallet or a plain file | `43_toolkits` |
| `sleuthkit` | 4.15.0 | Filesystem forensics tools that read disk images directly | `41_system` |
| `step-ca` | 0.30.2 | Private certificate authority with ACME | `41_system` |
| `step-cli` | 0.30.6 | The step command — creates a CA for step-ca, and requests, renews and inspects certificates | `41_system` |
| `yara` | 4.5.8 | Rule-based pattern matching for classifying files | `41_system` |
| `zenmap` | 7.991 | The GTK front end to nmap: scan profiles, saved results, topology map and scan comparison | `44_apps` |

Notes:

- `aircrack-ng`: `iw`, `pciutils` and `usbutils` are in `depends` because `airmon-ng` runs `iw`,
  `lspci` and `lsusb`; without them every capture fails as if the card had no monitor mode.
  `--enable-hwloc` only permits the probe, so hwloc is in `depends` too.
- `clamav`: bytecode signatures run in the interpreter: ClamAV's LLVM runtime accepts only LLVM 8 to
  13.
- `john`: one build per SIMD level, chained at run time by `-DCPU_FALLBACK`: `john` (AVX-512BW),
  `john-non-avx512bw`, `john-non-avx2`, `john-non-xop`, `john-non-avx` (SSE2). The level never goes
  into `CFLAGS`, or `john.c`, which tests the processor, is compiled for it and dies of SIGILL; a
  new level keeps the names `x86-64.h` expects; the baseline build runs last, because its helper
  programs are the ones shipped. `--disable-native-tests` keeps the build machine's instruction set
  out of the binary.
- `libewf`: libyal's `--with-<lib>` falls back to detection, so `config.h` is checked afterwards for
  fuse 3, zlib and `uuid_generate_random`. `--with-libuuid` names a directory, because the
  pkg-config branch of this release's `libuuid.m4` links nothing.
- `libsecret`: `-include fcntl.h` is required on musl: `secret-file-collection.c` takes `O_CREAT`
  and `O_RDWR` from `<sys/file.h>`, which pulls in `<fcntl.h>` only on glibc.
- `nmap`: `--with-libpcap`, `--with-libssh2`, `--with-libz` and `--with-libpcre` fall back to the
  bundled copies when the system library is missing, so `depends` holds them to the system ones; the
  directory spelling puts `-I/usr/include` ahead of libstdc++'s headers and breaks their
  `#include_next`. `script.db` is regenerated after `jdwp-exec` and `jdwp-info` are removed, or
  `--script default` names a script that is not there.
- `pass`: `tree` is a run-time dependency: `pass` with no argument lists the store with it.
- `pass-otp`: oath-toolkit is in `depends` because `otp.bash` finds `oathtool` on `PATH` at run
  time.
- `sleuthkit`: sleuthkit calls `libewf_handle_read_random`, which the libewf port does not export; a
  `CPPFLAGS` define maps it to `libewf_handle_read_buffer_at_offset`, which takes the same
  arguments.
- `step-ca`: cgo stays on: the PKCS#11 and YubiKey PIV key stores compile out silently without it,
  and a `kms` block naming either then fails only when the CA starts.
- `step-cli`: `BuildTime` stays unset, or every build is a different binary.

### containers

Container runtimes, image tools, container networking and sandbox primitives. 18 ports, under
`ports/core/containers/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `aardvark-dns` | 2.1.0 | Authoritative DNS server for container networks (Rust) | `41_system` |
| `bubblewrap` | 0.13.0 | Unprivileged sandboxing tool (bwrap), the sandbox xdg-desktop-portal decodes untrusted icons and sounds in | `41_system` |
| `buildah` | 1.45.1 | Build OCI / Docker container images | `44_apps` |
| `catatonit` | 0.2.1 | Static container init, for podman pods and --init | `41_system` |
| `conmon` | 2.2.1 | OCI container runtime monitor (used by Podman) | `41_system` |
| `containers-common` | 1 | /etc/containers — the engine, storage, registry and image-trust configuration podman, buildah and skopeo share | `41_system` |
| `crun` | 1.29.1 | OCI container runtime written in C, with a small memory footprint | `41_system` |
| `distrobox` | 1.8.2.5 | Wrapper around podman or docker that creates mutable development containers | `43_toolkits` |
| `fuse-overlayfs` | 1.18 | Implementation of overlay+shiftfs in FUSE for rootless containers | `41_system` |
| `lazydocker` | 0.25.2 | Terminal UI for containers, images and logs | `44_apps` |
| `libseccomp` | 2.6.1 | High-level userspace seccomp interface | `30_foundation` |
| `netavark` | 2.1.0 | Container network stack (network backend for Podman, Rust) | `41_system` |
| `passt` | 2026_07_28.f8df3f1 | User-mode networking for VMs and containers, unprivileged and without a tap device | `41_system` |
| `podman` | 6.1.2 | Daemonless container engine for OCI containers | `43_toolkits` |
| `podman-tui` | 2.0.0 | Terminal UI for podman | `44_apps` |
| `skopeo` | 1.24.1 | Inspect, copy, and sign container images and image registries | `44_apps` |
| `slirp4netns` | 1.3.5 | User-mode networking for unprivileged network namespaces | `41_system` |
| `xdg-dbus-proxy` | 0.1.9 | Filtering D-Bus proxy — the bus a sandboxed WebKitGTK web process is given | `41_system` |

Notes:

- `catatonit`: upstream links it fully static, and it must stay so: podman bind-mounts it into
  containers whose root has no musl. It installs into `/usr/lib/podman`, one of podman's helper
  directories, never `/usr/bin`.
- `passt`: `VERSION` is exported because the Makefile reads it from `git describe`; without it
  every binary reports `unknown version` to a caller checking for a new enough pasta. The default
  target also builds `passt.avx2` and `pasta.avx2`, which an AVX2 machine re-execs into at every
  start, so they always ship.
- `podman`, `buildah`, `skopeo`: `BUILDTAGS` replaces each Makefile's probed tag list, so it is
  the whole set, whatever the build root holds.
- `podman`: `kdos-podman-api` starts `podman system service` with the lock descriptor closed; a
  service that inherited it would block every later launch until it exited.
- `podman-tui`: the `containers_image_openpgp` and `exclude_graphdriver_btrfs` tags are what let
  it build with `CGO_ENABLED=0`.

### virt

Virtual machines, their tooling, and remote-desktop and VNC protocols. 25 ports, under
`ports/core/virt/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `aml` | 1.0.0 | Event loop library for main-loop driven Wayland services | `41_system` |
| `freerdp` | 3.32.0 | Remote Desktop Protocol library and the SDL3 client sdl-freerdp; the RDP engine of Remmina | `42_graphics` |
| `gtk-vnc` | 1.5.0 | VNC client library and GTK 3 viewer widget, with its PulseAudio bridge — virt-manager's VNC console | `43_toolkits` |
| `libcacard` | 2.8.2 | Virtual CAC smartcard emulation — the card a SPICE or QEMU guest sees, backed by NSS or a real reader | `41_system` |
| `libnbd` | 1.24.3 | NBD client library and the nbdsh, nbdcopy, nbdinfo, nbddump and nbdfuse tools | `41_system` |
| `libosinfo` | 1.12.0 | Library of operating-system facts for virtualisation — installer media, devices, requirements | `41_system` |
| `libslirp` | 4.9.5 | User-mode networking library (TCP/IP emulator) used by slirp4netns and QEMU | `41_system` |
| `libvirt` | 12.7.0 | Virtualisation API and the libvirtd daemon, driving QEMU/KVM guests, networks and storage; virsh | `43_toolkits` |
| `libvirt-glib` | 5.0.0 | GLib, GObject and GConfig wrappers of the libvirt API, with introspection data and Vala bindings | `43_toolkits` |
| `libvncserver` | 0.9.15 | VNC server and client libraries | `41_system` |
| `nbdkit` | 1.48.1 | NBD server toolkit with pluggable plugins and filters; serves disks to libvirt and QEMU | `41_system` |
| `neatvnc` | 1.0.1 | VNC server library | `42_graphics` |
| `osinfo-db` | 20260812 | The osinfo database: every operating system, installer and virtual device libosinfo knows | `41_system` |
| `osinfo-db-tools` | 1.12.0 | Tools that import, export, validate and locate the osinfo operating-system database | `41_system` |
| `phodav` | 3.0 | WebDAV server library on libsoup, with chezdav — the folder sharing behind spice-gtk's shared directory | `41_system` |
| `qemu` | 11.1.1 | Machine emulator and virtualiser | `43_toolkits` |
| `remmina` | 1.4.43 | Remote desktop client (GTK 3) for RDP, VNC, SPICE and SSH | `44_apps` |
| `spice` | 0.16.0 | SPICE server library — what QEMU links to offer a guest's display, sound and USB to a SPICE client | `42_graphics` |
| `spice-gtk` | 0.43 | SPICE client library and GTK 3 widget — the VM console of virt-manager — with spicy and the USB redirection | `43_toolkits` |
| `spice-protocol` | 0.14.5 | SPICE protocol headers — the message definitions spice-gtk and QEMU's SPICE display share | `41_system` |
| `usbredir` | 0.15.0 | USB-over-network redirection protocol libraries and usbredirect, for SPICE and QEMU | `41_system` |
| `virt-manager` | 5.1.0 | Creates, runs and connects to libvirt/QEMU virtual machines, with virt-install, virt-clone and virt-xml | `44_apps` |
| `virtiofsd` | 1.14.0 | The virtio-fs daemon — shares a host directory with a qemu guest | `41_system` |
| `wayvnc` | 0.10.1 | VNC server for wlroots compositors, serving the session over the network | `42_graphics` |
| `wlvncc` | 0.1.0.20260429 | Wayland-native VNC client drawing through EGL/GLES2, with H.264 decoding by FFmpeg | `42_graphics` |

Notes:

- `aml`: the header is `<aml1/aml.h>`, the pkg-config file `aml1.pc`, and the soname follows the
  library version. neatvnc and wayvnc ask for `aml1` by those names, so a build that flattens them
  leaves both unbuildable.
- `freerdp`: the client is `sdl-freerdp`, on SDL3. `CMAKE_DISABLE_FIND_PACKAGE_X11` keeps its
  optional X11 helpers from linking libX11 because it is installed. The smartcard channel loads
  libpcsclite at run time, so pcsc-lite in `depends` is what makes reader redirection work.
- `gtk-vnc`, `spice-gtk`: the coroutine backend is `gthread`, because musl has no ucontext
  functions.
- `libosinfo`: hwdata's `pci.ids` and `usb.ids` are named; given no path, setup downloads them.
- `libvirt`: clients connect to `libvirtd`'s socket (`-Dremote_default_mode=legacy`). The default
  connects to the modular daemons, which nothing starts, and every `qemu:///system` connection would
  fail. `-Ddocs=enabled` also generates `libvirt-api.xml`, without which python3-libvirt cannot
  build. `virtlogd` starts before `libvirtd`, which refuses a system-mode guest without its socket.
  `/etc/nftables.d/40-libvirt.nft` opens forwarding, DHCP and DNS on the `virbr*` bridges only;
  without it a guest gets no lease and reaches nothing.
- `nbdkit`: the libvirt plugin stays off, because libvirt's nbdkit backend runs this server and
  linking both ways is a dependency cycle. The torrent plugin stays off, so libtorrent-rasterbar is
  not put under libvirt.
- `neatvnc`: `tls` and `nettle` are `enabled`: an `auto` feature that misses its library turns
  itself off, and wayvnc then offers cleartext RFB only. `gbm` is `enabled` because `neatvnc.h`
  declares the gbm functions unconditionally, and a gbm-less library fails its consumers at link
  time.
- `qemu`: three targets, x86_64, aarch64 and riscv64. The guest firmware is built from `roms/`
  (`--disable-install-blobs`), except the riscv64 EDK2 image, which is upstream's (see
  [firmware and code for another
  processor](../01-philosophy/why-kdos.md#firmware-and-code-for-another-processor)). The edk2 step
  runs without `MAKEFLAGS`: an inherited `-j` races a module's `.efi` against the firmware volume
  that packs it. `qemu-bridge-helper` is installed without its setuid bit and serves only root.
- `remmina`: `WITH_AVAHI=OFF` is the switch that turns discovery off, because Remmina searches for
  every suggested package as `REQUIRED`. FreeRDP's three packages are made required, or the RDP
  plugin is dropped silently. `-lintl` is linked because musl leaves gettext to libintl.
- `spice-gtk`: `-Dpolkit=disabled` builds no setuid USB ACL helper, so USB redirection reaches only
  the device nodes the user can already open.
- `virtiofsd`: libvirt finds it only through the vhost-user descriptor in
  `/usr/share/qemu/vhost-user`, which names the installed binary; without one libvirt refuses a
  guest with a virtiofs share.
- `wayvnc`: the package owns the PAM service file `wayvnc`; with none, every login is refused. The
  stack names `pam_unix` with `nodelay` and never includes `system-auth`, whose `nullok` would
  admit a passwordless account from the network. pam_unix's fail delay would sleep in wayvnc's one
  event loop and stall every other client.
- `wlvncc`: every optional library is probed with `required: false` and dropped silently when
  absent, so each is in `depends`.

### net-libs

Networking and protocol libraries. 38 ports, under `ports/core/net-libs/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `asio` | 1.38.2 | Standalone, header-only C++ networking and asynchronous I/O library, without Boost | `41_system` |
| `avahi` | 0.9rc5 | mDNS/DNS-SD stack — what makes network printers discoverable | `41_system` |
| `c-ares` | 1.34.8 | C library for asynchronous DNS requests | `30_foundation` |
| `cppzmq` | 4.11.0 | Header-only C++ binding for ZeroMQ — Horizon EDA's pool updater and GNU Radio's gr-zeromq blocks | `41_system` |
| `curl` | 8.23.0 | Utility and a library used for transferring files | `30_foundation` |
| `glib-networking` | 2.90.0 | GIO modules for TLS and proxy resolution (the TLS backend every GIO and libsoup https connection needs) | `41_system` |
| `libevent` | 2.1.13 | Asynchronous event notification software library | `41_system` |
| `libidn` | 1.44 | Implementation of the Stringprep, Punycode and IDNA specifications | `41_system` |
| `libidn2` | 2.3.8 | Free software implementation of IDNA2008, Punycode and TR46 | `30_foundation` |
| `libmaxminddb` | 1.14.1 | MaxMind DB (GeoIP2, GeoLite2) reader library and mmdblookup | `41_system` |
| `libmicrohttpd` | 1.0.10 | Embeddable HTTP server — what kiwix-serve listens with | `41_system` |
| `libmnl` | 1.0.5 | Minimal user-space library for Netlink | `41_system` |
| `libndp` | 1.9 | IPv6 Neighbor Discovery Protocol library (NetworkManager dep) | `41_system` |
| `libnice` | 0.1.24 | ICE — how two WebRTC peers find a path to each other through NAT | `42_graphics` |
| `libnl` | 3.12.0 | Netlink protocol library (kernel <-> userspace networking) | `41_system` |
| `libpcap` | 1.11.0 | A system-independent interface for user-level packet capture | `41_system` |
| `libpsl` | 0.23.3 | Library for accessing and resolving information from the Public Suffix List | `30_foundation` |
| `librist` | 0.2.20 | Reliable Internet Stream Transport — the RIST protocol library, with ristsender and ristreceiver | `41_system` |
| `libsmi` | 0.5.0 | SNMP MIB and SMI module library — smilint, smidump, smiquery and the IETF and IANA MIBs | `41_system` |
| `libsoup3` | 3.8.0 | GNOME HTTP client/server library, the 3.x API (GStreamer's souphttpsrc and adaptivedemux2, GeoClue) | `41_system` |
| `libsrt` | 1.5.7 | Secure Reliable Transport — low-latency encrypted live video over lossy networks, with srt-live-transmit | `41_system` |
| `libsrtp` | 2.8.1 | Secure RTP library — the encrypted media transport of WebRTC calls | `41_system` |
| `libssh` | 0.12.2 | SSH protocol library (client and server) | `41_system` |
| `libssh2` | 1.11.1 | Client-side C library implementing the SSH2 protocol | `30_foundation` |
| `libtirpc` | 1.3.8 | Libraries that support programs that use the Remote Procedure Call (RPC) API | `41_system` |
| `libuv` | 1.52.1 | Multi-platform support library with a focus on asynchronous I/O | `30_foundation` |
| `libwebsockets` | 4.5.8 | C library for WebSocket and HTTP/1 and HTTP/2 clients and servers | `41_system` |
| `miniupnpc` | 2.3.3 | A UPnP IGD client, for opening a port on the home router | `41_system` |
| `neon` | 0.37.1 | HTTP and WebDAV client library with TLS, used by NUT's netxml-ups driver and Audacious | `41_system` |
| `nghttp2` | 1.70.0 | Hypertext Transfer Protocol version 2 implementation | `30_foundation` |
| `nghttp3` | 1.18.0 | HTTP/3 and QPACK library | `30_foundation` |
| `ngtcp2` | 1.25.0 | QUIC protocol library, with its OpenSSL crypto helper | `30_foundation` |
| `nng` | 1.12.4 | Nanomsg-next-gen messaging library — brokerless pub/sub, request/reply and pipeline | `41_system` |
| `poco` | 1.15.4 | POCO C++ libraries — foundation, XML, JSON, Zip, networking and TLS classes | `41_system` |
| `rpcsvc-proto` | 1.4.4 | rpcsvc protocol .x files and headers | `41_system` |
| `uriparser` | 1.0.2 | Strict RFC 3986 URI parsing and normalising library, with the uriparse tool | `41_system` |
| `xmlrpc-c` | 1.64.04 | XML-RPC for C and C++ — client over libcurl, the Abyss server, and the xmlrpc command-line client | `41_system` |
| `zeromq` | 4.3.5 | Messaging sockets library — the transport Jupyter kernels talk over | `41_system` |

Notes:

- `asio`: the headers and `asio.pc` are installed through their own targets, because the
  top-level `make install` first builds every example program under `src/`.
- `avahi`: the source is a tag archive with no `configure` and no prebuilt manual pages, so
  `autoreconf` and `xmltoman` run in the build. `avahi-ui-gtk3` cannot be built: GTK 3 depends on
  CUPS, which depends on avahi. `avahi` and `avahi-autoipd` are the accounts `postinstall.sh`
  makes; a daemon configured with any other name refuses to start.
- `glib-networking`: `gnome_proxy` stays disabled: it reads the `org.gnome.system.proxy` schema,
  which nothing installs, and GSettings aborts any process that asks for a missing schema.
- `libmicrohttpd`: `--enable-https` stays, because mosquitto's `http_api` listener asks for TLS
  whenever it has a certificate, and a library without TLS refuses that daemon.
- `libnice`: `-Dgstreamer=enabled` builds the `nicesrc` and `nicesink` elements that
  gst-plugins-bad's `webrtcbin` needs, so the port is built after gstreamer and before
  gst-plugins-bad.
- `libsoup3`: `glib-networking` stays in `depends` although nothing links it: TLS is its GIO module,
  loaded at run time, and without it every https request fails at the handshake.
- `libsrtp`: the ciphers come from OpenSSL because the built-in set has no AES-GCM, which WebRTC
  offers first.

### network

Host network configuration and connection management: links, Wi-Fi, DHCP, DNS forwarding, time,
modems, firewall and VPN. 28 ports, under `ports/core/network/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `babeld` | 1.14 | Babel routing daemon — routes across a mesh of wired and wireless links | `41_system` |
| `batctl` | 2026.3 | Control tool for B.A.T.M.A.N. advanced — the kernel's layer-2 mesh | `41_system` |
| `can-utils` | 2025.01 | candump, cansend, cangen and the rest of the SocketCAN command set | `41_system` |
| `chrony` | 4.9 | NTP client and server (the clock TLS depends on) | `41_system` |
| `dhcpcd` | 10.5.2 | An implementation of the DHCP client specified in RFC2131 | `41_system` |
| `dnsmasq` | 2.93 | DNS forwarder, DHCP server and TFTP server (NetworkManager hotspot mode) | `41_system` |
| `ethtool` | 7.1 | Network interface settings — link state, ring sizes, offloads | `41_system` |
| `hostapd` | 2.12 | Wireless access point daemon | `41_system` |
| `iproute2` | 7.2.0 | Programs for basic and advanced IPV4-based networking | `41_system` |
| `iptables` | 1.8.13 | iptables, ip6tables, ebtables and arptables over the nftables kernel API | `41_system` |
| `iw` | 6.17 | Wireless device configuration over nl80211, including monitor mode | `41_system` |
| `ldns` | 1.9.2 | DNS library, the drill query tool and the ldns DNSSEC tools | `41_system` |
| `libmbim` | 1.34.0 | MBIM mobile broadband modem protocol library, mbimcli and mbim-proxy | `41_system` |
| `libnftnl` | 1.3.2 | Userspace library for low-level interaction with nf_tables (nftables dep) | `41_system` |
| `libqmi` | 1.38.0 | QMI mobile broadband modem protocol library, qmicli, qmi-proxy and qmi-firmware-update | `41_system` |
| `libqrtr-glib` | 1.4.0 | Qualcomm IPC Router (QRTR) protocol library on GLib (used by libqmi and ModemManager) | `41_system` |
| `mobile-broadband-provider-info` | 20251101 | Carrier APN database for mobile broadband (NetworkManager's WWAN connection wizard) | `41_system` |
| `modemmanager` | 1.24.2 | Mobile broadband modem daemon (3G/4G/5G over QMI, MBIM, QRTR and AT), mmcli and libmm-glib | `41_system` |
| `networkmanager` | 1.58.1 | Network connection manager (Wi-Fi, Ethernet, mobile broadband, VPN; libnm for the panel's network surface) | `41_system` |
| `networkmanager-openvpn` | 1.12.5 | NetworkManager VPN plugin for OpenVPN | `41_system` |
| `nftables` | 1.1.7 | Netfilter packet filtering and classification framework, and the nft tool the firewall is written in | `41_system` |
| `openconnect` | 9.21 | Client for AnyConnect, GlobalProtect, Fortinet and Pulse VPNs | `41_system` |
| `openresolv` | 3.17.4 | resolvconf: one writer of /etc/resolv.conf for NetworkManager, dhcpcd and wg-quick | `41_system` |
| `openvpn` | 2.7.8 | SSL/TLS VPN daemon | `41_system` |
| `ppp` | 2.5.4 | Point-to-Point Protocol daemon (PPPoE and serial modems for NetworkManager) | `41_system` |
| `vpnc-script` | 20240208 | Routing and DNS script openconnect runs when a tunnel connects | `41_system` |
| `wireguard-tools` | 1.0.20260223 | WireGuard VPN userspace tools — wg and wg-quick | `41_system` |
| `wpa_supplicant` | 2.12 | IEEE 802.1X / WPA/WPA2/WPA3 supplicant (Wi-Fi authentication backend for NetworkManager) | `41_system` |

Notes:

- `babeld`: the makefile assigns `CFLAGS` itself, so the phase's flags go in through
  `CDEBUGFLAGS`.
- `can-utils`: `-include time.h` goes in `CFLAGS`, not `CPPFLAGS`, which cmake ignores;
  `libj1939.h` uses `struct timespec` without including `<time.h>`.
- `chrony`: `configure` is hand-written and ignores an option it does not know, so a new flag is
  checked against `./configure --help`. Its probes fall back silently, so `depends` is what keeps
  NTS (gnutls, nettle) and chronyc's line editing (libedit).
- `dhcpcd`: `--without-openssl` keeps the bundled SHA-2 and HMAC; with any prefix other than `/`,
  configure would otherwise link the root daemon to libcrypto.
- `iptables`: `iptables`, `ip6tables` and their save and restore tools are linked to
  `xtables-nft-multi`, so they write nftables rules and share the kernel's ruleset with `nft` and
  libvirt. Upstream's install points them at the legacy binary.
- `ldns`: GOST stays off: it needs an OpenSSL engine, which OpenSSL 4 cannot load. No port
  installs the compiled-in trust anchor `/etc/unbound/root.key`, so `drill -S` needs `-k`.
- `networkmanager`: every tool path is named, because an empty one is a build-time
  `find_program()` guess. `suspend_resume=consolekit` is named because `auto` would pick elogind
  once a libelogind exists. The Bluetooth PAN and DUN plugin is compiled only inside the WWAN
  switch, so turning WWAN off also drops phone tethering.
- `openconnect`: `--with-vpnc-script` and `--with-external-browser` name absolute paths; left to
  configure, they become `/etc/vpnc/vpnc-script`, which nothing installs, and whichever `xdg-open`
  the build found. p11-kit, pcsc-lite and nettle have no switch that fails, so `depends` is what
  keeps them.

### net-diag

Network diagnostics, capture, measurement and low-level socket tools. 16 ports, under
`ports/core/net-diag/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `bandwhich` | 0.23.1 | Shows bandwidth use by process and connection | `41_system` |
| `doggo` | 1.4.0 | Command-line DNS client | `41_system` |
| `iftop` | 1.0pre4 | Displays bandwidth usage on a network interface | `41_system` |
| `iperf3` | 3.21 | Network bandwidth measurement tool | `41_system` |
| `iputils` | 20250605 | ping, arping, tracepath and clockdiff — ping runs unprivileged through ICMP sockets | `41_system` |
| `mqttui` | 0.24.0 | Subscribe to an MQTT broker and watch the topic tree, in the terminal | `41_system` |
| `mtr` | 0.96 | Combined ping and traceroute | `41_system` |
| `net-snmp` | 5.9.5.2 | SNMP library, tools and agent — read network UPS cards, switches and printers (snmpget, snmpwalk, snmpd) | `41_system` |
| `netcat` | 1.238 | OpenBSD's netcat, reading and writing data over TCP and UDP connections | `41_system` |
| `sniffnet` | 1.5.1 | Watch network traffic by host, service, program and country, in a GPU-drawn window | `42_graphics` |
| `socat` | 1.8.1.3 | Multipurpose relay between two data channels: TCP, UDP, UNIX sockets, TLS, files and programs | `41_system` |
| `tcpdump` | 4.99.7 | Command-line packet analyser | `41_system` |
| `termshark` | 2.4.0 | Terminal UI for tshark | `44_apps` |
| `trippy` | 0.13.0 | Network diagnostic combining traceroute and ping, with per-hop loss and jitter | `41_system` |
| `whois` | 5.6.6 | WHOIS client for domains, IP blocks and AS numbers, and the mkpasswd password hasher | `41_system` |
| `wireshark` | 4.7.3 | Network protocol analyser — the Qt window, tshark and dumpcap | `43_toolkits` |

Notes:

- `iperf3`: the install passes `LDCONFIG=:`; with `--without-ldconfig` the variable is empty and
  the install hook is a shell syntax error.
- `mtr`: jansson, ncursesw and libcap are probes that fall back to off without an error, so
  `depends` is what keeps JSON output, the TUI and the privilege drop.
- `net-snmp`: configure prompts for its defaults unless every prompted value is on its command
  line. The install runs `-j1`, because its rules race under a parallel make.
- `netcat`: `b64.patch` and `base64.c` supply `b64_ntop`, which musl's libresolv lacks, and
  `SRCS` on the make line compiles `base64.c`. `-DIPTOS_DSCP_VA=0xb0` supplies the constant musl's
  `<netinet/ip.h>` omits.
- `tcpdump`: `--with-user=tcpdump` names the account `postinstall.sh` creates; tcpdump refuses to
  run without it.
- `termshark`: `wireshark` is a run-time dependency: termshark drives `tshark` and dissects nothing
  itself.
- `whois`: the feature macros in `config.h` key off glibc or a BSD, so musl's capabilities are
  asserted in `CPPFLAGS`, which the Makefile appends to; a `DEFS=` on the command line would drop
  `-DHAVE_LIBIDN2`. `HAVE_ICONV` is a make variable, not a define, because it also links
  `simple_recode.o`.
- `wireshark`: `ninja docs` runs before the install: with asciidoctor present, the install rule
  names every manual page in roff and HTML and stops on the first one missing.

### servers

Daemons that serve other machines: remote shell, web, mail, XMPP, IRC, CalDAV/CardDAV, SMB and DLNA.
10 ports, under `ports/core/servers/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `caddy` | 2.11.4 | Web server with built-in TLS certificate management | `41_system` |
| `maddy` | 0.9.5 | A mail server in one program — SMTP, submission and IMAP for a LAN | `41_system` |
| `minidlna` | 1.3.3 | ReadyMedia — a DLNA media server for the televisions and players on a LAN | `42_graphics` |
| `mosh` | 1.4.0 | Mobile shell, a replacement for SSH over unreliable networks | `41_system` |
| `mosquitto` | 2.1.2 | MQTT broker and the mosquitto_pub and mosquitto_sub clients | `41_system` |
| `ngircd` | 28 | An IRC server small enough for one room — chat on a LAN with no internet | `41_system` |
| `openssh` | 10.6p1 | OpenSSH client and server | `41_system` |
| `prosody` | 13.0.6 | XMPP chat server, written in Lua | `41_system` |
| `radicale` | 3.8.1 | A CalDAV and CardDAV server — shared calendars and contacts on a LAN with no internet | `41_system` |
| `samba` | 4.24.7 | SMB/CIFS file and print server and client tools | `41_system` |

Notes:

- `maddy`: cgo is on for the `libsqlite3` tag, which builds go-sqlite3 against the sqlite port
  instead of its vendored copy.
- `minidlna`: configure links libavahi-client whenever avahi is in the build root, so its probe is
  answered `no`; Avahi serves only TiVo support, which is off.
- `mosquitto`: `HTTP_API_DIR` is emptied: its cache default is a literal string an http_api listener
  would `realpath()` and refuse to start on. libmicrohttpd and libedit are found silently and
  dropped when missing, so both are in `depends`.
- `openssh`: configure links libmd whenever it is installed, with no switch, so libmd is in
  `depends`.
- `ngircd`: the `/etc/ngircd.conf` the install writes is removed: the service starts whenever that
  file exists, so shipping it would run a server on every machine.
- `prosody`: the interpreter, include directory and library of `lua54` are all named: `--lua-version
  5.4` alone would compile against the lua port's 5.5 headers.
- `radicale`: its Python closure lives under `/usr/lib/radicale`, because khard vendors vobject into
  site-packages too. vobject is held at 0.9.8: 0.9.9 imports dateutil to read its version, and the
  source-only fetch fails.
- `samba`: `XSLTPROC` carries `--maxdepth 10000`: `smb.conf.5` nests the DocBook templates deeper
  than libxslt's default allows. `vfs_io_uring` is named in `--with-shared-modules`, because samba
  builds it only when it finds liburing.

### transfer

File transfer, synchronisation, backup and BitTorrent. 14 ports, under `ports/core/transfer/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `croc` | 11.5.3 | One-off file transfer by code phrase, kept on this network | `41_system` |
| `deja-dup` | 50.2 | Scheduled, encrypted backups to a disk or folder over restic | `44_apps` |
| `deluge` | 2.2.0 | BitTorrent client with a daemon, a GTK 3 interface, a web UI and a console UI | `44_apps` |
| `filezilla` | 3.71.0 | FTP, FTPS and SFTP client (wxWidgets, GTK 3) with a two-pane site manager | `44_apps` |
| `fzssh` | 1.4.0 | FileZilla's SSH and SFTP client library | `41_system` |
| `libfilezilla` | 0.57.0 | FileZilla's C++ platform library: events, sockets, TLS over GnuTLS, hashing and time | `41_system` |
| `libtorrent-rasterbar` | 2.1.2 | Arvid Norberg's BitTorrent library, with its Python binding — the engine of qBittorrent and Deluge | `41_system` |
| `qbittorrent` | 5.2.3 | BitTorrent client (Qt 6) with a web UI, plus qbittorrent-nox for a machine with no display | `44_apps` |
| `rclone` | 1.75.1 | Syncs files between local disks and remote storage, and verifies copies with rclone check | `41_system` |
| `restic` | 0.19.1 | Deduplicating backup program, a single static binary | `41_system` |
| `rsync` | 3.5.1 | Utilities for synchronizing large file archives over a network | `41_system` |
| `syncthing` | 2.1.5 | Continuous peer-to-peer file synchronisation | `41_system` |
| `syncthingtray` | 2.1.7 | Status, folders, devices and control of a local Syncthing from the panel tray (Qt 6) | `44_apps` |
| `transmission` | 4.1.3 | BitTorrent daemon with a web UI, command-line tools and a Qt 6 client | `44_apps` |

Notes:

- `croc`: `/usr/bin/croc` is a wrapper that adds `--local`, because croc otherwise falls back
  silently to its public relay; `croc-relay` is the plain binary. The `croc_no_tailcat` tag leaves
  out the Tailscale transport and `croc ssh`.
- `deluge`: `chardet` and `ifaddr` stay out of its bundle; `ifaddr`'s site-packages paths belong to
  cura's bundle.
- `fzssh`: `with_server` stays off. The server half is under a proprietary licence.
- `libtorrent-rasterbar`: `-Dgnutls=OFF` keeps OpenSSL, because the GnuTLS build compiles DHT out.
  The Python binding needs boost built with its Python component.
- `restic`: `CGO_ENABLED=0` keeps it one static binary, so a repository restores from any machine.
- `rsync`: `rrsync` is a python3 script, which is why python3 is in `depends`.
- `syncthing`: `kdos-syncthing` seeds `/usr/share/kdos/syncthing-offline.xml`, with local discovery
  on and global announce, relays and NAT traversal off, only where no configuration exists.
- `transmission`: `REBUILD_WEB` stays off. It runs npm against the network, and the release carries
  the web client built.

### mail

Email clients, fetch and send tools, mail libraries, and feed readers. 13 ports, under
`ports/core/mail/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `aerc` | 0.22.0 | A terminal mail client that speaks IMAP, Maildir and notmuch | `44_apps` |
| `gmime` | 3.2.15 | MIME parsing and generation — notmuch reads every message through it | `43_toolkits` |
| `isync` | 1.5.1 | mbsync — synchronises IMAP mailboxes with a local Maildir, XOAUTH2 included | `41_system` |
| `kmbox` | 26.08.1 | KDE PIM library for reading and writing mbox mail folders | `43_toolkits` |
| `libetpan` | 1.10.1 | Mail protocol library — IMAP, SMTP, POP3, NNTP, MIME and mailbox formats | `41_system` |
| `libpst` | 0.6.76 | Outlook PST and OST reader — readpst, lspst and the libpst library | `41_system` |
| `mimetreeparser` | 26.08.1 | KDE PIM library that parses and renders signed and encrypted MIME messages | `43_toolkits` |
| `msmtp` | 1.8.34 | SMTP client — sends a message from standard input to an SMTP server | `41_system` |
| `newsboat` | 2.44 | RSS and Atom feed reader for the terminal | `41_system` |
| `notmuch` | 0.40 | Mail indexer with tag-based search | `43_toolkits` |
| `pizauth` | 1.1.0 | Obtains and refreshes OAuth2 tokens for mail clients | `41_system` |
| `sfeed` | 2.4 | RSS and Atom feeds as a tab-separated file, and the programs that read it | `44_apps` |
| `thunderbird` | 153.5.0 | Mail, calendar, contacts and feeds, offline-first, on GTK 3 for Wayland and X11 | `44_apps` |

Notes:

- `aerc`: the one Go port built with `CGO_ENABLED=1`, because the `notmuch` build tag binds
  libnotmuch through cgo. The `kdos-part` filter is the `KDOS_SH` heredoc in `build.sh`, which
  `testing/preflight.sh` parses as a shell script. The desktop entry carries `%u` and no
  `x-scheme-handler/mailto` type, because `xdg-open` would start aerc without a terminal.
- `gmime`: the tag archive is prepared with `gtkdocize` and `autoreconf`, never `autogen.sh`, which
  looks only for versioned automake names. `gtk-doc` stays in depends for its `GTK_DOC_CHECK` m4
  macro, without which configure fails on a literal macro name.
- `isync`: XOAUTH2 needs `cyrus-sasl` and the `cyrus-sasl-xoauth2` plugin, both in depends.
  `ac_cv_berkdb4=no` keeps `mdconvert` out whatever was built before it.
- `libetpan`: `--enable-iconv`, `--enable-debug` and `--disable-debug` are never passed. The first
  turns iconv off, and the second and third add `-DDEBUG`, whenever they are given at all.
- `newsboat`: `-lintl` goes in `LDFLAGS`. The Makefile adds it only when `uname -s` is not Linux,
  and overwrites `NEWSBOAT_LIBS` and `PODBOAT_LIBS`.
- `notmuch`: configure drops each `--with-*` quietly when its tool is absent and compiles its
  debugger check against `valgrind.h` whenever pkg-config finds it, so `doxygen`,
  `python3-sphinx`, `bash-completion` and `valgrind` in `depends` are what hold the package's
  contents fixed.
- `sfeed`: the desktop entry runs the `sfeed-read` wrapper, because `sfeed_curses` with no file
  arguments waits on standard input.
- `thunderbird`: links with `-rpath /usr/lib/thunderbird`, because musl does not satisfy a bare-name
  `DT_NEEDED` from a library already loaded. `RUSTC_WRAPPER` adds `-C target-feature=-crt-static`
  to every rustc call, build scripts included, where `RUSTFLAGS` reaches only the target crates.

### chat

Instant messaging, IRC, XMPP, Matrix, fediverse and voice clients, with their protocol and E2E
libraries. 16 ports, under `ports/core/chat/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `baresip` | 4.11.0 | SIP softphone for voice calls | `44_apps` |
| `dino` | 0.5.1 | XMPP chat (GTK 4, libadwaita) with OMEMO and OpenPGP encryption, file transfer, and voice and video calls | `44_apps` |
| `iamb` | 0.0.12 | Terminal Matrix client with vim key bindings | `41_system` |
| `ii` | 2.0 | An IRC client that is a directory of FIFOs and text files | `41_system` |
| `konversation` | 26.08.1 | IRC client (Qt 6, KDE Frameworks) with TLS, DCC transfers and Blowfish-encrypted channels | `44_apps` |
| `libomemo-c` | 0.5.1 | The Signal double ratchet with OMEMO's wire format — end-to-end encryption for XMPP clients | `41_system` |
| `libre` | 4.11.0 | The SIP, RTP and STUN protocol stack baresip is built on | `41_system` |
| `libstrophe` | 0.14.0 | The XMPP client library profanity speaks the protocol with | `41_system` |
| `mtxclient` | 0.10.1 | Client library for the Matrix protocol, with olm end-to-end encryption | `41_system` |
| `mumble` | 1.5.939 | Low-latency voice chat — the Mumble client and mumble-server, for group voice on a LAN | `44_apps` |
| `nheko` | 0.12.1 | Matrix chat client (Qt 6) — rooms, end-to-end encryption, voice and video calls | `44_apps` |
| `olm` | 3.2.16 | The olm and megolm end-to-end encryption ratchets of Matrix | `41_system` |
| `profanity` | 0.18.2 | Console XMPP client | `44_apps` |
| `quassel` | 0.14.0 | IRC client (Qt 5) — the monolithic client, the core that stays connected, and the client for it | `44_apps` |
| `toot` | 0.52.1 | The Fediverse from a terminal, as a TUI and as a command | `41_system` |
| `weechat` | 4.10.1 | Extensible IRC client for the terminal, with a relay and scripting | `41_system` |

Notes:

- `baresip`: `default-modules.patch` uncomments in the generated config the modules whose
  libraries are in `depends` (`opus`, `vp8`, `vp9`, `avcodec`, `sdl`); a module uncommented
  without its library is a start-up error. `MODULES` names every module built, and a named module
  whose library is missing is skipped silently, so each library a module links is in `depends`.
- `libre`: its version moves with `baresip`'s. baresip checks the libre ABI it was built against
  at start-up and aborts on a mismatch, which no build or link step catches.
- `mumble`: a Qt 5 program, so it links the `openssl3` slot with headers generated from the
  slot's release, as `qt5-qtbase` does. Its `find_pkg` tries a CMake config package before
  FindOpenSSL, so `CMAKE_IGNORE_PATH` hides OpenSSL 4's `/usr/lib/cmake/OpenSSL` (and the same
  directory reached through `/usr/lib64`, `/lib` and `/lib64`); found, that package wins over the `OPENSSL_*` paths. The build
  fails if `mumble` or `mumble-server` NEEDs anything but `libssl.so.3`.
- `nheko`: linked with `-z stack-size=1048576`: musl gives a thread 128 KiB of stack unless
  `PT_GNU_STACK` asks for more, and nheko's worker threads outgrow it.
- `quassel`: LDAP sign-in is off, because `libldap` links OpenSSL 4 and Quassel's Qt 5 links
  OpenSSL 3; the build fails if a binary or library NEEDs `libldap` or an OpenSSL 4 soname. Channel
  encryption goes through `qca-qt5`'s `ossl` provider on the `openssl3` slot, spell checking
  through `sonnet-qt5`, and the tray icon's menu through `libdbusmenu-qt5`, each required at
  configure time.
- `weechat`: `TCL_LIBRARY` names `libtcl9.0.so`: FindTCL searches no name past `tcl8.7`, and
  weechat turns a library it cannot find into a failed configure.

### browsers

Web, Gemini and text-mode browsers. 10 ports, under `ports/core/browsers/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `chromium` | 155.0.8059.84 | Chromium web browser, on GTK 3 with Wayland and X11 | `44_apps` |
| `falkon` | 26.08.1 | Qt WebEngine web browser from KDE, with KWallet and KIO integration | `44_apps` |
| `firefox-esr` | 153.5.0 | Firefox Extended Support Release web browser, on GTK 3 with Wayland and X11 | `44_apps` |
| `inlyne` | 0.5.3 | GPU-drawn Markdown and HTML viewer with live reload and no browser engine | `44_apps` |
| `lagrange` | 1.21.1 | Gemini, Gopher, Spartan and Finger browser, with a terminal build (clagrange) beside the SDL one | `44_apps` |
| `librewolf` | 158.0.1 | LibreWolf, the privacy-hardened Firefox fork, on GTK 3 with Wayland and X11 | `44_apps` |
| `lynx` | 2.9.3 | Text-based web browser | `41_system` |
| `netsurf` | 3.11 | Web browser with its own layout engine, on GTK 3 | `44_apps` |
| `qutebrowser` | 3.7.0 | Keyboard-driven web browser with vim-like bindings, on Qt WebEngine | `44_apps` |
| `w3m` | 0.5.6 | Text-mode web browser and pager that renders tables | `42_graphics` |

Notes:

- `chromium`: the `esbuild` port stays at the version `node_modules/esbuild` in the tarball names;
  DevTools' bundling refuses an esbuild binary of any other version.
- `chromium`: the phase's `-std=gnu11` is stripped from `CFLAGS`: the unbundle toolchain appends
  the environment's flags last, where it would override the C standard Chromium picks. `gn gen`
  rejects a tab anywhere in `--args`, so the argument list is indented with spaces.
- `firefox-esr`, `librewolf`: the binary, `/usr/lib/<name>`, the desktop entry, the Wayland app_id
  (`MOZ_APP_REMOTINGNAME`) and `StartupWMClass` carry the one name; when they differ the panel
  groups the window under no entry.
- `firefox-esr`, `librewolf`: libxul is linked with a run path into `/usr/lib/<name>`: musl does
  not resolve a bare-name `DT_NEEDED` against an already-loaded library, so without it libxul
  fails to load. `-crt-static` reaches every rustc call through `RUSTC_WRAPPER`, because
  `RUSTFLAGS` covers only the target's crates when mach passes `--target`, and a statically
  linked build script cannot dlopen libclang.
- `w3m`: no `/etc/w3m/config` ships. Any `inline_img_protocol` makes each start ask the terminal
  for its cell size, which kdos-term and the Linux console do not answer, holding every start for
  two seconds. `--with-browser=xdg-open` is a bare name, so `PATH` finds kdos-appbox's resolver
  in `/usr/local/bin` first.

### web-engines

Browser engines and the libraries that make them up. 14 ports, under `ports/core/web-engines/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `libcss` | 0.9.2 | NetSurf CSS parser and selection engine | `41_system` |
| `libdom` | 0.4.2 | NetSurf W3C DOM implementation, with HTML (hubbub) and XML (expat) parser bindings | `41_system` |
| `libhubbub` | 0.3.8 | NetSurf HTML5 parser, conforming to the WHATWG tokenisation and tree-building rules | `41_system` |
| `libnsbmp` | 0.1.7 | NetSurf BMP and ICO decoding library (imv decodes BMP with it) | `41_system` |
| `libnsgif` | 1.0.0 | NetSurf GIF decoding library (libkimg and imv decode GIF with it) | `41_system` |
| `libnslog` | 0.1.3 | NetSurf logging library — categorised logging with a filter language | `41_system` |
| `libnspsl` | 0.1.7 | NetSurf Public Suffix List library — a compiled-in copy of the list for cookie and domain checks | `41_system` |
| `libnsutils` | 0.1.1 | NetSurf utility library — base64, time and unistd helpers shared by the browser and its libraries | `41_system` |
| `libparserutils` | 0.2.5 | NetSurf parser building blocks — input streams, character-set conversion and buffers | `41_system` |
| `libsvgtiny` | 0.1.8 | NetSurf SVG Tiny renderer — parses SVG into a list of path and text shapes | `41_system` |
| `libwapcaplet` | 0.4.3 | NetSurf string internment library — interned, reference-counted strings | `41_system` |
| `nsgenbind` | 0.9 | NetSurf's JavaScript binding generator — turns WebIDL and binding files into Duktape glue C | `41_system` |
| `webkitgtk` | 2.54.0 | WebKitGTK web engine for GTK 3: the webkit2gtk-4.1 and javascriptcoregtk-4.1 APIs over libsoup 3 | `43_toolkits` |
| `webkitgtk6` | 2.54.0 | WebKitGTK web engine for GTK 4: the webkitgtk-6.0 and javascriptcoregtk-6.0 APIs | `43_toolkits` |

Notes:

- `libcss`, `libdom`, `libhubbub`, `libnslog`, `libnspsl`, `libnsutils`, `libparserutils`,
  `libsvgtiny`, `libwapcaplet`: each makefile adds `-Werror` before the environment's `CFLAGS`, so
  `-Wno-error` there keeps a warning a newer compiler adds from failing the build.
- `nsgenbind`: `VARIANT=release` is named, because any other variant adds `-Werror`.
- `webkitgtk`, `webkitgtk6`: `WebKitWebDriver` has one path for both APIs, so only `webkitgtk`
  builds it.

### x11

X11 protocol, client libraries and data, plus Xwayland, the one X server. 43 ports, under
`ports/core/x11/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `libICE` | 1.1.2 | X11 Inter-Client Exchange library | `41_system` |
| `libSM` | 1.2.6 | X11 Session Management library | `41_system` |
| `libX11` | 1.8.13 | Core X11 client-side library | `42_graphics` |
| `libXScrnSaver` | 1.2.5 | X11 MIT-SCREEN-SAVER extension client library | `42_graphics` |
| `libXau` | 1.0.12 | X11 authorisation protocol library | `41_system` |
| `libXcomposite` | 0.4.7 | X11 Composite extension client library | `42_graphics` |
| `libXcursor` | 1.2.3 | X11 cursor management library | `42_graphics` |
| `libXdamage` | 1.1.7 | X11 DAMAGE extension client library | `42_graphics` |
| `libXdmcp` | 1.1.5 | X Display Manager Control Protocol library | `41_system` |
| `libXext` | 1.3.7 | X11 miscellaneous extensions library (SHAPE, MIT-SHM, XSync and others) | `42_graphics` |
| `libXfixes` | 6.0.2 | X11 XFIXES extension client library | `42_graphics` |
| `libXfont2` | 2.0.9 | X font rasterisation library | `41_system` |
| `libXft` | 2.3.9 | X11 FreeType font rendering library over RENDER | `42_graphics` |
| `libXi` | 1.8.3 | X11 Input extension (XInput2) client library | `42_graphics` |
| `libXinerama` | 1.1.6 | X11 Xinerama extension client library | `42_graphics` |
| `libXmu` | 1.3.1 | X11 miscellaneous utility library (libXmu and libXmuu) | `42_graphics` |
| `libXpm` | 3.5.19 | X11 pixmap (XPM) image library | `42_graphics` |
| `libXpresent` | 1.0.2 | X11 Present extension client library | `42_graphics` |
| `libXrandr` | 1.5.5 | X11 RandR extension client library | `42_graphics` |
| `libXrender` | 0.9.12 | X11 RENDER extension client library | `42_graphics` |
| `libXres` | 1.2.3 | X11 X-Resource extension client library — per-client resource and pid queries | `42_graphics` |
| `libXt` | 1.3.1 | X11 Toolkit Intrinsics library | `42_graphics` |
| `libXtst` | 1.2.5 | X11 XTEST and RECORD extension client library | `42_graphics` |
| `libXv` | 1.0.13 | X11 Xvideo extension client library | `42_graphics` |
| `libXxf86vm` | 1.1.7 | X11 XFree86-VidModeExtension client library | `42_graphics` |
| `libfontenc` | 1.1.9 | Font encoding library | `41_system` |
| `libxcb` | 1.17.0 | X protocol C-language binding | `41_system` |
| `libxcvt` | 0.1.3 | Library providing a standalone version of the X server implementation of the VESA CVT standard timing modelines generator | `41_system` |
| `libxkbfile` | 1.2.0 | XKB file handling library | `42_graphics` |
| `libxshmfence` | 1.3.3 | Shared memory fences library | `41_system` |
| `xauth` | 1.1.5 | X authority file utility — the cookie handling ssh X11 forwarding runs | `42_graphics` |
| `xbitmaps` | 1.1.4 | X.Org bitmap files — the X11/bitmaps headers Motif compiles in | `41_system` |
| `xcb-proto` | 1.17.0 | X protocol XML descriptions (build-time) | `41_system` |
| `xcb-util` | 0.4.1 | XCB utility convenience functions | `41_system` |
| `xcb-util-cursor` | 0.1.6 | XCB cursor library (libxcb-cursor) | `41_system` |
| `xcb-util-image` | 0.4.1 | XCB port of Xlib XImage | `41_system` |
| `xcb-util-keysyms` | 0.4.1 | XCB keysym and keycode conversion library (libxcb-keysyms) | `41_system` |
| `xcb-util-renderutil` | 0.3.10 | XCB convenience functions for the Render extension | `41_system` |
| `xcb-util-wm` | 0.4.2 | ICCCM and EWMH helpers for XCB — wlroots' Xwayland xwm needs both | `41_system` |
| `xkbcomp` | 1.5.0 | XKB keymap compiler, invoked by Xwayland at runtime | `42_graphics` |
| `xorgproto` | 2025.1 | X.Org protocol headers | `41_system` |
| `xtrans` | 1.6.0 | X transport library (headers only, build-time) | `41_system` |
| `xwayland` | 24.1.14 | Rootless X server that runs X11 clients on a Wayland compositor | `42_graphics` |

Notes:

- `libICE`, `libXdmcp`: musl has no `arc4random_buf`, so configure links `libbsd-overlay` whenever
  libbsd is installed; libbsd stays in `depends` so the link does not follow build order.
- `libXpm`: `--with-localedir=no` keeps gettext off. configure finds musl's gettext in libc while
  the gettext port's `libintl.h` renames every call, and `cxpm` then fails to link. `sxpm` and
  `cxpm` are built only when libXt and libXext are found.

### wl

Wayland libraries and protocols, wlroots, and Wayland-only desktop utilities. 17 ports, under
`ports/core/wl/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `fcft` | 3.3.3 | Small library for font loading and glyph rasterization (foot/fuzzel dep) | `42_graphics` |
| `fuzzel` | 1.15.0 | Application launcher and dmenu replacement for wlroots compositors | `42_graphics` |
| `grim` | 1.5.0 | Screenshot tool for wlroots-based Wayland compositors | `42_graphics` |
| `libdecor` | 0.2.5 | Client-side window decorations for Wayland clients | `42_graphics` |
| `libdisplay-info` | 0.4.0 | EDID and DisplayID parser used by Wayland compositors | `41_system` |
| `slurp` | 1.5.0 | Region selector for Wayland compositors (pairs with grim for area screenshots) | `42_graphics` |
| `wayland` | 1.26.0 | Core Wayland protocol library (libwayland-server / libwayland-client) | `42_graphics` |
| `wayland-protocols` | 1.49 | Wayland protocol XML definitions (xdg-shell, layer-shell, etc.) | `42_graphics` |
| `wayland-utils` | 1.3.0 | Lists the globals, outputs, seats and formats a compositor offers (wayland-info) | `42_graphics` |
| `waylandpp` | 1.0.1 | Wayland C++ bindings and the wayland-scanner++ protocol generator | `42_graphics` |
| `waypipe` | 0.11.2 | Network proxy for Wayland clients — run a program on another machine and show it here, over SSH | `42_graphics` |
| `wev` | 1.1.0 | Wayland event viewer — print every input event a window receives | `42_graphics` |
| `wl-clipboard` | 2.3.0 | Wayland clipboard utilities (wl-copy, wl-paste) | `42_graphics` |
| `wl-kbptr` | 0.4.1 | Move and click the pointer from the keyboard — labelled screen regions over a layer-shell overlay | `44_apps` |
| `wl-mirror` | 0.18.5 | Mirror a Wayland output into a window, for a projector or a second screen | `42_graphics` |
| `wlroots` | 0.20.2 | Modular Wayland compositor library — the base kdos-comp is built on | `50_desktop` |
| `wtype` | 0.4 | xdotool type for Wayland — types text and presses keys through the virtual-keyboard protocol | `42_graphics` |

Notes:

- `wlroots`: every release breaks API, so a version bump is a `kdos-comp` port as well. The source
  comes from GitLab's `/-/archive/` path; the release download URL is refused. `-Dbackends` and
  `-Drenderers` accept no `headless` or `pixman`: both are always built and chosen at run time
  (`WLR_BACKENDS=headless`, `WLR_RENDERER=pixman`), and naming either fails configure. The recipe
  installs `protocol/*.xml` to `/usr/share/wlroots/protocols/`, which upstream does not;
  `kdos-shell`, `kdos-term`, `kdos-res` and `kdos-lock` generate their wlr protocol headers from
  there.
- `wtype`: `GIT_DIR=/nonexistent` stops meson's `git describe` from stamping an enclosing checkout's
  description into the version string.
- `wl-mirror`: `wl-present` is not installed; it drives wl-mirror through `pipectl`, which no port
  provides.

### gpu

The GPU stack: Mesa, DRM, GL and Vulkan loaders and headers, shader compilers, VA-API, OpenCL. 26
ports, under `ports/core/gpu/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `freeglut` | 3.8.0 | The OpenGL Utility Toolkit (libglut), X11 and GLX, for GLUT programs run under Xwayland | `42_graphics` |
| `glew` | 2.3.1 | The OpenGL Extension Wrangler — libGLEW, GLX mode, with glewinfo and visualinfo | `42_graphics` |
| `glfw` | 3.5.1 | A window, a GL or Vulkan context and input — Wayland first, X11 under Xwayland as the fallback | `42_graphics` |
| `glmark2` | 2023.01 | Benchmark, in wayland, drm and gbm flavours of GL and GLESv2, all on EGL | `42_graphics` |
| `glslang` | 16.6.0 | OpenGL and OpenGL ES shader front end and validator | `41_system` |
| `glu` | 9.0.3 | The OpenGL Utility library — GL/glu.h and libGLU | `42_graphics` |
| `intel-gmmlib` | 22.10.2 | Intel Graphics Memory Management Library for the media driver | `41_system` |
| `intel-media-driver` | 26.3.5 | Intel iHD VA-API driver for Broadwell and newer graphics | `42_graphics` |
| `libdrm` | 2.4.134 | Provides a user space library for accessing the DRM, direct rendering manager, on operating systems that support the ioctl interface | `42_graphics` |
| `libepoxy` | 1.5.10 | Library for handling OpenGL function pointer management | `42_graphics` |
| `libglvnd` | 1.7.0 | GL Vendor-Neutral Dispatch: libGL, libGLX, libOpenGL, libEGL and libGLES | `42_graphics` |
| `libplacebo` | 7.360.1 | The GPU video rendering pipeline mpv is built on — colour, scaling, tone mapping | `42_graphics` |
| `libva` | 2.24.1 | VA-API — hardware video decode and encode | `42_graphics` |
| `libva-intel-driver` | 2.4.1 | Intel i965 VA-API driver for GMA 4500 through Coffee Lake — the one Sandy Bridge, Ivy Bridge and Haswell decode with | `42_graphics` |
| `libva-utils` | 2.24.0 | VA-API utilities — vainfo and the conformance test programs | `42_graphics` |
| `mesa` | 26.2.3 | OpenGL compatible 3D graphics library: EGL and Vulkan for Wayland, GLX for X11 clients under Xwayland | `42_graphics` |
| `mesa-demos` | 9.0.0 | Mesa EGL, GLES2 and Vulkan demos for Wayland — es2gears_wayland, eglinfo, vkgears | `42_graphics` |
| `ocl-icd` | 2.3.5 | OpenCL ICD loader — libOpenCL.so dispatching to the drivers listed in /etc/OpenCL/vendors | `41_system` |
| `opencl-clhpp` | 2026.05.29 | Khronos OpenCL C++ bindings, the opencl.hpp header | `41_system` |
| `opencl-headers` | 2026.05.29 | Khronos OpenCL C API headers | `41_system` |
| `shaderc` | 2026.4 | glslc and libshaderc — GLSL and HLSL to SPIR-V, on the system glslang and SPIRV-Tools | `41_system` |
| `spirv-headers` | 1.202609.0 | SPIR-V headers and the machine-readable grammar the shader tools are built from | `41_system` |
| `spirv-tools` | 2026.4.rc2 | API and commands for processing SPIR-V modules | `41_system` |
| `vulkan-headers` | 1.4.363 | Vulkan API headers, with the registry XML and its generator scripts | `41_system` |
| `vulkan-loader` | 1.4.363 | Khronos Vulkan ICD loader (libvulkan.so.1) — dispatches to Mesa venus/lavapipe ICDs | `42_graphics` |
| `vulkan-tools` | 1.4.363 | Khronos Vulkan tools — vkcube, vkcubepp and vulkaninfo, Wayland and KMS display WSI | `42_graphics` |

Notes:

- `freeglut`: the Wayland backend binds `wl_shell` only, which kdos-comp does not offer, and the
  backend is chosen at compile time, so the X11 backend is built and GLUT programs run under
  Xwayland.
- `glew`: `GLEW_EGL` swaps GLX for EGL under the same soname, so one build serves one of them;
  this is the GLX build consumers are written against. `glewInit()` returns
  `GLEW_ERROR_NO_GLX_DISPLAY` under an EGL context with no GLX display.
- `glfw`: every windowing library (libwayland-*, libxkbcommon, libdecor, the X11 libraries, EGL,
  GL and Vulkan) is dlopened by soname, so each is in `depends` although no link line names it.
- `intel-gmmlib`: `RUN_TEST_SUITE` defaults on and runs the test binary as part of `cmake --build`,
  which fails the build wherever that binary cannot run.
- `intel-media-driver`: the driver installs into its libdir's `dri`, which must be the directory
  libva searches. `ENABLE_KERNELS=OFF` is not a size trim: it switches off every feature that
  needs a prebuilt shader.
- `libplacebo`: `-Dvulkan-registry` hands the generator the `vk.xml` of the vulkan-headers port,
  and the two must be the same release. Its shaders are generated through `python3-jinja2` and
  its GL loader through `python3-glad2`, since the release archive carries their submodules empty.
- `mesa`: libudev and xcb-keysyms have no switch and are linked whenever found, so eudev and
  xcb-util-keysyms are in `depends`.
- `shaderc`: each release names the glslang and SPIRV-Tools it was cut against, so its version
  moves with those ports'. Only the shared library ships: built against the installed archives,
  the static `libshaderc` and `libshaderc_combined` carry neither.
- `vulkan-loader`: every platform an ICD advertises is compiled in; without the XCB and Xlib WSI
  the loader drops those surface extensions and an X11 client under Xwayland cannot create a
  surface.

### graphics-libs

2D rendering, vector and SVG, text layout, colour management and graph drawing libraries. 27 ports,
under `ports/core/graphics-libs/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `babl` | 0.1.128 | Dynamic any-to-any pixel format conversion library, used by GEGL and GIMP | `41_system` |
| `blend2d` | 0.21.2 | 2D vector graphics engine with a JIT-compiled pipeline | `41_system` |
| `cairo` | 1.18.6 | 2D graphics library, with its Xlib and XCB surfaces | `42_graphics` |
| `cairomm` | 1.18.1 | C++ bindings for cairo, the 1.16 API (cairomm-1.16) that gtkmm 4 builds on | `42_graphics` |
| `cairomm1.14` | 1.14.6 | C++ bindings for cairo, the 1.0 API (cairomm-1.0) that gtkmm 3 builds on | `42_graphics` |
| `gdk-pixbuf` | 2.44.8 | Image-loading library used by GTK and many notification daemons | `41_system` |
| `gegl` | 0.4.72 | Graph-based image processing framework, the engine under GIMP | `43_toolkits` |
| `goocanvas` | 3.0.0 | Cairo canvas widget for GTK 3, with introspection data | `44_apps` |
| `graphene` | 1.10.8 | Thin layer of vector and matrix types for 2D and 3D graphics, used by GTK 4 | `41_system` |
| `graphite2` | 1.3.15 | Graphite smart-font engine — the shaper harfbuzz calls for SIL's Graphite fonts, with gr2fonttest | `41_system` |
| `graphviz` | 16.1.0 | Graph layout and rendering — dot, neato, sfdp and the cgraph/gvc libraries | `43_toolkits` |
| `gts` | 0.7.6.121130 | GNU Triangulated Surface library — 3D surface meshes, Delaunay triangulation, simplification and boolean operations | `41_system` |
| `harfbuzz` | 14.6.0 | OpenType text shaping engine | `42_graphics` |
| `kseexpr` | 6.0.0.0 | Krita fork of the SeExpr expression language, the Qt 6 build | `43_toolkits` |
| `lasem` | 0.5.1 | MathML and SVG rendering library on cairo and pango, with itex-to-MathML conversion | `42_graphics` |
| `lcms2` | 2.19.1 | Little CMS colour management engine — applies ICC profiles | `41_system` |
| `lib2geom` | 1.4 | 2D geometry library for C++, the path and curve engine of Inkscape | `42_graphics` |
| `libraqm` | 0.11.0 | Complex text layout — bidi, shaping and script itemisation over harfbuzz | `42_graphics` |
| `librsvg` | 2.63.2 | SVG rendering library (Rust-based; used by imv for SVG support) | `42_graphics` |
| `pango` | 1.58.2 | Text layout and rendering library, with pangoxft | `42_graphics` |
| `pangomm` | 2.58.0 | C++ bindings for Pango, the 2.48 API (pangomm-2.48) that gtkmm 4 builds on | `42_graphics` |
| `pangomm2.46` | 2.46.5 | C++ bindings for Pango, the 1.4 API (pangomm-1.4) that gtkmm 3 builds on | `42_graphics` |
| `pixman` | 0.46.4 | Library that provides low-level pixel manipulation features such as image compositing and trapezoid rasterization | `41_system` |
| `plutosvg` | 0.0.8 | Tiny SVG rendering library in C, with the FreeType hooks that draw OT-SVG colour glyphs | `41_system` |
| `plutovg` | 1.3.3 | Tiny 2D vector graphics library in C | `41_system` |
| `potrace` | 1.16 | Bitmap to vector tracer | `41_system` |
| `resvg` | 0.48.1 | SVG rendering library and SVG-to-PNG converter | `41_system` |

Notes:

- `cairo`: `xlib`, `xcb` and `xlib-xcb` install `cairo-xlib.pc` and `cairo-xcb.pc`; GTK 3 and 4
  stop at setup without them when their X11 backend is built.
- `gdk-pixbuf`: `others` carries the BMP, ICO, ANI, PNM, TGA, XPM, XBM, ICNS and QTIF loaders and
  defaults to disabled; without it no GTK program opens those formats.
- `graphviz`: upstream's install writes the plugin index (`dot -c`) only when `DESTDIR` is empty,
  so `build.sh` writes it into the package; without `config8` dot loads no plugin and knows no
  output format. Each renderer probe drops its plugin silently, so the plugin check after the
  install is what fails such a build.
- `harfbuzz`: `chafa` stays disabled: chafa reaches librsvg, which depends on harfbuzz, so it
  would be a cycle. Introspection is on because pango's GIR includes `HarfBuzz-0.0.gir`.
- `librsvg`: the vapi (`librsvg-2.0`) is what Vala programs such as gnome-mines compile against.
- `pango`: `xft` builds pangoxft, without which FLTK's hybrid Wayland and X11 build stops at
  configure; `libthai` is what breaks a line of Thai, which has no spaces between words.

### image-libs

Raster image formats, codecs, metadata, processing, barcodes, and terminal image output. 47 ports,
under `ports/core/image-libs/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `aalib` | 1.4rc5 | ASCII art graphics library — renders true-color images to text | `41_system` |
| `chafa` | 1.18.3 | Images as terminal graphics: sixel, kitty, iTerm2 or Unicode | `42_graphics` |
| `exempi` | 2.6.6 | XMP metadata library and the exempi tool, from Adobe's XMP SDK | `41_system` |
| `exiftool` | 13.59 | Reads and writes metadata in image, audio and video files | `41_system` |
| `exiv2` | 0.28.9 | Read and write image metadata — EXIF, IPTC and XMP | `41_system` |
| `farbfeld` | 4 | Lossless image format designed for piping, and its converters | `41_system` |
| `gexiv2` | 0.14.7 | GObject wrapper around exiv2 image metadata, the 0.14 API GIMP links | `42_graphics` |
| `giflib` | 6.1.3 | Libraries for reading and writing GIFs as well as programs for converting and working with GIF files | `41_system` |
| `graphicsmagick` | 1.3.48 | Image processing and Magick++, behind Octave's imread and imwrite | `41_system` |
| `imagemagick` | 7.1.2.33 | Converts, edits and composes raster images (provides `convert`) | `43_toolkits` |
| `imath` | 3.2.3 | C++ and python library of 2D and 3D vector, matrix, and math operations for computer graphics | `41_system` |
| `jasper` | 4.2.9 | JasPer JPEG-2000 codec library, with the jasper, imginfo and imgcmp tools | `41_system` |
| `jbigkit` | 2.1 | JBIG1 (ITU-T T.82 and T.85) bi-level image compression library and tools | `41_system` |
| `lensfun` | 0.3.4 | Lens correction library and its camera and lens database | `41_system` |
| `leptonica` | 1.87.0 | The image processing tesseract does its page analysis with | `41_system` |
| `libavif` | 1.4.2 | AVIF reader and writer — the AV1 still-image format, with avifenc and avifdec | `41_system` |
| `libdmtx` | 0.7.8 | Data Matrix two-dimensional barcode reading and writing library | `41_system` |
| `libexif` | 0.6.26 | Library for parsing, editing, and saving EXIF data | `41_system` |
| `libgd` | 2.3.3 | GD graphics library — draws lines, shapes, text and images into PNG, JPEG, GIF, WebP, TIFF, HEIF, AVIF and BMP | `42_graphics` |
| `libheif` | 1.23.6 | HEIF and AVIF image reader and writer | `41_system` |
| `libimagequant` | 4.4.1 | Palette quantisation that turns a 32-bit image into a small 8-bit PNG or GIF | `41_system` |
| `libiptcdata` | 1.0.5 | IPTC photo metadata library and the iptc tool | `41_system` |
| `libjpeg-turbo` | 3.2.0 | A fork of the original IJG libjpeg which uses SIMD to accelerate baseline JPEG compression and decompression | `41_system` |
| `libjxl` | 0.12.0 | JPEG XL reference implementation — libjxl, cjxl, djxl and the gdk-pixbuf loader | `41_system` |
| `libkdcraw` | 26.08.1 | Qt 6 wrapper around LibRaw for decoding camera raw files | `43_toolkits` |
| `libkexiv2` | 26.08.1 | Qt 6 wrapper around exiv2 for reading and writing picture metadata | `43_toolkits` |
| `libmypaint` | 1.6.1 | MyPaint brush engine library, used by GIMP and Krita | `41_system` |
| `libpng` | 1.6.59 | A collection of routines used to create PNG format graphics files | `41_system` |
| `libraw` | 0.22.2 | Camera raw file decoding library | `41_system` |
| `libsixel` | 1.10.5 | Sixel encoder and decoder library, for pictures in a terminal | `42_graphics` |
| `libspng` | 0.7.4 | Simple, fast PNG decoding and encoding library | `41_system` |
| `libtiff` | 4.7.2 | TIFF libraries and associated utilities | `41_system` |
| `libvips` | 8.18.6 | Streaming image-processing library with low memory use | `44_apps` |
| `libwebp` | 1.6.0 | Library and support programs to encode and decode images in WebP format | `41_system` |
| `libyuv` | 0.0.1971 | YUV conversion, scaling and rotation — libavif converts colour through it | `41_system` |
| `mypaint-brushes` | 2.0.2 | MyPaint brush collection, version 2 data set | `41_system` |
| `netpbm` | 11.16.1 | Several hundred converters and filters for the PBM, PGM, PPM and PAM image formats, and libnetpbm | `41_system` |
| `opencolorio` | 2.5.2 | OpenColorIO colour management for visual effects and animation | `42_graphics` |
| `openexr` | 3.5.0 | High dynamic range image format library | `41_system` |
| `openimageio` | 3.1.17.0 | Image reading, writing and processing for film pipelines — Blender's image I/O, with oiiotool | `42_graphics` |
| `openjpeg` | 2.5.4 | JPEG 2000 codec — what scanners, archives and PDFs put images in | `41_system` |
| `qrencode` | 4.1.1 | Write a QR code, including as terminal characters | `41_system` |
| `webp-pixbuf-loader` | 0.2.7 | WebP loader for gdk-pixbuf, so GTK programs open and thumbnail WebP images | `41_system` |
| `zbar` | 0.23.93 | Read a QR or barcode from an image or a camera | `43_toolkits` |
| `zimg` | 3.0.6 | Image scaling, colour space and depth conversion library | `41_system` |
| `zint` | 2.16.0 | Barcode encoder for over 50 symbologies, with a CLI and the Zint Barcode Studio GUI | `44_apps` |
| `zxing-cpp` | 2.3.0 | Barcode and QR code reading and writing — what `mutool barcode` is built on | `41_system` |

Notes:

- `aalib`: gpm is turned off by answering its library probe (`ac_cv_lib_gpm_Gpm_Repeat=no`), because
  `--with-gpm-mouse` takes a directory and `=no` would link `-lgpm` from `no/lib`.
- `gexiv2`: stays on the 0.14 API, because GIMP's meson.build accepts gexiv2 below 0.15 only.
- `graphicsmagick`: Octave's image functions need the ImageMagick 6 API, which GraphicsMagick keeps.
  The ImageMagick-named compatibility commands are not installed, so nothing collides with the
  imagemagick port.
- `imagemagick`: `ac_cv_path_PSDelegate=gs` keeps the delegate a bare name run from `PATH`; detected,
  it becomes an absolute path and adds a font table naming Ghostscript fonts the ghostscript port
  does not install.
- `lensfun`: `-DPYTHON=OFF` is what keeps the Python module out; `INSTALL_PYTHON_MODULE` is never
  read.
- `libimagequant`: cargo runs from the top of the source tree, where the vendor configuration is
  unpacked; from the `imagequant-sys` member every crate resolves as missing.
- `libvips`: links libimagequant, which is GPL-3.0-or-later, so the libvips this tree builds is
  GPL-3.0 as distributed.
- `libyuv`: the source is Debian's orig tarball, because googlesource generates a different archive
  on every request and no checksum holds.
- `qrencode`: the switch is spelled `--with-tools`; autoconf reads `--without-tools=NO` as a request
  for a package named `tools=NO`.
- `zbar`: `XML_CATALOG_FILES` names `/etc/xml/catalog`, or xsltproc tries to fetch the docbook
  stylesheet and the manual pages fail to build. `--without-graphicsmagick` makes
  `--with-imagemagick` binding; with the fallback left at its default, a missing MagickWand turns
  image scanning off with only a notice.
- `zxing-cpp`: stays on the 2.x line, because mupdf includes its headers by bare name, which 2.x's
  `zxing.pc` puts on the include path and 3.x's does not.

### graphics

Graphics applications: painting, vector, photo development and management, image viewers, screenshot
and image optimisers. 23 ports, under `ports/core/graphics/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `aa3d` | 1.0 | ASCII art stereogram generator — renders a depth map as a magic eye | `41_system` |
| `aview` | 1.3.0rc1 | ASCII art image browser and FLI/FLC player for the terminal | `44_apps` |
| `converseen` | 0.15.2.8 | Batch image converter and resizer over ImageMagick, for Qt 6 | `44_apps` |
| `darktable` | 5.6.1 | RAW photo developer and virtual lighttable | `44_apps` |
| `digikam` | 9.1.0 | digiKam and showFoto: photo library, tagging, face recognition, RAW import and editing, for Qt 6 | `44_apps` |
| `flameshot` | 14.0.0 | Screenshot tool with an annotation editor, capturing through the Screenshot portal | `44_apps` |
| `gimp` | 3.2.6 | GNU Image Manipulation Program — raster image editor and photo retoucher | `44_apps` |
| `gimp-help` | 3.2.0 | The GIMP user manual in English, as local HTML that F1 opens with no network | `41_system` |
| `glaxnimate` | 0.6.0 | Glaxnimate: vector animation and motion design, Lottie, SVG and video export, for Qt 6 | `44_apps` |
| `grafx2` | 2.9 | Pixel-art paint program in the Deluxe Paint tradition, on SDL2 with no toolkit | `44_apps` |
| `gthumb` | 4.0 | gThumb: browse, view, tag and edit photos and videos, for GTK 4 and libadwaita | `44_apps` |
| `gwenview` | 26.08.1 | Image viewer and browser — RAW, annotation, slideshow and a camera importer | `44_apps` |
| `imv` | 5.0.1 | Image viewer for Wayland (Wayland-only build, meson) | `44_apps` |
| `inkscape` | 1.4.4 | Vector graphics editor — SVG drawing, PDF import, CorelDRAW and Visio files | `44_apps` |
| `kolourpaint` | 26.08.1 | Paint program — draw, crop, recolour and scan | `44_apps` |
| `krita` | 6.0.4 | Krita: digital painting, illustration and frame-by-frame animation, for Qt 6 | `44_apps` |
| `optipng` | 7.9.1 | Lossless PNG optimiser | `41_system` |
| `oxipng` | 10.2.1 | Lossless PNG optimiser | `41_system` |
| `pencil2d` | 0.7.2 | Pencil2D: hand-drawn 2D animation with bitmap and vector layers, for Qt 6 | `44_apps` |
| `pngquant` | 2.18.0 | Lossy PNG compressor — reduces a truecolour PNG to a palette | `41_system` |
| `rawtherapee` | 5.13 | RAW photo developer with non-destructive, per-image processing profiles | `44_apps` |
| `swayimg` | 5.6 | Image viewer for Wayland and the console — no toolkit, Lua-configured | `44_apps` |
| `timg` | 1.6.3 | Terminal image and video viewer | `44_apps` |

Notes:

- `converseen`: Magick++'s headers size every Quantum from `MAGICKCORE_HDRI_ENABLE` and
  `MAGICKCORE_QUANTUM_DEPTH`, so both come from the installed `Magick++.pc`; a mismatch compiles
  and then reads every pixel as the wrong type.
- `darktable`: `BINARY_PACKAGE_BUILD` compiles darktable and its bundled rawspeed for the generic
  x86-64 target instead of the builder's processor.
- `digikam`: KDE's compiler settings make C strict C99; the panorama tool's flex and bison parser
  calls `fileno` and `strdup`, so C is GNU C99.
- `glaxnimate`: the project forces its own libraries static, and `BUILD_SHARED_LIBS` is not
  passed.
- `grafx2`: `LUAPKG` names Lua 5.4; the makefile's own search ends at plain `lua`, the 5.5 port,
  which the scripts were never run against.
- `gthumb`: the sources are C23 and the project names no standard; `-std=gnu23` comes after the
  tree's `-std=gnu11`.
- `inkscape`: Magick++ comes from GraphicsMagick: the ImageMagick++ probe accepts ImageMagick 6
  only, and without either the raster effects are absent.
- `krita`: WebP and libjpeg-turbo cannot be required at configure, because the requirement also
  applies to the config-package search their find modules make first, which both fail; they are
  checked in the generated build files after the configure instead. The three
  `KRITA_QT_HAS_*` switches name patches Krita's own Qt carries and this Qt does not; turned on,
  the events they cover go unhandled.
- `optipng`: the binary is installed with `install(FILES)`, which gives it mode 644.
- `pencil2d`: Qt 6's qmake reads no compiler flags from the environment, so the tree's flags, with
  the reproducibility maps, are passed on the qmake line.
- `pngquant`: the port stays on the 2 line: libimagequant 4 keeps the C interface pngquant 2 links,
  and pngquant 3 is a Rust program that vendors the library.
- `rawtherapee`: `PROC_TARGET_NUMBER=0` adds no `-march` or `-mtune`, so the package runs on every
  x86-64 machine.

### 3d

3D modelling, animation, voxel and mesh tools and libraries, and scientific visualisation. 17 ports,
under `ports/core/3d/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `admesh` | 0.98.5 | STL mesh checking and repair tool | `41_system` |
| `alembic` | 1.8.12 | Alembic: baked geometry and animation interchange (Ogawa .abc) — Blender's import and export | `41_system` |
| `assimp` | 6.0.5 | Reads and writes some 40 3D model formats (glTF, OBJ, FBX, Collada, STL) into one scene graph | `41_system` |
| `blender` | 5.2.2 | Blender: 3D modelling, sculpting, animation, rendering (Cycles, EEVEE), compositing and video editing | `44_apps` |
| `draco` | 1.5.7 | Google's compressed geometry and point-cloud format, library and codec tools | `41_system` |
| `embree` | 4.4.1 | Intel's CPU ray-tracing kernels for renderers, from SSE2 to AVX-512, chosen at run time | `42_graphics` |
| `gl2ps` | 1.4.2 | OpenGL scenes to PostScript, PDF and SVG, for Octave's figure printing | `42_graphics` |
| `goxel` | 0.15.1 | 3D voxel editor, drawn with OpenGL through GLFW and no toolkit | `44_apps` |
| `lib3mf` | 2.5.0 | Reference implementation of the 3D Manufacturing Format (3MF) for reading and writing 3D print files | `41_system` |
| `libspnav` | 1.2 | Client library for 6-DoF 3D mice (3Dconnexion SpaceNavigator and friends) through spacenavd | `42_graphics` |
| `manifold` | 3.5.4 | Guaranteed-manifold mesh boolean library, the geometry kernel OpenSCAD renders with | `42_graphics` |
| `meshoptimizer` | 1.3 | Mesh optimisation library — vertex cache, overdraw, simplification and compression — and gltfpack | `41_system` |
| `oidn` | 2.5.1 | Intel's AI denoising of path-traced images on the CPU, with oidnDenoise | `42_graphics` |
| `opencsg` | 1.8.2 | Image-based constructive solid geometry rendering with OpenGL | `42_graphics` |
| `opensubdiv` | 3.7.0 | Pixar's OpenSubdiv: subdivision surface evaluation on the CPU and in GLSL — Blender's Subdivision modifier | `42_graphics` |
| `openvdb` | 13.0.0 | OpenVDB and NanoVDB: sparse volumes — Blender's smoke, fire and volume objects | `42_graphics` |
| `vtk` | 9.5.2 | The Visualization Toolkit: meshes, filters and OpenGL rendering, with Python bindings | `42_graphics` |

Notes:

- `assimp`: its zip reader includes `<unzip.h>`, which minizip installs under `/usr/include/minizip`
  while `minizip.pc` names only `/usr/include`, so the build adds that directory to `CXXFLAGS`.
- `blender`: `WITH_STRICT_BUILD_OPTIONS=ON` stops the configure on an enabled feature whose library
  is missing, so every `ON` is a feature the package has. glog and gflags stay bundled: libmv
  includes glog's headers without the definitions glog 0.7's CMake target supplies. The desktop
  entry passes `--offline-mode`, which keeps the extensions platform from offering online access.
- `draco`: the transcoder stays off, because the release archive carries its tinygltf, Eigen and
  filesystem submodules empty.
- `goxel`: scons defaults to a debug build linked against the address and undefined-behaviour
  sanitisers; `mode=release` is what makes a shippable binary.
- `lib3mf`: the library and include directories are passed typed (`:PATH`): the project declares
  them as PATH cache entries, and an untyped relative value becomes a path under the source tree.
  The bindings stay under `include/Bindings/<language>`, which `lib3mfConfig.cmake` and OpenSCAD's
  `FindLib3MF` name literally.
- `opensubdiv`: osdGPU opens `libGL.so.1` at run time and asks it for `glXGetProcAddressARB`, so the
  GPU path needs the GLX-enabled libglvnd.
- `openvdb`: the version follows the one Blender pins, because Cycles compiles against the NanoVDB
  headers, whose API moves between minor releases. `CONCURRENT_MALLOC=Tbbmalloc` is named because
  `Auto` takes jemalloc whenever the build root holds it.
- `vtk`: hdf5 and fmt stay bundled and symbol-mangled: VTK 9.5 is written against the hdf5 1.x API
  and calls `fmt::localtime`, which fmt 12 removed. Python wrapping stays on, because FreeCAD's FEM
  post-processing filters are VTK Python objects.

### cad

Mechanical, architectural and 2D CAD, geometry kernels, meshing and FEM. 17 ports, under
`ports/core/cad/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `calculix` | 2.23 | Three-dimensional structural and thermal finite element solver (ccx) with an Abaqus-style input deck | `41_system` |
| `cgal` | 6.2.1 | Computational Geometry Algorithms Library (header-only C++) | `41_system` |
| `clipper2` | 2.0.1 | Polygon clipping and offsetting library | `41_system` |
| `coin` | 4.0.10 | Open Inventor 3D scene graph library, the viewer under FreeCAD | `42_graphics` |
| `freecad` | 1.1.3 | Parametric 3D CAD modeller: Part Design, Sketcher, TechDraw, FEM, CAM, BIM and Assembly | `44_apps` |
| `gmsh` | 4.15.2 | 3D finite element mesh generator with CAD, meshing and post-processing (FLTK) | `44_apps` |
| `ifcopenshell` | 0.8.5 | IFC (BIM) toolkit: the C++ parser and OpenCASCADE geometry, IfcConvert and the ifcopenshell Python module FreeCAD's BIM workbench imports | `42_graphics` |
| `librecad` | 2.2.1.5 | 2D CAD drafting with DXF and DWG import (Qt 5) | `44_apps` |
| `netgen` | 6.2.2604 | Automatic 3D tetrahedral mesh generator (nglib, OpenCASCADE geometry and the Python module FreeCAD's FEM drives) | `42_graphics` |
| `opencascade` | 7.9.3 | Open CASCADE Technology, the B-rep CAD kernel with STEP, IGES and glTF exchange | `42_graphics` |
| `openscad` | 2026.09.27 | The programmer's solid 3D CAD modeller: models written as scripts, rendered with Manifold and CGAL | `44_apps` |
| `pivy` | 0.6.11 | Python bindings for Coin and SoQt, the scene-graph API FreeCAD's workbenches script | `43_toolkits` |
| `polyclipping` | 6.4.2 | Clipper 1 — polygon clipping and offsetting library, the API libnest2d and CuraEngine link | `41_system` |
| `qcad` | 3.33.1.0 | 2D CAD with DXF, script add-ons and a large part library (community edition, Qt 6) | `44_apps` |
| `qhull` | 8.0.2 | Convex hulls, Delaunay triangulation and Voronoi diagrams | `41_system` |
| `solvespace` | 3.2 | Parametric 2D and 3D CAD with a constraint solver (Qt 6 interface) | `44_apps` |
| `soqt` | 1.6.4 | Qt 6 bindings for the Coin 3D scene graph (viewers and render areas) | `43_toolkits` |

Notes:

- `coin`: GLX and EGL are both built and Coin picks per context; built with one only, offscreen
  rendering fails under the other. With EGL built, Coin opens GLU at run time for NURBS
  tessellation and never links it, so `glu` is a run-time dependency.
- `freecad`: the source is the release tarball, not the tag archive: only it carries the
  OndselSolver, GSL and AddonManager submodules. It is flat, and kpkg strips one component from a
  first source, so the build unpacks it again unstripped.
- `freecad`: `COIN3D_VERSION` and its parts come from Coin's pkg-config file. The configure
  otherwise reads each part of Coin's version one digit long, so 4.0.10 reads as 4.0.1 and fails
  the check against the Coin `pivy` was built with.
- `freecad`: the program tree is `/usr/lib/freecad`, with only the launchers linked into
  `/usr/bin`: FreeCAD finds its modules beside its executable. The bin directory is given as an
  absolute path because the thumbnailer entry is written with it.
- `ifcopenshell`: `python-version.patch` gives the Python package its real version. FreeCAD reads
  `ifcopenshell.version`, and below 0.7 offers to fetch IfcOpenShell over the network.
- `netgen`: `NETGEN_VERSION_GIT` is `v$version-0-g$_commit`, so `_commit` is the tag's commit and
  moves with every bump; the tag archive has no `.git`, and without it the headers and the Python
  metadata say 6.2.0. `USE_NATIVE_ARCH=OFF` because its `-march=native` is public on ngcore and
  reaches every consumer's compile line.
- `opencascade`: `USE_XLIB=OFF` builds TKOpenGl against EGL, so the viewer works in a Wayland EGL
  context; with it on, the viewer is GLX and X11 only.
- `openscad`: the port follows upstream's development line at a pinned commit; the last release
  predates CGAL 6 and Manifold. `_sancommit`, `_micommit` and `_mcadcommit` are the submodule
  commits that commit pins, and move with it.
- `pivy`: `PIVY_USE_QT6` matches the Qt `soqt` was built with; pivy finds SoQt's configuration
  first and then requires that Qt's widgets.
- `qcad`: `qtjsapi` and `qcadjsapi` are the script bindings, built from the same tag; without
  them the program starts with no menus.
- `qhull`: stays at 8.0.2. matplotlib's meson names `v8.0.2` when its system-qhull switch is off,
  and upstream's later versions are alphas.
- `solvespace`: the source is the release tarball, not the tag archive: only it carries the
  libdxfrw and mimalloc submodules.

### fabrication

3D-printing slicers and hosts, and CNC control. 13 ports, under `ports/core/fabrication/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `bcnc` | 0.9.16 | GRBL CNC controller and G-code sender with an editor, probing, auto-levelling and CAM tools (Tk) | `44_apps` |
| `candle2` | 2.4 | GRBL CNC and laser controller with a G-code visualiser, height maps and jogging (Qt 5) | `44_apps` |
| `cncjs` | 1.11.5 | Web-based controller for Grbl, Marlin, Smoothieware and TinyG CNC machines, served to the browser | `41_system` |
| `cura` | 5.13.0 | UltiMaker's slicer: prepares 3D models for FDM printing, with printer definitions and material profiles | `44_apps` |
| `curaengine` | 5.13.0 | The slicer behind Cura, also usable on its own from the command line | `42_graphics` |
| `libarcus` | 5.11.1 | The protobuf message socket between Cura and CuraEngine | `41_system` |
| `libnest2d` | 5.11.0.alpha0 | Header-only 2D bin packing, which arranges parts on Cura's build plate | `41_system` |
| `libsavitar` | 5.12.0 | The 3MF scene reader and writer under Cura | `41_system` |
| `linuxcnc` | 2.9.10 | CNC machine controller for mills, lathes, routers and plasma: HAL, G-code interpreter and the AXIS GUI | `44_apps` |
| `orcaslicer` | 2.4.2 | G-code slicer for FDM printers with calibration tools and a wide printer-profile library | `44_apps` |
| `printrun` | 2.2.0 | pronsole and printcore — send a sliced job to a printer over USB serial | `41_system` |
| `prusaslicer` | 2.9.6 | Turns 3D models into G-code for FDM and resin printers, with the bundled vendor profiles | `44_apps` |
| `uranium` | 5.13.0 | The Python and Qt Quick application framework Cura is built on | `43_toolkits` |

Notes:

- `cncjs`: the desktop entry binds the server to loopback. It otherwise listens on every
  interface, and it drives the machine without a login.
- `linuxcnc`: the package ships no setuid file, so `rtapi_app` runs its threads without realtime
  priority, which is the simulator.
- `orcaslicer`: the build runs one job per 6 GiB of memory: a compiler on OpenVDB, CGAL or
  libslic3r holds 3 to 4 GB. Every archive the superbuild would download is a source of the port,
  accepted only against the SHA-256 the superbuild pins.

### fonts

Font packages, and font libraries and tooling. 25 ports, under `ports/core/fonts/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `bdftopcf` | 1.1 | Convert X font from Bitmap Distribution Format to Portable Compiled Format | `41_system` |
| `encodings` | 1.1.0 | X.org font encoding files | `41_system` |
| `font-adobe-75dpi` | 1.0.4 | X.org core bitmap fonts, served by Xwayland to its X11 clients | `41_system` |
| `font-caladea` | 20130214 | Caladea — a serif face metric-compatible with Cambria | `41_system` |
| `font-carlito` | 20130920 | Carlito — a sans face metric-compatible with Calibri | `41_system` |
| `font-cursor-misc` | 1.0.4 | X.org core bitmap fonts, served by Xwayland to its X11 clients | `41_system` |
| `font-liberation` | 2.1.5 | Liberation Sans, Serif, Mono and Sans Narrow — metric-compatible with Arial, Times New Roman, Courier New and Arial Narrow | `43_toolkits` |
| `font-manager` | 0.9.4 | Font Manager and Font Viewer: browse, compare, preview and enable fonts, for GTK 4 | `44_apps` |
| `font-misc-misc` | 1.1.3 | X.org core bitmap fonts, served by Xwayland to its X11 clients | `41_system` |
| `font-util` | 1.4.2 | X.Org font utilities | `41_system` |
| `fontconfig` | 2.18.3 | A library and support programs used for configuring and customizing font access | `41_system` |
| `fontforge` | 20251009 | Outline and bitmap font editor: the GTK editor window, scripts and the Python module | `43_toolkits` |
| `fonttools` | 4.66.0 | Inspect, subset and convert font files from a prompt | `41_system` |
| `freetype2` | 2.14.3 | Font rasterization library | `41_system` |
| `libspiro` | 20240903 | Spiro curve library — turns clothoid spline control points into Bézier outlines | `41_system` |
| `mkfontscale` | 1.2.4 | Create an index of scalable font files for X | `41_system` |
| `nerd-fonts-symbols` | 3.5.1 | Symbols Nerd Font — the icon codepoints a patched font carries, as a fallback face | `41_system` |
| `noto-cjk` | 2.004 | Noto Sans CJK — Chinese, Japanese and Korean coverage in one collection | `41_system` |
| `noto-emoji` | 2.051 | Noto Color Emoji — the desktop's colour emoji face | `44_apps` |
| `noto-fonts` | 2.015 | Noto Sans and Noto Sans Mono — the desktop's proportional and fixed faces | `41_system` |
| `noto-fonts-extra` | 1 | Noto Sans coverage for Arabic, Hebrew, Indic, Sinhala, Thai, Lao, Khmer, Myanmar, Ethiopic, Georgian and Armenian | `41_system` |
| `terminus-font` | 4.49.1 | Monospace bitmap console font | `41_system` |
| `terminus-ttf` | 4.49.3 | Scalable TTF build of Terminus, for the consumers that cannot draw a bitmap font | `44_apps` |
| `ttf-dejavu` | 2.37 | Font family based on the Bitstream Vera Fonts with a wider range of characters | `43_toolkits` |
| `woff2` | 1.0.2 | Web Open Font Format 2 reference implementation: the WOFF2 codec libraries and tools | `41_system` |

Notes:

- `font-caladea`: stays on the crosextrafonts release LibreOffice pins. Later Caladea releases
  have different metrics, and a Cambria document set in them paginates differently from Word.
- `fontconfig`: the system cache is `/usr/lib/fontconfig/cache`. The image and every pack exclude
  `var/cache`, so a cache there would never reach a booted machine and every first client would
  rescan the font tree.
- `fontforge`: every optional library is named `ON` or `OFF`; an `AUTO` that misses its library
  builds a narrower fontforge without a word. Its desktop entry is written by `build.sh` and claims
  no TrueType or OpenType type, so a double-clicked font opens font-manager's viewer and not the
  editor.
- `fonttools`: `FONTTOOLS_WITH_CYTHON=1` makes the compiled modules mandatory; unset, `setup.py`
  ships the pure-Python fallbacks whenever Cython is not importable.
- `freetype2`: `--with-harfbuzz=dynamic`: the autohinter dlopens `libharfbuzz.so.0`, because
  linking it would be a dependency cycle (harfbuzz builds against freetype2).
- `nerd-fonts-symbols`: the release tarball is flat, and kpkg's `--strip-components=1` leaves
  `$SRC` empty, so `build.sh` unpacks it. Upstream's `10-nerd-font-symbols.conf` is not installed:
  it prefers the double-width face for `monospace`. `66-nerd-font-symbols.conf` `<accept>`s the
  Mono face behind every text face and names `Terminus` and `Terminus (TTF)` one by one, since a
  pattern for a concrete family carries no generic.
- `noto-emoji`: the CBDT build, not COLRv1: the vector build needs a rasteriser this tree does not
  have. `BYPASS_SEQUENCE_CHECK` stays while the pinned nototools carries a newer emoji list than
  the artwork draws.
- `noto-fonts`: `56-noto-preferred.conf` must sort before the `57-dejavu-*.conf` set. Noto Sans
  Mono lacks arrows, dingbats and most symbols, and fcft finds them in DejaVu, the next face down
  the list.
- `noto-fonts-extra`: `version` is a meta-version and each face is pinned in its own helper.
  `ports/update` reads notofonts/arabic, whose newest tag can be a NotoNaskhArabic release with no
  NotoSansArabic asset, so it offers an upgrade the port must not take; bump the helpers by hand.
- `noto-fonts-extra`: `56-noto-scripts.conf` sorts between `56-noto-preferred` and `57-dejavu-*`,
  putting these faces ahead of DejaVu, which claims Arabic, Hebrew, Armenian, Georgian and Lao. A
  face goes on its lists only if its cmap carries no symbol or Latin coverage (Gurmukhi and
  Sinhala are installed but kept off), and no CJK face is ever named there.
- `terminus-font`: `75-yes-terminus.conf` exempts Terminus from fontconfig's `70-no-bitmaps.conf`;
  without it foot falls through to DejaVu.
- `terminus-ttf`: `build.sh` runs the steps of upstream's `mkttf.sh` itself, because that script
  stamps the build year into every face; the year comes from `SOURCE_DATE_EPOCH` instead, so two
  builds of one recipe give the same files.
- `ttf-dejavu`: the make target is `full-ttf`; the default target also regenerates the coverage
  reports, which need Unicode and fontconfig orthography data this tree does not carry.

### themes

Icon, cursor, sound and widget themes, and theme-configuration tools. 7 ports, under
`ports/core/themes/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `adwaita-icon-theme` | 51.0 | GNOME's Adwaita icon and cursor theme — the symbolic-icon fallback GTK applications expect | `44_apps` |
| `breeze` | 6.7.5 | Breeze widget style and colour schemes for Qt 6 applications | `43_toolkits` |
| `hicolor-icon-theme` | 0.18 | Freedesktop.org Hicolor icon theme | `41_system` |
| `icon-naming-utils` | 0.8.90 | Perl script used for maintaining backwards compatibility with current desktop icon themes | `41_system` |
| `qt5ct` | 1.9 | Qt 5 appearance settings — the palette, style, fonts and icon theme of every Qt 5 program, as a platform theme plugin | `44_apps` |
| `qt6ct` | 0.11 | Qt 6 appearance settings — the palette, style, fonts and icon theme of every Qt 6 program, as a platform theme plugin | `44_apps` |
| `sound-theme-freedesktop` | 0.8 | The freedesktop default sound theme — event sounds libcanberra plays | `41_system` |

Notes:

- `breeze`: frameworkintegration is required: without it the style builds without its KStyle helpers
  and the log does not say so.
- `qt5ct`: `kde-key.patch` makes the plugin answer `QT_QPA_PLATFORMTHEME=kde`, the value the session
  exports, so every Qt 5 program reads the files `kdos theme` writes.

### xdg

freedesktop desktop integration: portals, MIME, desktop entries, AppStream, tray and indicators,
notifications, event sounds, script dialogs. 16 ports, under `ports/core/xdg/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `appstream` | 1.2.0 | AppStream metadata library and appstreamcli — reads and validates software component data | `42_graphics` |
| `ayatana-ido` | 0.10.4 | Ayatana Indicator Display Objects — the GTK 3 menu widgets indicator menus are built from | `43_toolkits` |
| `chemical-mime-data` | 0.1.94 | MIME types for chemistry file formats — CML, PDB, MDL Molfile, XYZ, Gaussian and about 70 more | `41_system` |
| `desktop-file-utils` | 0.28 | Command line utilities for working with Desktop entries | `41_system` |
| `libayatana-appindicator` | 0.6.0 | Ayatana application indicators — a GTK 3 program's tray icon and menu as a StatusNotifierItem | `43_toolkits` |
| `libayatana-indicator` | 0.9.5 | Ayatana indicator library — the shared object behind application and system indicators | `43_toolkits` |
| `libcanberra` | 0.30 | XDG sound theme event sounds over ALSA, PulseAudio and GStreamer, with the GTK 3 module | `43_toolkits` |
| `libdbusmenu` | 16.04.0 | Menus passed over D-Bus — libdbusmenu-glib and the GTK 3 renderer libdbusmenu-gtk3 | `43_toolkits` |
| `libnotify` | 0.8.8 | Desktop notification client library and notify-send | `41_system` |
| `libportal` | 0.11.0 | Client library for the XDG desktop portals, with GTK 3 and GTK 4 parent windows | `43_toolkits` |
| `shared-mime-info` | 2.5.1 | The freedesktop.org shared MIME database and update-mime-database | `41_system` |
| `xdg-desktop-portal` | 1.22.1 | Desktop portal D-Bus framework (Wayland screencast / file-picker integration) | `42_graphics` |
| `xdg-desktop-portal-gtk` | 1.15.3 | GTK backend for xdg-desktop-portal — the Print and Email portals | `43_toolkits` |
| `xdg-desktop-portal-wlr` | 0.8.4 | wlroots backend for xdg-desktop-portal (ScreenCast and Screenshot) | `50_desktop` |
| `xdg-utils` | 1.2.1 | Command line tools that assist applications with a variety of desktop integration tasks | `41_system` |
| `zenity` | 4.2.2 | GTK dialog boxes from the command line, and the file chooser other programs call | `43_toolkits` |

Notes:

- `appstream`: the manual page is off: its stylesheet is the namespaced DocBook XSL release, which
  `docbook-xsl` does not ship, and setup stops without it.
- `libcanberra`: install runs with `-j1`. libtool relinks each backend module against the library
  as it installs it, and a parallel install can relink before the library is in place.
- `libdbusmenu`: with the tests off, configure never defines the `HAVE_VALGRIND` conditional and
  refuses to finish; the two values on the configure line settle it. The makefiles put `-Werror`
  ahead of `CFLAGS`, so `-Wno-error` is required.
- `xdg-desktop-portal`: the image and sound validators re-execute under bubblewrap, whose path is
  found at configure and compiled in, so `bubblewrap` stays in `depends`.
- `xdg-desktop-portal-gtk`: Settings, AppChooser, Lockdown and Wallpaper are off; which backend
  serves an interface is decided by `kdos-portals.conf`, not by the portal file's `UseIn=` line.

### accessibility

Screen readers, speech, braille and alternative input. 9 ports, under `ports/core/accessibility/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `at-spi2-core` | 2.62.0.1 | Assistive Technology Service Provider Interface — the accessibility bus, ATK and the ATK bridge | `42_graphics` |
| `brltty` | 6.9.1 | Braille display and console speech — the screen reader | `41_system` |
| `dasher` | 5.0.0.beta.20200420 | Text entry by steering a pointer, eye tracker or switch through a predictive language model | `44_apps` |
| `espeak-ng` | 1.52.0 | Speech synthesis for over 100 languages, in about 15 MB | `41_system` |
| `liblouis` | 3.39.0 | Braille translation — contracted and uncontracted tables for well over a hundred languages | `41_system` |
| `orca` | 51.0 | The screen reader: speech and braille over the AT-SPI accessibility tree | `44_apps` |
| `pcaudiolib` | 1.3 | The audio output layer espeak-ng speaks through | `41_system` |
| `piper` | 1.8.0 | Piper: neural text to speech on the CPU, with the voice model on the disk and nothing sent anywhere | `42_graphics` |
| `speech-dispatcher` | 0.12.1 | Speech synthesis server for screen readers, with espeak-ng as its voice | `41_system` |

Notes:

- `brltty`: the source is the release tarball, whose pre-generated `configure` works; regenerating
  it with this autoconf breaks the speech-driver lookup. The install variable is `INSTALL_ROOT`:
  `DESTDIR` is ignored and the install writes into the live root. The speech driver is spelled
  `eSpeak-NG`, matched case-sensitively. The screen-driver list ends in `-all`, or the rest build as
  external drivers and FileViewer fails to link `-ltinfo`; the speech list has none, because
  SpeechDispatcher links only as an external module.
- `liblouis`: the `louis` Python module, which Orca translates braille through, is installed from
  `python/` after `make`: the build writes its `__init__.py` with the soname of the library it
  just linked.
- `orca`: speech needs the `speechd` Python module, which speech-dispatcher installs only with
  `--enable-python`; without it Orca starts and says nothing. Braille needs brltty's and liblouis's
  Python modules in the same way.
- `pcaudiolib`: `--with-alsa` does not fail when its probe misses; alsa-lib in `depends` is what
  keeps the ALSA backend.
- `piper`: its bridge calls `espeak_TextToPhonemesWithTerminator`, which no espeak-ng release has,
  so a static espeak-ng is built from the pinned commit `_espeak` names, handed to CMake as an
  archive by `espeak-ng-local-archive.patch`; a bump keeps that commit at or past upstream's pin.
  Playback pipes to `pw-play`, because the ffmpeg port builds no `ffplay`.
- `speech-dispatcher`: voxin, baratinoo and kali build a shim that dlopens a proprietary engine when
  the engine is missing, so each is switched off by name. python3-pyxdg is a build dependency as
  well: `spd-conf`'s manual page is help2man's capture of `spd-conf --help`, which imports it.

### i18n

Unicode, locale, text shaping and segmentation, conversion, and translation. 15 ports, under
`ports/core/i18n/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `fribidi` | 1.0.17 | Implementation of the Unicode Bidirectional Algorithm (BIDI) | `41_system` |
| `gettext` | 1.0 | Utilities for internationalization and localization | `30_foundation` |
| `icu` | 78.3 | International Components for Unicode library | `30_foundation` |
| `iso-codes` | 4.20.1 | List of country, language and currency names | `41_system` |
| `libdatrie` | 0.2.14 | Double-array trie — the dictionary structure libthai looks words up in | `41_system` |
| `libgrapheme` | 3.0.0 | Unicode string segmentation — grapheme clusters, words, sentences and line breaks | `41_system` |
| `libthai` | 0.1.30 | Thai word segmentation — where pango may wrap a line of Thai | `41_system` |
| `libunibreak` | 8.0 | Unicode line, word and grapheme breaking — where libass may wrap a subtitle | `41_system` |
| `libunistring` | 1.4.2 | Library for manipulating Unicode strings and C strings | `30_foundation` |
| `opencc` | 1.4.2 | Conversion between Traditional and Simplified Chinese | `41_system` |
| `oniguruma` | 6.9.10 | Regular expression library with Ruby, Perl and POSIX syntaxes over many character encodings | `41_system` |
| `snowball` | 3.1.1 | Snowball stemming compiler and libstemmer, the stemmers for 30 languages | `41_system` |
| `uchardet` | 0.0.8 | Guesses the character encoding of a text file — how mpv reads a subtitle that is not UTF-8 | `41_system` |
| `utf8proc` | 2.11.3 | Unicode normalization, case folding and grapheme segmentation library | `41_system` |
| `utfcpp` | 4.2.1 | Header-only C++ library for checking and converting UTF-8, UTF-16 and UTF-32 text | `41_system` |

Notes:

- `gettext`: `--with-included-libxml`, because libxml2's own closure reaches gettext and the system
  copy would be a `depends` cycle. `--with-xz` fixes the format of autopoint's archive, which
  otherwise follows whichever compressor the build root holds and then needs it wherever autopoint
  runs.
- `icu`: built with gcc (`CC=gcc CXX=g++`), since configure prefers clang. The recipe compiles the
  `zoneinfo64` resource with this release's genrb, so node's Temporal and ICU's Intl read one
  time-zone database version.
- `libgrapheme`: `config.mk`'s own `CFLAGS` and `LDFLAGS` beat the environment, so the tree's flags
  go on the make command line.
- `snowball`: the makefile assigns `CFLAGS`, so the tree's flags and `-fPIC` go on the make command
  line; the shared library needs `-fPIC` in every object.

### spelling

Spell checking, hyphenation, thesaurus and their dictionaries. 10 ports, under
`ports/core/spelling/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `aspell` | 0.60.8.2 | GNU Aspell spell checker, library and command | `41_system` |
| `aspell-en` | 2026.02.25.0 | English dictionaries for Aspell (en_US, en_GB, en_CA, en_AU) | `41_system` |
| `enchant` | 2.8.21 | Spell-checking library over several engines, built with its Hunspell and Aspell providers | `41_system` |
| `gspell` | 1.14.5 | Spell checking for GTK 3 text views and entries, over enchant | `43_toolkits` |
| `hunspell` | 1.7.4 | Spell checker and morphological analyser — the library LibreOffice, Firefox and enchant check through | `41_system` |
| `hunspell-en` | 26.8.0.3 | English spelling dictionaries, hyphenation patterns and thesaurus from the LibreOffice dictionaries release | `41_system` |
| `hyphen` | 2.8.9 | Hyphenation library for TeX-style pattern files | `41_system` |
| `libspelling` | 0.4.10 | Spell checking for GTK 4 text views and GtkSourceView 5, over enchant | `44_apps` |
| `mythes` | 1.2.6 | Thesaurus library and the index generator for its data files | `41_system` |
| `qtspell` | 1.0.2 | Spell checking for Qt text widgets, through Enchant | `43_toolkits` |

Notes:

- `aspell-en`: configure asks the installed `aspell` where dictionaries go and compiles the word
  lists with it, so aspell is in `depends`.
- `hyphen`: `hyph_DATA` is emptied for the build and the install: hunspell-en owns
  `/usr/share/hyphen/hyph_en_US.dic`.

### gtk

The GLib and GTK platform and GNOME/Xfce desktop-platform libraries: widgets, settings, bindings,
GIO plug-ins. 28 ports, under `ports/core/gtk/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `atkmm2.28` | 2.28.5 | C++ bindings for ATK, the 1.6 API (atkmm-1.6) that gtkmm 3 builds on | `42_graphics` |
| `blueprint-compiler` | 0.22.2 | Markup language and compiler for GTK 4 user interfaces | `43_toolkits` |
| `dconf` | 51.0 | Low-level configuration store — the GSettings backend, its D-Bus service and the dconf tool | `41_system` |
| `exo` | 4.20.0 | Xfce extension library for GTK 3 applications | `43_toolkits` |
| `glib` | 2.90.0 | Low-level libraries useful for providing data structure handling for C, portability wrappers and interfaces | `41_system` |
| `glib-introspection` | 2.90.0 | GLib's introspection data: the GLib, GObject, GModule, Gio and GIRepository GIR and typelib files | `41_system` |
| `glibmm` | 2.90.0 | C++ bindings for GLib and GIO, the 2.68 API (glibmm-2.68, giomm-2.68) that gtkmm 4 builds on | `41_system` |
| `glibmm2.66` | 2.66.10 | C++ bindings for GLib and GIO, the 2.4 API (glibmm-2.4, giomm-2.4) that gtkmm 3 builds on | `41_system` |
| `gobject-introspection` | 1.86.0 | GObject introspection: g-ir-scanner, g-ir-compiler, libgirepository-1.0 and the base GIR/typelib data (cairo, freetype2, fontconfig, libxml2, GL, DBus) | `41_system` |
| `gsettings-desktop-schemas` | 51.0 | GSettings schemas shared by GTK and GNOME applications: interface, fonts, proxy, privacy | `41_system` |
| `gtk3` | 3.24.52 | GTK 3 widget toolkit, with the Wayland and X11 backends | `43_toolkits` |
| `gtk4` | 4.24.0 | GTK 4 widget toolkit, with the Wayland and X11 backends, Vulkan and GStreamer media | `43_toolkits` |
| `gtkmm3` | 3.24.11 | C++ bindings for GTK 3 (gtkmm-3.0), with the atkmm and X11 APIs | `43_toolkits` |
| `gtkmm4` | 4.24.0 | C++ bindings for GTK 4 (gtkmm-4.0) | `43_toolkits` |
| `gtksourceview4` | 4.8.4 | Source code editing widget for GTK 3 — highlighting, completion, search | `43_toolkits` |
| `gtksourceview5` | 5.22.0 | Source code editing widget for GTK 4 — highlighting, completion, search | `43_toolkits` |
| `gvfs` | 1.62.0 | GIO virtual filesystem — sftp, smb, dav, trash, recent, MTP, camera and device volumes for GTK applications | `43_toolkits` |
| `libadwaita` | 1.10.0 | GNOME's building blocks for GTK 4 applications — adaptive widgets and styles | `43_toolkits` |
| `libgee` | 0.20.8 | GObject collection library — lists, maps, sets and queues for Vala and C | `41_system` |
| `libgudev` | 238 | GObject bindings for libudev (used by upower and NetworkManager) | `41_system` |
| `libhandy` | 1.8.3 | Adaptive widgets for GTK 3 applications | `43_toolkits` |
| `libpeas` | 1.38.1 | GObject plugin engine, 1.x API — Rhythmbox's plugin loader | `43_toolkits` |
| `libsigc++2` | 2.12.1 | Typesafe callback framework for C++, the 2.x API (sigc++-2.0) that gtkmm 3 builds on | `41_system` |
| `libsigc++3` | 3.6.0 | Typesafe callback framework for C++, the 3.x API (sigc++-3.0) that gtkmm 4 builds on | `41_system` |
| `libxfce4ui` | 4.20.2 | Xfce widget library, with its X11 and Wayland paths | `43_toolkits` |
| `libxfce4util` | 4.20.1 | Xfce utility library | `41_system` |
| `vte3` | 0.84.1 | Terminal emulator widget for GTK 3 and GTK 4 | `43_toolkits` |
| `xfconf` | 4.20.0 | Xfce configuration store — libxfconf and the xfconfd daemon | `41_system` |

Notes:

- `glib-introspection`: builds the glib source with the glib port's options and stays at glib's
  version, or its GIR and typelib files describe a library other than the one installed; it packages
  only those files, which the glib port leaves out.
- `gtk3`: every input method is compiled into libgtk (`-Dbuiltin_immodules=all`), because a loadable
  module is found only through `immodules.cache`, which no install step regenerates. The X11 backend
  stays on: it is the only one that starts the ATK bridge.
- `gtksourceview5`: font fallback for the space-drawing glyphs needs fontconfig and pangoft2 at
  build time; without them it is compiled out with no message.
- `libpeas`: stays on the 1.x API (libpeas-1.0) that Rhythmbox asks for, at 1.38 or later, because
  Rhythmbox's Python plugins refuse a libpeas that loads typelibs through girepository 1.0.
- `xfconf`: the GSettings backend stays unbuilt; installed under `gio/modules` it would offer every
  GSettings program an xfconf store beside dconf.

### qt6

The Qt 6 modules, and everything in `group = qt6`. 29 ports, under `ports/core/qt6/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `python3-pyside6` | 6.12.0 | PySide6 and Shiboken6, the official Qt 6 bindings for Python (FreeCAD's scripting GUI) | `43_toolkits` |
| `qt6-qt3d` | 6.12.0 | Qt 6 3D — scene graph, render, input, logic, animation and extras aspects, C++ and QML | `43_toolkits` |
| `qt6-qt5compat` | 6.12.0 | Qt 6 Core5Compat — QTextCodec, QRegExp and the Qt 5 graphical effects for code ported from Qt 5 | `43_toolkits` |
| `qt6-qtbase` | 6.12.0 | Qt 6 core, GUI, widgets, network, SQL, D-Bus and printing, with the Wayland and X11 platform plugins | `43_toolkits` |
| `qt6-qtcharts` | 6.12.0 | Qt 6 Charts — chart widgets and QML chart types | `43_toolkits` |
| `qt6-qtconnectivity` | 6.12.0 | Qt 6 Bluetooth and NFC — QtBluetooth over BlueZ and QtNfc over PC/SC | `43_toolkits` |
| `qt6-qtdeclarative` | 6.12.0 | Qt 6 QML and Qt Quick — the language, the engine, Qt Quick Controls and the qml tools | `43_toolkits` |
| `qt6-qtgraphs` | 6.12.0 | Qt 6 Graphs — 2D and 3D data visualisation for QML and widgets | `43_toolkits` |
| `qt6-qtimageformats` | 6.12.0 | Qt 6 image format plugins — TIFF, WebP, ICNS, TGA and WBMP | `43_toolkits` |
| `qt6-qtlanguageserver` | 6.12.0 | Qt 6 Language Server Protocol and JSON-RPC library, used by qmlls | `43_toolkits` |
| `qt6-qtlocation` | 6.12.0 | Qt 6 Location — maps, places and routing for QML | `43_toolkits` |
| `qt6-qtmqtt` | 6.12.0 | Qt 6 MQTT — MQTT 3.1, 3.1.1 and 5 client, for LabPlot's live data sources | `43_toolkits` |
| `qt6-qtmultimedia` | 6.12.0 | Qt 6 Multimedia — audio, video playback and capture through FFmpeg and PipeWire | `43_toolkits` |
| `qt6-qtnetworkauth` | 6.12.0 | Qt 6 Network Authorization — OAuth 1 and OAuth 2 clients | `43_toolkits` |
| `qt6-qtpositioning` | 6.12.0 | Qt 6 Positioning — position sources from GeoClue and NMEA | `43_toolkits` |
| `qt6-qtquick3d` | 6.12.0 | Qt 6 Quick 3D — 3D scenes in QML | `43_toolkits` |
| `qt6-qtremoteobjects` | 6.12.0 | Qt 6 Remote Objects — QObjects shared between processes | `43_toolkits` |
| `qt6-qtscxml` | 6.12.0 | Qt 6 SCXML and StateMachine — state charts compiled or interpreted | `43_toolkits` |
| `qt6-qtsensors` | 6.12.0 | Qt 6 Sensors — accelerometer, gyroscope and light sensors through iio-sensor-proxy (no sensor daemon is ported) | `43_toolkits` |
| `qt6-qtserialport` | 6.12.0 | Qt 6 SerialPort — serial and USB-serial device access | `43_toolkits` |
| `qt6-qtshadertools` | 6.12.0 | Qt 6 shader tools — qsb, the GLSL/HLSL/MSL/SPIR-V baker Qt Quick and Qt RHI shaders are compiled with | `43_toolkits` |
| `qt6-qtspeech` | 6.12.0 | Qt 6 TextToSpeech — speech synthesis through speech-dispatcher | `43_toolkits` |
| `qt6-qtsvg` | 6.12.0 | Qt 6 SVG rendering — QSvgRenderer, QSvgWidget and the SVG image and icon engine plugins | `43_toolkits` |
| `qt6-qttools` | 6.12.0 | Qt 6 tools — Designer, Linguist, Assistant, Qt Help, UiTools, lrelease, qdbus and qtdiag | `43_toolkits` |
| `qt6-qttranslations` | 6.12.0 | Qt 6 English catalogues: the plural forms of the Qt libraries and tools | `43_toolkits` |
| `qt6-qtwayland` | 6.12.0 | Qt 6 Wayland compositor library and the client decoration and shell plugins | `43_toolkits` |
| `qt6-qtwebchannel` | 6.12.0 | Qt 6 WebChannel — QObjects shared with HTML and JavaScript clients | `43_toolkits` |
| `qt6-qtwebengine` | 6.11.2 | Qt 6 WebEngine and Qt PDF — the Chromium web engine as Qt widgets and Qt Quick types | `43_toolkits` |
| `qt6-qtwebsockets` | 6.12.0 | Qt 6 WebSockets — RFC 6455 client and server | `43_toolkits` |

Notes:

- `python3-pyside6`: naming `MODULES` makes every binding in it required; probed, the set would gain
  or lose bindings with whichever Qt modules happened to be installed first. The install step writes
  a `PySide6Config.abi3.cmake` that names the build tree's typesystem and glue directories, so the
  build tree's copy, which names the installed ones, is installed over it. pyside-tools' deployment
  scripts are moved from `/usr/bin` into the package's `scripts` directory.
- `qt6-qt3d`: the model importer is Qt's bundled Assimp: Qt 3D asks for Assimp 5, and the assimp
  port's 6.x declares `SameMajorVersion`, so a system build finds nothing and drops the feature.
- `qt6-qtbase`: the `INSTALL_*` paths set here are read by every later Qt 6 module from qtbase's
  installed build internals, and no module passes its own; changing one here moves all of them.
- `qt6-qtmultimedia`: ALSA stays off: upstream makes it mutually exclusive with PulseAudio, which
  carries the audio to PipeWire.
- `qt6-qtwayland`: `TEST_dmabuf_client_buffer=ON` is required: configure runs that test before it
  has looked EGL up, so it fails however the headers stand, and the dmabuf client buffer is dropped.
- `qt6-qtwebengine`: `NINJAFLAGS` is the only job limit of the Chromium build inside Qt's; unset, it
  starts a job per core at 2–3 GB each. FFmpeg, libvpx and libxml2 stay Chromium's bundled copies:
  the ffmpeg port is a newer major than Chromium's media code accepts, VA-API does not build
  against a system libvpx, and Blink's XSLT code is written to an older libxml2 API.

### qt5

The Qt 5 modules, `group = qt5`. 11 ports, under `ports/core/qt5/`.

Qt 5.15 has no public security fixes for 2026, so a program builds against Qt 6 whenever its
release does. The programs below stay on Qt 5 because no release of theirs builds with Qt 6:

| Port | Why it stays on Qt 5 |
|---|---|
| `candle2` | 2.4 draws through `QGLWidget`, which Qt 6 removed, and there is no later release |
| `gnuradio` | gr-qtgui on the 3.10 line is Qt 5, PyQt5 and Qwt; the Qt 6 port is on an unreleased branch |
| `keepassxc` | Its Qt 6 build arrives with 2.8.0, which is released only as a beta |
| `librecad` | 2.2.1.5 accepts Qt 6 in its project files, but its code still uses `QRegExp`, `QTextCodec`, `QPrinter::PageSize` and `QDesktopWidget` |
| `lmms` | 1.2.2 is the last stable release; 1.3 is an alpha |
| `mumble` | The 1.5 series is Qt 5; Qt 6 arrives with 1.6, whose only release is a release candidate |
| `qsstv` | 9.5.8 builds against Qt 5 only |
| `quassel` | 0.14.0 is the last release, and its main branch is Qt 5 too; the Qt 6 port is an unmerged pull request |
| `smplayer` | Its Qt 6 build does not compile: `QTextStream::setCodec`, `QRegExp` and `QDesktopWidget` throughout |
| `sqlitebrowser` | 3.13.1 finds Qt 5 only |
| `vlc` | VLC 3's interface is Qt 5; the Qt 6 interface is VLC 4's, which is unreleased |
| `wsjtx` | 3.0.1 finds Qt 5 only |

`python3-pyqt5`, `qwt-qt5`, `qca-qt5` and `qt5ct` are the Qt 5 platform those programs and their
plugins build on. A Qt 5 port that links OpenSSL links the `openssl3` slot, as `qt5-qtbase` does
([A second version beside the first](../05-developer/writing-ports.md#a-second-version-beside-the-first)).

| Port | Version | Description | Phase |
|---|---|---|---|
| `qt5-qtbase` | 5.15.19 | Qt 5 core, GUI, widgets, network, SQL, D-Bus and printing, with the X11 platform plugin, for programs that build against Qt 5 only | `43_toolkits` |
| `qt5-qtdeclarative` | 5.15.19 | Qt 5 QML and Qt Quick — the declarative UI language, its JavaScript engine and the Quick scene graph | `43_toolkits` |
| `qt5-qtgraphicaleffects` | 5.15.19 | Qt 5 Quick graphical effects — the blur, shadow, glow and colour QML types | `43_toolkits` |
| `qt5-qtmultimedia` | 5.15.19 | Qt 5 multimedia — audio, video and camera through GStreamer, PulseAudio and ALSA | `43_toolkits` |
| `qt5-qtquickcontrols2` | 5.15.19 | Qt 5 Quick Controls 2 — the QML control set with its Fusion, Material, Universal and Imagine styles | `43_toolkits` |
| `qt5-qtserialport` | 5.15.19 | Qt 5 serial port access, with port enumeration through udev | `43_toolkits` |
| `qt5-qtsvg` | 5.15.19 | Qt 5 SVG rendering — QSvgRenderer, QSvgWidget and the SVG image and icon engine plugins | `43_toolkits` |
| `qt5-qttools` | 5.15.19 | Qt 5 tools — Designer, Linguist with lrelease and lupdate, Assistant, qdbus and the Qt help and UI tools libraries | `43_toolkits` |
| `qt5-qtwayland` | 5.15.19 | Qt 5 Wayland platform plugin, so Qt 5 programs run as native Wayland clients, and the QtWaylandCompositor module | `43_toolkits` |
| `qt5-qtwebsockets` | 5.15.19 | Qt 5 WebSocket client and server, from C++ and QML | `43_toolkits` |
| `qt5-qtx11extras` | 5.15.19 | Qt 5 X11 extras — QX11Info, which Qt 5 programs use to reach the X11 display under Xwayland | `43_toolkits` |

Notes:

- `qt5-qtbase`: `qt5-base-cflags.patch` makes qmake read `CFLAGS`, `CXXFLAGS` and `LDFLAGS` from the
  environment, here and in every later Qt 5 module and qmake program; without it no reproducibility
  flag reaches a Qt 5 binary. GSSAPI is off because krb5 links OpenSSL 4. The OpenSSL 3 headers are
  generated with the `openssl3` port's own options: a different option set defines different
  `OPENSSL_NO_*` macros, and QtNetwork then calls what the library lacks. Every tool gets a `-qt5`
  name in `/usr/bin`, and `qmake` keeps its plain name too, since Qt 6 installs only `qmake6`.
- `qt5-qtwayland`: the `wayland.xml` patch must describe the interface versions of the system
  `libwayland-client`; a wayland port newer than the patch stops the build on a protocol that
  disagrees with its headers.

### qt-extra

Third-party and KDE-hosted Qt add-on libraries that are not in KF6. 21 ports, under
`ports/core/qt-extra/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `cpp-utilities` | 5.37.0 | Martchus's C++ utility library: argument parsing, conversions, I/O and the CMake modules his programs build with | `41_system` |
| `kcolorpicker` | 0.3.1 | Qt colour picker button with a palette popup, used by kImageAnnotator | `43_toolkits` |
| `kddockwidgets` | 2.4.1 | KDAB dock widget framework for Qt 6, with the Widgets and Qt Quick front ends | `43_toolkits` |
| `kdsingleapplication` | 1.2.1 | KDAB helper class for single-instance Qt 6 applications | `43_toolkits` |
| `kimageannotator` | 0.7.2 | Qt image annotation widgets — arrows, text, blur and stickers — used by Gwenview | `43_toolkits` |
| `kirigami-addons` | 1.14.2 | Additional Kirigami components: form cards, dialogs and pickers | `43_toolkits` |
| `kqtquickcharts` | 26.08.1 | KDE QtQuick chart components — the line and bar charts in KTouch's statistics | `43_toolkits` |
| `libdbusmenu-lxqt` | 0.4.0 | DBusMenu for Qt 6 — exports and imports the menus of tray icons over D-Bus | `43_toolkits` |
| `libdbusmenu-qt5` | 0.9.3.20160218 | DBusMenu for Qt 5 — exports and imports the menus of tray icons over D-Bus | `43_toolkits` |
| `mpvqt` | 1.2.0 | Qt Quick item that renders libmpv, used by Haruna | `43_toolkits` |
| `phonon` | 4.12.0 | KDE multimedia API for Qt 6; playback goes through a backend port | `43_toolkits` |
| `phonon-backend-vlc` | 0.12.0 | Phonon backend that plays through libvlc | `43_toolkits` |
| `polkit-qt6` | 0.201.1 | Qt 6 wrapper around the polkit client and agent libraries | `43_toolkits` |
| `python3-qscintilla` | 2.14.1 | QScintilla for Python as PyQt6.Qsci, for QGIS's console and script editor | `43_toolkits` |
| `qscintilla` | 2.14.1 | The Scintilla editor widget for Qt 6, in Octave and QGIS | `43_toolkits` |
| `qtforkawesome` | 0.3.4 | The Fork Awesome icon font as a Qt 6 library, icon engine plugin and Qt Quick image provider | `43_toolkits` |
| `qtutilities` | 6.22.2 | Martchus's Qt 6 utility library: settings dialogs, about dialog, notifications and resources | `43_toolkits` |
| `quazip` | 1.7.2 | Qt 6 C++ wrapper for zip archives | `43_toolkits` |
| `qwt` | 6.3.0 | Qt widgets for plots, dials and scales, for QGIS | `43_toolkits` |
| `sonnet-qt5` | 5.116.0 | KDE Frameworks 5 Sonnet — spell checking for Qt 5 widgets, through Hunspell and Aspell | `43_toolkits` |
| `qwt-qt5` | 6.3.0 | Qwt for Qt 5 — plot, dial and scale widgets for GNU Radio's gr-qtgui and Qt 5 SDR tools | `43_toolkits` |

Notes:

- `python3-qscintilla`: built from the qscintilla tarball's `Python` directory; with no `src` beside
  it, the project links the installed `libqscintilla2_qt6` instead of compiling a second copy.
- `libdbusmenu-qt5`, `sonnet-qt5`: Quassel's tray-icon menu and its input-line spell checking.
  `libdbusmenu-qt5` is the last snapshot upstream published, from Ubuntu's archive. `sonnet-qt5` is
  the last KDE Frameworks 5 release, built with KDE Frameworks 6's `extra-cmake-modules`
  (`QT_MAJOR_VERSION=5`); it shares no path with `sonnet`, whose libraries, plugins and trigram
  tool carry `6` or `kf6` in their names.
- `qwt`: `qwt` and `qwt-qt5` build one tarball against Qt 6 and Qt 5. The libraries are `libqwt-qt6`
  and `libqwt-qt5`, so both install side by side. `qwt-qt5`'s fourth patch puts its headers in
  `/usr/include/qwt-qt5`, the first directory GNU Radio's FindQwt tries; `/usr/include/qwt` is the
  Qt 6 build's, and its HTML reference is removed because `qwt` installs the same pages at the same
  path.

### kf6

KDE Frameworks 6: every port whose homepage is under invent.kde.org/frameworks, whatever it does. 67
ports, under `ports/core/kf6/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `attica` | 6.30.0 | KDE Frameworks: Open Collaboration Services client library | `43_toolkits` |
| `baloo` | 6.30.0 | KDE Frameworks: file search and tagging library, without the indexer service | `43_toolkits` |
| `bluez-qt` | 6.30.0 | KDE Frameworks: Qt wrapper for the BlueZ 5 D-Bus API | `43_toolkits` |
| `breeze-icons` | 6.30.0 | Breeze icon theme, light and dark, and its Qt resource library | `43_toolkits` |
| `extra-cmake-modules` | 6.30.0 | Extra modules and scripts for CMake (KDE) | `41_system` |
| `frameworkintegration` | 6.30.0 | KDE Frameworks: platform integration plugins and KPackage install handlers | `43_toolkits` |
| `karchive` | 6.30.0 | KDE Frameworks: reading and writing compressed archives | `43_toolkits` |
| `kauth` | 6.30.0 | KDE Frameworks: execute actions as a privileged user through polkit | `43_toolkits` |
| `kbookmarks` | 6.30.0 | KDE Frameworks: bookmark storage and menus | `43_toolkits` |
| `kcalendarcore` | 6.30.0 | KDE Frameworks: iCalendar data model and parser | `43_toolkits` |
| `kcmutils` | 6.30.0 | KDE Frameworks: configuration modules for widget and QML settings pages | `43_toolkits` |
| `kcodecs` | 6.30.0 | KDE Frameworks: string encodings and charset detection | `43_toolkits` |
| `kcolorscheme` | 6.30.0 | KDE Frameworks: colour-scheme loading and roles | `43_toolkits` |
| `kcompletion` | 6.30.0 | KDE Frameworks: text completion helpers and widgets | `43_toolkits` |
| `kconfig` | 6.30.0 | KDE Frameworks: persistent application configuration | `43_toolkits` |
| `kconfigwidgets` | 6.30.0 | KDE Frameworks: widgets for configuration dialogs | `43_toolkits` |
| `kcontacts` | 6.30.0 | KDE Frameworks: vCard address book data model | `43_toolkits` |
| `kcoreaddons` | 6.30.0 | KDE Frameworks: core non-GUI utilities, plugins and jobs | `43_toolkits` |
| `kcrash` | 6.30.0 | KDE Frameworks: crash handler that restarts or reports a crashed application | `43_toolkits` |
| `kdbusaddons` | 6.30.0 | KDE Frameworks: convenience classes for Qt D-Bus | `43_toolkits` |
| `kdeclarative` | 6.30.0 | KDE Frameworks: QML integration and controls | `43_toolkits` |
| `kded` | 6.30.0 | KDE Frameworks: kded6, the daemon that hosts session modules | `43_toolkits` |
| `kdesu` | 6.30.0 | KDE Frameworks: running programs as another user through sudo | `43_toolkits` |
| `kdnssd` | 6.30.0 | KDE Frameworks: DNS-SD service discovery through Avahi | `43_toolkits` |
| `kdoctools` | 6.30.0 | KDE Frameworks: DocBook to HTML and manual page tools (meinproc6) | `43_toolkits` |
| `kfilemetadata` | 6.30.0 | KDE Frameworks: file metadata and text extraction | `43_toolkits` |
| `kglobalaccel` | 6.30.0 | KDE Frameworks: global keyboard shortcuts through kglobalacceld | `43_toolkits` |
| `kguiaddons` | 6.30.0 | KDE Frameworks: colours, fonts, clipboard and key helpers for Qt GUIs | `43_toolkits` |
| `kholidays` | 6.30.0 | KDE Frameworks: public holiday, season and astronomical calendars | `43_toolkits` |
| `ki18n` | 6.30.0 | KDE Frameworks: gettext-based translation for Qt programs | `43_toolkits` |
| `kiconthemes` | 6.30.0 | KDE Frameworks: icon theme lookup and icon dialogs | `43_toolkits` |
| `kidletime` | 6.30.0 | KDE Frameworks: user idle-time reporting | `43_toolkits` |
| `kimageformats` | 6.30.0 | KDE Frameworks: QImage plugins for AVIF, HEIF, JPEG XL, JPEG 2000, EXR, RAW, PSD, XCF, Krita and more | `43_toolkits` |
| `kio` | 6.30.0 | KDE Frameworks: network-transparent file access, workers and file dialogs | `43_toolkits` |
| `kirigami` | 6.30.0 | KDE Frameworks: QtQuick components for convergent applications | `43_toolkits` |
| `kitemmodels` | 6.30.0 | KDE Frameworks: proxy and helper item models for Qt | `43_toolkits` |
| `kitemviews` | 6.30.0 | KDE Frameworks: widget add-ons for Qt item views | `43_toolkits` |
| `kjobwidgets` | 6.30.0 | KDE Frameworks: widgets that track the progress of KJob tasks | `43_toolkits` |
| `kmime` | 6.30.0 | KDE Frameworks: MIME message parsing and assembly | `43_toolkits` |
| `knewstuff` | 6.30.0 | KDE Frameworks: downloading and installing add-on content | `43_toolkits` |
| `knotifications` | 6.30.0 | KDE Frameworks: desktop notifications through the freedesktop D-Bus interface | `43_toolkits` |
| `knotifyconfig` | 6.30.0 | KDE Frameworks: configuration dialog for desktop notifications | `43_toolkits` |
| `kpackage` | 6.30.0 | KDE Frameworks: loading and installing non-binary content packages | `43_toolkits` |
| `kparts` | 6.30.0 | KDE Frameworks: document-centric embeddable components | `43_toolkits` |
| `kpeople` | 6.30.0 | KDE Frameworks: a unified address book of contacts | `43_toolkits` |
| `kplotting` | 6.30.0 | KDE Frameworks: a plotting widget | `43_toolkits` |
| `kpty` | 6.30.0 | KDE Frameworks: pseudo-terminal devices for terminal emulators | `43_toolkits` |
| `kquickcharts` | 6.30.0 | KDE Frameworks: GPU-drawn charts for QtQuick | `43_toolkits` |
| `kservice` | 6.30.0 | KDE Frameworks: desktop entry and plugin lookup (kbuildsycoca6) | `43_toolkits` |
| `kstatusnotifieritem` | 6.30.0 | KDE Frameworks: StatusNotifierItem tray icons over D-Bus | `43_toolkits` |
| `ktexteditor` | 6.30.0 | KDE Frameworks: the KatePart text editor component | `43_toolkits` |
| `ktexttemplate` | 6.30.0 | KDE Frameworks: text template engine, for KDevelop's templates | `43_toolkits` |
| `ktextwidgets` | 6.30.0 | KDE Frameworks: rich text editing widgets with spell checking and speech | `43_toolkits` |
| `kunitconversion` | 6.30.0 | KDE Frameworks: unit conversion | `43_toolkits` |
| `kwallet` | 6.30.0 | KDE Frameworks: the KWallet client API and kwalletd6, which keeps wallets in the Secret Service | `43_toolkits` |
| `kwidgetsaddons` | 6.30.0 | KDE Frameworks: add-on widgets and classes for Qt Widgets | `43_toolkits` |
| `kwindowsystem` | 6.30.0 | KDE Frameworks: access to the windowing system, Wayland and X11 | `43_toolkits` |
| `kxmlgui` | 6.30.0 | KDE Frameworks: XML-described menus, toolbars and shortcut editors | `43_toolkits` |
| `modemmanager-qt` | 6.30.0 | KDE Frameworks: Qt wrapper for the ModemManager D-Bus API | `43_toolkits` |
| `networkmanager-qt` | 6.30.0 | KDE Frameworks: Qt wrapper for the NetworkManager D-Bus API | `43_toolkits` |
| `prison` | 6.30.0 | KDE Frameworks: barcode generation and scanning | `43_toolkits` |
| `purpose` | 6.30.0 | KDE Frameworks: the share menu and its local share targets | `43_toolkits` |
| `qqc2-desktop-style` | 6.30.0 | KDE Frameworks: QtQuick Controls style that follows the desktop palette and icons | `43_toolkits` |
| `solid` | 6.30.0 | KDE Frameworks: hardware discovery through udev, UDisks2 and UPower | `43_toolkits` |
| `sonnet` | 6.30.0 | KDE Frameworks: spell checking through Hunspell and Aspell | `43_toolkits` |
| `syntax-highlighting` | 6.30.0 | KDE Frameworks: syntax highlighting engine for structured text | `43_toolkits` |
| `threadweaver` | 6.30.0 | KDE Frameworks: high-level job-based multithreading | `43_toolkits` |

Notes:

- `kdoctools`: `kdoctools_install` installs every translated manual page and handbook whatever
  `KF_SKIP_PO_PROCESSING` says, so the build keeps only the English ones; kpackage and kservice
  prune theirs the same way.
- `ktexteditor`: saving a file the user cannot write goes through the KAuth helper, which polkit
  refuses unless `50-kdos.rules` grants it.
- `kwallet`: `/etc/xdg/kwalletrc` sets `[KSecretD] Enabled=false`. With ksecretd not built,
  kwalletd6 otherwise sends to a name nothing owns, and every wallet fails to open.
- `purpose`: the upload share targets (imgur, YouTube, Nextcloud, Review Board, Phabricator) and
  Bluetooth are deleted after the install, and a target whose plugin is not found fails the build,
  so a renamed plugin cannot ship its target silently.

### kde

KDE Plasma and Gear platform pieces outside Frameworks that have no other domain: KIO workers,
thumbnailers, Plasma integration, KDE Wayland glue. 9 ports, under `ports/core/kde/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `baloo-widgets` | 26.08.1 | File metadata panel, tag and rating editors used by Dolphin | `43_toolkits` |
| `ffmpegthumbs` | 26.08.1 | KIO thumbnailer for video files, through FFmpeg | `43_toolkits` |
| `kdegraphics-mobipocket` | 26.08.1 | Mobipocket (.mobi, .prc, .azw) reader library Okular and the thumbnailers open e-books through | `43_toolkits` |
| `kdegraphics-thumbnailers` | 26.08.1 | KIO thumbnailers for PostScript, PDF, Blender and camera raw files | `43_toolkits` |
| `kio-extras` | 26.08.1 | Extra KIO workers and thumbnailers: archives, man pages, MTP and iOS devices, file previews | `43_toolkits` |
| `kwayland` | 6.7.5 | Qt-style client library for the Wayland and Plasma Wayland protocols | `43_toolkits` |
| `libksysguard` | 6.7.5 | Process list, sensors and system statistics libraries, for KDevelop's attach to process | `43_toolkits` |
| `plasma-integration` | 6.7.5 | Qt platform theme that gives every Qt 6 and KF6 application the KDE palette, fonts, icons and file dialogs | `43_toolkits` |
| `plasma-wayland-protocols` | 1.22.0 | KDE Plasma Wayland protocol definitions | `43_toolkits` |

Notes:

- `kio-extras`: the network workers (smb, sftp, nfs) are off by refusing their libraries, and the
  KCMs that configure them are off too. Every local worker's library is required, because a
  missing one drops its worker with only a line in the configure summary.
- `libksysguard`: the per-application network plugin and the KAuth helper stay off. The first
  needs `CAP_NET_RAW` set by `setcap`; the second is a root D-Bus service for acting on other
  users' processes.
- `plasma-wayland-protocols`: qt6-qtbase stays in `depends` though nothing is compiled: ECM's
  KDEInstallDirs asks Qt's `qtpaths` for its prefix and stops without it.

### toolkits

Other GUI and TUI toolkits for applications. 4 ports, under `ports/core/toolkits/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `fltk` | 1.4.5 | The Fast Light Toolkit — a small C++ GUI library, Wayland first with an X11 fallback, and FLUID | `43_toolkits` |
| `motif` | 2.5.2 | The Motif widget toolkit (libXm, libMrm, uil) for X11 programs run under Xwayland | `43_toolkits` |
| `stfl` | 0.24 | Curses-based structured terminal forms library | `41_system` |
| `wxwidgets` | 3.2.9 | wxWidgets on GTK 3 — the native-widget C++ toolkit under KiCad, PrusaSlicer, FileZilla, VeraCrypt and wxPython | `43_toolkits` |

Notes:

- `fltk`: with both backends built, a program picks Wayland whenever a Wayland display is reachable
  and X11 otherwise; `FLTK_BACKEND=x11` forces Xwayland. The X11 half needs cairo with Xlib and
  pango with Xft, or configure stops.
- `motif`: `mwm` and `xmbind` are not shipped: X windows are managed by kdos-comp's Xwayland window
  manager. `--with-mwmrcdir` keeps mwm's rc file off `/etc/X11`, which must not exist.
- `stfl`: the build includes `<ncursesw/ncurses.h>`, so it gets a directory with that one link into
  the flat widec headers, and `-D_XOPEN_SOURCE_EXTENDED`, without which the wide-character calls
  compile against the narrow API. `CFLAGS` on the command line replaces the Makefile's, so it
  carries `-fPIC`. The recipe makes the `libstfl.so.0` link; without it newsboat links and then
  does not start.
- `wxwidgets`: a failed probe turns a backend off with only a warning, so every optional backend is
  named on and the checks after the install fail the build when one is missing.

### audio-io

The sound stack: ALSA, PipeWire and WirePlumber, PulseAudio client, and portable audio I/O APIs. 15
ports, under `ports/core/audio-io/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `alsa-lib` | 1.2.16.1 | ALSA library used by programs (including ALSA Utilities) requiring access to the ALSA sound interface | `41_system` |
| `alsa-topology-conf` | 1.2.5.1 | ALSA topology configuration files | `41_system` |
| `alsa-ucm-conf` | 1.2.16.1 | ALSA Use Case Manager configuration (and topologies) | `41_system` |
| `alsa-utils` | 1.2.16 | Various utilities for controlling sound card | `41_system` |
| `libao` | 1.2.2 | Audio output library over ALSA and PulseAudio | `41_system` |
| `libpulse` | 17.0 | PulseAudio client libraries and pactl/pacat — the server is pipewire-pulse | `41_system` |
| `libsoundio` | 2.0.0 | Cross-backend real-time audio input and output, through PulseAudio or ALSA | `41_system` |
| `openal-soft` | 1.25.2 | OpenAL 3D positional audio for games, played through PipeWire, Pulse or ALSA | `42_graphics` |
| `pipewire` | 1.6.9 | Audio and video stream router (replaces PulseAudio; carries Wayland screencast) | `42_graphics` |
| `portaudio` | 19.7.0 | Portable real-time audio I/O library, with its C++ binding, over ALSA and JACK | `42_graphics` |
| `pulseaudio-qt` | 1.9.0 | Qt 6 bindings for the PulseAudio client API, served here by pipewire-pulse | `43_toolkits` |
| `rtaudio` | 6.0.1 | C++ real-time audio I/O classes over ALSA and PulseAudio | `41_system` |
| `shairplay` | 0.9.0.git20180824 | AirPlay (RAOP) audio receiver library — Kodi's AirTunes target | `41_system` |
| `wiremix` | 0.11.0 | PipeWire mixer and routing panel for the terminal | `42_graphics` |
| `wireplumber` | 0.5.17 | PipeWire's session manager — device policy, routing and Bluetooth profiles | `42_graphics` |

Notes:

- `libpulse`: the client half only. The server is pipewire-pulse, and a second daemon would fight
  it for the socket. `-lintl` rides `LDFLAGS`: the probe finds musl's `dgettext` while gettext's
  `libintl.h` renames every call.
- `pipewire`: `-Dpipewire-v4l2` stays off, because its LD_PRELOAD shim calls `openat64` and
  `mmap64`, which musl does not export, and every process under it faults. The rlimits file is not
  installed: kdos-getty raises the realtime limits, and the `@pipewire` group it matches does not
  exist.
- `portaudio`: built serially; the library and the C++ binding race under a parallel make.
- `wiremix`: `crt-static` is off so the bindgen build script can dlopen libclang.
- `wireplumber`: the Lua version is named, `5.5`. Left to `auto` it settles for any older Lua it
  finds.

### audio-codecs

Audio codecs, containers and tagging, including the Bluetooth audio codecs. 28 ports, under
`ports/core/audio-codecs/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `a52dec` | 0.8.0 | liba52, the ATSC A/52 (AC-3) audio decoder library, and the a52dec tool | `41_system` |
| `audiofile` | 0.3.6 | SGI's audio file library: reads and writes AIFF, WAVE, NeXT/Sun, IRCAM, AVR, CAF and FLAC through one API | `41_system` |
| `codec2` | 1.2.0 | The open low-bitrate speech codec and FreeDV modem library, with freedv_tx and freedv_rx | `41_system` |
| `faad2` | 2.11.3 | MPEG-2 and MPEG-4 AAC decoder: libfaad and the faad command | `41_system` |
| `fdk-aac` | 2.0.3 | Fraunhofer AAC codec, used for AAC over Bluetooth | `41_system` |
| `flac` | 1.5.0 | Free Lossless Audio Codec library and tools | `41_system` |
| `gsm` | 1.0.24 | GSM 06.10 lossy speech codec — libgsm and the toast, untoast and tcat tools | `41_system` |
| `id3lib` | 3.8.3 | ID3v1 and ID3v2 tag library, with the id3tag, id3info, id3convert and id3cp commands | `41_system` |
| `lame` | 4.0 | LAME MP3 encoder | `41_system` |
| `ldacbt` | 2.0.2.6 | Sony LDAC encoder, for the Bluetooth headsets that negotiate it | `41_system` |
| `libfreeaptx` | 0.2.2 | Free aptX and aptX HD codec, for Bluetooth headsets that speak them | `41_system` |
| `libid3tag` | 0.16.4 | ID3 tag reading and writing library | `41_system` |
| `liblc3` | 1.1.3 | Low Complexity Communication Codec, the mandatory codec of Bluetooth LE Audio | `41_system` |
| `libmad` | 0.16.4 | MPEG audio decoder library | `41_system` |
| `libogg` | 1.3.6 | Ogg container library | `41_system` |
| `libopusenc` | 0.3 | The high-level Opus encoder opusenc is built on | `41_system` |
| `libsndfile` | 1.2.2 | Library for reading and writing files containing sampled sound (WAV, AIFF, FLAC, etc.) | `41_system` |
| `libvorbis` | 1.3.7 | Vorbis audio codec | `41_system` |
| `mpg123` | 1.33.7 | MPEG audio decoder — libmpg123, libout123, libsyn123 and the mpg123 and out123 commands | `41_system` |
| `opus` | 1.6.1 | Opus audio codec for voice and music | `41_system` |
| `opus-tools` | 0.2 | Encode, decode and inspect Opus from a prompt | `41_system` |
| `opusfile` | 0.12 | Decode and seek .opus files — the read half opus-tools needs | `41_system` |
| `sbc` | 2.2 | Bluetooth Sub-Band Codec library (mandatory codec for A2DP audio) | `41_system` |
| `speex` | 1.2.1 | Speex speech codec library, with the speexenc and speexdec tools | `41_system` |
| `taglib` | 2.3.2 | Library for reading and editing audio file metadata | `41_system` |
| `twolame` | 0.4.0 | MPEG Audio Layer 2 (MP2) encoder library and command-line encoder | `41_system` |
| `vorbis-tools` | 1.4.3 | oggenc, oggdec, ogginfo and vorbiscomment | `41_system` |
| `wavpack` | 5.9.0 | WavPack hybrid lossless audio codec — library and the wavpack, wvunpack, wvgain and wvtag commands | `41_system` |

Notes:

- `audiofile`: Debian's patch series is applied by name, because a glob sorts `0013` before `01`.
- `codec2`: the command-line tools are installed by hand, so they are linked with the install rpath
  (`CMAKE_BUILD_WITH_INSTALL_RPATH`); a build-tree binary keeps a RUNPATH into the build directory.
- `flac`: with LTO, `AR`, `NM` and `RANLIB` are the `gcc-` wrappers: plain `ar` leaves the
  bytecode-only internal archives without an index and the link fails.
- `gsm`: the makefile's `CFLAGS` overrides the environment, so the tree's flags go in through
  `CCFLAGS`. The shared library is linked by hand from the `-fPIC` archive.
- `id3lib`: the iomanip.h patch edits `configure.in`, so the build system is regenerated, without
  `autoheader`: it would rewrite the shipped `config.h.in` from the obsolete `acconfig.h`.
- `lame`: the static library is built and then removed: the frontend calls `hip_*` functions the
  shared library does not export, and links against the archive.
- `ldacbt`: the source is the release asset, which carries the libldac submodule; GitHub's tag
  archive leaves it empty.
- `libfreeaptx`: `LIBDIR` is relative to `PREFIX`; an absolute `/usr/lib` installs into
  `/usr/usr/lib`.
- `liblc3`: `--default-library=shared` stays: pipewire's bluez5 plugin links liblc3 through
  pkg-config, and a static-only build satisfies the probe with a plugin that cannot link.
- `libsndfile`: `ENABLE_EXTERNAL_LIBS` is what gives FLAC, Ogg, Vorbis and Opus, and MP3 needs both
  lame and mpg123. Each codec is required through `CMAKE_REQUIRE_FIND_PACKAGE_*`, because CMake
  drops a missing one silently.
- `opus-tools`: libpcap is linked whenever it is found, with no switch, and gives `opusrtp
  --extract` its reader; it is in `depends` so the feature does not follow build order.
- `speex`: configure drops speexenc's denoiser and Ogg I/O without an error when speexdsp or libogg
  is missing, so both are in `depends`.
- `twolame`: configure builds the `twolame` command only when it finds libsndfile, and warns rather
  than fails without it, so libsndfile is in `depends`.
- `vorbis-tools`: FLAC input and ogg123's libao, curl and Opus support rest on `depends`:
  configure only warns and drops the feature when a library is missing.

### audio-dsp

Audio DSP, resampling, analysis, spatial audio, and the LV2/LADSPA/Vamp plug-in APIs with their RDF
libraries. 27 ports, under `ports/core/audio-dsp/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `aubio` | 0.4.9 | Audio labelling library — onset, pitch, beat and tempo detection | `41_system` |
| `chromaprint` | 1.6.1 | AcoustID audio fingerprinting library and the fpcalc tool | `42_graphics` |
| `iir1` | 1.10.0 | Realtime C++ IIR filter library (Butterworth, Chebyshev, RBJ) | `41_system` |
| `ladspa` | 1.17 | LADSPA audio plugin API header, example plugins and the analyseplugin, applyplugin and listplugins tools | `41_system` |
| `libbs2b` | 3.1.0 | Bauer stereophonic-to-binaural crossfeed library, with the bs2bconvert and bs2bstream commands | `41_system` |
| `libebur128` | 1.2.6 | EBU R 128 loudness measurement library | `41_system` |
| `libmysofa` | 1.3.5 | Reader for AES SOFA files — head-related transfer functions for spatial audio | `41_system` |
| `libsamplerate` | 0.2.2 | Secret Rabbit Code — audio sample rate converter library | `41_system` |
| `libsbsms` | 2.3.0 | Subband sinusoidal modeling time stretch and pitch shift library | `41_system` |
| `libspatialaudio` | 0.4.1 | Ambisonic and object audio rendering to speakers or headphones — the spatial audio behind MLT and VLC | `41_system` |
| `lilv` | 0.28.0 | LV2 plugin host library — discovers, loads and instantiates LV2 plugins | `41_system` |
| `lrdf` | 0.6.1 | RDF metadata library for LADSPA plugins | `41_system` |
| `lv2` | 1.18.10 | LV2 audio plugin standard — the specification headers and bundle data | `41_system` |
| `rnnoise` | 0.2 | Recurrent neural network noise suppression for speech, with its trained model | `41_system` |
| `rubberband` | 4.0.0 | Audio time-stretching and pitch-shifting library | `41_system` |
| `serd` | 0.32.10 | Turtle and NTriples RDF reader and writer — what LV2 plugin data is parsed with | `41_system` |
| `sord` | 0.16.22 | In-memory RDF quad store over serd, used by lilv to query LV2 plugin data | `41_system` |
| `soundtouch` | 2.4.1 | Tempo, pitch and playback-rate changer for audio streams | `41_system` |
| `sox` | 14.4.2 | Audio conversion, resampling, filtering and analysis from the command line | `41_system` |
| `soxr` | 0.1.3 | SoX Resampler library — one-dimensional sample-rate conversion | `41_system` |
| `speexdsp` | 1.2.1 | Resampling, echo cancellation and jitter buffering — wireshark decodes RTP with it | `41_system` |
| `sratom` | 0.6.22 | Serialises LV2 atoms to and from RDF | `41_system` |
| `stk` | 5.0.1 | The Synthesis ToolKit — C++ physical-model and signal-processing instruments, with their rawwave samples | `41_system` |
| `suil` | 0.10.26 | Embeds LV2 plugin user interfaces in GTK 3, Qt 6 and X11 hosts | `43_toolkits` |
| `vamp-sdk` | 2.10.0 | Vamp audio analysis plugin SDK, host library and the vamp-simple-host tool | `41_system` |
| `webrtc-audio-processing` | 2.1 | WebRTC's audio processing module — echo cancellation, noise suppression, gain control | `41_system` |
| `zix` | 0.8.2 | C data structures and portability layer under serd, sord and lilv | `41_system` |

Notes:

- `aubio`: the build runs the separate waf release, because the bundled waf imports Python's
  removed `imp` module. The feature switches go to `configure`, `build` and `install` alike; waf
  reverts a switch given only to `configure`.
- `libbs2b`: the shipped `configure` is used as it is; regenerating it fails on `dist-lzma`, an
  option this automake does not have.
- `rnnoise`: the release tarball is the source because it carries the trained weights; a git
  checkout fetches them from a model server.
- `stk`: the autoconf build, not CMake. Only it gives the library a versioned soname and compiles
  in `RAWWAVE_PATH`, without which every instrument plays silence.
- `vamp-sdk`: built serially; the makefile races itself under a parallel make.
- `webrtc-audio-processing`: `--wrap-mode=nofallback` keeps meson from downloading abseil; the
  library must link the system `abseil-cpp`, whose ABI every other consumer sees.

### music-libs

Music-format libraries: tracker modules, chip emulation, MIDI, OSC, synthesis and soundfonts. 18
ports, under `ports/core/music-libs/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `adplug` | 2.4 | AdLib/OPL2 sound player library for DOS-era game and tracker music, with the adplugdb database tool | `41_system` |
| `fluidr3-gm-sf3` | 4.7.5 | FluidR3 Mono General MIDI SoundFont, compressed — the default instrument set for fluidsynth and every MIDI player | `41_system` |
| `fluidsynth` | 2.6.1 | SoundFont MIDI synthesiser | `42_graphics` |
| `libbinio` | 1.5 | Binary I/O stream class library, the file layer AdPlug reads through | `41_system` |
| `libgig` | 4.6.0 | Gigasampler, DLS, SoundFont 2 and KORG sample-library file access, with the gigextract and dlsdump tools | `41_system` |
| `libgme` | 0.6.5 | Play video-game console music: NES, SNES, Game Boy, Genesis, PC Engine and more | `41_system` |
| `liblo` | 0.36 | Open Sound Control implementation — OSC messages over UDP and TCP | `41_system` |
| `libmikmod` | 3.3.14 | Tracker module player library — S3M, XM, IT, MOD | `41_system` |
| `libmodplug` | 0.8.9.0 | Play tracker modules: MOD, S3M, XM, IT and more, the ModPlug engine | `41_system` |
| `libmt32emu` | 2.8.3 | Roland MT-32, CM-32L and LAPC-I synthesiser emulation (needs the original control and PCM ROMs) | `41_system` |
| `libmusicbrainz5` | 5.1.0 | MusicBrainz client library — disc IDs to artist, album and track metadata | `41_system` |
| `libopenmpt` | 0.8.9 | Tracker module playback library (MOD, S3M, XM, IT, MPTM and more) and openmpt123 | `41_system` |
| `libresidfp` | 1.2.2 | Cycle-exact MOS 6581/8580 SID chip emulation library, the reSIDfp engine libsidplayfp plays through | `41_system` |
| `libsidplayfp` | 3.1.1 | C64 SID music player library — libsidplayfp and libstilview, playing through reSIDfp | `41_system` |
| `libxmp` | 4.7.3 | Play tracker modules: MOD, S3M, XM, IT and some ninety other formats | `41_system` |
| `portmidi` | 2.0.8 | Portable real-time MIDI input and output library over ALSA sequencer | `41_system` |
| `portsmf` | 239 | Standard MIDI File and Allegro score reader and writer library | `41_system` |
| `rtmidi` | 6.0.0 | C++ real-time MIDI input and output classes over the ALSA sequencer | `41_system` |

Notes:

- `fluidr3-gm-sf3`: the SoundFont is also linked as `/usr/share/sounds/sf2/FluidR3_GM.sf2`, the
  only path SDL2_mixer tries; without it SDL2_mixer's MIDI music is silent.
- `fluidsynth`: gcem is a second `source =`, unpacked into `gcem/`, where `FindGCEM.cmake` looks
  before it downloads.
- `libgig`: `pkglibdir=/usr/lib` and `pc-libdir.patch` put the libraries on musl's library path.
  The default, `/usr/lib/libgig`, links and then fails to load.
- `libresidfp`: `--with-simd=runtime`. A fixed instruction set builds a library that faults on a
  processor without it.
- `libsidplayfp`: reSIDfp is found through pkg-config, with no configure switch, so `libresidfp`
  stays in depends; without it only the SIDLite engine is built and audacious-plugins drops its SID
  plugin.

### studio

Music production: DAWs, audio editors, trackers, drum machines and notation. 12 ports, under
`ports/core/studio/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `ardour` | 9.8.0 | Digital audio workstation: multitrack recording, editing, mixing and LV2 plugins (YTK, X11) | `44_apps` |
| `audacity` | 4.0.0 | Multitrack audio editor and recorder — the Qt 6 Audacity 4, with LV2, LADSPA and Nyquist effects | `44_apps` |
| `ft2-clone` | 2.24 | Fasttracker II clone — the XM tracker, pixel for pixel, on SDL2 | `44_apps` |
| `furnace` | 0.6.8.3 | Chiptune tracker for over fifty sound chips of consoles, computers and arcades | `44_apps` |
| `hydrogen` | 1.2.6 | Pattern-based drum machine and sequencer with sample drum kits | `44_apps` |
| `kwave` | 26.08.1 | KDE sound editor — record, cut, filter and convert WAV, FLAC, MP3, Ogg Vorbis and Opus | `44_apps` |
| `lmms` | 1.2.2 | Pattern-based music production: sequencer, synthesizers, samples and LADSPA effects (Qt 5) | `44_apps` |
| `milkytracker` | 1.06 | Fast Tracker II–style music tracker for XM and MOD files, on SDL2 | `44_apps` |
| `musescore` | 4.7.5 | MuseScore Studio — music notation: write, play back, print and export scores, with the MS Basic soundfont | `44_apps` |
| `pt2-clone` | 1.92 | ProTracker 2 clone — the Amiga MOD tracker, pixel for pixel, on SDL2 | `44_apps` |
| `schismtracker` | 20260524 | Impulse Tracker clone — compose and play IT, S3M, XM and MOD music, on SDL3 | `44_apps` |
| `tenacity` | 1.3.5 | Multitrack audio editor on wxWidgets — the Audacity 3 lineage with the network code removed | `44_apps` |

Notes:

- `ardour`: `--cxx17` is required: the sources assign `u8""` literals to `std::string` and
  `const char *`, which C++20, GCC's default, types as `char8_t`.
- `audacity`: `EXTDEPS_CACHE` points the dependency engine at the tarball's
  `offline-deps/downloads`, so nothing is downloaded. utf8cpp is rebuilt from the tarball's copy,
  which a blanket `EXTDEPS_OVERRIDE_ALL=SYSTEM` would swap for the utfcpp port's CMake package. git
  is a build dependency: the engine applies its patches with `git apply`.
- `lmms`: `CMAKE_CXX_FLAGS` carries cairo's include directory: the fltk port is built with Cairo,
  so its headers include `<cairo.h>`, and the ZynAddSubFX editor takes only FLTK's directory.
- `musescore`: `FETCHCONTENT_SOURCE_DIR_*` points the MNX importer's DOM library and schema
  validator at the recipe's sources. The harfbuzz port is required: a missing one makes CMake
  download a build recipe. `MUE_BUILD_MUSESOUNDS_QML=ON` is required, because the app shell's QML
  module needs the MuseSounds QML target.
- `tenacity`: FFmpeg import and export stay off: they know libavformat up to 61, and the ffmpeg port
  is newer. Each codec and plugin host is a `cmake_dependent_option` that turns itself off when its
  library is missing, with only a status line.

### music-players

Music players, library managers and taggers. 10 ports, under `ports/core/music-players/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `audacious` | 4.6.1 | Playlist-oriented music player — Qt 6 interface, MPRIS on the session bus | `43_toolkits` |
| `audacious-plugins` | 4.6.1 | Audacious decoders, outputs, effects and the Qt interface — without them the player opens no window and plays nothing | `44_apps` |
| `cmus` | 2.12.0 | Console music player with a library view | `44_apps` |
| `elisa` | 26.08.1 | KDE music player — a local music collection, played through libVLC | `44_apps` |
| `mpd` | 0.24.15 | Music Player Daemon — plays music for separate client programs | `42_graphics` |
| `picard` | 3.0.0rc4 | Tag and rename music files from the MusicBrainz database | `44_apps` |
| `rhythmbox` | 3.5.1 | GNOME music player and library organiser | `44_apps` |
| `rmpc` | 0.11.0 | An MPD client drawn in ratatui, with album art in the terminal | `44_apps` |
| `strawberry` | 1.2.30 | Music player and collection organiser — tags, playlists, loudness normalisation, CD and MTP devices | `44_apps` |
| `termusic` | 0.13.2 | A terminal music player drawn in ratatui, with its own playback daemon | `41_system` |

Notes:

- `cmus`: no pulse output. cmus ranks pulse above ALSA, so building it would move every stream onto
  pipewire-pulse.
- `rhythmbox`: `gst-libav` in `depends` is the AAC decoder, needed at run time only; without it an
  `.m4a` or `.aac` file in the library does not play.
- `rmpc`: `VERGEN_GIT_DESCRIBE` and `VERGEN_GIT_SHA` carry the version, because a tarball has no git
  and the binary would print a placeholder.
- `strawberry`: the one music player whose desktop entry claims audio types. The entries of
  audacious, elisa, picard and rhythmbox leave `MimeType` out, so an audio file opens in one
  program.

### video-libs

Video codecs, containers, subtitles, disc playback and effects libraries. 23 ports, under
`ports/core/video-libs/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `dav1d` | 1.5.4 | AV1 cross-platform decoder | `41_system` |
| `frei0r-plugins` | 3.6.0 | frei0r video effect plugins and their plugin API | `42_graphics` |
| `gavl` | 1.4.0 | Gmerlin audio and video library — colourspace, scaling and sample-format conversion for frei0r's plugins | `41_system` |
| `libass` | 0.17.5 | SSA/ASS subtitle renderer — what lets ffmpeg burn subtitles in | `42_graphics` |
| `libbluray` | 1.5.0 | Blu-ray playlists and titles — mpv's bd:// and ffmpeg's bluray: protocol | `41_system` |
| `libde265` | 1.1.3 | HEVC decoder — libheif's decode half | `41_system` |
| `libdovi` | 3.4.0 | C API of dovi_tool's dolby_vision crate: parse, convert and write Dolby Vision RPU metadata | `44_apps` |
| `libdvbpsi` | 1.3.3 | MPEG-TS PSI/SI table decoder — VLC's transport-stream and DVB demuxing | `41_system` |
| `libdvdcss` | 1.6.0 | Reads a CSS-encrypted DVD-Video disc, for libdvdread and every DVD player above it | `41_system` |
| `libdvdnav` | 7.0.0 | DVD-Video navigation — mpv's dvd:// and GStreamer's rsndvdbin, whose menus it drives | `41_system` |
| `libdvdread` | 7.1.1 | Reads the file system and title structure of a DVD-Video disc | `41_system` |
| `libebml` | 1.4.7 | EBML container library — the layer under libmatroska | `41_system` |
| `libmatroska` | 1.7.2 | Matroska and WebM container library — VLC's and MKVToolNix's .mkv reader | `41_system` |
| `libmpeg2` | 0.5.1 | MPEG-1 and MPEG-2 video stream decoder library | `41_system` |
| `libtheora` | 1.2.0 | The Theora video codec, the one in Ogg video files | `41_system` |
| `libudfread` | 1.2.0 | Reads the UDF file system of a Blu-ray disc or image | `41_system` |
| `libvpx` | 1.17.0 | VP8 and VP9 codecs — BSD, so this one does not relicense ffmpeg | `41_system` |
| `movit` | 1.7.2 | High-quality GPU video filters on OpenGL — the GLSL effect chain MLT's movit module drives | `42_graphics` |
| `openh264` | 2.6.0 | Cisco's H.264 baseline encoder and decoder, compiled from source | `41_system` |
| `svt-av1` | 4.2.0 | Scalable AV1 encoder | `41_system` |
| `vidstab` | 1.1.2 | Video stabilisation library, used by ffmpeg and MLT | `41_system` |
| `x264` | 20250608.1624 | H.264 encoder — GPL-2+, and it relicenses the ffmpeg that links it | `41_system` |
| `x265` | 4.2 | HEVC encoder — GPL-2+ | `41_system` |

Notes:

- `frei0r-plugins`: `FREI0R_VERSION` is what `frei0r.pc` reports; left out, the project calls
  itself 3.0.0.
- `gavl`: `--with-cpuflags=none`, because the default compiles for the builder's processor and the
  library then faults on an older one.
- `libbluray`: BD-J menus are off (`-Dbdj_jar=disabled`), because building the jar puts a JDK under
  ffmpeg. A disc plays its titles without the Java menu.
- `libdvdread`: `-Dlibdvdcss=enabled` links libdvdcss directly. Left at `auto`, a missing
  `libdvdcss.so.2` at run time is a disc that silently does not open.
- `libebml`, `libmatroska`: `BUILD_SHARED_LIBS` defaults to off, and a static copy would sit in
  every consumer, out of reach of an update to the port.
- `libvpx`: `--as=nasm`, because the default takes yasm whenever it is installed and the assembler
  would follow build order.
- `movit`: only the library target is built, because `make` alone builds the unit tests against a
  GoogleTest source tree. configure still requires `sdl2.pc`.
- `x264`, `x265`: GPL-2.0-or-later. ffmpeg links them only under `--enable-gpl`, which makes that
  ffmpeg GPL, as the ffmpeg port's `LICENSE.notice` records.
- `x265`: one `libx265.so` carries the 8-, 10- and 12-bit encoders, upstream's
  `build/linux/multilib.sh` layout; an 8-bit-only library makes ffmpeg refuse every 10-bit pixel
  format. The vendored json11 needs `<cstdint>` forced in. The LTO warning that
  `x265_analysis_distortion_data` breaks the ODR is expected.

### media-frameworks

Multimedia frameworks, capture, and media inspection. 18 ports, under
`ports/core/media-frameworks/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `ffmpeg` | 9.0.2 | Record, convert and stream audio and video | `42_graphics` |
| `ffmpegthumbnailer` | 2.3.1 | Video thumbnail generator for file managers | `42_graphics` |
| `gst-libav` | 1.28.8 | GStreamer FFmpeg-based codec plugins | `42_graphics` |
| `gst-plugins-bad` | 1.28.8 | GStreamer "bad" plugin set (less mature plugins) | `43_toolkits` |
| `gst-plugins-base` | 1.28.8 | GStreamer base plugin set (provides gstreamer-pbutils, audio/video conversion, file I/O) | `42_graphics` |
| `gst-plugins-good` | 1.28.8 | GStreamer "good" plugin set (well-maintained LGPL plugins) | `43_toolkits` |
| `gst-plugins-ugly` | 1.28.8 | GStreamer "ugly" plugin set (patent/license-encumbered codecs) | `42_graphics` |
| `gstreamer` | 1.28.8 | GStreamer streaming media framework — core library | `42_graphics` |
| `libcamera` | 0.7.2 | Camera stack that puts ISP pipelines and sensor control behind one API | `42_graphics` |
| `libmediainfo` | 26.05 | Media file inspection library — codec, profile, bit depth and every track | `41_system` |
| `libzen` | 0.4.41 | ZenLib, the portability layer libmediainfo is written on | `41_system` |
| `mediainfo` | 26.05 | The command line over libmediainfo | `41_system` |
| `mlt` | 7.42.0 | MLT multimedia framework: the video editing engine under Kdenlive and Shotcut | `43_toolkits` |
| `opentimelineio` | 0.18.1 | OpenTimelineIO: interchange format and C++ API for editorial timelines | `41_system` |
| `orc` | 0.4.44 | Oil Runtime Compiler — JIT for the SIMD inner loops of GStreamer's plugins | `41_system` |
| `qv4l2` | 1.32.0 | qv4l2 and qvidcap — Qt 6 test benches for Video4Linux devices: every control, format and stream, and a capture viewer | `44_apps` |
| `totem-pl-parser` | 3.26.7 | Playlist parser library — m3u, pls, xspf and podcast feeds | `41_system` |
| `v4l-utils` | 1.32.0 | Video4Linux userspace — libv4l2 and libv4lconvert, libdvbv5, v4l2-ctl, media-ctl, ir-keytable, cec-ctl | `41_system` |

Notes:

- `ffmpeg`: `--disable-sdl2`, and with it no `ffplay`: sdl2-compat reaches back to ffmpeg through
  sdl3 and pipewire, a dependency cycle.
- `gst-plugins-bad`: `-Dgpl=enabled` stands beside x265, faad and resindvd; without it each GPL
  plugin is skipped with nothing but a line in meson's summary.
- `gst-plugins-good`: `souphttpsrc` and the `adaptivedemux2` HLS and DASH elements `dlopen`
  libsoup 3 at run time, so `libsoup3` stays in `depends` though no build step needs it; without
  it both plugins load and register no element.
- `gst-plugins-ugly`: `-Dgpl=enabled` stands beside x264, a52dec, dvdread and mpeg2dec; without it
  the package builds with those plugins missing.
- `libcamera`: not byte-reproducible. The build generates a fresh IPA signing key, so
  `kpkg verify --repro libcamera` reports a difference. `-Dv4l2` stays off: the `libcamerify`
  preload shim resolves glibc-only symbols and faults on musl. `-Dipas` names every pipeline that
  has an IPA, or that pipeline runs without auto-exposure and white balance. `cam`'s SDL2 sink
  stays off, because sdl3 pulls pipewire, which depends on libcamera.
- `opentimelineio`: RapidJSON is the commit upstream pins, unpacked into `src/deps/rapidjson`; the
  rapidjson port's 1.1.0 predates the API the code uses.

### video-players

Video players and media centres, and their front-end add-ons. 8 ports, under
`ports/core/video-players/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `celluloid` | 0.30 | GTK 4 video player over libmpv | `44_apps` |
| `haruna` | 1.8.1 | Haruna: a Qt 6 and Kirigami video player over libmpv | `44_apps` |
| `kodi` | 21.3 | Ten-foot media centre for local and network video, music and pictures, with add-ons | `44_apps` |
| `mpv` | 0.41.0 | Command-line media player for Wayland, with no toolkit | `42_graphics` |
| `mpv-mpris` | 1.3 | Puts mpv on the session bus, so the media keys and the panel reach it | `44_apps` |
| `smplayer` | 26.8.29 | Qt front end for mpv that remembers where every file stopped | `44_apps` |
| `uosc` | 5.13.0 | Minimalist, proximity-based on-screen controls, menus and playlist for mpv | `44_apps` |
| `vlc` | 3.0.24 | VLC media player 3 with its Qt 5 interface — plays files, discs, devices and streams | `43_toolkits` |

Notes:

- `kodi`: every library Kodi would download is a source of the recipe, checked against the hashes
  in Kodi's `VERSION` files and linked statically; Kodi 21 builds against neither the ffmpeg port's
  FFmpeg nor fmt 12. Groovy must be 4.0.33 or later to read the openjdk port's class files. The SIMD
  level is fixed at SSE2, the thread stack is raised to 2 MiB for musl, and
  `CMAKE_POLICY_VERSION_MINIMUM` is exported so it reaches every nested CMake run. `ninja crossguid`
  runs before the full build, because `libcrossguid.a` is not a tracked output and a parallel link
  can reach it before it exists. Add-on updates default to never through `appliance.xml`.
- `mpv`: `-Dx11=enabled` serves front ends that embed mpv by window id (SMPlayer), `-Dsixel=enabled`
  is the video output on a terminal with no compositor, and `-Dcplugins=enabled` loads mpv-mpris.
  `build-date` is off, so two builds of one source match.
- `smplayer`: runs under Xwayland (`QT_QPA_PLATFORM=xcb`), because mpv draws into its window by X11
  window id. qmake reads no compiler flags from the environment, so they are passed as `QMAKE_*`
  assignments.
- `vlc`: every module with an outside library is named on or off, so a missing port fails
  configure. `--enable-lirc` is the exception: a missing library drops the module silently, and
  lirc in `depends` keeps it. Lua is 5.4 through `LUA_CFLAGS` and `LUA_LIBS`, because the system
  lua is 5.5. `plugins.dat` is not shipped: it records each plugin's mtime, which packaging resets.

### video-tools

Video editing, transcoding, muxing, recording, streaming and webcam apps. 10 ports, under
`ports/core/video-tools/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `avidemux` | 2.8.1 | Qt 6 video cutter, filter and encoder | `44_apps` |
| `blind` | 1.1 | Video editing as a pipeline of small programs over raw frames | `44_apps` |
| `handbrake` | 1.11.2 | HandBrake video transcoder: the GTK 4 application and HandBrakeCLI | `44_apps` |
| `kamoso` | 26.08.1 | KDE webcam booth — take pictures and record videos from a camera | `44_apps` |
| `kdenlive` | 26.08.1 | Kdenlive: the KDE multi-track video editor, on MLT | `44_apps` |
| `mkvtoolnix` | 102.0 | MKVToolNix: create, split, edit and inspect Matroska files, with the Qt 6 GUI | `44_apps` |
| `obs-studio` | 32.2.2 | OBS Studio: screen recording and live streaming, with PipeWire screen capture on Wayland | `44_apps` |
| `shotcut` | 26.8.1 | Shotcut: a Qt 6 video editor on MLT, with no KDE Frameworks | `44_apps` |
| `wf-recorder` | 0.6.0 | Screen recorder for wlroots compositors, through wlr-screencopy | `44_apps` |
| `yt-dlp` | 2026.08.19 | Download video and audio from YouTube and thousands of other sites | `41_system` |

Notes:

- `avidemux`: built in upstream's `bootStrap.bash` order, each part against the ones installed in
  `$PKG` before it (`FAKEROOT`). The core carries its own patched FFmpeg 4.4, linked privately as
  `libADM6av*`.
- `handbrake`: links its own patched FFmpeg and codecs statically into libhb. Each is a recipe
  source under the name its `contrib/*/module.defs` fetches, and `--disable-df-fetch` makes a
  missing one a build error instead of a download.
- `kamoso`: needs `qml6glsink` from gst-plugins-good; without it Kamoso opens a window with no
  picture.
- `mkvtoolnix`: an empty `ac_cv_path_INKSCAPE` keeps the Windows icon render out when Inkscape and
  ImageMagick are both installed.
- `obs-studio`: `OBS_VERSION_OVERRIDE` is required: the release tarball has no `.git`, and the
  version would read 0.0.1.
- `shotcut`: `SHOTCUT_VERSION` is set; unset, it is the build date and two builds differ.
- `wf-recorder`: PipeWire audio is named on. Left at `auto`, pulse wins whenever libpulse-simple is
  installed.

### game-libs

Game engines and game-development libraries, SDL in all versions, and shared game libraries. 18
ports, under `ports/core/game-libs/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `enet` | 1.3.18 | Reliable UDP networking for games | `41_system` |
| `glm` | 1.0.3 | OpenGL Mathematics: header-only C++ vectors and matrices in GLSL's terms | `41_system` |
| `libkdegames` | 26.08.1 | Common code, card decks and sound for the KDE games | `43_toolkits` |
| `libkmahjongg` | 26.08.1 | Tile sets, backgrounds and the rendering code shared by the KDE Mahjongg games | `43_toolkits` |
| `love` | 11.5 | Lua 2D game framework, and the runtime that opens a .love game | `42_graphics` |
| `physfs` | 3.2.0 | Read game data from directories and ZIP, 7z, WAD, GRP and other archives as one tree | `41_system` |
| `sdl12-compat` | 1.2.76 | The SDL 1.2 library, headers and build files, answering every SDL 1.2 call through SDL2 | `42_graphics` |
| `sdl2-compat` | 2.32.72 | The SDL2 library, headers and build files, answering every SDL2 call through SDL3 | `42_graphics` |
| `sdl2-gfx` | 1.0.4 | Lines, circles, polygons, rotation, zoom and a frame limiter for SDL2 programs | `42_graphics` |
| `sdl2-image` | 2.8.12 | Load PNG, JPEG, WebP, AVIF, JPEG XL, TIFF, SVG and older formats into SDL2 surfaces | `42_graphics` |
| `sdl2-mixer` | 2.8.2 | Play music and sound effects from SDL2 programs: Ogg, Opus, FLAC, MP3, WavPack, MOD, MIDI and chiptunes | `42_graphics` |
| `sdl2-net` | 2.4.0 | Portable TCP and UDP sockets for SDL2 programs | `42_graphics` |
| `sdl2-pango` | 2.1.5 | Draw Pango-laid-out text into SDL2 surfaces | `42_graphics` |
| `sdl2-ttf` | 2.24.0 | Render TrueType and OpenType text into SDL2 surfaces, shaped by HarfBuzz | `42_graphics` |
| `sdl3` | 3.4.16 | Simple DirectMedia Layer 3 — windows, audio and input; SDL2 programs reach it through sdl2-compat | `42_graphics` |
| `sdl3-image` | 3.4.6 | Load PNG, JPEG, WebP, AVIF, JPEG XL, TIFF, SVG and animated formats into SDL3 surfaces | `42_graphics` |
| `sdl3-mixer` | 3.2.4 | Play music and sound effects from SDL3 programs: Ogg, Opus, FLAC, MP3, WavPack, MOD, MIDI and chiptunes | `42_graphics` |
| `sdl3-ttf` | 3.2.2 | Render TrueType and OpenType text into SDL3 surfaces and GPU text, shaped by HarfBuzz | `42_graphics` |

Notes:

- `sdl3`: `SDL_KMSDRM=OFF`. A KMSDRM program would take the card and the input devices from the
  running compositor; without the driver, a program started outside `kdos-comp` exits with "No
  available video device".
- `sdl3`: libusb and liburing are in `depends` because `SDL_HIDAPI_LIBUSB` and `SDL_LIBURING` only
  permit a probe, which configures without the feature when nothing is found. HIDAPI over libusb
  reaches the GameCube adapter and the Switch 2 controllers.
- `sdl2-compat`: `SDL2COMPAT_X11` stays off while sdl3 is built without its X11 driver; the two go
  on together. `sdl2-config` is removed, so a consumer that pkg-config cannot answer fails instead
  of taking a hardcoded prefix.
- `sdl12-compat`: `sdl-config` is kept, because SDL 1.2 makefiles run it directly.
- `sdl2-image`, `sdl3-image`, `sdl2-mixer`, `sdl3-mixer`: every codec switch is named and
  `DEPS_SHARED=OFF`. Upstream's defaults pick bundled decoders or a dlopen, and a missing codec
  library is then a format that silently stops working instead of a link the solver sees. The
  mixers keep modplug off: it and libxmp claim the same formats, and the first found wins.

### games-action

Action, arcade, racing, platform and sandbox games. 9 ports, under `ports/core/games-action/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `ddnet` | 20.1 | DDraceNetwork — the cooperative Teeworlds racing game, its client, server and map tools | `44_apps` |
| `luanti` | 5.17.0 | Voxel game engine, with Minetest Game for offline play | `44_apps` |
| `moon-buggy` | 1.1.0 | Drive a buggy across the moon, jumping craters and shooting meteors | `41_system` |
| `neverball` | 1.6.0 | Neverball and Neverputt — tilt the floor to roll a ball through 3D obstacle courses, and minigolf on the same engine | `44_apps` |
| `powder-toy` | 100.1.400 | The Powder Toy — a falling-sand physics sandbox of air pressure, heat, gravity and electronics, with local saves | `44_apps` |
| `srb2` | 2.2.15 | Sonic Robo Blast 2 — a 3D Sonic fan game on a heavily modified Doom Legacy engine, with its game data | `44_apps` |
| `stk-assets` | 1.5 | SuperTuxKart game data — karts, tracks, arenas, music, sounds, models and textures | `41_system` |
| `supertux` | 0.7.0 | Classic 2D side-scrolling platformer with Tux, with a level editor | `44_apps` |
| `supertuxkart` | 1.5 | 3D kart racing with story mode, grand prix, battles and soccer | `44_apps` |

Notes:

- `ddnet`: the cargo vendor bundle is unpacked in the source root, because cargo finds its
  configuration by walking up from the build directory.
- `moon-buggy`: installed through named make targets, not `make install`, whose
  `install-data-hook` makes the binary setgid `games` for a shared score file.
- `neverball`: the makefile's `CFLAGS := -O2` replaces the build's flags and it asks a
  `sdl2-config` that does not exist, so `build.sh` passes the flags (with `-fcommon`) and the SDL
  and PNG flags from pkgconf as make variables.
- `powder-toy`: `can_install=no` keeps the game from writing its own desktop entry and MIME type
  into the home directory at start-up.
- `srb2`: GME, libopenmpt and miniupnpc are looked up `QUIET` upstream and required by name here.
  The executable is named outright: left empty, its name comes from the git branch, which a
  release tarball does not have.
- `supertuxkart`: ENet stays bundled, because STK's IPv6 support lives in its own copy.

### games-shooter

First-person shooters, Doom and Quake engines, and their data. 11 ports, under
`ports/core/games-shooter/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `chocolate-doom` | 3.1.1 | Doom engine faithful to the DOS original — Doom, Heretic, Hexen and Strife | `44_apps` |
| `crispy-doom` | 7.1 | Limit-removing Doom engine — Chocolate Doom with higher resolution and quality-of-life fixes | `44_apps` |
| `deutex` | 5.2.3 | Doom WAD composer and decomposer — builds an IWAD from lumps, PNGs and sounds | `41_system` |
| `dsda-doom` | 0.29.4 | Speedrunning Doom engine from the PrBoom+ line — Boom, MBF21, UMAPINFO, OpenGL renderer | `44_apps` |
| `freedoom` | 0.13.0 | Free game data for Doom engines — Phase 1, Phase 2 and FreeDM IWADs built from their lumps | `42_graphics` |
| `ioquake3` | 1.36.20260917 | Quake III Arena engine — the maintained id Tech 3 with SDL, OpenAL, VoIP and current renderers | `44_apps` |
| `ironwail` | 0.8.2 | Quake engine from the QuakeSpasm line, with an OpenGL renderer that keeps the original look | `44_apps` |
| `sauerbraten` | 2020.12.29 | Cube 2 engine shooter with in-game co-operative map editing, and its data | `44_apps` |
| `xonotic` | 0.8.6 | Arena first-person shooter on the DarkPlaces engine: SDL client and dedicated server | `44_apps` |
| `xonotic-data` | 0.8.6 | Xonotic's game data — maps, models, textures, sounds and music, about 1.2 GB | `41_system` |
| `yamagi-quake2` | 8.70 | Quake II engine kept faithful to the original — OpenGL 1.4, 3.2, GLES3 and software renderers | `44_apps` |

Notes:

- `ioquake3`, `ironwail`: the engine finds its renderers, game modules or `ironwail.pak` beside the
  path it was started by, so `/usr/bin` holds scripts that start it by its full path, not links.
- `yamagi-quake2`: the engine loads its renderers from its own directory and nowhere else, so the
  whole `release/` tree is installed together and `/usr/bin` holds links into it.
- `sauerbraten`: the makefile assigns its own `CXXFLAGS` and link lines, so the build's flags are
  passed ahead of upstream's, for the engine and the ENet sub-make, and `LDFLAGS` leads both link
  lines; without them the binary records the build path and loses the tree's hardening.
- `xonotic`: the build flags travel in `CPUOPTIMIZATIONS`, the one variable the makefile adds to
  its own, and `LDFLAGS` rides there too; the exported `-std=gnu11` is removed because
  `gcc-15.patch` leaves C23 `bool` in use. libpng, libogg, libvorbis, libtheora and libcurl are
  dlopened at run time, so they are dependencies no link line shows.

### games-strategy

Strategy, simulation and space-trading games, and their data. 12 ports, under
`ports/core/games-strategy/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `endless-sky` | 0.11.2 | 2D space trading, exploration and combat in the spirit of Escape Velocity | `44_apps` |
| `fheroes2` | 1.1.17 | Heroes of Might and Magic II engine, for a player's own copy of the game data | `44_apps` |
| `freeciv` | 3.2.6 | Turn-based empire-building strategy in the Civilization tradition, SDL2 client and server | `44_apps` |
| `openrct2` | 0.5.5 | RollerCoaster Tycoon 2 engine, for a player's own copy of the RCT2 or RCT Classic data | `44_apps` |
| `openttd` | 15.3 | Transport Tycoon Deluxe engine — build rail, road, air and sea networks | `44_apps` |
| `openttd-opengfx` | 8.0 | OpenGFX: the free graphics base set for OpenTTD | `41_system` |
| `openttd-openmsx` | 0.4.2 | OpenMSX: the free music base set for OpenTTD | `41_system` |
| `openttd-opensfx` | 1.0.3 | OpenSFX: the free sound base set for OpenTTD | `41_system` |
| `pioneer` | 20260907 | Open-ended space trading and combat across a procedurally generated Milky Way, with Newtonian flight | `44_apps` |
| `warzone2100` | 4.7.0 | Post-apocalyptic real-time strategy with campaigns, skirmish and a research tree | `44_apps` |
| `wesnoth` | 1.18.8 | Turn-based fantasy strategy with campaigns, skirmishes and a map editor | `44_apps` |
| `widelands` | 1.3.1 | Real-time strategy about building an economy and its roads, in the line of The Settlers II | `44_apps` |

Notes:

- `fheroes2`: only the game target is built: the `translations` target re-encodes every catalogue
  with GNU iconv's `//TRANSLIT`, which musl's iconv does not implement.
- `openttd`: the link asks for a 1 MiB thread stack; musl's default 128 KiB overflows in the
  pathfinder and script threads.
- `pioneer`: `ninja build-data` runs the model compiler that writes the `.sgm` models the install
  copies; without it the package carries no ship or station models. GLEW's CMake package is
  ignored so that FindGLEW searches for the library itself: through the package it leaves
  `GLEW_LIBRARIES` empty. SSE4.2 and AVX2 are off because upstream turns them on for every x86
  target.
- `wesnoth`: Lua is the patched copy in `src/modules`, built as C++; the system Lua is C and cannot
  stand in for it.
- `widelands`: gettext's `libintl.h` maps its calls to `libintl_*` symbols musl does not carry, so
  `-lintl` ends every link line.

### games-rpg

RPGs, roguelikes and interactive fiction. 8 ports, under `ports/core/games-rpg/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `brogue-ce` | 1.15.1 | Brogue Community Edition — a roguelike of 26 dungeon levels, drawn in SDL2 tiles or glyphs | `44_apps` |
| `cataclysm-dda` | 0.I.1 | Cataclysm: Dark Days Ahead — turn-based survival in a procedurally generated post-apocalyptic world, in its tiles build | `44_apps` |
| `crawl-tiles` | 0.34.1 | Dungeon Crawl Stone Soup — the open-ended roguelike of the Orb of Zot, in its graphical tiles build | `44_apps` |
| `devilutionx` | 1.5.5 | Diablo and Hellfire engine, for a player's own copy of the game data | `44_apps` |
| `flare-engine` | 1.15 | Engine of the Flare isometric action RPG, run by the game data of flare-game or any other mod | `42_graphics` |
| `flare-game` | 1.15 | Flare: Empyrean Campaign — the art, music, maps and story that make the FLARE engine a game | `44_apps` |
| `frotz` | 2.55 | Z-machine interpreter — Infocom-era interactive fiction in a terminal | `41_system` |
| `nethack` | 5.0.0 | Roguelike dungeon game in characters, with saves under the player's own home | `41_system` |

Notes:

- `cataclysm-dda`: `WARNINGS` replaces upstream's warning list, which carries `-Werror`.
- `flare-engine`, `flare-game`: the data directory is compiled into the engine, so both build with
  `DATADIR=share/flare`.
- `frotz`: the Makefile appends `-g` after `CFLAGS`, so the debug information is dropped at the
  link with `-Wl,--strip-debug`.
- `nethack`: the hints file written by `build.sh` is the whole configuration; `LIBS` goes in its
  `#-POST` segment, because the stock Makefile body beats anything assigned in `#-PRE`.
  `-D_GNU_SOURCE` and `-D_XOPEN_SOURCE_EXTENDED` are what let the curses interface compile against
  musl and the wide-character ncurses. `hacklib.a` is built before `make install`: under a
  parallel make the two archive runs of `src/` and `util/` can meet and leave an archive with no
  index. A build with SYSCF refuses to start without `/etc/nethack/sysconf`.

### games-board

Board, card, puzzle, chess and classic small games, whatever toolkit or desktop project they come
from. 18 ports, under `ports/core/games-board/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `aisleriot` | 3.22.35 | Over eighty solitaire card games, each written in Scheme | `44_apps` |
| `black-hole-solver` | 1.14.0 | Solver for the Black Hole, All in a Row and Golf patience games | `41_system` |
| `bsd-games` | 2.17 | Three of the BSD games — Colossal Cave, tetris and worm | `41_system` |
| `chess-tui` | 2.7.1 | Chess in a terminal, against Stockfish or a second player | `41_system` |
| `freecell-solver` | 6.16.0 | Solver for Freecell, Simple Simon and related patience games, as a library and a command | `41_system` |
| `gnome-mahjongg` | 51.1 | Match pairs of mahjong tiles until the board is clear | `44_apps` |
| `gnome-mines` | 50.0 | Clear hidden mines from a minefield | `44_apps` |
| `gnome-sudoku` | 51.0.1 | Sudoku, the number-grid puzzle, generated at four difficulties | `44_apps` |
| `katomic` | 26.08.1 | Slide atoms into place to build the molecule | `44_apps` |
| `kblocks` | 26.08.1 | The falling-blocks game, against the clock or a computer player | `44_apps` |
| `kbounce` | 26.08.1 | Fence off the field while the balls bounce around it | `44_apps` |
| `kmahjongg` | 26.08.1 | Mahjong solitaire: clear the board by matching pairs of free tiles | `44_apps` |
| `kmines` | 26.08.1 | The classic minesweeper, on a themed board | `44_apps` |
| `kpat` | 26.08.1 | KPatience — fourteen solitaire card games, with a solver for each deal | `44_apps` |
| `kreversi` | 26.08.1 | Reversi against the computer or a second player | `44_apps` |
| `ksudoku` | 26.08.1 | Sudoku, jigsaw, killer and 3-D Roxdoku puzzles, generated and solved | `44_apps` |
| `qqwing` | 1.3.4 | Sudoku generator and solver, as a C++ library and a command | `41_system` |
| `stockfish` | 19 | UCI chess engine, with its evaluation network built in | `41_system` |

Notes:

- `black-hole-solver`, `freecell-solver`: the configure runs Perl (and, for freecell-solver,
  Python) generators whose modules — Path::Tiny, Moo and its dependencies, the Template Toolkit,
  pysol_cards — are build-only sources unpacked beside the tree and never installed.
  freecell-solver's Python deal generators import pysol_cards at run time, so they are removed.
- `bsd-games`: `build_dirs` names the three games built. hangman stays out: it exits at start-up
  without `/usr/share/dict/words`, and no port ships a word list. libbsd is in `depends` for its
  `<sys/cdefs.h>`, which bsd-games' own wrapper includes and musl lacks; nothing links it. The
  tree's flags go in through `other_cflags`/`other_ldflags`, because `Makeconfig` assigns `CFLAGS`
  with `:=` and drops the environment's. The tetris score file is `0666` and the binary carries no
  setgid bit: tetris exits when it cannot write the file.
- `ksudoku`: Roxdoku is compiled in only when OpenGL and GLU are both found, and is otherwise
  dropped silently; GLU comes from the `glu` port in `depends`.
- `stockfish`: the evaluation network is a recipe source copied into `src/`, so the build fetches
  nothing. `ARCH=x86-64-universal` builds one binary that picks its instruction-set level at
  start-up, and the `build` target skips profile-guided optimisation, whose benchmark-timed
  profile differs between builds.

### emulators

Standalone emulators of computers and consoles, and compatibility layers. 18 ports, under
`ports/core/emulators/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `amiberry` | 8.3.0 | Amiga emulator from the A500 to the A4000 and CD32, with WHDLoad booting and the AROS ROMs | `44_apps` |
| `dosbox-staging` | 0.83.0 | DOS PC emulator for games and old software | `44_apps` |
| `dosbox-x` | 2026.08.31 | x86 PC emulator for DOS, Windows 3.x/9x, PC-98 and PCjr/Tandy software | `44_apps` |
| `fuse-emulator` | 1.10.0 | The Free Unix Spectrum Emulator: ZX Spectrum 16K to +3, Pentagon and Timex, with its ROMs | `44_apps` |
| `hatari` | 2.6.1 | Atari ST, STE, TT and Falcon emulator, with the Hatari UI configuration front end | `44_apps` |
| `libretro-mgba` | 0.10.5 | mGBA as a libretro core — Game Boy Advance, Game Boy and Game Boy Color in RetroArch | `41_system` |
| `libspectrum` | 1.7.0 | ZX Spectrum emulator file formats — snapshots, tapes, disks and RZX recordings | `41_system` |
| `mednafen` | 1.32.1 | Command-line multi-system emulator: NES, SNES, Game Boy, GBA, Mega Drive, PC Engine, PlayStation, Saturn and more | `44_apps` |
| `mgba` | 0.10.5 | Game Boy Advance, Game Boy and Game Boy Color emulator, SDL front end | `44_apps` |
| `mupen64plus` | 2.6.0 | Nintendo 64 emulator: core, console front end, SDL audio and input, HLE RSP, Rice and Glide64mk2 video | `44_apps` |
| `openmsx` | 21.0 | MSX home computer emulator with C-BIOS, a built-in debugger and GUI | `44_apps` |
| `ppsspp` | 1.20.4 | PlayStation Portable emulator, OpenGL and Vulkan, drawn through SDL | `44_apps` |
| `scummvm` | 2026.3.0 | Runs classic point-and-click adventure and role-playing games from their original data files | `44_apps` |
| `stella` | 7.0c | Atari 2600 VCS emulator with a built-in debugger | `44_apps` |
| `vice` | 3.10 | Commodore 64, 128, VIC-20, PET, Plus/4 and CBM-II emulators, with their ROMs | `44_apps` |
| `wine` | 11.0 | Runs Windows programs on Linux, 64- and 32-bit, on Wayland or X11 | `44_apps` |
| `wine-gecko` | 2.47.4 | The HTML engine Wine's MSHTML uses, 32- and 64-bit, as upstream builds it | `41_system` |
| `wine-mono` | 10.4.1 | The .NET Framework runtime Wine installs into a prefix, as upstream builds it | `41_system` |

Notes:

- `libretro-mgba`: only the core is copied into the package; the tarball's header and licence
  install rules would collide with `mgba`'s.
- `mupen64plus`: Glide64mk2 links libglvnd's `libGL`, not `libOpenGL`. It calls the ARB and EXT
  entry points by name, and musl binds every symbol when the plugin loads.
- `openmsx`, `scummvm`, `stella`: configure asks `sdl2-config`, which does not exist here; a
  build-local shim first on `PATH` answers it from pkgconf.
- `openmsx`: the makefile prefixes every `LDFLAGS` word with `-Wl,`, so the tree's flags are
  passed with their own `-Wl,` removed.
- `wine`: the unix libraries name each other by bare file name, and musl matches an
  already-loaded library only when a search found it, so the `$ORIGIN` run path the patch adds is
  what lets them load.
- `wine-gecko`, `wine-mono`: Wine accepts only the version its own `appwiz.cpl`
  (`GECKO_VERSION`) and `mscoree` (`WINE_MONO_VERSION`) name, so both move with the `wine` port.

### libretro

RetroArch, its assets, databases and libretro cores. 12 ports, under `ports/core/libretro/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `libretro-beetle-psx` | 20260927 | Beetle PSX as libretro cores — Mednafen's PlayStation emulator, software and OpenGL/Vulkan renderers | `44_apps` |
| `libretro-core-info` | 1.22.2 | libretro core info files — what each core is called, which files it opens and which firmware it wants | `41_system` |
| `libretro-database` | 1.22.1 | libretro game databases — the checksums RetroArch's scanner names a game by, and the cursors that query them | `41_system` |
| `libretro-gambatte` | 20260821 | Gambatte as a libretro core — an accuracy-first Game Boy and Game Boy Color emulator | `41_system` |
| `libretro-genesis-plus-gx` | 20260912 | Genesis Plus GX as a libretro core — Mega Drive, Mega-CD, Master System and Game Gear (non-commercial licence) | `41_system` |
| `libretro-melonds` | 20260719 | melonDS as a libretro core — Nintendo DS emulator | `44_apps` |
| `libretro-mupen64plus-next` | 20260912 | Mupen64Plus-Next as a libretro core — Nintendo 64 emulator with the GLideN64 renderer | `44_apps` |
| `libretro-nestopia` | 20260925 | Nestopia UE as a libretro core — Nintendo Entertainment System and Famicom Disk System emulator | `41_system` |
| `libretro-snes9x` | 1.63 | Snes9x as a libretro core — Super Nintendo emulator (non-commercial licence) | `41_system` |
| `retroarch` | 1.22.2 | The libretro frontend: one window, one menu and one set of controls for every emulator core | `44_apps` |
| `retroarch-assets` | 1.22.0 | RetroArch menu artwork — the XMB, Ozone, RGUI and GLUI themes, their icons, fonts and sounds | `41_system` |
| `retroarch-joypad-autoconfig` | 1.22.0 | RetroArch controller profiles — a game pad is mapped the moment it is plugged in | `41_system` |

Notes:

- `libretro-database`: the cheat collection is not shipped, so the skeleton `retroarch.cfg` leaves
  the cheat directory at its per-user default, where the online updater writes cheats.
- `retroarch`: `-std=gnu11` is removed from `CFLAGS`. configure tests C++ with `CFLAGS` and
  `-Werror`, so it would fail every C++ standard and drop the Vulkan driver without an error. The
  desktop entry replaces upstream's, which claims every ROM type, mgba's Game Boy Advance types
  among them.

### education

Learning software, offline encyclopedias and reference dictionaries. 16 ports, under
`ports/core/education/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `gcompris` | 26.2 | Educational activities for children aged 2 to 10 — reading, counting, science, games and art | `44_apps` |
| `goldendict-ng` | 26.8.0 | Dictionary lookup — StarDict, DSL, MDict, ZIM, Babylon and hunspell morphology, offline | `44_apps` |
| `kalzium` | 26.08.1 | KDE periodic table of the elements — properties, isotopes, spectra and a molar mass calculator | `44_apps` |
| `kgeography` | 26.08.1 | KDE geography trainer — maps, capitals and flags of countries and regions, as quizzes | `44_apps` |
| `kiwix-desktop` | 2.5.1 | Offline reader for ZIM libraries such as Wikipedia, with search, tabs and a reading list | `44_apps` |
| `kiwix-tools` | 3.8.2 | kiwix-serve, kiwix-manage and kiwix-search over ZIM archives | `41_system` |
| `kolibri` | 0.19.5 | Offline learning platform — courses, lessons and quizzes from content channels, served on the LAN | `41_system` |
| `ktouch` | 26.08.1 | KDE touch typing tutor — graded courses, keyboard layouts and progress statistics | `44_apps` |
| `kturtle` | 26.08.1 | KDE educational programming environment — steer a turtle with TurtleScript and learn to program | `44_apps` |
| `kwordquiz` | 26.08.1 | KDE flashcard trainer — flashcards, multiple choice and question-and-answer quizzes on KVTML decks | `44_apps` |
| `libkeduvocdocument` | 26.08.1 | KDE vocabulary document library — reads and writes the KVTML files of Parley and KWordQuiz | `43_toolkits` |
| `libkiwix` | 14.2.1 | The library under kiwix-serve and kiwix-desktop that searches and serves ZIM archives | `41_system` |
| `libzim` | 9.8.2 | Library that reads and writes ZIM archives, the format offline Wikipedia ships in | `41_system` |
| `parley` | 26.08.1 | KDE vocabulary trainer — spaced repetition over KVTML word lists, with themes and HTML export | `44_apps` |
| `sdcv` | 0.5.5 | Look a word up in a StarDict dictionary, offline | `41_system` |
| `tuxpaint` | 0.9.35 | Drawing program for children — brushes, stamps, shapes, text and magic tools | `44_apps` |

Notes:

- `kalzium`: upstream's `chemical/x-cml` entry writes `-molecule %f`, which `QCommandLineParser`
  reads as `-m olecule`; the recipe's replacement entry is what opens the file.
- `kiwix-desktop`: Qt 6's qmake reads no compiler flags from the environment, so the tree's flags
  are passed in, and `-Wno-error` goes behind `-after` because the project file appends `-Werror`
  after the command line.
- `kolibri`: the launcher sets `KOLIBRI_DISABLE_PING`. Kolibri lets the environment override
  `options.ini`, so only the environment turns the telemetry pingback back on.
- `sdcv`: the `lang` target is outside the default build and the install copies its output, so it
  is built by name or the install fails.
- `tuxpaint`: the makefile is not safe in parallel; it builds and installs with one job.

### office

Office suites, word processors, spreadsheets, presentations, accounting and finance, and
office-format import libraries. 23 ports, under `ports/core/office/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `abiword` | 3.0.8 | AbiWord word processor — reads and writes Word, OpenDocument, RTF and HTML | `44_apps` |
| `freexl` | 2.0.0 | Reads Excel .xls, .xlsx and LibreOffice .ods sheets, for SpatiaLite and GDAL | `41_system` |
| `gnucash` | 5.17 | Double-entry accounting — personal and small-business books, invoices, reports | `44_apps` |
| `gnumeric` | 1.12.62 | GNOME spreadsheet — accurate statistics, and reads Excel, OpenDocument and Lotus files | `44_apps` |
| `goffice` | 0.10.62 | GLib/GTK office library — charts, formats and canvas shared by Gnumeric and AbiWord | `43_toolkits` |
| `homebank` | 5.10.3 | Personal finance manager — accounts, budgets, reports, QIF, CSV and OFX import | `44_apps` |
| `ledger` | 3.4.1 | Double-entry accounting over a plain-text journal | `44_apps` |
| `libcdr` | 0.1.9 | CorelDRAW file import library | `41_system` |
| `libgsf` | 1.14.59 | GNOME Structured File library — OLE2, ZIP and ODF containers for office formats | `41_system` |
| `libixion` | 0.20.0 | Threaded spreadsheet formula engine, the calculation half of orcus | `41_system` |
| `libofx` | 0.10.9 | Open Financial Exchange (OFX, QFX) statement parser library, ofxdump and ofx2qif | `41_system` |
| `liborcus` | 0.20.2 | Spreadsheet import filters for ODS, XLSX, Excel 2003 XML, Gnumeric, CSV, JSON and YAML | `41_system` |
| `libreoffice` | 26.8.0.3 | Office suite — Writer, Calc, Impress, Draw, Math and Base, with the gtk3 and qt6 front ends | `44_apps` |
| `librevenge` | 0.0.6 | Base library for document import filters: the drawing, text and spreadsheet interfaces | `41_system` |
| `libvisio` | 0.1.11 | Microsoft Visio diagram import library | `41_system` |
| `libwpd` | 0.10.3 | WordPerfect document import library | `41_system` |
| `libwpg` | 0.3.4 | WordPerfect Graphics import library | `41_system` |
| `libxlsxwriter` | 1.2.4 | C library for writing Excel XLSX files | `41_system` |
| `mdds` | 3.2.1 | Header-only multi-dimensional data structures for ixion and orcus | `41_system` |
| `presenterm` | 0.16.1 | Slides from a markdown file, drawn in the terminal | `41_system` |
| `sc-im` | 0.8.5 | Terminal spreadsheet with vi keys | `44_apps` |
| `tryton` | 7.0.44 | Desktop client for the Tryton business platform and GNU Health, in GTK 3 | `44_apps` |
| `wv` | 1.2.9 | Library and converters for Microsoft Word 2000, 97, 95 and 6 documents | `41_system` |

Notes:

- `abiword`: the plugin list is explicit, so a missing library fails configure rather than
  dropping a file format. The desktop entry claims only `application/x-abiword`; the shared
  office types belong to LibreOffice.
- `gnucash`: `-Wno-error` follows the `-Werror` upstream prepends on every Unix build, so a newer
  compiler's warning does not stop it. GTest is required at configure time with the tests off.
- `gnumeric`: the desktop entry claims only `application/x-gnumeric`; the spreadsheet types belong
  to LibreOffice Calc.
- `ledger`: `BOOST_REGEX_UNICODE_RUNS=1` is set because its probe does not compile under GCC 15
  and would report no, dropping case-folded matching of non-ASCII names. `ninja doc` runs before
  the install, whose rule names the info manual whether or not it was built. The vendored utfcpp
  include goes in `CXXFLAGS`, or the unit tests do not compile.
- `libreoffice`: the vendor bundle holds every file `download.lst` names, at the name and sha256
  it gives. On a version bump it is rebuilt from the new `download.lst` (each `*_TARBALL`,
  `*_JAR` or `*_TTF` with its `*_SHA256SUM`), fetched from `dev-www.libreoffice.org/src` or
  `/extern`, placed under `externals/` and packed with `ports/fetch`'s vendor tar flags plus
  `--mode=go-w`. The configure patches change `configure` alone, because a `configure.ac` newer
  than the configuration makes the build rerun `autogen.sh`. `container` is set because the
  top-level Makefile refuses to run as root without it.
- `liborcus`: its library names carry the series (`-0.20`), which LabPlot's finder looks for.
- `libxlsxwriter`: `USE_DTOA_LIBRARY=ON` keeps a comma-decimal locale from writing `1,5` into a
  cell's XML.
- `presenterm`: the highlighting sets are bat's, copied from `/usr/share/bat/assets` before cargo
  runs and read as unversioned bincode of presenterm's own syntect types, so a bump of `bat` or
  `presenterm` checks that both `Cargo.lock` files name the same syntect major.
- `sc-im`: `CFLAGS` is exported, never passed on the make line, because the Makefile builds its
  configuration with `CFLAGS +=`. `LUA_PKGNAME=lua` keeps the scripting Lua from following
  whichever of `lua` and `lua54` is installed.
- `tryton`: stays on the 7.0 series, which GNU Health 5.0 runs on; a client speaks only to a
  server of its own series.
- `wv`: `MKDIR_P` is given to configure, which accepts only GNU or BusyBox `mkdir` and otherwise
  falls back to a bundled `install-sh` with no `-d`, so the install stops at its first directory.

### documents

PDF, PostScript, DjVu, EPUB, CHM and DOCX viewers, libraries and tools, and ebook managers. 22
ports, under `ports/core/documents/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `calibre` | 9.15.0 | E-book library manager, converter, viewer and editor | `44_apps` |
| `catdoc` | 0.95 | The text of Word, Excel and PowerPoint 97–2003 files on standard output — catdoc, xls2csv and catppt | `41_system` |
| `chmlib` | 0.40a | Library for reading Microsoft Compiled HTML Help (CHM) files, with extract_chmLib | `41_system` |
| `convertlit` | 1.8 | clit, which unpacks and converts Microsoft Reader (.lit) ebooks to OEB | `41_system` |
| `djvulibre` | 3.5.30 | DjVu document library and command-line tools — ddjvu, djvused, c44, cjb2 | `42_graphics` |
| `docx2txt` | 1.4 | Extracts the text of a .docx to standard output | `41_system` |
| `doxx` | 0.1.4 | A .docx reader in the terminal | `41_system` |
| `ebook-tools` | 0.2.2 | EPUB reading library (libepub) and the einfo tool | `41_system` |
| `epy` | 2023.6.11 | An ebook reader in the terminal — epub, mobi, azw3, fb2 | `41_system` |
| `jbig2dec` | 0.20 | JBIG2 image decoder, the scanned-page codec in PDF | `41_system` |
| `koreader` | 2026.07.2 | E-book and document reader for EPUB, PDF, DjVu, FB2 and comics, on SDL3 with no toolkit | `42_graphics` |
| `libharu` | 2.4.6 | Haru: a C library for writing PDF files — Blender's Grease Pencil PDF export | `41_system` |
| `libspectre` | 0.2.12 | PostScript rendering library over the Ghostscript API | `41_system` |
| `mupdf` | 1.28.4 | mutool — the PDF the command line can take apart — and the mupdf-gl and mupdf-x11 viewers | `42_graphics` |
| `okular` | 26.08.1 | Universal document viewer — PDF, PostScript, DjVu, EPUB, XPS, comics, Markdown | `44_apps` |
| `pdf4qt` | 1.6.0.0 | PDF viewer and editor — annotate, redact, sign, compare, split and merge pages | `44_apps` |
| `pdfio` | 1.6.5 | C library for reading and writing PDF files | `41_system` |
| `pdfium` | 7988 | Chromium's PDF renderer, as a shared library with its C headers | `42_graphics` |
| `podofo` | 1.1.2 | C++ library to read, create and modify PDF documents | `41_system` |
| `poppler` | 26.09.0 | PDF rendering library, and pdftotext with the other poppler utilities | `43_toolkits` |
| `poppler-data` | 0.4.12 | CMaps and encoding tables poppler needs to render CJK and Cyrillic PDFs | `41_system` |
| `qpdf` | 12.4.1 | Restructure a PDF — split, merge, decrypt, repair | `41_system` |

Notes:

- `calibre`: apsw's `setup.apsw` is deleted before the vendored modules install, so apsw links the
  sqlite port instead of downloading SQLite's source. `setup.py install` empties calibre's library
  directory, so the vendored modules are copied into `/usr/lib/calibre` after it.
- `chmlib`: `--disable-io64` stays: musl's `off_t` is 64-bit and musl exports no `pread64` or
  `lseek64`, so the LFS64 path fails to link.
- `koreader`: the second source, `koreader-thirdparty-<version>.tar.xz`, is made by hand: the tree
  after `make TARGET= KODEBUG= download-all`, with every `base/thirdparty/*/build/downloads`
  directory packed as `ports/fetch` packs a bundle. A version bump remakes it. `PKG_CONFIG_PATH` is
  unset so base's patched Leptonica and Tesseract are found before the system's, which k2pdfopt does
  not compile against.
- `mupdf`: `USE_SYSTEM_LIBS=yes` switches every vendored library to the system's at once; gumbo,
  mujs and lcms2mt are mupdf's own forks with no port and stay bundled through their
  `USE_SYSTEM_*=no`.
- `pdfium`: the vendor bundle's Chromium `//build` and abseil must be the revisions the branch's
  `DEPS` names (`_build_rev`, `_abseil_rev`), which the build checks first. `libpdfium.pc` carries
  the Chromium version python3-pypdfium2 reads and `pdfium.pc` the branch number libvips compares,
  so neither may become a link to the other.
- `poppler`: `-DENABLE_UNSTABLE_API_ABI_HEADERS=ON` installs the private headers GDAL's PDF driver
  compiles against, so a poppler bump is test-built with gdal. NSS stays off: with it on, NSS
  becomes the default signature backend in place of GPG.

### pim

Personal information: calendars, contacts, tasks, time tracking, notes and genealogy. 12 ports,
under `ports/core/pim/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `calcurse` | 4.8.2 | A calendar and todo list whose storage is two text files | `41_system` |
| `gramps` | 6.0.8 | Genealogy: family trees, people, places, sources and reports | `44_apps` |
| `khal` | 0.14.1 | Terminal calendar over a vdir | `41_system` |
| `khard` | 0.21.0 | Terminal address book over a vdir | `41_system` |
| `libical` | 4.0.5 | Reference implementation of the iCalendar and vCard formats | `41_system` |
| `nb` | 7.25.5 | Notes and bookmarks in a git repository, from one command | `41_system` |
| `qownnotes` | 26.9.13 | Plain-text Markdown notes with a live preview, to-do lists and optional Nextcloud sync | `44_apps` |
| `taskwarrior` | 3.5.0 | Tasks with dates, dependencies and a query language | `41_system` |
| `taskwarrior-tui` | 0.27.0 | Terminal UI for taskwarrior | `41_system` |
| `timewarrior` | 1.10.0 | Time tracking in a text file | `41_system` |
| `vdirsyncer` | 0.21.0 | Makes a CalDAV or CardDAV collection and a local vdir equal | `41_system` |
| `zim` | 0.77.2 | Desktop wiki — linked notebook pages, journal, tasks and attachments in plain text | `44_apps` |

Notes:

- `calcurse`: the source is a tag archive with no generated manual, so asciidoc and a2x are
  required. Configure's `auto` default demands both; `--with-asciidoc` alone leaves `calcurse.1`
  with no rule and fails the build after every binary has compiled. a2x's `manpage.xsl` imports
  docbook-xsl by URL, which resolves only through `XML_CATALOG_FILES`; without it xsltproc exits 5.
- `khard`: `vobject` stays at 0.9.8: 0.9.9 reads its version by importing itself, which needs
  dateutil in the isolated metadata build, and the download fails.
- `taskwarrior`: `LIBSQLITE3_SYS_USE_PKG_CONFIG` links the system sqlite, and
  `CMAKE_CXX_STANDARD_LIBRARIES=-lsqlite3` puts it after the Rust archive on every C++ link line, or
  `task` fails on `sqlite3_*`. cxxbridge is built from the vendor bundle with the workspace's
  `Cargo.lock`, because corrosion accepts only the exact cxx version and otherwise downloads one.
  `SYSTEM_CORROSION=ON` keeps corrosion from being cloned with FetchContent.
- `timewarrior`: naming `ASCIIDOCTOR_EXECUTABLE` makes the manual pages rendered rather than copied
  from the tarball, which would make the shipped pages depend on build order. The rendering branch
  installs only what it renders, so the `timew-day`, `timew-week` and `timew-month` redirects are
  installed from the tarball.
- `vdirsyncer`: `multidict` stays on 6.x, because aiohttp declares `multidict<7`. `expandvars` and
  `hatch-fancy-pypi-readme` are build backends of the bundle alone and go into a private
  `PYTHONPATH` directory, never into the package or the build root.
- `zim`: `setup.py` lists its data files before its build step renders `xdg/hicolor`, so the icons
  are copied after the install, or the Start row has no picture.

### printing

Printing, printer drivers, labels, scanning and OCR. 26 ports, under `ports/core/printing/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `brlaser` | 6.2.8 | CUPS driver for Brother monochrome laser printers | `41_system` |
| `cups` | 2.4.20 | Print spooler and associated utilities | `41_system` |
| `cups-browsed` | 2.1.1 | Daemon that turns printers announced on the network into local CUPS queues | `43_toolkits` |
| `cups-filters` | 2.0.1 | The filters CUPS needs to turn a job into something a printer prints | `43_toolkits` |
| `foomatic-db` | 20260926 | Foomatic printer database — printer and driver XML plus manufacturer PPDs for CUPS | `44_apps` |
| `foomatic-db-engine` | 4.0.13 | Foomatic PPD generator — builds CUPS PPDs from the foomatic-db printer database | `43_toolkits` |
| `ghostscript` | 10.08.0 | PostScript and PDF interpreter | `41_system` |
| `gimagereader` | 3.4.3 | OCR front end for Tesseract — scan or open images and PDFs, recognise and edit text | `44_apps` |
| `glabels` | 3.4.1 | Labels, business cards and envelopes, with barcodes and mail merge | `44_apps` |
| `gutenprint` | 5.3.5 | Printer drivers for inkjet and dye-sublimation printers | `41_system` |
| `hplip` | 3.26.4 | HP printer and scanner drivers — hpcups, the hp backend, PPDs and the hpaio SANE backend | `44_apps` |
| `ipp-usb` | 0.9.34 | A USB printer that speaks IPP-over-USB, presented to cups as a network one | `41_system` |
| `ksanecore` | 26.08.1 | Qt 6 library that drives SANE scanners, without a user interface | `43_toolkits` |
| `libcupsfilters` | 2.2.1 | The filter functions behind every CUPS print filter, as a library | `43_toolkits` |
| `libksane` | 26.08.1 | Qt 6 scanner widget over ksanecore, used by Skanlite and KolourPaint | `43_toolkits` |
| `libpaper` | 2.3.0 | Library and paper(1) tool for the system's default paper size and the known paper sizes | `41_system` |
| `libppd` | 2.1.1 | The PPD handling CUPS 3 drops, as a library for its filters and daemons | `43_toolkits` |
| `ocrmypdf` | 17.12.1 | Adds an invisible OCR text layer to a scanned PDF, in place | `42_graphics` |
| `ptouch-print` | 1.9 | Prints text and images on Brother P-touch label printers over USB | `42_graphics` |
| `sane-airscan` | 0.99.38 | Driverless scanning over eSCL and WSD | `44_apps` |
| `sane-backends` | 1.4.0 | The scanner driver set, and the udev rules 70-kdos-*.rules already grant | `43_toolkits` |
| `skanlite` | 26.08.1 | Image scanning application — preview, select and save scans through SANE | `44_apps` |
| `splix` | 2.0.2 | CUPS driver for Samsung, Xerox, Dell and Lexmark SPL2/SPLc laser printers | `44_apps` |
| `system-config-printer` | 1.5.18 | Printer configuration tool for CUPS — add, configure and manage print queues (GTK 3) | `44_apps` |
| `tessdata-eng` | 4.1.0 | English and script-detection models for the tesseract OCR engine | `41_system` |
| `tesseract` | 5.5.3 | OCR engine — text from scanned images | `41_system` |

Notes:

- `cups-filters`: every filter is named on the configure line: each switch answers a missing
  dependency by turning its filter off, and the package then installs and cannot render a PDF, an
  image or a PostScript job. foomatic-rip is C17 code, in which `()` is not `(void)`. The install
  runs `-j1`, because the bundled install-sh races on directories it creates in parallel.
- `foomatic-db`: the manufacturer PPDs are compressed with `gzip -n`; upstream's bare gzip records
  each file's time and name and makes the package differ on every build.
- `ghostscript`: the bundled trees of libraries the tree ships are deleted, not disabled: configure
  picks bundled or system per library by whether its directory exists, and stops on a mix such as
  a bundled libjpeg with a system libtiff. `lcms2mt`, `leptonica` and `tesseract` stay: gs's lcms is
  a thread-safe fork the system lcms2 is not, and the OCR device is version-locked to its
  leptonica and tesseract. `--enable-dynamic` is refused alongside the default hidden visibility.
- `glabels`: the Zint backend calls `ZBarcode_Render`, which the zint port's API lacks, so it stays
  off; QR codes come from qrencode and the rest from the built-in encoders.
- `gutenprint`: the dye-sub backend (`gutenprint53+usb`) is built only when configure finds
  libusb-1.0, so libusb stays in `depends`.
- `hplip`: `--disable-imageProcessor-build` keeps hpcups off `libImageProcessor`, a prebuilt closed
  object in the tarball; no prebuilt object from the tarball may reach the package.
- `libcupsfilters`: configure runs `gs` and `pdftoppm` to find them, so ghostscript and poppler are
  build dependencies as well as run-time ones.
- `libppd`: `--disable-ppdc-utils` is required: cups installs `ppdc`, `ppdhtml`, `ppdi`, `ppdmerge`
  and `ppdpo` under the same names, and two packages cannot own one file.
- `ocrmypdf`: `Occulta.ttf` is regenerated with `time.time()` pinned to `SOURCE_DATE_EPOCH`, and
  each glyph's advance comes from Python's `unicodedata`, so a python3 bump that moves the Unicode
  version changes the font. `pydantic` is pinned to the release that names python3-pydantic-core's
  exact version.
- `ptouch-print`: the source file is `git archive --prefix=ptouch-print-$version/ $_commit | gzip -n
  -9` of the `v$version` tag and exists only in the source archive; upstream's cgit serves no
  snapshots. git must be on the path or cmake refuses to configure.
- `sane-backends`: the GitLab tag archive has no `.tarball-version`, which build.sh writes; without
  it the version is `UNKNOWN` and every backend fails to compile inside `SANE_VERSION_CODE`. udev
  gets the scanner table as `20-sane.hwdb` and `65-libsane.rules` from `tools/sane-desc` in the
  udev+hwdb form: the plain form names a `scanner` group this system does not have, which eudev
  resolves to gid 0.
- `splix`: JBIG stays on (`DISABLE_JBIG=0`): with it off the colour models are still listed in the
  `.drv` files and fail at print time.
- `tesseract`: `DISABLE_*=OFF` only means "look", and FindCURL cannot be made required against the
  autotools-built curl, so build.sh checks `config_auto.h` for TIFF, libarchive and libcurl and
  stops when one is missing.

### math

Mathematics applications and libraries: computer algebra, arbitrary precision, solvers, calculators,
statistics and plotting apps. 27 ports, under `ports/core/math/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `analitza` | 26.08.1 | KDE mathematical expression library — the parser, evaluator and 2D/3D plotter behind KAlgebra | `43_toolkits` |
| `cantor` | 26.08.1 | Worksheet front end for Python, R, Octave, Maxima, Qalculate, KAlgebra and Lua | `43_toolkits` |
| `ceres-solver` | 2.2.0.git20251109 | Ceres Solver: non-linear least squares — Blender's camera and motion tracker | `41_system` |
| `coolprop` | 8.0.0 | Thermophysical properties for 122 fluids, IAPWS-95 water/steam and humid air | `41_system` |
| `cudd` | 3.0.0 | CUDD, the Colorado decision-diagram package — BDDs, ADDs and ZDDs for yices2's bit-vector reasoning | `41_system` |
| `glpk` | 5.0 | Linear and mixed-integer programming, with its own modelling language | `41_system` |
| `gmp` | 6.3.0 | Arbitrary-precision arithmetic library | `20_selfhost` (as a dependency) |
| `gnuplot` | 6.0.5 | Plotting program driven by scripts, drawing to the terminal, to a Qt window or to PNG | `43_toolkits` |
| `highs` | 1.15.1 | Linear and mixed-integer programming solver | `41_system` |
| `kalgebra` | 26.08.1 | KDE graph calculator — expressions, 2D and 3D plots, and a console calculator | `44_apps` |
| `labplot` | 2.12.1 | Data visualisation and analysis: plots, fits, FFT, spreadsheets and live data | `44_apps` |
| `libpoly` | 0.2.1 | Polynomial arithmetic and real algebraic numbers — the library yices2's MCSAT solver reasons with | `41_system` |
| `libqalculate` | 5.12.0 | Calculator library with unit and expression parsing | `41_system` |
| `mpc` | 1.4.1 | A library for the arithmetic of complex numbers with arbitrarily high precision | `20_selfhost` (as a dependency) |
| `mpfr` | 4.2.2 | Functions for multiple precision math | `20_selfhost` (as a dependency) |
| `mpdecimal` | 4.0.1 | Correctly rounded arbitrary precision decimal floating point arithmetic — the library behind Python's decimal module | `30_foundation` |
| `nlopt` | 2.11.0 | Nonlinear optimisation library used by slicers and nesting tools | `41_system` |
| `numbat` | 1.24.0 | Calculator with physical units and dimension checking | `41_system` |
| `octave` | 11.3.0 | MATLAB-compatible numerical computing, with its Qt 6 desktop | `44_apps` |
| `pari` | 2.17.4 | Number theory and arbitrary-precision arithmetic, at a prompt | `41_system` |
| `qalculate-qt` | 5.12.0 | Qalculate! desktop calculator (Qt) — units, currencies, symbolic algebra, plots | `44_apps` |
| `rkward` | 0.8.3 | KDE front end to R: data editor, plots, dialogs and R Markdown | `44_apps` |
| `sympy` | 1.14.0 | Symbolic mathematics for Python | `41_system` |
| `units` | 2.27 | Unit conversion program and its units database | `41_system` |
| `veusz` | 4.2.1 | Publication-quality 2D and 3D scientific plots, in Python and PyQt6 | `44_apps` |
| `yices2` | 2.7.0 | The SMT solver SymbiYosys proves properties with | `41_system` |
| `z3` | 5.1.0 | Microsoft Research's SMT solver and theorem prover — libz3, the z3 command and its Python bindings | `41_system` |

Notes:

- `ceres-solver`: the version is a commit, not a release, because the 2.2.0 release asks for Eigen
  3.3 exactly and the eigen port is 5.x.
- `coolprop`: every `CPMAddPackage` is given a local tree through `CPM_<name>_SOURCE`; one left out
  is a network fetch at configure time.
- `highs`: `BUILD_OPENBLAS` stays off, or configure fetches OpenBLAS. `libhighs_extras.so`, which
  libhighs opens at solve time for `--solver hipo`, is installed by hand, because upstream installs
  it only in a static build.
- `labplot`: Cantor and NetCDF are checked in `build.ninja` after configure instead of required,
  because their find modules pass a requirement on as a version. `EIGEN3_*` are set from
  `eigen3.pc`, because Eigen's config file defines none of them.
- `numbat`: the modules under `/usr/share/numbat` are run-time data; without them numbat starts and
  knows no units.
- `qalculate-qt`: qmake 6 reads no compiler flags from the environment, so `CFLAGS`, `CXXFLAGS` and
  `LDFLAGS` are passed as `QMAKE_*` assignments.

### sci-libs

Numerical and scientific-data libraries, and the unprefixed core Python science stack. 21 ports,
under `ports/core/sci-libs/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `arpack-ng` | 3.9.1 | Large sparse eigenvalue problems, for Octave's eigs and SciPy-style solvers | `41_system` |
| `eigen` | 5.0.1 | Header-only C++ linear algebra library (nextpnr's timing analysis uses it) | `41_system` |
| `fftw` | 3.3.11 | Fast Fourier transform library | `41_system` |
| `gsl` | 2.8 | GNU Scientific Library — numerical routines outside linear algebra | `41_system` |
| `hdf5` | 2.2.0 | Hierarchical Data Format library for scientific data | `41_system` |
| `libcerf` | 3.8 | Complex error, Faddeeva, Voigt and Dawson functions | `41_system` |
| `libmed` | 5.0.0 | MED-file, the Salome mesh and field format library over HDF5 (FreeCAD FEM meshes) | `41_system` |
| `matio` | 1.6.0 | Reads and writes MATLAB MAT files, v4, v5 and the HDF5-based v7.3 | `41_system` |
| `matplotlib` | 3.11.2 | 2D plotting library for Python, Agg backend only | `42_graphics` |
| `nco` | 5.4.0 | Subset, regrid and do arithmetic on gridded data without loading it | `42_graphics` |
| `netcdf-c` | 4.10.1 | Library for netCDF, the self-describing array format of climate, ocean and atmosphere data | `41_system` |
| `numpy` | 2.5.3 | N-dimensional array library for Python | `41_system` |
| `openblas` | 0.3.34 | Optimised BLAS and LAPACK library | `41_system` |
| `pandas` | 3.0.6 | Data frames for Python — tables, joins and group-bys | `42_graphics` |
| `qrupdate` | 1.2.0 | QR and Cholesky factorisation updates — Octave's qrupdate, cholupdate and friends | `41_system` |
| `scipy` | 1.18.1 | Scientific computing for Python — optimisation, integration, signal, sparse, statistics and physical constants | `41_system` |
| `spooles` | 2.2 | Sparse direct solver library (LU and Cholesky factorisation, serial and threaded), the default solver of CalculiX | `41_system` |
| `suitesparse` | 7.14.1 | Sparse matrix algebra — CHOLMOD, UMFPACK, KLU, SPQR and CXSparse, under Octave | `41_system` |
| `sundials` | 7.9.0 | ODE and DAE solvers, behind Octave's ode15s and ode15i | `41_system` |
| `udunits` | 2.2.28 | Unit conversion library for scientific data | `41_system` |
| `xsimd` | 14.3.0 | C++ wrappers for SIMD intrinsics (headers) | `41_system` |

Notes:

- `fftw`: configure builds one precision per run, so double, single (`--enable-float`) and long
  double are three builds into one package; the long-double run takes no SIMD flags. `F77=gfortran`
  is named so a missing Fortran compiler fails configure instead of dropping the Fortran wrappers.
- `gsl`: the patch makes `gsl.pc` and `gsl-config --libs` name OpenBLAS for the `cblas_*` symbols
  libgsl leaves undefined; `libgslcblas` is still installed. The installed headers are re-encoded
  from ISO-8859-1 to UTF-8, because a consumer that reads a header as text fails on upstream's.
- `hdf5`: the tarball's members are prefixed `./`, so kpkg's stripped unpack leaves the tree one
  level down, and `build.sh` starts with `cd "$name-$version"`.
- `libmed`: the HDF5 patch replaces upstream's check for exactly 1.12 with a floor of 1.12.1 and
  moves the internal-model guards past 1.14; the hdf5 port is 2.x, whose minor number is below both.
- `matplotlib`: `system-freetype`, `system-libraqm` and `system-qhull` are required: each default
  downloads its own copy, and the build has no network.
- `netcdf-c`: the HDF5 filter plugins install to `/usr/lib/hdf5/plugin`, which
  `NETCDF_WITH_PLUGIN_DIR` also puts first on libnetcdf's search path; hdf5's compiled-in default is
  under `/usr/local`, and without the plugins a zstd- or bzip2-compressed variable is unreadable.
- `openblas`: `TARGET=CORE2` names the CPU the code outside the `DYNAMIC_ARCH` kernels is built for;
  unset, getarch takes the build machine's, and the library can then fault on an older CPU before it
  reaches a kernel. `libblas`, `liblapack` and `libcblas` are links to `libopenblas`: consumers look
  the libraries up by those names, and without them a configure script reports no BLAS.
- `scipy`: `-Duse-pythran=false` is required while pythran is not a port: with it on, meson's lookup
  is unconditional and the build stops before compiling anything.
- `spooles`: the tarball has no wrapping directory, so `build.sh` unpacks it again unstripped. Make
  runs `-j1`: each makefile updates one archive member per object, and parallel `ar` runs corrupt
  the archive. One shared library carries the serial and the threaded solvers.
- `suitesparse`: `SUITESPARSE_USE_64BIT_BLAS=OFF` matches the openblas port's 32-bit integers; a
  mismatched width corrupts every BLAS call instead of failing the link.

### astronomy

Astronomy, planetaria, ephemerides, FITS and satellite prediction. 14 ports, under
`ports/core/astronomy/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `astropy` | 8.0.1 | Coordinates, times, FITS, WCS and cosmology | `42_graphics` |
| `celestia` | 1.7.0.git20260926 | Real-time 3D space simulator — fly through the solar system, the stars and the galaxies | `44_apps` |
| `celestia-content` | 1.7.0.git20260923 | Celestia's universe — star and deep-sky catalogues, planet textures, models and orbits | `43_toolkits` |
| `cfitsio` | 4.7.0 | Library for reading and writing FITS, the standard astronomical data format | `41_system` |
| `cspice` | 67.0.0.git20260416 | NASA NAIF's SPICE toolkit (N0067) for planetary ephemerides and observation geometry, as a shared C library | `41_system` |
| `erfa` | 2.0.1 | Essential Routines for Fundamental Astronomy — the IAU standard, relicensed | `41_system` |
| `gnuastro` | 0.24 | The GNU astronomy toolset — arithmetic, cropping, photometry, on FITS | `41_system` |
| `gpredict` | 2.6 | Real-time satellite tracking and pass prediction with radio and rotator control | `44_apps` |
| `libnova` | 0.15.0 | Celestial mechanics and astronomical calculation library — sun and moon rise, set and phase for Viking | `41_system` |
| `predict` | 3.0.2 | Curses satellite tracker — passes, doppler and footprints from a TLE | `41_system` |
| `sgp4` | 3.0 | C++ library for satellite orbit propagation from two-line element sets | `41_system` |
| `skyfield` | 1.55 | Planet, moon and satellite positions from JPL kernels | `41_system` |
| `stellarium` | 26.2 | Planetarium — a realistic sky in 3D, as seen with the eye, binoculars or a telescope | `44_apps` |
| `wcslib` | 8.9 | The FITS World Coordinate System — pixel to sky, and back | `41_system` |

Notes:

- `cfitsio`: `-D_GNU_SOURCE` is required: `drvrnet.c` calls `gethostbyname()`, which musl declares
  only under `_GNU_SOURCE` or `_BSD_SOURCE`, and the file builds whatever `--disable-curl` says.
  `F77=gfortran` makes a missing Fortran compiler a configure failure rather than a library without
  the Fortran API.
- `cspice`: the project turns on CMake's user package registry, so the configure runs with `HOME`
  inside the work tree.
- `gnuastro`: ghostscript is a build dependency: configure defines `PATH_GHOSTSCRIPT` only when it
  finds `gs`, and `pdf.c` uses it unconditionally.
- `sgp4`: the project declares GoogleTest through FetchContent even with the tests off; the recipe
  carries it as a source and points `FETCHCONTENT_SOURCE_DIR_GOOGLETEST` at it.
- `stellarium`: kpkg copies a `.zip` source into `$SRC` unextracted, so the build unpacks QXlsx and
  INDI itself before CPM's per-package source overrides name them.

### bioscience

Bioinformatics, chemistry and medical informatics or imaging. 12 ports, under
`ports/core/bioscience/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `avogadrolibs` | 2.0.0 | Avogadro libraries — molecular data, file formats, OpenGL rendering and Qt widgets for chemistry programs | `43_toolkits` |
| `bcftools` | 1.24 | Call and filter variants | `41_system` |
| `dcmtk` | 3.7.0 | The OFFIS DICOM toolkit — medical image conversion, network storage and query tools | `41_system` |
| `diamond` | 2.2.8 | Protein and translated-DNA sequence aligner, a faster BLASTP/BLASTX | `41_system` |
| `gnuhealth` | 5.0.7 | Hospital and clinic information system — patients, labs, pharmacy, imaging and stock, on Tryton | `42_graphics` |
| `hmmer` | 3.4 | Profile hidden Markov models — the sequence search Pfam is built on | `41_system` |
| `htslib` | 1.24 | The SAM/BAM/CRAM/VCF reader samtools and bcftools are built on | `41_system` |
| `mafft` | 7.526 | Multiple sequence alignment | `41_system` |
| `minimap2` | 2.31 | Align long reads to a reference | `41_system` |
| `openbabel` | 3.2.1 | Chemistry toolbox — reads, writes and converts over a hundred molecular file formats | `42_graphics` |
| `samtools` | 1.24 | Sort, index and view alignments | `41_system` |
| `seqkit` | 2.13.0 | FASTA/FASTQ manipulation toolkit | `41_system` |

Notes:

- `bcftools`: `--enable-libgsl` turns on the polysomy and association plugins and makes the binary
  GPL-3.0-or-later; without GSL it is MIT.
- `gnuhealth`: vobject is pinned to 0.9.8 in the bundle; 0.9.9's metadata imports dateutil, which
  the isolated fetch build lacks.
- `htslib`: `htslib`, `samtools` and `bcftools` share one version and move together; a mismatch
  fails at run time, not at link time.
- `mafft`: `PREFIX` is written into the wrapper script at build time, so it is the installed path
  and `DESTDIR` carries the staging root.
- `openbabel`: Boost is kept out and the Maestro and Coordgen formats off, because with Boost found
  they download their parsers at configure time.

### gis

GIS, maps, projections, GPS, navigation and location. 20 ports, under `ports/core/gis/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `gdal` | 3.13.3 | Raster and vector geospatial data format library | `43_toolkits` |
| `geoclue` | 2.8.2 | D-Bus geolocation service (what xdg-desktop-portal's Location portal asks) | `41_system` |
| `geos` | 3.15.0 | Computational geometry library for GIS software | `41_system` |
| `go-pmtiles` | 1.31.2 | Create, inspect and serve PMTiles map archives | `41_system` |
| `gpsd` | 3.27.5 | GPS daemon that presents receivers as a socket | `41_system` |
| `gpxsee` | 16.16 | GPXSee: GPS track, route and waypoint viewer and analyser over offline raster and vector maps | `44_apps` |
| `josm` | 19613 | Java OpenStreetMap editor: map data, GPS traces, imagery and validation (Java Swing) | `42_graphics` |
| `libgeotiff` | 1.7.4 | Reads and writes the georeferencing tags of GeoTIFF rasters, with listgeo and geotifcp | `41_system` |
| `librttopo` | 1.1.0 | RT Topology Library — PostGIS-style topology and validity functions, for SpatiaLite | `41_system` |
| `libspatialindex` | 2.1.0 | R-tree spatial indexing, under QGIS | `41_system` |
| `marble` | 26.08.1 | Marble: virtual globe and world atlas, with offline Atlas, satellite and OpenStreetMap vector views | `44_apps` |
| `opencpn` | 5.14.0 | Chart plotter and navigation for S-57/S-63 ENC and raster charts, AIS and GPS (wxWidgets) | `44_apps` |
| `organicmaps` | 2025.09.05.1 | Organic Maps: offline maps with search and turn-by-turn routing for walking, cycling and driving, from OpenStreetMap data | `44_apps` |
| `pdal` | 2.10.2 | Point cloud translation and processing: LAS/LAZ, E57, Draco, HDF and the pdal tool | `43_toolkits` |
| `proj` | 9.9.0 | Cartographic projection and coordinate transformation library | `41_system` |
| `qgis` | 3.44.15 | Desktop geographic information system, with PyQGIS and Processing | `44_apps` |
| `qmapshack` | 1.21.1 | QMapShack: offline topographic maps, elevation models, GPS tracks and route planning with Routino | `44_apps` |
| `routino` | 3.4.4 | Offline route planning over an OSM extract | `41_system` |
| `shapelib` | 1.6.3 | Read and write ESRI shapefiles and their dBase attribute tables | `41_system` |
| `viking` | 1.11 | GPS track, waypoint and route manager on maps, with MBTiles and gpsd (GTK 3) | `44_apps` |

Notes:

- `geoclue`: no consent agent runs, and upstream holds every `GetClient` until one registers;
  `no-agent.patch` lets a client through when the agent whitelist is empty, so the whitelist in
  the shipped `/etc/geoclue/conf.d/90-kdos.conf` stays empty or the Location portal's call hangs.
- `gpsd`: SConscript drops `dbus_export`, `bluez`, `usb` and `ncurses` without an error when their
  library is missing, so the `depends` line is what keeps them. Build and install are one `scons`
  run: a second `scons install` re-reads an option cache the first run does not leave, and fails.
- `gpxsee`: Qt 6's qmake reads no compiler flags from the environment, so the tree's flags, with
  the reproducibility maps, are passed on the qmake line.
- `opencpn`: GLEW's CMake package is ignored under `/usr/lib` and `/lib` alike, so FindGLEW
  searches for the library itself; through the package `GLEW_LIBRARY` is left `NOTFOUND`.
  `OCPN_TARGET_TUPLE` is given because the build otherwise asks `lsb_release`, which does not
  exist here.
- `qmapshack`: PROJ's exported target requires a C standard of whatever links it, and the project
  enables C++ alone; the project include that enables C is what lets CMake generate.
- `routino`: `Makefile.conf` assigns `CFLAGS` and `LDFLAGS` outright, so both are passed on the
  make line, with upstream's defines and libraries and without its `-g`. `HAVE_SWIG=` keeps out a
  Python binding that cannot build against Python 3.
- `shapelib`: `CMAKE_INSTALL_LIBDIR` is not passed: the project declares it a `PATH` cache entry,
  and a relative value from the command line is made absolute against the build directory.

### eda

Electronics: schematic and PCB, circuit simulation, antenna modelling, FPGA synthesis and
simulation, logic analysers and bench instruments. 29 ports, under `ports/core/eda/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `digital` | 0.31 | Digital logic designer and circuit simulator for teaching, with FSM and HDL export (Java Swing) | `42_graphics` |
| `horizon-eda` | 2.7.2 | Schematic capture and PCB layout around a pool of parts with unique identities | `44_apps` |
| `icestorm` | 1.1 | The iCE40 bitstream database, packer and programmer | `41_system` |
| `iverilog` | 13_0 | Event-driven Verilog simulation with real delays and X propagation | `41_system` |
| `kicad` | 10.0.6 | Schematic capture, PCB layout, Gerber viewer and SPICE simulation for electronics design | `44_apps` |
| `kicad-footprints` | 10.0.6 | KiCad's official PCB footprint library, with the global fp-lib-table template | `41_system` |
| `kicad-symbols` | 10.0.6 | KiCad's official schematic symbol library, with the global sym-lib-table template | `41_system` |
| `kicad-templates` | 10.0.6 | KiCad's official project templates — board outlines and starting points for common form factors | `41_system` |
| `liblxi` | 1.22 | Discover and talk to an LXI instrument | `41_system` |
| `libmodbus` | 3.2.0 | Modbus RTU and TCP library | `41_system` |
| `librepcb` | 2.1.1 | Schematic and PCB design with managed libraries, STEP 3D export and Gerber output (Qt 6, Slint) | `44_apps` |
| `libsigrok` | 0.5.2 | Hardware drivers for logic analysers, oscilloscopes and meters | `41_system` |
| `libsigrokdecode` | 0.5.3 | Protocol decoder library, with the decoders written in Python | `41_system` |
| `logisim-evolution` | 5.0.0 | Digital logic designer and simulator with FPGA synthesis export (Java Swing) | `42_graphics` |
| `lxi-tools` | 2.8 | Drive a bench instrument over ethernet with SCPI, from the lxi command or the lxi-gui window | `44_apps` |
| `mbpoll` | 1.5.4 | Read and write Modbus registers from a prompt, over RTU or TCP | `41_system` |
| `nec2c` | 1.3.3 | NEC-2 antenna simulation from a card deck | `41_system` |
| `nextpnr` | 0.11.1 | Place and route — the second stage of the open FPGA flow | `44_apps` |
| `ngspice` | 47 | SPICE circuit simulation from a prompt — transient, AC, DC, noise, Monte Carlo | `41_system` |
| `openfpgaloader` | 1.1.1 | FPGA programming tool for the boards the FPGA flow targets | `41_system` |
| `prjtrellis` | 1.4.git20260920 | The ECP5 bitstream database and packer | `41_system` |
| `qucs-s` | 26.1.1 | Circuit schematic capture and simulation front end for ngspice, with RF filter, attenuator and line calculators | `44_apps` |
| `sigrok-cli` | 0.7.2 | Command-line capture and protocol decoding for sigrok hardware | `41_system` |
| `sigrok-firmware-fx2lafw` | 0.1.7 | The firmware libsigrok uploads to a Cypress FX2 logic analyser, compiled here with sdcc | `41_system` |
| `surfer` | 0.7.0 | Waveform viewer for VCD, FST and GHW simulation traces, drawn with egui | `42_graphics` |
| `symbiyosys` | 0.69 | Formal property verification over yosys — sby, and the solvers behind it | `44_apps` |
| `verilator` | 5.052 | Verilog simulator and linter | `41_system` |
| `xnec2c` | 4.4.18 | GTK antenna modelling with NEC2: patterns, impedance, gain and SWR over frequency | `44_apps` |
| `yosys` | 0.69 | Verilog synthesis — the first stage of the open FPGA flow | `43_toolkits` |

Notes:

- `digital`: the jar is compiled from its dependencies' source jars on javac's source path. XStream
  loads its mappers and converters by name, so all of it is compiled except the drivers for
  libraries KDOS does not carry. The manifest's revision, `v<version>-kdos`, is longer than seven
  characters, which keeps Digital from asking api.github.com for a newer release.
- `kicad`: `common/pcb_lexer.h` is built as its own target first: the generated header is not
  ordered before its first consumer, and a parallel build can compile against it before it exists.
- `libsigrok`: `60-libsigrok.rules` only marks supported devices with `ID_SIGROK`;
  `fs/etc/udev/rules.d/70-kdos-sigrok.rules` turns the mark into the `dialout` grant. VXI-11 is
  built only when configure links `clnt_create`, which needs libtirpc's flags and
  `-D_DEFAULT_SOURCE`; the build checks `HAVE_RPC`.
- `libsigrokdecode`: the library links `pkg-config --libs python3-embed`: configure's probe falls
  through to plain `python3`, which carries no `-lpython`, and every consumer then fails to link on
  Python symbols.
- `librepcb`: the vendor bundle is unpacked into `$SRC_ROOT`: Cargo reads its configuration from
  `CARGO_HOME` and resolves the bundle's relative vendor directory against that directory's parent.
- `logisim-evolution`: the libraries that find their parts by name at run time — FlatLaf,
  RSyntaxTextArea, JavaHelp, SwingX's look-and-feel add-ons and slf4j-simple — are compiled whole;
  of the rest, javac compiles what Logisim reaches.
- `mbpoll`: `MBPOLL_CUSTOM_RTS_GPIO=OFF`: with it on and libpiduino found, mbpoll installs setuid
  root.
- `nextpnr`: icestorm's and prjtrellis's chip databases are read when nextpnr is compiled, so a
  database installed after it is never seen. `pybind11_DIR` names the system pybind11, whose CMake
  package sits inside site-packages; a miss falls back to the bundled copy without a word.
  `CMP0167=NEW` finds Boost through its own config package, which knows Boost.System is header-only.
  Only the MachXO2 parts in `MACHXO2_DEVICES` get a chip database.
- `ngspice`: `--without-x` is spelled out, because configure looks for X by default and stops on a
  missing Xaw. One configure builds either the program or `libngspice`, so each has its own build
  directory.
- `prjtrellis`: the database tarball is unpacked at the top of the source tree, where
  `install(DIRECTORY ../database)` reads it; anywhere else it installs empty and nextpnr's ECP5 chip
  database fails to generate. `CURRENT_GIT_VERSION` is set because libtrellis takes its version from
  `git describe`, and `BUILD_PYTHON=ON` builds the pytrellis module nextpnr's ECP5 target requires.
- `qucs-s`: the source is the release archive, which carries the qucsator-rf, RxCalc and S-parameter
  viewer submodules; a tag archive leaves them out and the build still succeeds.
- `surfer`: the vendor bundle is made by hand, `cargo vendor` over the tree with the f128 and
  instruction-decoder submodules in place; `ports/fetch` vendors from the first source alone, where
  those path dependencies are missing.
- `verilator`: `help2man` is a build dependency with no switch to drop it: the default target pipes
  each program's `--help` through it, and without it the build exits 127 at `verilator_gantt.1`.
- `yosys`: the source is the release `yosys.tar.gz`, which carries the abc submodule a tag archive
  omits. It has no top-level directory, and kpkg strips one from the first source, so the build
  unpacks it again unstripped.

### embedded

Microcontroller and bare-metal development: cross toolchains, C libraries, flashers, debug probes
and serial consoles. 30 ports, under `ports/core/embedded/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `avr-libc` | 2.3.2 | The C library for AVR microcontrollers | `41_system` |
| `avrdude` | 8.3 | Programmer for AVR microcontrollers — flash, EEPROM and fuse bytes | `41_system` |
| `binutils-arm-none-eabi` | 2.47 | The assembler, linker and object tools for arm-none-eabi | `41_system` |
| `binutils-avr` | 2.47 | The assembler, linker and object tools for avr | `41_system` |
| `binutils-riscv64-unknown-elf` | 2.47 | The assembler, linker and object tools for riscv64-unknown-elf | `41_system` |
| `dfu-util` | 0.11 | Flashes a device over USB DFU, with no separate programmer | `41_system` |
| `espflash` | 4.6.0 | Flash an ESP32 or ESP8266 over its own ROM bootloader, and watch its serial output | `41_system` |
| `flashrom` | 1.8.0 | Read and write a SPI flash chip — including this machine's BIOS | `41_system` |
| `gcc-arm-none-eabi` | 16.2.0 | The bare-metal Cortex-M and Cortex-R compiler | `41_system` |
| `gcc-avr` | 16.2.0 | GCC for AVR microcontrollers (ATmega and ATtiny) | `41_system` |
| `gcc-riscv64-unknown-elf` | 16.2.0 | The RISC-V bare-metal compiler, for MCUs and the soft cores the FPGA flow synthesises | `41_system` |
| `gputils` | 1.5.2 | Assembler, linker and librarian for Microchip PIC microcontrollers — the back end of sdcc's PIC14 and PIC16 ports | `41_system` |
| `libjaylink` | 0.5.0 | Library to drive SEGGER J-Link debug probes over USB and TCP | `41_system` |
| `libstdcxx-arm-none-eabi` | 16.2.0 | The C++ standard library for arm-none-eabi, built against picolibc | `41_system` |
| `libstdcxx-riscv64-unknown-elf` | 16.2.0 | The C++ standard library for riscv64-unknown-elf, built against picolibc | `41_system` |
| `lrzsz` | 0.12.20 | X, Y and ZMODEM file transfer over a serial line | `41_system` |
| `minicom` | 2.11.1 | Serial terminal program | `41_system` |
| `openocd` | 0.12.0 | On-chip debugging and flashing over JTAG or SWD | `41_system` |
| `picocom` | 3.1 | Minimal serial terminal | `41_system` |
| `picolibc-arm-none-eabi` | 1.8.12 | The C library for arm-none-eabi — newlib trimmed for a microcontroller | `41_system` |
| `picolibc-riscv64-unknown-elf` | 1.8.12 | The C library for riscv64-unknown-elf — newlib trimmed for a microcontroller | `41_system` |
| `picotool` | 2.3.1 | RP2040 and RP2350 UF2 inspection and USB loading | `41_system` |
| `probe-rs` | 0.32.0 | ARM and RISC-V flash and debug over CMSIS-DAP, ST-Link and J-Link, with RTT | `41_system` |
| `sdcc` | 4.6.0 | A C compiler for the 8-bit families AVR does not cover — 8051, STM8, Z80, PIC | `41_system` |
| `srecord` | 1.65 | HEX, S-record and TI-TXT conversion, fill, CRC and offset | `41_system` |
| `stlink` | 1.8.0 | The ST-Link tools — flash and debug an STM32 directly | `41_system` |
| `stlink-gui` | 1.8.0 | GTK 3 window over the ST-Link tools: connect a probe, read and write STM32 flash | `44_apps` |
| `stm32flash` | 0.7 | Flash an STM32 over its own ROM bootloader, with no debugger attached | `41_system` |
| `tio` | 3.9 | Serial terminal that reconnects when the device returns | `41_system` |
| `xa` | 2.4.1 | Cross-assembler for the 6502, 65816 and R65C02 (xa65), with the o65 relocation tools | `41_system` |

Notes:

- `gcc-arm-none-eabi`, `gcc-avr`, `gcc-riscv64-unknown-elf`: only `all-gcc` and
  `all-target-libgcc` are built; a plain `make` reaches for the target libstdc++, which needs a C
  library that does not exist yet. Multilib stays on: each core gets its own libgcc, and without
  it only the default core links. The `libstdcxx-*` ports configure the same source and keep the
  compiler's prefix, sysroot and multilib options.
- `libstdcxx-arm-none-eabi`, `libstdcxx-riscv64-unknown-elf`: the install runs in the target's
  `libstdc++-v3` directory; the top-level `install-target-libstdc++-v3` installs libgcc first and
  would give the package every file the cross compiler's port owns.
- `lrzsz`: the install runs with one job: `install-exec-local` links `lsz` to `lsb` with no
  dependency on the rule that installs `lsz`.
- `minicom`: configured through `autoreconf`, not `autogen.sh`, which runs `aclocal-1.16` by its
  versioned name.
- `openocd`: `--disable-linuxgpiod` stays while openocd's driver is written to libgpiod's v1 API
  and the port is v2. Every adapter the build enables needs its USB ids in
  `fs/etc/udev/rules.d/70-kdos-debug.rules` or `70-kdos-serial.rules`; upstream's rules file is not
  installed, and an adapter missing there opens for root only.
- `picocom`: `NO_CUSTOM_BAUD`: musl does not expose `termios2`, so only the standard rates up to
  4000000 work. `UUCP_LOCK_DIR` must match every other serial program's lock directory.
- `picotool`: the install is `cmake --install`, never `make install`, which reruns each device
  stub's ExternalProject install and copies the stubs into the package under the source tree's
  path.
- `srecord`: the three programs are built by name; `all` takes in the `doc` targets, which need
  groff, a2ps and TeX.
- `stlink`: `-Wno-error` goes in `CMAKE_C_FLAGS_RELEASE`, because upstream appends `-Werror` to
  `CMAKE_C_FLAGS` after the environment's flags. Upstream's udev rules (`MODE:="0666"`) are
  deleted; `fs/etc/udev/rules.d/70-kdos-debug.rules` grants vendor 0483 to `dialout`.
- `xa`: built and installed with one job: `all` deletes `./xa` beside the target that links it.

### sdr-hw

SDR hardware drivers and the SoapySDR device layer. 19 ports, under `ports/core/sdr-hw/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `airspy` | 1.0.10 | libairspy and the airspy_* tools for the Airspy R2 and Mini receivers | `41_system` |
| `airspyhf` | 1.8.1.git20260722 | libairspyhf and the airspyhf_* tools for the Airspy HF+ Discovery and Dual Port receivers | `41_system` |
| `bladerf` | 2025.10 | libbladeRF and bladeRF-cli for the Nuand bladeRF and bladeRF 2.0 micro transceivers | `41_system` |
| `hackrf` | 2026.01.3 | libhackrf and the hackrf_* tools for the HackRF One, Jawbreaker and rad1o transceivers | `41_system` |
| `libad9361` | 0.4.0 | AD936x transceiver helpers over libiio — filter design and multichip sync for ADALM-Pluto in gr-iio | `41_system` |
| `libfobos` | 2.4.1.git20260618 | Driver library and fobos_* tools for the RigExpert Fobos SDR receiver | `41_system` |
| `libiio` | 0.26 | Linux Industrial I/O client library — ADALM-Pluto and other ADI SDRs over USB and the network, for gr-iio and SDRangel | `41_system` |
| `libmirisdr` | 2.0.0 | Driver library and the miri_sdr and miri_fm tools for Mirics MSi2500/MSi001 receivers (libmirisdr-4) | `41_system` |
| `libperseus-sdr` | 0.8.2 | Driver library and perseustest for the Microtelecom Perseus HF receiver | `41_system` |
| `librfnm` | 0.2.0.git20240716 | Host library and rfnm_info for RFNM software-defined radio boards | `41_system` |
| `limesuite` | 23.11.0.git20260603 | Driver library, LimeUtil and the SoapySDR module for LimeSDR boards | `41_system` |
| `limesuite-gui` | 23.11.0.git20260603 | The wxWidgets control panel for LimeSDR boards: LMS7002M registers, calibration and FFT viewer | `44_apps` |
| `rtl-sdr` | 2.0.3 | RTL2832U SDR driver library and the rtl_* tools | `41_system` |
| `soapyairspy` | 0.2.0 | The SoapySDR module for Airspy R2 and Mini receivers, through libairspy | `41_system` |
| `soapybladerf` | 0.4.2 | The SoapySDR module for the bladeRF family, through libbladeRF | `41_system` |
| `soapyhackrf` | 0.3.4 | The SoapySDR module for the HackRF family, through libhackrf | `41_system` |
| `soapyrtlsdr` | 0.3.3 | The SoapySDR module for RTL2832U sticks, through librtlsdr | `41_system` |
| `soapysdr` | 0.8.1 | Vendor-neutral API over SDR hardware | `41_system` |
| `uhd` | 4.10.0.0 | USRP Hardware Driver — Ettus/NI USRP radios for GNU Radio's gr-uhd, with the uhd_* utilities and Python API | `41_system` |

Notes:

- `airspy`: `-std=gnu17` is required: `airspy.c` declares its own `typedef int bool`, which C23
  rejects.
- `bladerf`: the release archive carries its no-OS and bladeRF-fsk submodules empty, so each is a
  source of its own at the commit the tag pins; configure stops without bladeRF-fsk.
- `libfobos`: the source is upstream's branch at 2.4.1, not the 2.4.0 tag, which lacks the
  device-open fix.
- `libiio`: the port stays on the 0.x line: gr-iio and libad9361 are written against its buffer API,
  which 1.0 removes. The man install puts each page one level down (`man1/man`), so the build moves
  them.
- `libperseus-sdr`: `AM_CFLAGS` is given on make's command line, because configure writes
  `-march=native`, `-ffast-math` and `-fopenmp` into it.
- `librfnm`: the source is pinned to the last revision with the class API SDR++'s RFNM source is
  written against.
- `limesuite`: `ENABLE_SIMD_FLAGS=none`: the default tunes to the build machine.
  `CMAKE_INSTALL_LIBDIR=lib` places the SoapySDR module, whose directory comes from SoapySDR's
  GNUInstallDirs, not from `LIB_SUFFIX`.
- `rtl-sdr`: `DETACH_KERNEL_DRIVER` pairs with the DVB-T driver blacklist in
  `fs/etc/modprobe.d/kdos-sdr.conf`. A stick added to librtlsdr's table needs its ids in
  `fs/etc/udev/rules.d/70-kdos-sdr.rules`.
- `uhd`: the port follows the 4.10 line: 4.11 moves the N3xx, E3xx and X4xx devices onto gRPC, which
  is not a port.

### sdr

SDR DSP libraries, GNU Radio and its blocks, receivers, decoders and signal analysis. 24 ports,
under `ports/core/sdr/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `aptdec` | 1.7.0.git20250920 | NOAA APT weather-satellite image decoding library (libapt), the libaptdec branch SDRangel's APT demodulator links | `41_system` |
| `cm256cc` | 1.1.2 | Cauchy MDS erasure codes over GF(256); SDRangel's remote sink and source | `41_system` |
| `cubicsdr` | 0.2.7 | Software radio receiver with an OpenGL spectrum and waterfall, over SoapySDR and liquid-dsp | `44_apps` |
| `dsdcc` | 1.9.6 | Digital voice decoder for D-STAR, DMR, dPMR, NXDN and YSF, and the dsdccx tool | `41_system` |
| `gnuradio` | 3.10.12.0 | Signal-processing blocks for software radio, and GNU Radio Companion to wire them into flow graphs | `43_toolkits` |
| `gqrx` | 2.17.7 | Software radio receiver with a spectrum and waterfall, over GNU Radio and gr-osmosdr | `44_apps` |
| `gr-funcube` | 3.10.0.git20260208 | GNU Radio source and control blocks for the FUNcube Dongle Pro and Pro+ | `43_toolkits` |
| `gr-iqbal` | 0.38.3 | GNU Radio blocks that estimate and correct I/Q imbalance, used by gr-osmosdr | `43_toolkits` |
| `gr-osmosdr` | 0.2.6 | GNU Radio source and sink blocks for RTL-SDR, HackRF, Airspy, bladeRF, SoapySDR and network radios | `43_toolkits` |
| `inspectrum` | 0.4.0 | Inspect recorded radio signals: spectrogram, cursors, and demodulated traces | `44_apps` |
| `libdab` | 0.8.git20260323 | The DAB and DAB+ decoder library from dab-cmdline; SDRangel's DAB demodulator | `41_system` |
| `libinmarsatc` | 20260112 | Inmarsat-C demodulator, decoder and message parser libraries, the fork SDRangel's Inmarsat demodulator links | `41_system` |
| `libosmo-dsp` | 0.5.0 | Osmocom complex-vector DSP and I/Q imbalance estimation | `41_system` |
| `libsigmf` | 20260606 | C++ reader and writer for SigMF signal recordings, with the SDRangel namespace | `41_system` |
| `libvolk` | 3.3.0 | Vector-Optimized Library of Kernels — runtime-dispatched SIMD kernels for GNU Radio and SDR tools | `41_system` |
| `liquid-dsp` | 1.8.3 | Filters, modems, resamplers and FEC for software radio, in C | `41_system` |
| `mbelib` | 1.3.0 | AMBE and IMBE vocoder decoding for digital voice radio | `41_system` |
| `multimon-ng` | 1.6.1 | POCSAG, FLEX, AFSK, DTMF and the rest, demodulated from a stream of samples | `42_graphics` |
| `rtl-433` | 25.12 | Decodes 433/868 MHz transmissions from weather stations, sensors and tyre monitors | `41_system` |
| `satdump` | 1.2.2 | Receive, demodulate and decode weather and science satellites into images and products | `44_apps` |
| `sdrangel` | 7.27.2 | Software radio receiver, transmitter and analyser with decoders for ADS-B, AIS, APRS, FT8, DATV and more | `44_apps` |
| `sdrpp` | 1.3.0.dev.20260704 | Software radio receiver with its own GPU-drawn interface, no toolkit | `44_apps` |
| `serialdv` | 1.1.5 | AMBE3000 hardware vocoder dongles over serial and UDP, and the dvtest tool | `41_system` |
| `urh` | 2.10.0 | Record, demodulate, decode and replay wireless protocols | `44_apps` |

Notes:

- `aptdec`: only the library is built: the command-line program needs the argparse submodule, which
  the archive carries empty, so its PNG and libsndfile lookups stay disabled.
- `cm256cc`: the library has no scalar path, so on x86-64 it needs SSSE3; `ENABLE_DISTRIBUTION`
  fixes that floor instead of probing the build machine, which would bake in AVX2 or AVX-512.
- `cubicsdr`: audio is the bundled RtAudio: the rtaudio port has the 6.x API, which reports errors
  by return code where CubicSDR catches `RtAudioError`.
- `dsdcc`: SerialDV's include directory and library are named, because the bundled find module looks
  for them where the serialdv port does not install them.
- `gnuradio`: `CMAKE_POLICY_DEFAULT_CMP0167=NEW` makes `find_package(Boost)` read Boost's own config
  package, since the legacy module looks for a `libboost_system` file and Boost.System is
  header-only; FindPythonLibs is handed the python library and headers, because its version table is
  older than the python port. `gqrx`, `gr-funcube`, `gr-iqbal`, `gr-osmosdr` and `sdrangel` set the
  same policy. The installed top-level `sip.py` re-exports `PyQt5.sip`: gr-qtgui, ControlPort's
  plotter and the code GRC generates for a Qt sink run `import sip`, which no port provides
  otherwise.
- `gr-funcube`: hidapi is named outright: the bundled find module knows only hidapi's libusb
  backend, and the hidapi port builds the hidraw one alone.
- `libdab`: `LIB_INSTALL_DIR` must be set: the install rule reads it and never sets it, and would
  put the library in `/usr`. `X64_DEFINED` (the SSE2 Viterbi decoder) is passed on x86-64 only,
  because the NEON switch forces 32-bit Arm code.
- `libsigmf`: the FlatBuffers release beside the source fills the empty submodule; flatc is built
  from it, and its headers are installed with the generated ones. musl has no `strtoll_l`, so
  `FLATBUFFERS_LOCALE_INDEPENDENT=0` is defined here and in every consumer that includes those
  headers, `sdrangel` among them.
- `libvolk`: the cpu_features release beside the source fills the empty submodule and is linked in
  statically. `FMT_DEPRECATED_HEAVY_CORE` is defined because `qa_utils.cc` includes only
  `<fmt/core.h>`, which in fmt 12 includes `format.h` only under that macro.
- `satdump`: `SOL_NO_LUA_HPP` makes the bundled sol2 include the bundled Lua 5.4 headers; through
  `<lua.hpp>` it reaches the system Lua 5.5's and fails. `satdump-dvb-generic.patch` adds a second
  DVB plugin built without `-msse4.1`, which registers only on a CPU the SSE4.1 build refuses, so
  DVB-S and DVB-S2 work on a baseline x86-64 machine.
- `sdrangel`: `ARCH_OPT` goes straight to `-march` and defaults to `native`, so it names the
  architecture's baseline. `ENABLE_EXTERNAL_LIBRARIES=OFF` stops configure cloning helper libraries
  from git. The four remote plugins need SSE3 or NEON at compile time and are built on aarch64 only.
- `sdrpp`: upstream publishes no release after 1.0.4, only rolling builds, so the port follows a
  master commit. `HAVE_SSE=OFF` skips the bundled libcorrect's `-march=native` probe.
- `urh`: `setup.py` takes its device switches only on its own command line, so it builds the wheel
  and pip installs it. Each radio is named; an unnamed one is built only when a probe compile finds
  its library.

### hamradio

Amateur-radio operation: rig control, digital modes, logging, packet and APRS, propagation, and mesh
radio. 26 ports, under `ports/core/hamradio/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `ardopcf` | 1.0.4.1.3 | ARDOP HF sound-card modem that Pat drives for Winlink email over radio | `41_system` |
| `chirp` | 20260927 | Programs the memory channels of hundreds of amateur radios, with a wxPython interface and the chirpc CLI | `44_apps` |
| `contact` | 1.7.1 | A terminal client for a Meshtastic radio — chat, nodes and settings in curses | `41_system` |
| `direwolf` | 1.8.1 | Software TNC — AX.25, APRS and KISS over a sound card | `41_system` |
| `flamp` | 2.2.14 | Amateur Multicast Protocol file transfer to many stations at once through fldigi | `44_apps` |
| `fldigi` | 4.2.13 | Sound-card digital modes for amateur radio (PSK, RTTY, Olivia, MFSK, CW) with flarq | `44_apps` |
| `flmsg` | 4.0.24 | Amateur radio message forms (ICS, Radiogram, Red Cross) sent through fldigi | `44_apps` |
| `flrig` | 2.0.12 | Transceiver control over CAT for amateur radio, and the rig server fldigi talks to | `44_apps` |
| `flxmlrpc` | 1.0.1 | XML-RPC library shared by fldigi, flmsg and flamp | `41_system` |
| `freedv-gui` | 2.4.0 | HF digital voice for amateur radio: RADE, 700D, 700E and 1600 modes over a sound card | `44_apps` |
| `ggmorse` | 0.1.0.git20250920 | Morse code decoding library, the fork SDRangel's Morse decoder links | `41_system` |
| `gpsman` | 6.4.4.2 | GPS Manager — waypoints, routes and tracks to and from Garmin, Lowrance and Magellan receivers, with the gpsmanshp Shapefile layer | `44_apps` |
| `hamlib` | 4.7.2 | Radio and rotator control library | `41_system` |
| `js8call` | 3.0.3 | Weak-signal keyboard-to-keyboard and store-and-forward messaging for HF amateur radio | `44_apps` |
| `klog` | 2.6 | Amateur radio logbook with DXCC and award tracking, ADIF, LoTW and a DX cluster | `44_apps` |
| `meshtastic-firmware` | 2.7.26 | Meshtastic radio firmware images, to flash a LoRa node with no network | `41_system` |
| `nomadnet` | 1.4.3 | Messages, pages and files over a Reticulum mesh, in a terminal | `42_graphics` |
| `pat` | 1.0.0 | Winlink email client for amateur radio, over ARDOP, AX.25 packet and telnet | `41_system` |
| `qsstv` | 9.5.8 | Slow-scan television and digital image modes (HamDRM) for amateur radio | `44_apps` |
| `rnode-firmware` | 1.86 | RNode LoRa radio firmware images, for rnodeconf to flash with no network | `41_system` |
| `sideband` | 2.1.0 | LXMF messaging, voice, telemetry and maps over a Reticulum mesh | `42_graphics` |
| `splat` | 1.4.2 | Terrain-aware VHF/UHF path loss and coverage over Longley-Rice | `41_system` |
| `tlf` | 1.4.1 | Curses contest logger with hamlib and Cabrillo output | `41_system` |
| `voacapl` | 0.7.7 | HF propagation prediction (VOACAP) | `41_system` |
| `wsjtx` | 3.0.1 | FT8, FT4, JT65, Q65, WSPR and other weak-signal digital modes for amateur radio | `44_apps` |
| `xastir` | 2.2.4 | APRS mapping and messaging over TNCs, AX.25 and APRS-IS (Motif, under Xwayland) | `44_apps` |

Notes:

- `ardopcf`: the makefile assigns `CFLAGS` and `LDFLAGS` outright, so the tree's flags reach it only
  on the make command line, and `-MMD` stays among them because the makefile includes the dependency
  files it writes.
- `chirp`: the desktop entry runs `chirp --no-install-desktop-app`; without it the first run offers
  to write a per-user entry that shadows the packaged one.
- `direwolf`: the package replaces upstream's desktop entry, which runs `xterm -e direwolf`.
- `fldigi`: `--without-libmbedtls` keeps the bundled mbedtls 2.x copy the network code is written
  against; the mbedtls port's 3.x API has no `net.h` or `certs.h`. flarq's window class is `fldigi`,
  so its entry's `StartupWMClass` is `fldigi` too.
- `freedv-gui`: the CMake generator is Unix Makefiles, never Ninja: the Opus and RNNoise
  ExternalProjects build with a literal `$(MAKE)`, which Ninja refuses.
- `js8call`: `GIT_EXECUTABLE` is cleared so the version string is not taken from whatever repository
  the build directory sits in.
- `pat`: the web interface listens on port 8083, because 8080 to 8082 belong to kiwix-serve, kolibri
  and llama-server and `pat http` stops at once when its port is taken.
- `sideband`: the `platlib` path is resolved with both `base` and `platbase` set to the prefix; with
  `base` alone it resolves to the build root's own site-packages, and the removal of the bundled
  Windows and macOS libraries reaches outside `$PKG`.
- `splat`: upstream's `build` script is not used: it exits 0 after a failed compile, so a broken
  build would package an empty directory.
- `tlf`: `-largp` goes in `LIBS`, not `LDFLAGS`: automake puts `LDFLAGS` before the objects, where
  the static argp archive resolves nothing.
- `voacapl`: builds with `make -j1`; the makefile declares no order between Fortran modules, and a
  parallel build compiles a module's user before its `.mod` file exists.
- `wsjtx`: `-Wno-error` goes in `CMAKE_CXX_FLAGS_RELEASE`, which follows upstream's `-Werror` on the
  compiler line; in `CXXFLAGS` it would come before it and do nothing.
- `xastir`: image maps come through GraphicsMagick, because configure refuses ImageMagick 7.

### ai

Machine learning inference, speech recognition, computer vision and neural translation. 7 ports,
under `ports/core/ai/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `libggml` | 0.25.3 | ggml, the tensor library under llama.cpp and whisper.cpp: CPU variants, OpenBLAS and Vulkan backends as loadable modules | `42_graphics` |
| `llama.cpp` | 0.5.0 | Local language models on the CPU or a Vulkan GPU: llama-cli, llama-server and the GGUF tools, with nothing sent anywhere | `42_graphics` |
| `onnxruntime` | 1.30.0 | ONNX Runtime: inference for ONNX models on the CPU, as a C/C++ library and a Python module | `41_system` |
| `opencv` | 4.14.0 | Computer vision and machine-learning library, with the tracking, optical-flow and extended image-processing contrib modules and Python bindings | `43_toolkits` |
| `translatelocally` | 0.0.2.20250330 | translateLocally: machine translation on the CPU with Bergamot models, with the text never leaving the machine | `44_apps` |
| `whisper-model-base-en` | 20241029 | The Whisper base.en speech model in ggml form, for whisper.cpp and kdos-rec | `41_system` |
| `whisper.cpp` | 1.9.4 | Speech to text on the CPU or a Vulkan GPU, with the model on the disk and nothing sent anywhere | `42_graphics` |

Notes:

- `libggml`: `GGML_NATIVE` stays off, or the library is tuned to the build machine and dies with
  SIGILL elsewhere. `GGML_CPU_ALL_VARIANTS` builds one CPU backend per microarchitecture under
  `/usr/lib/ggml` and needs `GGML_BACKEND_DL`, which needs shared libraries; a program that finds no
  module there has no CPU backend.
- `onnxruntime`: the pinned third-party archives are the `ort-*` sources, linked into the mirror
  directory `onnxruntime_CMAKE_DEPS_MIRROR_DIR` names; one missing from it is downloaded, which the
  offline build cannot do. `FETCHCONTENT_TRY_FIND_PACKAGE_MODE=NEVER` keeps the tree's abseil and
  protobuf out, or two abseils load into one process.
- `opencv`: the port stays on the 4.x line, the API digiKam, MLT and SDRangel are written against.
  Contrib modules are moved in one by one; `face` stays out, because its configure downloads a
  model.
- `translatelocally`: `BUILD_ARCH` defaults to `native`, which tunes to the build machine; `x86-64`
  keeps the binary portable, and intgemm picks its AVX2 or AVX-512 kernels at run time.
- `whisper.cpp`: `WHISPER_SDL2` also builds talk-llama, which fetches llama.cpp at configure time;
  `sdl2-stream-only.patch` limits the flag to the streaming program.

### python

The python3 interpreter, tkinter, and Python build, packaging and extension tooling. 36 ports, under
`ports/core/python/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `python3` | 3.14.8 | The Python 3 interpreter and standard library, without tkinter | `30_foundation` |
| `python3-calver` | 2025.10.20 | Date-based versions for setuptools — trove-classifiers builds with it | `30_foundation` |
| `python3-cffi` | 2.1.1 | Foreign function interface for calling C from Python | `40_lang` |
| `python3-cppy` | 1.3.1 | C++ headers for writing CPython extensions — kiwisolver builds with it | `40_lang` |
| `python3-cython` | 3.3.0 | The C extension compiler numpy is written against | `40_lang` |
| `python3-extension-helpers` | 1.5.0 | The setuptools glue astropy compiles its C extensions through | `40_lang` |
| `python3-flit-core` | 4.1.0 | The minimal PEP 517 backend packaging itself builds with | `30_foundation` |
| `python3-hatch-jupyter-builder` | 0.10.0 | The hatchling hook that builds and checks Jupyter front-end assets | `40_lang` |
| `python3-hatch-nodejs-version` | 0.4.0 | Hatchling plugins that read version and metadata from package.json | `40_lang` |
| `python3-hatch-vcs` | 0.5.0 | Version from VCS for hatchling projects, through setuptools-scm | `30_foundation` |
| `python3-hatchling` | 1.32.4 | Hatch's PEP 517 backend, which bootstraps itself — scikit-build-core builds with it | `30_foundation` |
| `python3-maturin` | 1.15.0 | Build backend for Rust-based Python extensions | `40_lang` |
| `python3-meson-python` | 0.21.1 | The PEP 517 backend numpy, scipy and matplotlib build through | `40_lang` |
| `python3-nanobind` | 3.1.0 | C++17 binding library for Python — pikepdf and CoolProp generate through it | `40_lang` |
| `python3-packaging` | 26.3 | Version and marker parsing every build backend uses | `30_foundation` |
| `python3-pathspec` | 1.1.1 | Gitignore-style path matching — how hatchling and scikit-build-core choose files | `30_foundation` |
| `python3-pip` | 26.2.1 | Python package installer | `30_foundation` |
| `python3-pkgconfig` | 1.6.0 | Python interface to pkg-config — how uharfbuzz finds the system harfbuzz | `40_lang` |
| `python3-pluggy` | 1.6.0 | The plugin hook system hatchling and ocrmypdf are extended through | `30_foundation` |
| `python3-poetry-core` | 2.5.0 | Poetry's PEP 517 backend, which bootstraps itself — tomlkit builds with it | `30_foundation` |
| `python3-pybind11` | 3.1.0 | C++11 bindings — matplotlib and scipy generate through it | `40_lang` |
| `python3-pycparser` | 3.0 | A C parser in pure python — what cffi reads a header with | `40_lang` |
| `python3-pyproject-metadata` | 0.12.1 | PEP 621 metadata, which meson-python reads | `40_lang` |
| `python3-pyqt-builder` | 1.19.1 | The sip-build backend every PyQt6 binding is configured with | `40_lang` |
| `python3-scikit-build-core` | 1.0.3 | The PEP 517 backend for CMake projects — pybind11, nanobind, pikepdf and CoolProp build through it | `40_lang` |
| `python3-semantic-version` | 2.10.0 | Semantic-version parsing — what setuptools-rust compares against | `40_lang` |
| `python3-setuptools` | 84.0.0 | Library for building, packaging and installing Python projects | `30_foundation` |
| `python3-setuptools-rust` | 1.13.0 | Builds a Rust extension from setup.py — how maturin bootstraps itself | `40_lang` |
| `python3-setuptools-scm` | 10.3.4 | Version from VCS — matplotlib requires it at build time | `30_foundation` |
| `python3-sip` | 6.16.1 | The binding generator PyQt6, QScintilla and QGIS are built with | `40_lang` |
| `python3-tkinter` | 3.14.8 | Python's binding to Tk, the _tkinter module and the tkinter package (X11 under Xwayland) | `42_graphics` |
| `python3-tomlkit` | 0.15.1 | Style-preserving TOML reading and writing — hatchling requires it | `30_foundation` |
| `python3-trove-classifiers` | 2026.9.21.13 | The canonical PyPI classifier list hatchling validates metadata against | `30_foundation` |
| `python3-vcs-versioning` | 2.5.0 | The VCS version machinery setuptools-scm is built on | `30_foundation` |
| `python3-versioneer` | 0.29 | Derives a package version from git metadata — a build-time import pandas needs | `40_lang` |
| `python3-wheel` | 0.48.0 | The reference implementation of the Python wheel format | `40_lang` |

Notes:

- `python3`: `--with-ensurepip=no` keeps CPython's prebuilt pip wheel out; `python3 -m venv`
  installs pip from the wheel python3-pip puts in `/usr/share/python-wheels`. OpenSSL 4 has no SSLv3
  and no per-version method constructors and declares no guard for either, so build.sh sets the
  `OPENSSL_NO_*` guards or `_ssl` fails to import. The Tk-only standard library (turtle, idlelib,
  turtledemo, idle3) is removed with tkinter. `EXTERNALLY-MANAGED` makes pip refuse a system-wide
  install outside a virtual environment; a port's `pip install --root=$PKG` is unaffected.
- `python3-maturin`: `MATURIN_SETUP_ARGS` replaces setup.py's argument list whole, so it repeats
  `--no-default-features` and names every feature: upstream's `full` set less `xwin`.
- `python3-pip`: without the wheel in `/usr/share/python-wheels`, `python3 -m venv` exits 1 and
  leaves an environment with no pip and no activate scripts.
- `python3-tkinter`: built from python3's source at python3's version: the module is compiled for
  that interpreter's ABI.

### python-libs

General-purpose python3-* runtime libraries: text, templating, markup, parsing, config, CLI, system,
files, crypto, imaging. 67 ports, under `ports/core/python-libs/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `python3-attrs` | 26.1.0 | Classes without boilerplate: declared attributes, validators and converters for Python | `40_lang` |
| `python3-bcrypt` | 5.0.0 | The bcrypt password hash for Python, in Rust | `40_lang` |
| `python3-click` | 8.5.0 | Composable command-line interfaces for python programs | `40_lang` |
| `python3-click-log` | 0.4.0 | Logging integration for click command-line programs | `40_lang` |
| `python3-colorama` | 0.4.6 | ANSI colour codes in Python that also work on a Windows console | `40_lang` |
| `python3-configobj` | 5.0.9 | INI-style configuration files with validation, for python programs | `40_lang` |
| `python3-cryptography` | 50.0.1 | Cryptographic primitives and recipes for Python | `40_lang` |
| `python3-cssselect` | 1.5.0 | CSS selectors translated to XPath — Inkscape's extension library selects with it | `40_lang` |
| `python3-cups` | 2.0.4 | libcups bindings for Python, for system-config-printer | `41_system` |
| `python3-dateutil` | 2.9.0.post0 | Date parsing, relative deltas and time zones — pandas and matplotlib import it | `40_lang` |
| `python3-decorator` | 5.3.1 | Signature-preserving function decorators for Python | `40_lang` |
| `python3-defusedxml` | 0.7.1 | XML parsing for Python hardened against entity expansion attacks | `40_lang` |
| `python3-distro` | 1.9.0 | Linux distribution identification from os-release for Python | `40_lang` |
| `python3-docutils` | 0.23 | Set of tools for processing plaintext docs into formats such as HTML, XML, or LaTeX | `30_foundation` |
| `python3-ecdsa` | 0.19.2 | Pure-python ECDSA and EdDSA signatures — adafruit-nrfutil signs nRF52 firmware packages with it | `40_lang` |
| `python3-glad2` | 2.0.8 | OpenGL and EGL loader generator — writes libplacebo's GL loader at build time | `40_lang` |
| `python3-hatch-tryton` | 0.1.1 | Hatchling plugin that builds Tryton modules and their metadata | `40_lang` |
| `python3-isodate` | 0.7.2 | ISO 8601 date, time and duration parsing and formatting for Python | `40_lang` |
| `python3-jellyfish` | 1.2.1 | Approximate and phonetic string matching for Python, in Rust | `40_lang` |
| `python3-jinja2` | 3.1.6 | The template engine libplacebo generates its shader sources with | `30_foundation` |
| `python3-jsonschema` | 4.26.0 | JSON Schema validation for Python | `40_lang` |
| `python3-jsonschema-specifications` | 2025.9.1 | The JSON Schema meta-schemas and vocabularies, packaged for jsonschema | `40_lang` |
| `python3-keyring` | 25.7.0 | Python access to the desktop's Secret Service, with the keyring command | `40_lang` |
| `python3-lark` | 1.3.1 | Parsing toolkit for context-free grammars, under JupyterLab's RFC 3987 validator | `40_lang` |
| `python3-libvirt` | 12.7.0 | Python binding of the libvirt API — what virt-manager and virt-install drive libvirt through | `43_toolkits` |
| `python3-lxml` | 6.1.3 | libxml2 and libxslt bindings for Python | `40_lang` |
| `python3-mako` | 1.4.3 | Templating library for Python | `40_lang` |
| `python3-markdown` | 3.11 | Markdown to HTML conversion for Python | `40_lang` |
| `python3-markdown-it-py` | 4.2.0 | CommonMark parser for python — what rich renders Markdown with | `40_lang` |
| `python3-markupsafe` | 3.0.3 | Markup-safe string type for XML, HTML and XHTML in Python | `30_foundation` |
| `python3-mdurl` | 0.1.2 | URL parsing and formatting for markdown-it-py | `40_lang` |
| `python3-networkx` | 3.7 | Graphs and networks for python: the structures and the algorithms over them | `40_lang` |
| `python3-openpyxl` | 3.1.5 | Read and write xlsx from python — what visidata opens one with | `42_graphics` |
| `python3-orjson` | 3.12.0 | JSON for Python, written in Rust | `40_lang` |
| `python3-pathvalidate` | 3.3.1 | Validates and sanitises file names and paths for every platform's rules | `40_lang` |
| `python3-pefile` | 2024.8.26 | Read and rewrite the headers of a PE executable, the format of every EFI binary | `40_lang` |
| `python3-pikepdf` | 10.13.0.post1 | qpdf bindings — read and rewrite a PDF without rasterising it | `42_graphics` |
| `python3-pillow` | 12.3.0 | Python imaging library | `42_graphics` |
| `python3-pillow-heif` | 1.8.0 | HEIF and AVIF for Pillow, through libheif | `42_graphics` |
| `python3-platformdirs` | 4.11.12 | Where a python program's config, cache and data directories are | `40_lang` |
| `python3-ply` | 3.11 | Lex and yacc for python — libcamera generates its control tables with it | `40_lang` |
| `python3-protobuf` | 7.36.2 | Protocol Buffers runtime for Python, with the upb C extension | `40_lang` |
| `python3-psutil` | 7.2.2 | Processes, CPU, memory, disks and network from python | `40_lang` |
| `python3-psycopg2` | 2.9.13 | The PostgreSQL adapter for Python, over libpq | `41_system` |
| `python3-pydantic-core` | 2.46.5 | pydantic's validation core, in Rust — pinned to the version pydantic names | `40_lang` |
| `python3-pyemf3` | 3.3 | Pure-Python Enhanced Metafile writer, for Veusz's EMF export | `40_lang` |
| `python3-pygments` | 2.21.0 | Python syntax highlighter | `30_foundation` |
| `python3-pyparsing` | 3.3.3 | Grammar-based text parsing — matplotlib's mathtext and fontconfig patterns | `40_lang` |
| `python3-pypdfium2` | 5.13.0 | Python bindings to PDFium — render, read and edit a PDF from Python | `42_graphics` |
| `python3-pytz` | 2026.4 | The Olson time zone database for python datetime | `40_lang` |
| `python3-pyxdg` | 0.28 | The freedesktop.org base directories, desktop entries, menus and icon themes in Python | `41_system` |
| `python3-qrcode` | 8.2 | QR code generator for python, as text or as an image | `42_graphics` |
| `python3-referencing` | 0.37.0 | JSON reference resolution across JSON Schema dialects, under jsonschema | `40_lang` |
| `python3-rich` | 15.0.0 | Colour, tables and progress bars in a terminal, for python programs | `40_lang` |
| `python3-rpds-py` | 2026.6.3 | Persistent data structures in Rust, under jsonschema's referencing | `40_lang` |
| `python3-ruamel-yaml` | 0.19.1 | YAML 1.2 loader and dumper for python that keeps comments and key order through a round trip | `40_lang` |
| `python3-scour` | 0.38.2 | SVG optimiser — the scour command and Inkscape's Optimized SVG output | `40_lang` |
| `python3-send2trash` | 2.1.0 | Send files to the freedesktop.org trash instead of deleting them | `40_lang` |
| `python3-six` | 1.17.0 | Python 2 and 3 compatibility shims — python-dateutil imports it | `40_lang` |
| `python3-sphinx` | 9.1.0 | Documentation generator — the manual pages of LLVM, CMake, mpd and flashrom | `30_foundation` |
| `python3-tinycss2` | 1.5.1 | CSS parser for Python — Inkscape's extension library reads style with it | `40_lang` |
| `python3-typing-extensions` | 4.16.0 | Backported and experimental type hints for Python's typing module | `40_lang` |
| `python3-uharfbuzz` | 0.56.2 | Cython bindings to HarfBuzz — text shaping from Python | `42_graphics` |
| `python3-wcwidth` | 0.9.1 | The column width of a Unicode string in a terminal | `40_lang` |
| `python3-yaml` | 6.0.3 | Python bindings for YAML, using libyaml | `30_foundation` |
| `python3-yapps` | 2.2.0 | Yet Another Python Parser System, the yapps2 LL(1) parser generator | `40_lang` |
| `python3-zstandard` | 0.25.0 | Python bindings for the zstd library | `40_lang` |

Notes:

- `python3-attrs`, `python3-jsonschema`, `python3-keyring`: the build-only backend plugin
  (hatch-fancy-pypi-readme, coherent.licensed) goes into a private `PYTHONPATH` directory;
  installed into the build root it would be a file no package owns.
- `python3-cryptography`: the Rust vendor bundle unpacks at the source root, not at `src/rust`:
  cargo reads `.cargo/config` from the directory maturin runs in, and a config beside
  `src/rust/Cargo.toml` leaves every crate unresolved offline.
- `python3-keyring`: the vendored wheels unpack outside the source tree; setuptools' flat-layout
  discovery takes a `vendor/` beside `keyring/` for a second top-level package and stops.
- `python3-pillow`: every codec is set to `enable`, so a missing header fails the build instead of
  dropping the format. With `-flto`, `AR`, `NM` and `RANLIB` must be the `gcc-` wrappers, or the
  static mode library is indexed empty and `import PIL.Image` fails on `getRawModeData`.
- `python3-pillow-heif`: upstream compiles with `-Werror` and refuses a libheif older than
  1.23.4, so a libheif header that grows a deprecation warning fails this build.
- `python3-protobuf`: its version is the `protobuf` port's with a leading 7, and code protoc
  generates checks the runtime against it; the two ports move together.
- `python3-pydantic-core`: `series` holds it at the exact version the pydantic vendored in
  ocrmypdf pins; a bump of one is a bump of both.
- `python3-pypdfium2`: the ctypes bindings are generated from `/usr/include/pdfium` by
  pypdfium2's own ctypesgen fork. Without ctypesgen on `PATH` setup falls back to bindings for
  another pdfium release, and without `GIVEN_FULLVER` it lists Chromium's tags over the network.
- `python3-sphinx`: the vendored closure lives under `/usr/lib/python3-sphinx`, because other
  ports put the same modules in site-packages; `import sphinx` from a plain `python3` fails, and
  only the `/usr/bin` launchers reach it. `--ignore-installed` keeps a copy already in the build
  root from satisfying a requirement the prefix then lacks.
- `python3-uharfbuzz`: `USE_SYSTEM_LIBS` links harfbuzz-subset and harfbuzz-raster, so the
  `harfbuzz` port must build both.
- `python3-wcwidth`: `CIBUILDWHEEL=1` makes the C extension mandatory; unset, a failed compile
  falls back to the pure-Python tables without a word.
- `python3-zstandard`: the system zstd must be at least the release the bindings were cut
  against, or the module refuses to import.

### python-net

python3-* HTTP, web-framework, TLS-trust and network-stack modules. 22 ports, under
`ports/core/python-net/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `python3-asgiref` | 3.12.1 | ASGI specification helpers and sync/async adapters for Python | `40_lang` |
| `python3-beautifulsoup4` | 4.15.0 | Lenient HTML and XML parsing, for nbconvert and toot | `40_lang` |
| `python3-blinker` | 1.9.0 | In-process object-to-object and broadcast signalling for Python | `40_lang` |
| `python3-certifi` | 2026.7.22 | The CA bundle python libraries look for by name | `30_foundation` |
| `python3-charset-normalizer` | 3.5.1 | Character encoding detection for undeclared text | `40_lang` |
| `python3-flask` | 3.1.3 | Flask, a WSGI web application framework for Python | `40_lang` |
| `python3-flask-cors` | 6.0.5 | Cross-origin resource sharing (CORS) headers for Flask applications | `40_lang` |
| `python3-html5lib` | 1.1 | An HTML parser following the WHATWG HTML specification, for Python | `40_lang` |
| `python3-httplib2` | 0.32.0 | An HTTP client with caching, keep-alive and digest authentication | `40_lang` |
| `python3-idna` | 3.20 | Internationalised domain names in applications (IDNA 2008 and UTS #46) | `40_lang` |
| `python3-itsdangerous` | 2.2.0 | Cryptographically signed serialisation of data for passing through untrusted channels | `40_lang` |
| `python3-lxmf` | 1.1.1 | Store-and-forward messaging over Reticulum, and the lxmd propagation node | `40_lang` |
| `python3-meshtastic` | 2.7.11 | Meshtastic LoRa mesh client — text and position messages | `40_lang` |
| `python3-pysocks` | 1.7.1 | SOCKS4 and SOCKS5 proxy client module for Python sockets and urllib | `40_lang` |
| `python3-requests` | 2.34.2 | HTTP client library for Python | `40_lang` |
| `python3-rns` | 1.5.4 | Reticulum encrypted mesh networking over LoRa, packet radio, WiFi or anything that carries bytes | `40_lang` |
| `python3-soupsieve` | 2.10 | CSS selectors for Beautiful Soup | `40_lang` |
| `python3-truststore` | 0.10.4 | Verify TLS against the system certificate store through the ssl module | `40_lang` |
| `python3-urllib3` | 2.8.0 | HTTP client with connection pooling, retries and TLS verification | `40_lang` |
| `python3-waitress` | 3.0.2 | Waitress, a pure-Python production WSGI server | `40_lang` |
| `python3-webencodings` | 0.6.1 | The WHATWG Encoding standard's labels and legacy decoders, for Python | `40_lang` |
| `python3-werkzeug` | 3.1.9 | Werkzeug, the WSGI toolkit under Flask: request, response, routing and a development server | `40_lang` |

Notes:

- `python3-certifi`: the file `certifi.where()` names is a link to `/etc/ssl/cert.pem`, so Python
  trusts the system roots; the link dangles until `ca-certificates` is installed, which is why it
  is a dependency.
- `python3-charset-normalizer`: `CHARSET_NORMALIZER_USE_CYTHON=1` makes a missing Cython fail
  the build instead of shipping the pure-Python modules.
- `python3-meshtastic`: bleak is held at 3.0.1, since 3.0.2 builds with uv_build, which is not a
  port; `SKIP_CYTHON` installs dbus-fast as pure Python, because its Cython accelerator does not
  build with the Cython port; build isolation stays off because pypubsub declares
  `setuptools<77`.

### python-sci

python3-* science, numerics, astronomy and geo modules. 13 ports, under `ports/core/python-sci/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `python3-astropy-iers-data` | 0.2026.9.21.0.56.25 | The IERS Earth-orientation and leap-second tables astropy reads, frozen at release | `40_lang` |
| `python3-contourpy` | 1.4.0 | Contour lines and filled contours over a 2D grid — matplotlib's contouring | `41_system` |
| `python3-cycler` | 0.12.1 | Composable style cycles — matplotlib's property cycle | `40_lang` |
| `python3-gmpy2` | 2.3.1 | Multiple-precision integers, rationals, reals and complex numbers from python, through GMP, MPFR and MPC | `40_lang` |
| `python3-h5py` | 3.16.0 | HDF5 files from Python as numpy arrays, for Veusz's HDF5 import | `41_system` |
| `python3-iminuit` | 2.33.0 | Minuit2 minimiser and error analysis from Python, for Veusz's fitting | `41_system` |
| `python3-jplephem` | 2.24 | Reads JPL ephemeris kernels for planetary positions | `41_system` |
| `python3-kiwisolver` | 1.5.1 | Cassowary constraint solver — matplotlib's layout engine | `40_lang` |
| `python3-mpmath` | 1.4.1 | Arbitrary-precision floating point — sympy computes numerically through it | `40_lang` |
| `python3-owslib` | 0.36.0 | Python client for OGC web services (WMS, WFS, WCS, CSW, OGC API), for QGIS MetaSearch | `40_lang` |
| `python3-pyerfa` | 2.0.1.5 | The python binding for ERFA — fundamental astronomy, and astropy's floor | `41_system` |
| `python3-sgp4` | 2.27 | SGP4 satellite orbit propagation from a TLE | `41_system` |
| `python3-shapely` | 2.1.2 | Planar geometry (points, lines, polygons and their set operations) over GEOS, for Python | `41_system` |

Notes:

- `python3-sgp4`: `PYTHON_SGP4_COMPILE=always` makes the C++ propagator mandatory; without it a
  failed compile installs the slower pure-Python path and the build passes.

### python-gui

python3-* GUI toolkit, desktop and D-Bus bindings. 16 ports, under `ports/core/python-gui/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `python3-cairo` | 1.29.1 | Cairo drawing for Python, and the cairo half of PyGObject | `42_graphics` |
| `python3-dasbus` | 1.7 | D-Bus library for Python on top of GLib's GDBus, which Orca talks through | `42_graphics` |
| `python3-dbus` | 1.5.0 | The libdbus bindings for Python that system-config-printer and PyQt6's D-Bus main loop use | `41_system` |
| `python3-gobject` | 3.58.0 | GObject introspection bindings: GLib, GTK and every introspected library, from Python | `42_graphics` |
| `python3-kivy` | 2.3.1 | Python UI framework drawing with OpenGL through SDL2 | `42_graphics` |
| `python3-opengl` | 3.1.10 | The OpenGL, GLU and GLUT bindings for Python, loaded through ctypes | `42_graphics` |
| `python3-pyqt5` | 5.15.11 | Python bindings for Qt 5, which GNU Radio's gr-qtgui and the Qt 5 Python tools are written in | `43_toolkits` |
| `python3-pyqt5-sip` | 12.19.0 | The sip runtime module every PyQt5 extension is linked through | `41_system` |
| `python3-pyqt6` | 6.11.0 | Python bindings for Qt 6, which Calibre, Anki, QGIS, Picard and git-cola are written in | `43_toolkits` |
| `python3-pyqt6-sip` | 13.12.0 | The sip runtime module every PyQt6 extension is linked through | `41_system` |
| `python3-pyqt6-webengine` | 6.11.0 | QtWebEngine from Python, for the Calibre viewer, Anki and Spyder | `43_toolkits` |
| `python3-pyqtgraph` | 0.14.0 | Fast plotting and image display for Python on Qt and NumPy | `43_toolkits` |
| `python3-qtpy` | 2.4.3 | One Qt import layer over PyQt6, for git-cola and Spyder | `43_toolkits` |
| `python3-urwid` | 4.1.7 | Console user interface library for python | `41_system` |
| `python3-wxpython` | 4.2.5 | The wxWidgets GTK 3 toolkit for Python programs such as CHIRP and Pronterface | `43_toolkits` |
| `python3-xlib` | 0.33 | The X11 client protocol implemented in pure Python | `41_system` |

Notes:

- `python3-cairo`, `python3-dbus`, `python3-gobject`: built with meson, not pip; only that build
  installs the `.pc` files and headers PyGObject's cairo support, PyQt6's
  `dbus.mainloop.pyqt6` and C embedders compile against.
- `python3-kivy`: `python314-ast-str.patch` must keep its exact text: Sideband checks the
  installed kv parser for it at start-up and rewrites the file when it is missing, which ends the
  program on a read-only system directory. Build isolation stays off, since pyproject pins a
  Cython 3.0 whose generated C does not build against Python 3.14.
- `python3-pyqt5`: modules for Qt 5 libraries that are not ported are disabled by name, so the
  module set cannot change with build order; the build fails if any expected module is missing.
  QtNetwork and the modules that link `libQt5Network` (QtQml, QtQuick, QtQuickWidgets,
  QtMultimedia, QtMultimediaWidgets, QtWebSockets) bring OpenSSL 3 into a Python process whose
  `_hashlib` and `_ssl` link OpenSSL 4; see
  [known gaps](known-gaps.md#a-pyqt5-program-that-also-uses-pythons-ssl-or-hashlib-can-crash).
  The install runs with `-j1`: the generated install rules race.
- `python3-pyqt6-webengine`: its bindings are generated against the `.sip` files `python3-pyqt6`
  installs, so the two must be the same release. The install runs with `-j1`.
- `python3-wxpython`: the sdist's sip-generated C++ is built as shipped, so its release must match
  the `wxwidgets` port's; `build.py` fetches waf unless the sdist's `bin/` carries the pinned
  `waf-<version>`, so a release without it fails the offline build.

### python-dev

python3-* Jupyter, kernel, language-server and debugger modules. 14 ports, under
`ports/core/python-dev/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `python3-cloudpickle` | 3.1.2 | Pickling of functions and classes, which Spyder's kernel sends variables with | `40_lang` |
| `python3-comm` | 0.2.3 | The Jupyter comm protocol for kernel-side widgets | `41_system` |
| `python3-debugpy` | 1.8.22 | The Debug Adapter Protocol server for Python, behind the debuggers of Spyder and the IPython kernel | `40_lang` |
| `python3-ipykernel` | 6.31.0 | The IPython kernel for Jupyter, under JupyterLab and Spyder | `41_system` |
| `python3-jupyter-client` | 8.10.0 | The Jupyter messaging protocol and kernel manager | `41_system` |
| `python3-jupyter-core` | 5.9.1 | Jupyter's paths, configuration and the jupyter command | `41_system` |
| `python3-jupyterlab-pygments` | 0.3.0 | Pygments highlighting in JupyterLab's colours, for nbconvert | `40_lang` |
| `python3-lsp-ruff` | 2.3.4 | Ruff's diagnostics, fixes and formatting inside the Python language server | `41_system` |
| `python3-lsp-server` | 1.15.0 | The Python language server, with Pylint, flake8, Rope, YAPF and Black | `41_system` |
| `python3-nbconvert` | 7.17.1 | Notebooks to HTML, PDF, Markdown and scripts, with nbformat and nbclient | `41_system` |
| `python3-nest-asyncio` | 1.6.0 | Re-entrant asyncio event loops, which ipykernel runs cells in | `40_lang` |
| `python3-pygccxml` | 3.0.2 | C++ declarations from CastXML as a Python object tree | `40_lang` |
| `python3-pyzmq` | 27.2.0 | Python bindings for ZeroMQ, the transport of Jupyter kernels | `41_system` |
| `python3-tornado` | 6.5.10 | The asynchronous web server and networking library under Jupyter | `40_lang` |

Notes:

- `python3-lsp-ruff`: the vendor bundle unpacks beside the source tree, not in it: a `vendor/`
  directory at its top is a second top-level package to setuptools' discovery, which then refuses to
  build.

### python-hw

python3-* hardware, serial, USB, instrument, CAN and device-protocol modules. 12 ports, under
`ports/core/python-hw/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `python3-adafruit-nrfutil` | 0.5.3.post16 | Serial DFU flasher for nRF52 boards; rnodeconf flashes RAK4631, T-Echo and Heltec T114 RNodes with it | `40_lang` |
| `python3-cantools` | 44.1.0 | Decode a live candump into named signals, and generate the MCU-side C | `42_graphics` |
| `python3-esptool` | 5.4.0 | Talk to the ESP8266 and ESP32 ROM bootloader over serial | `40_lang` |
| `python3-obd` | 0.7.3 | Reads and clears engine fault codes over an ELM327 OBD-II adapter | `40_lang` |
| `python3-pyarcus` | 5.12.0 | Python bindings for the Arcus socket Cura talks to CuraEngine through | `41_system` |
| `python3-pynest2d` | 5.11.0.alpha0 | Python bindings for libnest2d, the build-plate arranger in Cura | `41_system` |
| `python3-pysavitar` | 5.12.0 | Python bindings for Savitar, Cura's 3MF reader and writer | `41_system` |
| `python3-pyserial` | 3.5 | Serial port access from Python | `40_lang` |
| `python3-pyusb` | 1.3.1 | USB access from python through libusb — what pyvisa-py opens a USB instrument with | `41_system` |
| `python3-pyuvula` | 1.1.0 | UV unwrapping and projection for Cura's paint tool, with its Python module | `41_system` |
| `python3-pyvisa` | 1.16.2 | VISA instrument-automation API for Python, with no NI-VISA dependency | `40_lang` |
| `python3-pyvisa-py` | 0.8.1 | The pure-python VISA backend — USB, TCPIP and serial with no vendor library | `41_system` |

Notes:

- `python3-esptool`: `BUILD_EXTENSION=yes` makes websockets' C speedups mandatory; unset, a
  failed compile falls back to pure Python without a word.
- `python3-obd`: the vendor bundle unpacks beside the source, not inside it: obd is a flat layout
  and setuptools refuses to choose between `obd` and a `vendor/` next to it. pint stays in the
  0.24 series obd declares.
- `python3-pyarcus`, `python3-pynest2d`, `python3-pysavitar`: sip's own backend builds the module
  in place of upstream's Conan and CMake build. `-std=c++17` in `extra-compile-args` must stay:
  sip compiles as C++11, and the headers need C++17. pyArcus links every library protobuf's
  pkg-config names, because protobuf's headers call into abseil.
- `python3-pyusb`: libusb is loaded at run time through ctypes; without it every device reports
  "No backend available".
- `python3-pyuvula`: a CMake shim answers Conan's `standardprojectsettings` package and
  `clipper::clipper` target from this system. The library has no install rules, so the recipe
  copies the module into site-packages.
- `python3-pyvisa-py`: `python3-pyusb` must stay in depends: the USBTMC and USB-raw sessions are
  dropped without an error when pyusb fails to import.

### perl-cpan

The perl interpreter and every perl-* CPAN module. 20 ports, under `ports/core/perl-cpan/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `perl` | 5.44.0 | The Perl programming language | `30_foundation` |
| `perl-authen-sasl` | 2.2100 | SASL authentication framework for perl | `40_lang` |
| `perl-class-inspector` | 1.36 | Get information about a perl class and its structure | `30_foundation` |
| `perl-crypt-urandom` | 0.55 | The operating system's cryptographic random source for perl | `40_lang` |
| `perl-db-file` | 1.861 | Perl access to Berkeley DB hash, btree and recno files (DB_File) | `41_system` |
| `perl-digest-hmac` | 1.05 | Keyed-hashing for message authentication (HMAC) in perl | `40_lang` |
| `perl-file-homedir` | 1.006 | Find the home and XDG user directories of the current or another user, in pure perl | `40_lang` |
| `perl-file-sharedir` | 1.118 | Locate per-distribution and per-module shared files of perl modules | `30_foundation` |
| `perl-file-sharedir-install` | 0.14 | Install shared files of a perl distribution — the configure-time half of File::ShareDir | `30_foundation` |
| `perl-file-which` | 1.27 | Find the full path of an executable on PATH, in pure perl | `40_lang` |
| `perl-font-ttf` | 1.06 | Read, edit and write TrueType and OpenType font tables from perl | `40_lang` |
| `perl-io-socket-ssl` | 2.099 | TLS sockets for perl, over Net::SSLeay | `40_lang` |
| `perl-io-string` | 1.08 | Emulate a file handle over an in-memory string in perl | `40_lang` |
| `perl-mime-base32` | 1.303 | Base32 encoding and decoding for perl (MIME::Base32) | `40_lang` |
| `perl-net-ssleay` | 1.96 | OpenSSL bindings for perl | `40_lang` |
| `perl-parse-yapp` | 1.21 | An LALR parser generator in perl — samba builds its IDL compiler with it | `40_lang` |
| `perl-uri` | 5.37 | URI class for Perl | `40_lang` |
| `perl-xml-parser` | 2.59 | Expat-based XML parser module for perl | `30_foundation` |
| `perl-xml-simple` | 2.25 | Perl module that reads and writes XML as nested data structures (config files especially) | `40_lang` |
| `perl-yaml-tiny` | 1.76 | Read and write a subset of YAML in pure perl | `40_lang` |

Notes:

- `perl`: `-Duseshrplib` builds `libperl.so`; the static default is not PIC, and `perf script`'s
  Perl engine cannot link it into a PIE. GDBM_File, NDBM_File and ODBM_File have no switch and are
  built only when Configure finds gdbm, so gdbm stays in `depends`. Configure reads neither `CFLAGS`
  nor `LDFLAGS`: the flags go in `optimize`, `ldflags` and `lddlflags`, which `Config` records for
  every XS module built afterwards.
- `perl-net-ssleay`: `OPENSSL_PREFIX=/usr` pins the openssl port, where `Makefile.PL` would
  otherwise guess among directories, and `PERL_MM_USE_DEFAULT` answers its network-test prompt,
  which otherwise waits on input.

## KDOS's own programs

These ports are KDOS's own code, filed under `src/` by the part of the system they belong to rather
than on a shelf. Three programs under `src/` have no recipe and are not catalogued:
`src/system/kdos-kpkg`, the package manager, which `10_bootstrap` compiles by script; and
`src/devtools/kdosbuild` and `src/devtools/kdos-portup`, the build orchestrator and the upstream
version checker behind `ports/update`, which run on the build host rather than in the image.
`src/libs` holds the `libk*` libraries these programs compile in; it holds no ports.

### src/system

The system layer: the `kdos` command and its services, packs and boxes, the installer, and the book on the image. On the
`PORT_REPO` of every phase from `40_lang` to `60_kernel`. 6 ports.

| Port | Version | Description | Phase |
|---|---|---|---|
| `kdos-appbox` | 1.2.0 | KDOS alien app runtime and appbox manager | `44_apps` |
| `kdos-boxinit` | 0.1.0 | pid 1 inside a pack box, in place of distrobox-init | `41_system` |
| `kdos-doc` | 1.0.0 | The KDOS book as HTML, kdos(7) and a manual page per kdos-* command | `41_system` |
| `kdos-installer` | 4.0 | KDOS installer — the disk wizard, C with no libraries | `10_bootstrap` (by script) |
| `kdos-pack` | 0.1.0 | Build, sign, index and delta packs | `41_system` |
| `kdos-tools` | 1.2.0 | KDOS system tools — kdos, services, getty, screenshots, fetch | `42_graphics` |

Notes:

- `kdos-appbox`: the `xdg-open` link to it is installed by this package in `/usr/local/bin`, ahead
  of xdg-utils' `/usr/bin/xdg-open` on `PATH`; a link under `fs/` would outlive the binary it names.
  The store catalogue ships with the program: without it the store opens empty and reports no error.
- `kdos-boxinit`: links statically; it runs inside a box whose `/usr` is a glibc pack, where a
  dynamically linked musl binary finds no loader.
- `kdos-installer`: every `.c` in its directory is compiled by glob, as
  `script/phases/10_bootstrap/130_kinstall.sh` compiles it; a name list in either place lets the two
  builds become different programs.
- `kdos-tools`: `kdos-bootctl` is linked into `/usr/bin`, the path `090_initramfs.sh` copies it from.
  `-DKDOS_HAVE_LIBPNG` gates the wallpaper retint, so the host selftest builds without libpng.

### src/art

The look of the system: theme, icons, cursors, the GTK theme, the boot splash and the demo. On the
`PORT_REPO` of every phase from `40_lang` to `60_kernel`. 6 ports.

| Port | Version | Description | Phase |
|---|---|---|---|
| `kdos-bb` | 1.3.0 | The AAlib demo, hard-forked and rebranded, with a threaded mixer | `41_system` |
| `kdos-cursors` | 2.0 | KDOS phosphor cursor theme | `41_system` |
| `kdos-gtk-theme` | 6.5 | KDOS phosphor GTK theme — a recoloured adw-gtk3 | `41_system` |
| `kdos-icons` | 20250501 | KDOS phosphor icon theme — a recoloured Papirus | `41_system` |
| `kdos-splash` | 1.1 | Boot splash — CRT power-on animation drawn on the framebuffer | `41_system` |
| `kdos-theme` | 1.0.0 | KDOS theme generators — GTK stylesheet, icons, cursors | `41_system` |

Notes:

- `kdos-bb`: built without configure; `src/aconfig.h` holds the platform answers.
- `kdos-cursors`, `kdos-gtk-theme`, `kdos-icons`: the vendored artwork ships under
  `/usr/share/kdos/`, because `kdos theme <accent>` regenerates each theme from it on the running
  system. `kdos-gtk-theme` and `kdos-icons` install no `/etc/skel` copy: `70_image/050_theme.sh`
  generates it with `kdos theme phosphor`.
- `kdos-icons`: the marks (`start-here`, `kdos-launcher`, `distributor-logo-kdos`) go into
  `icons/hicolor` as well as the theme. libkicon searches only its atlas and `hicolor`, so without
  them the Start button draws a fallback glyph. The atlas is optional: without
  `atlas/atlas.kia` the desktop draws its glyph tier.
- `kdos-splash`: runs from the initramfs, so it is linked statically, takes libkcolor's header
  only, and ships the console font decompressed to need no zlib.

### src/desktop

The compositor, the panel, the terminal, the lock screen, the resource monitor, the box socket, the
recorder and the portal. On the `PORT_REPO` of `50_desktop` only. 8 ports.

| Port | Version | Description | Phase |
|---|---|---|---|
| `kdos-boxsock` | 0.1.0 | Per-box tagged Wayland socket — the security-context-v1 engine | `50_desktop` |
| `kdos-comp` | 0.20.0 | The KDOS compositor — a hard fork of labwc 0.20.0 | `50_desktop` |
| `kdos-lock` | 0.2.0 | The KDOS lock screen and its setuid password checker | `50_desktop` |
| `kdos-record` | 0.1.0 | Screen recorder: the ScreenCast portal into a GStreamer pipeline | `50_desktop` |
| `kdos-res` | 0.2.0 | KDOS Resources — per-device pages, a process table and an application rollup | `50_desktop` |
| `kdos-shell` | 0.2.0 | The KDOS shell — a character-cell panel on layer-shell | `50_desktop` |
| `kdos-term` | 0.1.0 | KDOS terminal emulator | `50_desktop` |
| `xdg-desktop-portal-kdos` | 0.3.0 | FileChooser, Settings, AppChooser and Access portal backend for KDOS | `50_desktop` |

Notes:

- `kdos-lock`, `kdos-res`, `kdos-shell`, `kdos-term`: libkwl compiles whole, and `kwl.c` includes
  every protocol header it can use, so each consumer generates the client header and the
  private-code of the whole set (cursor-shape, tablet, viewporter, fractional-scale,
  primary-selection, xdg-shell, xdg-decoration, xdg-foreign, ext-session-lock, layer-shell,
  foreign-toplevel); a missing one fails the compile or the link. fontconfig is named beside fcft,
  which carries it only as `Requires.private`.
- `kdos-lock`, `kdos-res`: the setuid helpers `kdos-checkpass` and `kdos-resctl` are compiled on
  their own lines and link libc, libcrypt and libkbase only, never the Wayland stack.
- `kdos-res`: every `.c` but `resctl.c` is compiled by glob, as the selftest compiles it.
- `kdos-shell`: only the build-time `mkcharidx` links ICU; `kdos-shell` must not. The style picker
  is linked as `kdos-style`, because `kdos-theme` is the generator's port and path.
- `kdos-term`: ncurses is in `depends` for its terminfo database, not to link.
- `kdos-record`: GStreamer is a run-time dependency; the pipeline runs as `gst-launch-1.0`.
- `xdg-desktop-portal-kdos`: the backend installs as `/usr/lib/xdg-desktop-portal-kdos`, where
  `kdos-desktop-start` looks for portal daemons. Without `kdos.portal` (its `UseIn`) or the D-Bus
  activation file the backend is never used.

### src/daemons

The root daemons. On the `PORT_REPO` of `50_desktop` only. 5 ports.

| Port | Version | Description | Phase |
|---|---|---|---|
| `kdos-energyd` | 0.1.0 | Relative per-app Energy Impact from RAPL, attributed by container | `50_desktop` |
| `kdos-mountd` | 0.1.0 | Removable media for a desktop that is not root | `50_desktop` |
| `kdos-oomd` | 0.1.0 | PSI-triggered memory-pressure killer that spares the desktop | `50_desktop` |
| `kdos-packd` | 0.1.0 | Mounts packs, composes box root filesystems, verifies signatures | `50_desktop` |
| `kdos-powerd` | 0.1.0 | Suspend, poweroff and reboot for a desktop that is not root | `50_desktop` |

Notes:

- Every daemon here links only the `libk*` libraries its `build.sh` names: each is code running as
  root.
- `kdos-powerd`: libkcolor is linked so the `accent` verb matches its argument against the
  compiled-in scheme list; that match is the verb's whole authorisation check.

## The application-developer platform

These library ports are named by no recipe's `depends =`: nothing in the tree links them. They are
built and shipped as the platform a program written on KDOS builds against, so that the GTK 4, Qt 6,
KDE Frameworks, Qt 5 and C/C++ toolkits an application developer reaches for are complete on the
image rather than missing their less common pieces. Together they install about 230 MB, 189 MB of it
`webkitgtk6`, the GTK 4 web view. Each is in its shelf's table above as well.
`testing/bookcounts.py` fails when a port here gains a consumer, because it then belongs to that
consumer rather than to the platform.

| Port | Shelf | Platform |
|---|---|---|
| `bluez-qt` | `kf6` | KDE Frameworks: Bluetooth |
| `glog` | `devlibs` | C++ logging |
| `goocanvas` | `graphics-libs` | GTK 3 canvas |
| `gtkmm4` | `gtk` | GTK 4 from C++ |
| `gumbo-parser` | `formats` | HTML5 parsing in C |
| `kdesu` | `kf6` | KDE Frameworks: running a command as another user |
| `kholidays` | `kf6` | KDE Frameworks: holiday calendars |
| `kwayland` | `kde` | Qt 6 Wayland client protocols |
| `libdbusmenu-lxqt` | `qt-extra` | Qt 6 tray-icon menus |
| `libetpan` | `mail` | Mail protocols in C |
| `libgrapheme` | `i18n` | Unicode segmentation in C |
| `libspelling` | `spelling` | GTK 4 spell checking |
| `libxdg-basedir` | `base-libs` | XDG base directories in C |
| `networkmanager-qt` | `kf6` | KDE Frameworks: NetworkManager |
| `qt5-qtquickcontrols2` | `qt5` | Qt 5 Quick Controls 2 |
| `rttr` | `devlibs` | C++ run-time reflection |
| `sdl3-mixer` | `game-libs` | SDL 3 audio mixing |
| `webkitgtk6` | `web-engines` | The GTK 4 web view |

## Ports named in more than one list

A phase's list may name a port an earlier phase already installed. `kpkg` skips a port whose recipe
is unchanged since it was installed, wherever it is listed, so the later listing rebuilds the port
only when its recipe has changed; the tables above give the phase that installs it first. Each of
these is a toolchain or base port. `20_selfhost` installs the compilers and their tools, and
`30_foundation` names them again so that a changed recipe is rebuilt before anything uses it.
No other list names a port an earlier phase installs.

| Port | Named by | Installed by |
|---|---|---|
| `binutils` | `20_selfhost`, `30_foundation` | `20_selfhost` |
| `diffutils` | `20_selfhost`, `30_foundation` | `20_selfhost` |
| `fortify-headers` | `20_selfhost`, `30_foundation` | `20_selfhost` |
| `gawk` | `20_selfhost`, `30_foundation` | `20_selfhost` |
| `gcc` | `20_selfhost`, `30_foundation` | `20_selfhost` |
| `m4` | `20_selfhost`, `30_foundation` | `20_selfhost` |
| `musl` | `20_selfhost`, `30_foundation` | `20_selfhost` |
| `tar` | `20_selfhost`, `30_foundation` | `20_selfhost` |
| `zlib` | `20_selfhost`, `30_foundation` | `20_selfhost` |

## See also

- [Packaging](../03-architecture/packaging.md) — recipes, the repositories, shelves, phase lists
  and the `group =` key
- [The build system](../05-developer/build-system.md) — how phases are discovered, run and
  snapshotted
- [Writing ports](../05-developer/writing-ports.md) — every recipe key, how to choose a port's
  shelf, and how to add it to a list
- [Build troubleshooting](../05-developer/build-troubleshooting.md) — when a port in these tables
  fails to build
- [Repository layout](repository-layout.md) — where `ports/`, `script/` and `src/` sit in the tree

<!-- book-nav -->
---

*Part VI — Reference, chapter 38.* Previous: [37. Testing](../05-developer/testing.md) · [Contents](../README.md) · Next: [39. Command index](command-index.md)
