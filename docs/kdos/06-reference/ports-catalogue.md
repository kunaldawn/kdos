# The ports catalogue

This chapter lists every port KDOS can build: each of the 1,999 recipes under `ports/core` and the
24 recipes KDOS writes itself under `src/`, 2,023 in all, each exactly once. It is a reference for
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
`ports/shelves`, one line per shelf giving its name and what belongs on it, and the order of that
file is the order this chapter uses. A shelf is only a place to file a port. A port's identity is
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
at the head of `30_foundation`'s list, and in the `00-order.txt` file that sorts first in `40_lang`
and `41_system`.

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
`libstdcxx-arm-none-eabi` (`gcc-arm-none-eabi`), `qca` and `qca-qt5` (`qca`), `qwt` and `qwt-qt5`
(`qwt`), `qscintilla` and `python3-qscintilla` (`qscintilla`), `webkitgtk` and `webkitgtk6`
(`webkitgtk`), `texlive` and `texlive-doc` (`texlive`), `mgba` and `libretro-mgba` (`mgba`), and
`supertuxkart` and `stk-assets` (`supertuxkart`). A family released together carries one key for all
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
4. [Ports named in more than one list](#ports-named-in-more-than-one-list) and
   [Not installed](#not-installed): the exceptions.

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
| `20_selfhost` | Self-Hosting Bootstrap | `packages.txt` | 8 | 14 |
| `30_foundation` | Build Foundation | `packages.txt` | 125 | 115 |
| `31_compilers` | Compilers | `packages.txt` | 22 | 22 |
| `40_lang` | Languages | `packages.d/`, 14 files | 195 | 167 |
| `41_system` | System | `packages.d/`, 94 files | 967 | 967 |
| `42_graphics` | Graphics Stack | `packages.d/`, 54 files | 186 | 186 |
| `43_toolkits` | Toolkits | `packages.d/`, 46 files | 242 | 242 |
| `44_apps` | Applications | `packages.d/`, 55 files | 280 | 280 |
| `50_desktop` | Desktop | `packages.txt` | 22 | 22 |
| `60_kernel` | Kernel | `packages.txt` | 2 | 2 |
| `70_image` | Image | none: steps | — | — |
| Total | | | 2,049 | 2,017 |

The lists hold 2,049 lines naming 2,013 distinct ports; 34 of those ports are named by more than one
phase. Following `depends =` from the lists reaches 2,017 ports. With `kdos-installer`, which
`10_bootstrap` builds by script, that leaves 5 recipes that no phase installs.

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
| `10_bootstrap/062_toybox.sh` | `toybox` |
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

Every recipe, counted by where it lives. "Installed" counts the ports a phase installs, from its
list or by script.

| Shelf or area | Ports | Installed | Not installed |
|---|---|---|---|
| [`base`](#base) | 27 | 27 | 0 |
| [`base-libs`](#base-libs) | 16 | 16 | 0 |
| [`auth`](#auth) | 13 | 13 | 0 |
| [`boot`](#boot) | 14 | 14 | 0 |
| [`toolchain`](#toolchain) | 36 | 35 | 1 |
| [`buildtools`](#buildtools) | 19 | 18 | 1 |
| [`lang`](#lang) | 26 | 26 | 0 |
| [`devtools`](#devtools) | 33 | 33 | 0 |
| [`vcs`](#vcs) | 10 | 10 | 0 |
| [`editors`](#editors) | 12 | 11 | 1 |
| [`doctools`](#doctools) | 26 | 26 | 0 |
| [`devlibs`](#devlibs) | 34 | 34 | 0 |
| [`formats`](#formats) | 34 | 34 | 0 |
| [`database`](#database) | 15 | 15 | 0 |
| [`archiver`](#archiver) | 28 | 28 | 0 |
| [`shells`](#shells) | 10 | 10 | 0 |
| [`cli`](#cli) | 19 | 19 | 0 |
| [`files`](#files) | 11 | 11 | 0 |
| [`sysmon`](#sysmon) | 8 | 8 | 0 |
| [`hardware`](#hardware) | 27 | 27 | 0 |
| [`input`](#input) | 15 | 15 | 0 |
| [`mobile`](#mobile) | 14 | 14 | 0 |
| [`disk`](#disk) | 29 | 29 | 0 |
| [`filesystem`](#filesystem) | 20 | 20 | 0 |
| [`optical`](#optical) | 13 | 13 | 0 |
| [`crypto`](#crypto) | 31 | 31 | 0 |
| [`security`](#security) | 20 | 20 | 0 |
| [`containers`](#containers) | 18 | 18 | 0 |
| [`virt`](#virt) | 25 | 25 | 0 |
| [`net-libs`](#net-libs) | 33 | 33 | 0 |
| [`network`](#network) | 28 | 28 | 0 |
| [`net-diag`](#net-diag) | 15 | 15 | 0 |
| [`servers`](#servers) | 10 | 10 | 0 |
| [`transfer`](#transfer) | 14 | 14 | 0 |
| [`mail`](#mail) | 13 | 13 | 0 |
| [`chat`](#chat) | 16 | 16 | 0 |
| [`browsers`](#browsers) | 10 | 10 | 0 |
| [`web-engines`](#web-engines) | 14 | 14 | 0 |
| [`x11`](#x11) | 42 | 42 | 0 |
| [`wl`](#wl) | 17 | 17 | 0 |
| [`gpu`](#gpu) | 25 | 25 | 0 |
| [`graphics-libs`](#graphics-libs) | 25 | 25 | 0 |
| [`image-libs`](#image-libs) | 43 | 43 | 0 |
| [`graphics`](#graphics) | 23 | 23 | 0 |
| [`3d`](#3d) | 14 | 14 | 0 |
| [`cad`](#cad) | 17 | 17 | 0 |
| [`fabrication`](#fabrication) | 13 | 13 | 0 |
| [`fonts`](#fonts) | 24 | 24 | 0 |
| [`themes`](#themes) | 7 | 6 | 1 |
| [`xdg`](#xdg) | 15 | 15 | 0 |
| [`accessibility`](#accessibility) | 9 | 9 | 0 |
| [`i18n`](#i18n) | 14 | 14 | 0 |
| [`spelling`](#spelling) | 10 | 10 | 0 |
| [`gtk`](#gtk) | 28 | 28 | 0 |
| [`qt6`](#qt6) | 29 | 29 | 0 |
| [`qt5`](#qt5) | 11 | 11 | 0 |
| [`qt-extra`](#qt-extra) | 18 | 18 | 0 |
| [`kf6`](#kf6) | 67 | 67 | 0 |
| [`kde`](#kde) | 8 | 8 | 0 |
| [`toolkits`](#toolkits) | 4 | 4 | 0 |
| [`audio-io`](#audio-io) | 15 | 15 | 0 |
| [`audio-codecs`](#audio-codecs) | 28 | 28 | 0 |
| [`audio-dsp`](#audio-dsp) | 27 | 27 | 0 |
| [`music-libs`](#music-libs) | 17 | 17 | 0 |
| [`studio`](#studio) | 12 | 12 | 0 |
| [`music-players`](#music-players) | 10 | 10 | 0 |
| [`video-libs`](#video-libs) | 22 | 22 | 0 |
| [`media-frameworks`](#media-frameworks) | 17 | 17 | 0 |
| [`video-players`](#video-players) | 8 | 8 | 0 |
| [`video-tools`](#video-tools) | 9 | 9 | 0 |
| [`game-libs`](#game-libs) | 18 | 18 | 0 |
| [`games-action`](#games-action) | 9 | 9 | 0 |
| [`games-shooter`](#games-shooter) | 11 | 11 | 0 |
| [`games-strategy`](#games-strategy) | 12 | 12 | 0 |
| [`games-rpg`](#games-rpg) | 8 | 8 | 0 |
| [`games-board`](#games-board) | 18 | 18 | 0 |
| [`emulators`](#emulators) | 18 | 18 | 0 |
| [`libretro`](#libretro) | 12 | 12 | 0 |
| [`education`](#education) | 16 | 16 | 0 |
| [`office`](#office) | 22 | 22 | 0 |
| [`documents`](#documents) | 20 | 20 | 0 |
| [`pim`](#pim) | 12 | 12 | 0 |
| [`printing`](#printing) | 26 | 26 | 0 |
| [`math`](#math) | 24 | 24 | 0 |
| [`sci-libs`](#sci-libs) | 21 | 21 | 0 |
| [`astronomy`](#astronomy) | 14 | 14 | 0 |
| [`bioscience`](#bioscience) | 12 | 12 | 0 |
| [`gis`](#gis) | 20 | 20 | 0 |
| [`eda`](#eda) | 29 | 29 | 0 |
| [`embedded`](#embedded) | 27 | 27 | 0 |
| [`sdr-hw`](#sdr-hw) | 18 | 18 | 0 |
| [`sdr`](#sdr) | 24 | 24 | 0 |
| [`hamradio`](#hamradio) | 25 | 25 | 0 |
| [`ai`](#ai) | 7 | 7 | 0 |
| [`python`](#python) | 36 | 36 | 0 |
| [`python-libs`](#python-libs) | 63 | 63 | 0 |
| [`python-net`](#python-net) | 22 | 22 | 0 |
| [`python-sci`](#python-sci) | 13 | 13 | 0 |
| [`python-gui`](#python-gui) | 15 | 15 | 0 |
| [`python-dev`](#python-dev) | 13 | 13 | 0 |
| [`python-hw`](#python-hw) | 12 | 12 | 0 |
| [`perl-cpan`](#perl-cpan) | 18 | 17 | 1 |
| [`src/system`](#srcsystem) | 5 | 5 | 0 |
| [`src/art`](#srcart) | 6 | 6 | 0 |
| [`src/desktop`](#srcdesktop) | 8 | 8 | 0 |
| [`src/daemons`](#srcdaemons) | 5 | 5 | 0 |
| Total | 2,023 | 2,018 | 5 |

## The shelves

Each table lists one shelf's ports in alphabetical order. The line under each heading is the shelf's
own description from `ports/shelves`.

### base

The minimal userland and system daemons that every KDOS image boots with. 27 ports, under
`ports/core/base/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `bash` | 5.3 | The Bourne-Again SHell | `30_foundation`, named again by `40_lang` |
| `bc` | 1.08.2 | An arbitrary precision calculator language | `30_foundation`, named again by `40_lang` |
| `ca-certificates` | 3.130 | Bundle of CA Root Certificates from Mozilla | `30_foundation` |
| `coreutils` | 9.12 | The GNU core utilities, for the commands toybox implements too narrowly: expr and ln | `41_system` |
| `dbus` | 1.16.2 | Message bus system for communication between processes | `41_system` |
| `diffutils` | 3.12 | Utility programs for comparing files | `20_selfhost`, named again by `30_foundation`, `40_lang` |
| `eudev` | 3.2.14 | Programs for dynamic creation of device nodes | `30_foundation`, named again by `40_lang` |
| `file` | 5.48 | Identify a file by its content, and the magic database behind it | `41_system` |
| `findutils` | 4.11.0 | GNU utilities to locate files | `30_foundation`, named again by `40_lang` |
| `gawk` | 5.4.1 | GNU awk pattern scanning and processing language | `20_selfhost`, named again by `30_foundation` |
| `gzip` | 1.15 | GNU compression utility | `30_foundation`, named again by `40_lang` |
| `iana-etc` | 20260911 | /etc/services, /etc/protocols and /etc/rpc, generated from IANA's registries | `41_system` |
| `kbd` | 2.10.0 | Key-table files, console utilities and the vlock console lock | `42_graphics` |
| `kmod` | 34.2 | Libraries and utilities for loading kernel modules | `30_foundation`, named again by `40_lang` |
| `less` | 710 | A text file viewer | `30_foundation` |
| `lsof` | 4.99.7 | lsof — lists open files held by running processes | `41_system` |
| `patch` | 2.8 | GNU patch — apply a diff file to an original | `30_foundation` |
| `procps-ng` | 4.0.7 | Programs for monitoring processes | `41_system` |
| `psmisc` | 23.7 | Programs for displaying information about running processes | `41_system` |
| `seatd` | 0.9.3 | Minimal seat management daemon for non-systemd Wayland setups | `41_system` |
| `sed` | 4.10 | GNU sed — the extensions upstream build systems assume | `30_foundation`, named again by `40_lang` |
| `snooze` | 0.6 | Runs a command at a given time — one process per job in place of a cron daemon | `41_system` |
| `sysklogd` | 2.7.2 | System logging daemon with built-in rotation | `41_system` |
| `tar` | 1.35 | Tar utility | `20_selfhost`, named again by `30_foundation` |
| `toybox` | 0.8.14 | All-in-one Linux command-line utility | `30_foundation`, named again by `40_lang` |
| `tzdata` | 2026d | IANA time zone database — musl reads /usr/share/zoneinfo natively | `41_system` |
| `util-linux` | 2.42.4 | Miscellaneous system utilities for Linux | `30_foundation`, named again by `40_lang` |

### base-libs

Low-level C libraries that the base system links. 16 ports, under `ports/core/base-libs/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `acl` | 2.4.0 | Utilities to administer Access Control Lists, which are used to define more fine-grained discretionary access rights for files and directories | `30_foundation` |
| `attr` | 2.6.0 | Utilities to administer the extended attributes on filesystem objects | `30_foundation`, named again by `40_lang` |
| `basu` | 0.2.1 | sd-bus library extracted from systemd (no-systemd sd-bus provider) | `41_system` |
| `keyutils` | 1.6.3 | Tools to control the Linux key management system | `41_system` |
| `libbsd` | 0.12.2 | Functions commonly found on BSD systems, such as strlcpy() | `41_system` |
| `libcap` | 2.78 | Implements the user-space interfaces to the POSIX 1003.1e capabilities | `30_foundation` |
| `libcap-ng` | 0.9.6 | Alternate POSIX capabilities library (required by openvpn 2.6+) | `30_foundation` |
| `libedit` | 20260512 | NetBSD line editor with history, key bindings and tab completion | `41_system` |
| `libffi` | 3.8.0 | Portable foreign function interface library | `30_foundation` |
| `libmd` | 1.2.0 | Message Digest functions from BSD systems | `41_system` |
| `libxdg-basedir` | 1.2.3 | A small C library for the XDG Base Directory specification | `41_system` |
| `ncurses` | 6.6 | System V Release 4.0 curses emulation library | `20_selfhost` (as a dependency), named again by `30_foundation`, `40_lang` |
| `newt` | 0.52.25 | Programming library for color text-mode widget UIs (provides libnewt for nmtui) | `41_system` |
| `popt` | 1.19 | Popt libraries which are used by some programs to parse command-line options | `41_system` |
| `readline` | 8.3 | GNU readline library | `20_selfhost` (as a dependency), named again by `30_foundation`, `40_lang` |
| `slang` | 2.3.3 | S-Lang library — multi-platform programmer's library for text-mode UIs (newt dep) | `41_system` |

### auth

Authentication, authorisation, accounts and directory services. 13 ports, under `ports/core/auth/`.

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
| `pam` | 1.7.2 | Linux Pluggable Authentication Modules (libpam + pam_unix against /etc/shadow) | `41_system` |
| `polkit` | 127 | Authorization framework for unprivileged processes (NetworkManager, fwupd, bolt, fprintd) | `41_system` |
| `shadow` | 4.20.3 | Password and user-account management programs | `30_foundation` |
| `sudo` | 1.9.17p2 | allows a system administrator to give certain users (or groups of users) the ability to run some (or all) commands as root or another user while logging the commands and arguments | `41_system` |

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
| `linux` | 7.2.7 | Linux kernel | `60_kernel` |
| `linux-firmware` | 20260916 | Device firmware blobs — upstream's complete tree, unpruned | `41_system` |
| `memtest86plus` | 8.10 | Standalone memory tester that runs outside the operating system | `41_system` |
| `sof-firmware` | 2026.09.1 | Intel Sound Open Firmware DSP firmware and topologies | `41_system` |
| `wireless-regdb` | 2026.09.03 | Signed wireless regulatory database — without it the kernel applies the restrictive world domain | `41_system` |

### toolchain

The native compilers, the linker, the C library, and the LLVM family. 36 ports, under
`ports/core/toolchain/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `argp-standalone` | 1.4.1 | Standalone version of arguments parsing functions from GLIBC | `30_foundation` |
| `bindgen` | 0.73.2 | Generates Rust FFI bindings from C and C++ headers | `31_compilers` |
| `binutils` | 2.47 | A collection of binary tools | `20_selfhost`, named again by `30_foundation`, `40_lang` |
| `bison` | 3.8.2 | The GNU Bison parser generator | `30_foundation`, named again by `40_lang` |
| `bmake` | 20260912 | NetBSD make, portable — the make that BSD-style makefiles need | `41_system` |
| `cbindgen` | 0.29.4 | A project for generating C bindings from Rust code | `31_compilers` |
| `clang` | 23.1.2 | The C/C++ compiler, plus clangd, clang-tidy and clang-format | `31_compilers` |
| `clang21` | 21.1.8 | Clang 21 libraries and compiler, installed under /usr/lib/llvm21 for zig | `31_compilers` |
| `compiler-rt` | 23.1.2 | clang's runtime libraries — builtins, the profile runtime and UndefinedBehaviorSanitizer | `31_compilers` |
| `elfutils` | 0.196 | utilities and libraries for handling ELF files | `30_foundation`, named again by `40_lang` |
| `flex` | 2.6.4 | Lexical analyser generator | `30_foundation`, named again by `40_lang` |
| `gcc` | 16.2.0 | The GNU compiler collection | `20_selfhost`, named again by `30_foundation` |
| `gperf` | 3.3 | Generates a perfect hash function from a key set | `30_foundation` |
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
| `make` | 4.4.1 | GNU make utility | `30_foundation`, named again by `40_lang` |
| `musl` | 1.2.6 | Musl C library | `20_selfhost`, named again by `30_foundation` |
| `musl-fts` | 1.2.7 | Implementation of fts(3) for musl libc | `30_foundation` |
| `musl-ldd` | 1.2.5 | LDD script for Musl | `40_lang` |
| `musl-locales` | 20260425 | A locale command and message catalogues for musl | not installed |
| `musl-obstack` | 1.2.3 | Obstack standalone library | `30_foundation` |
| `musl-rpmatch` | 1.0 | Implementation of rpmatch(3) for musl libc | `30_foundation` |
| `nasm` | 3.02 | The Netwide Assembler, a portable 80x86 assembler | `30_foundation` |
| `openmp` | 23.1.2 | LLVM's OpenMP runtime, libomp — what clang -fopenmp links | `31_compilers` |
| `patchelf` | 0.19.1 | Utility to modify ELF executables | `30_foundation` |
| `spirv-llvm-translator` | 23.1.1 | Tool and library for translation between LLVM IR and SPIR-V | `41_system` |
| `swig` | 4.5.1 | Generate scripting interfaces to C/C++ code | `30_foundation` |
| `yasm` | 1.3.0 | Assembler for the x86 and AMD64 instruction sets, a rewrite of NASM | `30_foundation` |

### buildtools

Build systems and build helpers. 19 ports, under `ports/core/buildtools/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `autoconf` | 2.73 | Programs for producing shell scripts that can automatically configure source code | `30_foundation`, named again by `40_lang` |
| `autoconf-archive` | 2024.10.16 | The AX_* m4 macros a configure.ac reaches for and autoconf does not carry | `40_lang` |
| `automake` | 1.19 | Programs for generating Makefiles for use with Autoconf | `30_foundation`, named again by `40_lang` |
| `buildsystem` | 1.10 | NetSurf shared build framework (Makefile fragments used by libnsgif and friends) | `40_lang` |
| `cargo-c` | 0.10.25 | Cargo subcommand to build and install C-ABI-compatible dynamic and static libraries | `31_compilers` |
| `ccache` | 4.14 | Compiler cache | `31_compilers` |
| `cmake` | 4.4.3 | Cross-platform build-system generator | `30_foundation` |
| `corrosion` | 0.6.1 | The CMake bridge that builds a cargo crate as a CMake target | `40_lang` |
| `gn` | 0.2480 | Chromium's meta-build system, which writes ninja files from BUILD.gn | `40_lang` |
| `intltool` | 0.51.0 | An internationalization tool used for extracting translatable strings from source files | `30_foundation`, named again by `40_lang` |
| `itstool` | 2.0.7 | Translates XML documents with PO files | `30_foundation` |
| `libtool` | 2.6.2 | The GNU generic library support script | `30_foundation`, named again by `40_lang` |
| `meson` | 1.12.1 | Build system that generates Ninja build files | `30_foundation` |
| `ninja` | 1.13.2 | Low-level build system that runs generated build files | `30_foundation` |
| `pkgconf` | 3.0.7 | Package compiler and linker metadata toolkit | `30_foundation`, named again by `40_lang` |
| `scons` | 4.11.1 | Python-based build tool (gpsd builds with it) | `40_lang` |
| `setconf` | 0.7.7 | Utility for changing settings in configuration files | not installed |
| `unifdef` | 2.12 | Selectively remove C preprocessor conditionals | `40_lang` |
| `util-macros` | 1.20.2 | X.Org Autotools macros | `40_lang` |

### lang

Language implementations, plus the modules of any language that has no module shelf of its own. 26
ports, under `ports/core/lang/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `R` | 4.6.1 | R — the language and environment for statistics, with the recommended packages | `42_graphics` |
| `bwidget` | 1.10.1 | BWidget — high-level Tk widgets written in pure Tcl: trees, notebooks, combo boxes, dialogs | `42_graphics` |
| `cabal-install` | 3.18.1.0 | The cabal command — builds and installs Haskell packages | `31_compilers` |
| `duktape` | 2.7.0 | Embeddable JavaScript engine (used by polkit for rule processing) | `40_lang` |
| `esbuild` | 0.25.1 | JavaScript and TypeScript bundler and minifier | `40_lang` |
| `ghc` | 9.12.4 | The Glasgow Haskell Compiler, its libraries and GHCi | `31_compilers` |
| `go` | 1.27.1 | The Go programming language toolchain (with bootstrap) | `31_compilers` |
| `guile` | 3.0.11 | GNU Guile 3.0 — the Scheme implementation GnuCash and Aisleriot embed, with guild | `41_system` |
| `lua` | 5.5.1 | Lightweight programming language designed for extending applications | `30_foundation` |
| `lua54` | 5.4.9 | Lua 5.4, installed beside Lua 5.5 for software that requires 5.4 | `30_foundation` |
| `lua54-luaexpat` | 1.5.2 | Expat XML parser binding for Lua 5.4 — Prosody reads XMPP stanzas with it | `30_foundation` |
| `lua54-luafilesystem` | 1.9.0 | stat, mkdir and readdir for Lua 5.4 | `30_foundation` |
| `lua54-luasec` | 1.3.2 | TLS for LuaSocket on Lua 5.4 — Prosody encrypts its connections with it | `30_foundation` |
| `lua54-luasocket` | 3.1.0 | TCP, UDP and DNS for Lua 5.4 — Prosody's transport | `30_foundation` |
| `luajit` | 20260914 | Just-in-time compiler and drop-in replacement for Lua. | `40_lang` |
| `nodejs` | 26.10.0 | JavaScript runtime built on Chrome's V8 JavaScript engine | `31_compilers` |
| `ocaml` | 5.5.1 | OCaml — the native and bytecode compilers, runtime and standard library | `40_lang` |
| `ocaml-facile` | 1.1.4 | FaCiLe — constraint programming library for OCaml, the solver behind Kalzium's equation balancer | `40_lang` |
| `openjdk` | 25.0.4.1 | OpenJDK, the Java Development Kit: the JVM, the compiler and the class library (LTS) | `42_graphics` |
| `ruby` | 4.0.7 | The Ruby programming language | `31_compilers` |
| `rust` | 1.98.1 | The Rust compiler, with cargo, clippy, rustdoc and rustfmt | `31_compilers` |
| `tcl` | 9.0.4 | The Tcl scripting language — the interpreter yosys and weechat embed | `41_system` |
| `tk` | 9.0.4 | Tk, the GUI toolkit for Tcl — wish and libtcl9tk, drawn through X11 under Xwayland | `42_graphics` |
| `vala` | 0.56.19 | Vala compiler and vapigen — C#-like language compiled to GObject C | `41_system` |
| `yarn` | 4.18.1 | Yarn — the JavaScript package manager, the Berry line | `40_lang` |
| `zig` | 0.16.0 | Zig — general-purpose programming language and toolchain | `31_compilers` |

### devtools

Debuggers, tracers, profilers, analysers, linters, language servers and developer utilities. 33
ports, under `ports/core/devtools/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `bcc` | 0.37.0 | The BPF Compiler Collection library, USDT probe support and the libbpf-tools tracers | `40_lang` |
| `bpftool` | 7.7.0 | Inspect and manage BPF programs and maps, and generate BPF skeletons | `40_lang` |
| `bpftrace` | 0.27.0 | High-level tracing language for eBPF, for tracing inside the kernel | `41_system` |
| `capstone` | 5.0.9 | Disassembly engine library (rizin disassembles with it) | `40_lang` |
| `delta` | 0.19.2 | delta — a syntax-highlighting pager for git, diff, and grep output | `41_system` |
| `difftastic` | 0.71.0 | Syntax-aware structural diff | `40_lang` |
| `dwarves` | 1.32 | pahole and the DWARF tools — encodes the kernel's BTF type information | `60_kernel` |
| `gdb` | 17.2 | GNU Debugger | `40_lang` |
| `gef` | 2026.01 | GDB extension with context, heap and ROP views | `40_lang` |
| `gopls` | 0.23.0 | The Go language server | `40_lang` |
| `hurl` | 8.0.1 | Runs HTTP requests and their assertions from a plain-text file | `40_lang` |
| `hyperfine` | 1.20.0 | Statistical command-line benchmarking with warmup and outliers | `40_lang` |
| `imhex` | 1.38.1 | Hex editor for reverse engineering — pattern language, disassembler, diffing and data inspectors | `42_graphics` |
| `just` | 1.58.0 | just — a command runner for project-specific recipes | `40_lang` |
| `kdevelop-pg-qt` | 2.4.0 | KDevelop-PG-Qt — LL(1) parser generator, for KDevelop's QMake project manager | `43_toolkits` |
| `libbpf` | 1.7.0 | The userspace side of BPF — load a program, read a map | `40_lang` |
| `libtraceevent` | 1.9.0 | Parser for the kernel's ftrace event format | `40_lang` |
| `libtracefs` | 1.8.3 | API for the tracefs filesystem | `40_lang` |
| `ltrace` | 0.8.1 | ltrace — library call tracer | `40_lang` |
| `perf` | 7.2.7 | The kernel's own profiler: sampling, counters, tracepoints and BPF | `41_system` |
| `rizin` | 0.9.1 | Reverse-engineering framework — disassembler and binary analysis in the terminal | `41_system` |
| `ruff` | 0.16.9 | Ruff — the Python linter and formatter, as the ruff command and the Python module that runs it | `40_lang` |
| `rust-analyzer` | 2026.09.21 | The Rust language server | `40_lang` |
| `shellcheck` | 0.11.0 | Static analysis for sh and bash scripts — quoting, word splitting and portability | `41_system` |
| `shfmt` | 3.14.1 | Shell parser, formatter and linter | `40_lang` |
| `strace` | 7.2 | strace — system call tracer | `40_lang` |
| `tokei` | 15.0.0 | Counts lines of code by language | `40_lang` |
| `tree-sitter` | 0.27.0 | The parser library and CLI — grammars compiled here, never fetched | `40_lang` |
| `universal-ctags` | 6.2.1 | Source-code tag index generator | `41_system` |
| `valgrind` | 3.27.1 | valgrind — instrumentation framework for memory debugging and profiling | `40_lang` |
| `xh` | 0.26.2 | Command-line HTTP client | `40_lang` |
| `zeal` | 0.9.1 | Offline documentation browser — searchable API docsets for languages and libraries | `44_apps` |
| `zls` | 0.16.0 | The Zig language server | `40_lang` |

### vcs

Version control and diff/merge tools. 10 ports, under `ports/core/vcs/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `gh` | 2.101.0 | gh — official GitHub CLI | `40_lang` |
| `git` | 2.55.0 | Distributed version control system | `30_foundation` |
| `git-cola` | 4.19.0 | Git Cola — a Git GUI for staging, committing and reviewing, with the git-dag history viewer | `44_apps` |
| `git-lfs` | 3.8.0 | Git extension that stores large files out of band | `40_lang` |
| `jujutsu` | 0.45.1 | Git-compatible version control system with an operation log | `40_lang` |
| `kdiff3` | 1.12.6 | KDiff3 — two- and three-way file and folder comparison and merge | `44_apps` |
| `lazygit` | 0.65.1 | lazygit — terminal UI for git | `40_lang` |
| `libgit2` | 1.9.7 | libgit2 — portable, linkable C implementation of the Git core methods | `40_lang` |
| `libkomparediff2` | 26.08.1 | libkomparediff2 — diff parsing and models, for KDevelop's patch review | `43_toolkits` |
| `tig` | 2.6.1 | ncurses interface for reading a git repository | `40_lang` |

### editors

Text editors, IDEs, notebooks and REPLs. 12 ports, under `ports/core/editors/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `geany` | 2.1 | GTK programmer's editor — syntax highlighting, symbols, build commands | `44_apps` |
| `helix` | 25.07.1 | helix — modal text editor (Rust) | not installed |
| `ipython` | 9.17.1 | Interactive Python shell | `41_system` |
| `jupyterlab` | 4.6.4 | JupyterLab and Jupyter Notebook — computational notebooks served locally to the web browser | `44_apps` |
| `kate` | 26.08.1 | KDE advanced text editor — tabs, split views, LSP, projects, search, terminal | `44_apps` |
| `kdevelop` | 26.08.1 | KDevelop — the KDE IDE for C, C++ and more, with libclang code analysis | `44_apps` |
| `lite-xl` | 2.1.8 | Text editor written in Lua, drawn with SDL3 and no toolkit | `44_apps` |
| `micro` | 2.0.15 | micro — terminal text editor (Go) | `41_system` |
| `nano` | 9.2 | Simple text editor which aims to replace Pico, the default editor in the Pine package | `41_system` |
| `neovim` | 0.12.5 | neovim — extensible Vim-based text editor | `41_system` |
| `qt-creator` | 20.0.2 | Qt Creator — the Qt IDE for C++, QML and Python, with a clangd code model | `44_apps` |
| `spyder` | 6.1.7 | Spyder — the scientific Python IDE: editor, IPython console, variable explorer and plots | `44_apps` |

### doctools

Documentation toolchains, man pages, markup converters and TeX typesetting. 26 ports, under
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
| `go-md2man` | 2.0.7 | Converts Markdown to roff manual pages | `40_lang` |
| `groff` | 1.24.1 | GNU roff — troff, nroff, eqn, tbl, pic and the PDF, PostScript and HTML output devices | `41_system` |
| `gtk-doc` | 1.36.1 | The m4 macro and gtk-doc.make that GNOME-lineage autotools projects include | `40_lang` |
| `help2man` | 1.49.3 | Turns a program's own --help into a man page | `30_foundation` |
| `lowdown` | 3.2.1 | Markdown translator to roff manual pages, HTML and LaTeX | `41_system` |
| `lyx` | 2.5.3 | Document processor that writes LaTeX — structure-first editing with typeset output | `44_apps` |
| `man-pages` | 6.19 | Linux kernel and C library manual pages — system calls, library functions, devices, file formats, overviews | `40_lang` |
| `mandoc` | 1.14.6 | Compact suite of tools for BSD mdoc and man formatting | `30_foundation` |
| `pandoc` | 3.11 | Converts between markup formats — Markdown, HTML, LaTeX, DOCX, ODT, EPUB and more | `31_compilers` |
| `scdoc` | 1.11.5 | Simple man page generator for POSIX systems written in C99 | `30_foundation` |
| `sgml-common` | 0.6.3 | Creating and maintaining centralized SGML catalogs | `30_foundation` |
| `smu` | 1.5 | Markdown to HTML in one C file | `40_lang` |
| `texinfo` | 7.3 | Programs for reading, writing, and converting info pages | `30_foundation`, named again by `40_lang` |
| `texlive` | 20260301 | TeX Live 2026 — TeX, LaTeX, pdfTeX, XeTeX, LuaTeX, MetaPost, BibTeX and dvips, with a curated texmf tree | `42_graphics` |
| `texlive-doc` | 20260301 | TeX Live 2026 documentation — the English manuals and examples of every package the texlive port ships, for texdoc | `42_graphics` |
| `texstudio` | 4.9.8 | LaTeX editor — completion, live PDF preview with SyncTeX, spelling and grammar checking | `44_apps` |
| `typst` | 0.15.1 | Markup-based typesetting system | `40_lang` |
| `xmlto` | 0.0.29 | Front-end to an XSL toolchain | `30_foundation` |
| `xmltoman` | 0.6 | Convert manual pages written in XML to groff or HTML | `40_lang` |

### devlibs

General-purpose C and C++ utility libraries with no domain. 34 ports, under `ports/core/devlibs/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `abseil-cpp` | 20260817.0 | Abseil — Google's C++ common libraries (strings, containers, synchronization, time) | `41_system` |
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
| `jemalloc` | 5.4.0 | jemalloc — a malloc that holds fragmentation down under many threads | `41_system` |
| `lager` | 0.1.3 | Value-oriented unidirectional data-flow architecture for C++ (headers) | `41_system` |
| `libdaemon` | 0.14 | C library for writing UNIX daemons (avahi dependency) | `41_system` |
| `libtommath` | 1.3.0 | Multiple-precision integers — tcl bundles a renamed copy and yosys needs the real one | `41_system` |
| `liburcu` | 0.15.7 | Userspace RCU — required by xfsprogs, no --disable option exists | `41_system` |
| `mustache` | 4.1 | Kainjow Mustache, a header-only Mustache template engine for C++11 | `41_system` |
| `onetbb` | 2023.1.0 | oneAPI Threading Building Blocks: task-parallel C++ runtime | `42_graphics` |
| `pcre2` | 10.48 | Perl Compatible Regular Expressions library, version 2 | `30_foundation` |
| `pystring` | 1.2.0 | C++ functions with the behaviour of Python's string methods | `41_system` |
| `range-v3` | 0.12.0 | range-v3 — the header-only C++ ranges library C++20 ranges grew from | `41_system` |
| `re2` | 2025.11.05 | Google's linear-time regular expression library, with full Unicode properties from ICU | `41_system` |
| `rinutils` | 0.10.3 | Header-only C utility macros shared by Shlomi Fish's solvers | `41_system` |
| `rttr` | 0.9.6 | Run-time type reflection library for C++ | `41_system` |
| `simde` | 0.8.2 | Portable implementations of SIMD intrinsics, header-only | `41_system` |
| `sparsehash` | 2.0.4 | Memory-efficient C++ hash map and set templates | `41_system` |
| `spdlog` | 1.17.0 | C++ logging library, over the system fmt | `41_system` |
| `talloc` | 2.5.0 | Hierarchical reference-counted memory pool library from Samba | `41_system` |
| `tllist` | 1.1.0 | C header-only typed linked list library (foot/fcft dep) | `41_system` |
| `uthash` | 2.4.0 | A hash table for C structures (header-only) | `41_system` |
| `xxhash` | 0.8.4 | Non-cryptographic hash algorithm library | `20_selfhost` (as a dependency) |
| `zug` | 0.1.2 | Transducers for C++ (headers) | `41_system` |

### formats

Data-format and serialisation libraries (XML, JSON, YAML, TOML, INI, RDF, protobuf, thrift), plus
data query and convert tools. 34 ports, under `ports/core/formats/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `cereal` | 1.3.2 | Header-only C++ serialisation — bpftrace's BTF cache format | `41_system` |
| `cjson` | 1.7.19 | cJSON, a small ANSI C JSON parser (mosquitto links it) | `41_system` |
| `dotconf` | 1.4.1 | Configuration file parser (speech-dispatcher dependency) | `41_system` |
| `expat` | 2.8.5 | A stream oriented C library for parsing XML | `30_foundation` |
| `fx` | 39.2.0 | fx — an interactive viewer and processor for JSON and YAML | `41_system` |
| `gumbo-parser` | 0.14.1 | HTML5 parsing library in C | `41_system` |
| `inih` | r62 | Tiny C library for parsing INI files (used by xdg-desktop-portal-wlr config) | `41_system` |
| `iniparser` | 4.3.0 | Small C library for parsing INI files | `41_system` |
| `jansson` | 2.15.1 | C library for encoding/decoding/manipulating JSON (NetworkManager dep) | `41_system` |
| `jq` | 1.8.2 | jq — command-line JSON processor | `41_system` |
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
| `thrift` | 0.24.0 | Apache Thrift — the IDL compiler, the C++ library and the Python module; GNU Radio's ControlPort transport | `41_system` |
| `tinyxml` | 2.6.2 | Small C++ XML parser (TinyXML 1) | `41_system` |
| `tomlplusplus` | 3.4.0 | TOML parser and serializer for C++17 | `41_system` |
| `visidata` | 3.4 | Terminal spreadsheet for tabular files | `42_graphics` |
| `xerces-c` | 3.3.0 | Validating XML parser library in C++ (DOM, SAX, schema) | `41_system` |
| `yajl` | 2.1.0 | Yet Another JSON Library — small, event-driven C JSON parser | `41_system` |
| `yaml` | 0.2.5 | C library for parsing and emitting YAML | `30_foundation` |
| `yaml-cpp` | 0.9.0 | YAML 1.2 parser and emitter for C++ | `41_system` |
| `yq` | 4.53.6 | yq — a portable YAML / JSON / TOML / XML processor (Go, Mike Farah) | `41_system` |

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
| `tdb` | 1.4.15 | Trivial database library — Rhythmbox's metadata cache | `41_system` |
| `unixodbc` | 2.3.14 | unixODBC — the ODBC driver manager KiCad and LibreOffice Base connect to databases through | `41_system` |
| `usql` | 0.21.6 | Command-line SQL client for PostgreSQL, SQLite and other databases | `41_system` |
| `valkey` | 9.1.2 | In-memory key-value store — the BSD continuation of Redis | `41_system` |
| `xapian-core` | 2.1.0 | The search engine library recoll indexes into | `41_system` |

### archiver

Compression libraries and archive tools. 28 ports, under `ports/core/archiver/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `7zip` | 26.03 | 7-Zip's 7zz — packs and unpacks 7z, zip, rar, tar, iso, cab, deb, rpm and disk images | `41_system` |
| `ark` | 26.08.1 | Archive manager — browse, extract and create tar, zip, 7z and compressed archives | `44_apps` |
| `brotli` | 1.2.0 | Brotli compression library | `30_foundation` |
| `bzip2` | 1.0.8 | Programs for compressing and decompressing files | `30_foundation`, named again by `40_lang` |
| `c-blosc` | 1.21.6 | Blosc: a blocking, shuffling compressor for binary data — OpenVDB's grid codec | `41_system` |
| `cabextract` | 1.11 | Extract Microsoft cabinet (.cab) files | `41_system` |
| `lhasa` | 0.6.0 | Free LHA/LZH archive library and lha extractor — MilkyTracker's .lha module archives | `41_system` |
| `libaec` | 1.1.7 | Adaptive Entropy Coding library and its libsz drop-in replacement for SZIP | `41_system` |
| `libarchive` | 3.8.9 | Reading/writing various compression formats | `30_foundation` |
| `libdeflate` | 1.26 | DEFLATE/zlib/gzip compression library | `41_system` |
| `libmspack` | 0.11alpha | Library for Microsoft compression formats — CAB, CHM, HLP, LIT, KWAJ, SZDD | `41_system` |
| `libzip` | 1.11.4 | C library for reading and writing zip archives | `41_system` |
| `lz4` | 1.10.0 | LZ4 compression library and command-line tool | `30_foundation` |
| `lzip` | 1.26 | Lossless data compressor for the .lz format | `41_system` |
| `lzo` | 2.10 | Data compression library | `30_foundation` |
| `minizip` | 1.3.2 | Read and write ZIP archives: the minizip library from zlib's contrib | `41_system` |
| `minizip-ng` | 4.2.2 | Zip archive manipulation library, the zlib-ng fork of minizip | `41_system` |
| `ouch` | 0.8.3 | Compresses and decompresses many archive formats with one command | `41_system` |
| `par2cmdline-turbo` | 1.5.0 | PAR2 recovery files with SIMD-accelerated Reed-Solomon coding | `41_system` |
| `unshield` | 1.6.2 | Extract InstallShield cabinet (.cab) archives | `41_system` |
| `unzip` | 6.0 | ZIP extraction utilities | `41_system` |
| `xz` | 5.8.4 | XZ compression utilities | `30_foundation`, named again by `40_lang` |
| `zip` | 3.0 | Info-ZIP archive creation utilities | `41_system` |
| `zlib` | 1.3.2 | Compression library implementing the deflate compression method | `20_selfhost`, named again by `30_foundation` |
| `zlib-ng` | 2.3.3 | zlib-ng — zlib data compression with SIMD, under its own zng_ names beside the system zlib | `41_system` |
| `zopfli` | 1.0.3 | Deflate compressor that trades time for size, with zopflipng for PNG files | `41_system` |
| `zstd` | 1.5.7 | Zstandard compression library and tools | `30_foundation` |
| `zziplib` | 0.13.80 | Read files inside ZIP archives through a stdio-like API — MilkyTracker's zipped modules and TeX Live's LuaTeX | `41_system` |

### shells

Interactive shells and their add-ons, terminal multiplexers, and terminal emulators. 10 ports, under
`ports/core/shells/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `atuin` | 18.23.0 | Shell history in a searchable SQLite database, local only | `41_system` |
| `bash-completion` | 2.18.0 | Programmable completion for the bash shell | `41_system` |
| `byobu` | 7.19 | Text-based window manager and terminal multiplexer | `41_system` |
| `direnv` | 2.37.1 | direnv — loads and unloads environment variables per directory | `41_system` |
| `foot` | 1.28.0 | Wayland terminal emulator | `42_graphics` |
| `konsole` | 26.08.1 | KDE terminal emulator — tabs, split views, profiles, SSH manager | `43_toolkits` |
| `starship` | 1.26.0 | starship — configurable shell prompt | `41_system` |
| `tmux` | 3.7c | tmux - Terminal multiplexer | `41_system` |
| `zoxide` | 0.10.0 | zoxide — a cd command that ranks directories by use | `41_system` |
| `zsh` | 5.9.2 | Z shell, with an extensive completion system | `41_system` |

### cli

Command-line productivity utilities: search, view, navigation and text wrangling in the terminal. 19
ports, under `ports/core/cli/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `bat` | 0.26.1 | bat — a cat clone with syntax highlighting and Git integration | `41_system` |
| `bitwise` | 0.70 | Bit-level manipulation and base conversion, watched live in the terminal | `41_system` |
| `chezmoi` | 2.72.2 | Manage dotfiles across machines from one source of truth | `41_system` |
| `entr` | 5.8 | Run an arbitrary command when files change | `41_system` |
| `eza` | 0.23.5 | eza — an ls replacement | `41_system` |
| `fd` | 10.5.0 | fd — a file finder, an alternative to find | `41_system` |
| `fzf` | 0.74.4 | fzf — general-purpose command-line fuzzy finder | `41_system` |
| `glow` | 3.0.0 | Render markdown on the command line | `41_system` |
| `hexyl` | 0.17.0 | Hex viewer that colours bytes by class | `41_system` |
| `lesspipe` | 2.28 | An input filter for less that shows what is inside a file | `41_system` |
| `lnav` | 0.14.1 | Log navigator with SQL over log lines | `41_system` |
| `parallel` | 20260922 | Run shell command lines in parallel across cores and hosts | `41_system` |
| `pv` | 1.12.0 | Pipe viewer — progress, throughput and ETA for any stream | `41_system` |
| `ripgrep` | 15.2.0 | ripgrep — recursively search directories for a regex pattern | `41_system` |
| `ripgrep-all` | 0.10.10 | ripgrep that also searches inside PDFs, archives and spreadsheets | `44_apps` |
| `sd` | 1.1.0 | Find-and-replace, a sed alternative for the common case | `41_system` |
| `tealdeer` | 1.9.0 | Example-first help — the tldr pages, pre-seeded and never fetched | `41_system` |
| `tree` | 2.3.2 | display directory contents, including directories, files, links, in a terminal | `41_system` |
| `ttyper` | 1.6.0 | Terminal touch-typing practice with per-key statistics | `41_system` |

### files

File managers, disk-usage analysers, duplicate finders and file locators. 11 ports, under
`ports/core/files/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `dolphin` | 26.08.1 | File manager — tabs, split views, places, previews, a terminal panel and devices | `44_apps` |
| `duf` | 0.9.1 | duf — disk usage and free space, in the manner of df | `41_system` |
| `dust` | 1.2.6 | dust — disk usage as a tree, a du alternative (Rust) | `41_system` |
| `filelight` | 26.08.1 | Disk usage — folders drawn as concentric rings, sized by what they hold | `44_apps` |
| `lf` | 42 | lf — terminal file manager (Go, single binary) | `41_system` |
| `mc` | 4.8.33 | GNU Midnight Commander — Norton-style TUI file manager | `41_system` |
| `ncdu` | 2.11.1 | ncdu — NCurses Disk Usage analyzer | `41_system` |
| `plocate` | 1.1.25 | Finds a file by name in a prebuilt index, per user and without a setgid bit | `41_system` |
| `qdirstat` | 2.0 | QDirStat — disk usage as a tree and a treemap, with cleanup actions | `44_apps` |
| `rdfind` | 1.8.0 | Find duplicate files and replace them with hardlinks or symlinks | `41_system` |
| `yazi` | 26.9.1 | Asynchronous terminal file manager with image previews | `44_apps` |

### sysmon

Process, resource and system-information monitors. 8 ports, under `ports/core/sysmon/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `bottom` | 0.14.9 | bottom (btm) — graphical process and system monitor for the terminal | `41_system` |
| `btop` | 1.4.7 | A monitor for system resources | `41_system` |
| `fastfetch` | 2.68.1 | fastfetch — system information tool, ships the KDOS banner config | `44_apps` |
| `htop` | 3.5.3 | Interactive process viewer | `41_system` |
| `iotop` | 1.31 | iotop — top-like utility for I/O (C rewrite) | `41_system` |
| `nvtop` | 3.3.2 | GPU utilisation from DRM fdinfo — top for every vendor's GPU | `42_graphics` |
| `procs` | 0.14.12 | procs — a ps replacement | `41_system` |
| `resources` | 1.10.2 | Resources — system monitor for CPU, memory, GPU, NPU, disks, network, battery and processes (GTK 4) | `44_apps` |

### hardware

Device access, bus and sensor tools, power management, and Bluetooth. 27 ports, under
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
| `libpciaccess` | 0.19 | X11 PCI access library | `41_system` |
| `libserialport` | 0.1.2 | Enumerate and open a serial port, portably | `41_system` |
| `libusb` | 1.0.30 | Library used by some applications for USB device access | `41_system` |
| `lirc` | 0.10.2 | Infrared remote control daemon, client library and tools — lircd, irexec, irw, liblirc_client and the Python bindings | `41_system` |
| `lm-sensors` | 3.6.2 | Hardware sensor readings — temperatures, fans and voltages from hwmon | `41_system` |
| `numactl` | 2.0.19 | NUMA policy library (libnuma) and the numactl, numastat and migratepages tools | `41_system` |
| `nut` | 2.8.5 | Network UPS Tools — watch a UPS or inverter and shut down before the battery does | `41_system` |
| `pciutils` | 3.15.0 | Set of programs for listing PCI devices | `41_system` |
| `powerman` | 2.4.4 | PowerMan — control remote power distribution units and BMCs over telnet, serial, HTTP, Redfish and SNMP | `41_system` |
| `powertop` | 2.16 | Power consumption diagnosis and tuning tool | `41_system` |
| `thermald` | 2.5.13 | Intel thermal daemon — keeps a laptop off its throttle point under load | `41_system` |
| `tlp` | 1.10.2 | Laptop power saving — pure shell over /sys, no daemon | `41_system` |
| `upower` | 1.91.4 | D-Bus daemon for power-related stats and policy (battery, AC, suspend hints) | `41_system` |
| `usbutils` | 019 | Utilities used to display information about USB devices | `41_system` |

### input

Input-device libraries, keyboard data, input methods and on-screen keyboards. 15 ports, under
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
| `libinput` | 1.32.0 | library that handles input devices for display servers and other applications that need to directly deal with input devices | `41_system` |
| `libwacom` | 2.20.0 | The tablet database libinput reads to pair a stylus with its pad and tell a pen display from a tablet | `41_system` |
| `libxkbcommon` | 1.13.2 | Keymap compiler and support library, with its Wayland tools and xkbcommon-x11 | `42_graphics` |
| `mtdev` | 1.1.7 | Multitouch Protocol Translation Library which is used to transform all variants of kernel MT (Multitouch) events to the slotted type B protocol | `41_system` |
| `wvkbd` | 0.20 | On-screen keyboard for Wayland — layer-shell and virtual-keyboard, drawn with cairo and pango | `42_graphics` |
| `xkeyboard-config` | 2.48 | Keyboard configuration database (keymap data, used by libxkbcommon) | `41_system` |

### mobile

Phones, media players and cameras over USB or the network. 14 ports, under `ports/core/mobile/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `android-file-transfer` | 4.5 | An Android phone's storage in a window, as a FUSE directory (aft-mtp-mount) and in the aft-mtp-cli shell | `44_apps` |
| `android-tools` | 37.0.0 | Android platform tools — adb, fastboot, and the sparse-image and boot-image utilities | `41_system` |
| `gphoto2` | 2.5.32 | Tethered capture and card download from a prompt | `44_apps` |
| `ifuse` | 1.2.1 | FUSE filesystem that mounts an iPhone's or iPad's media and app documents | `41_system` |
| `kdeconnect` | 26.08.1 | KDE Connect — files, clipboard, notifications, media control and remote input between this machine and a phone, over the LAN or Bluetooth | `44_apps` |
| `libgphoto2` | 2.5.34 | Drive a camera over PTP — the udev rules already grant the class | `42_graphics` |
| `libgpod` | 0.8.3 | Library to read and write the music database of an iPod classic, nano and shuffle | `41_system` |
| `libimobiledevice` | 1.4.0 | Library and idevice* tools that speak the iPhone and iPad protocols: pairing, backup, files, syslog | `41_system` |
| `libimobiledevice-glue` | 1.3.2 | Common socket, thread and utility code shared by the libimobiledevice libraries | `41_system` |
| `libmtp` | 1.1.23 | Talk MTP to a phone or a player — the class gphoto2 cannot reach | `41_system` |
| `libplist` | 2.7.0 | Apple property list library and plistutil — binary, XML, JSON and OpenStep plists | `41_system` |
| `libtatsu` | 1.0.5 | Client for Apple's Tatsu Signing Server, as used to restore and personalise iOS devices | `41_system` |
| `libusbmuxd` | 2.1.1 | Client library for usbmuxd, with iproxy and inetcat to tunnel TCP to an iOS device | `41_system` |
| `usbmuxd` | 1.1.1.20251206 | USB multiplexing daemon that carries every connection to an iPhone or iPad over its cable | `41_system` |

### disk

Block devices: partitioning, RAID, LVM, block encryption, SMART, NVMe, recovery, image writing and
async I/O. 29 ports, under `ports/core/disk/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `caligula` | 0.5.0 | Writes an image to a removable disk, with a progress display and a verify pass | `41_system` |
| `cryptsetup` | 2.8.8 | Used to set up transparent encryption of block devices using the kernel crypto API | `41_system` |
| `ddrescue` | 1.30 | Image a failing disk with a resumable mapfile | `41_system` |
| `f3` | 10.0 | Tests a flash drive's real capacity, detecting counterfeit drives | `41_system` |
| `gnome-disk-utility` | 46.1 | GNOME Disks — partition, format, encrypt, image and benchmark drives over udisks2 (GTK 3) | `44_apps` |
| `gparted` | 1.8.1 | GParted — create, resize, move, copy and check partitions and filesystems | `44_apps` |
| `gptfdisk` | 1.0.10 | A text-mode partitioning tool that works on GUID Partition Table (GPT) disks | `41_system` |
| `hdparm` | 9.65 | Set and read ATA power management, spindown and secure erase | `41_system` |
| `impression` | 3.8.0 | Impression — write a disk image to a USB drive or memory card | `44_apps` |
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
| `udisks2` | 2.11.2 | Disk management daemon and D-Bus API for toolkit applications | `43_toolkits` |
| `veracrypt` | 1.26.29 | VeraCrypt — create and mount encrypted volumes and containers, TrueCrypt-compatible | `44_apps` |
| `volume_key` | 0.3.12 | Library for manipulating storage volume encryption keys and storing them separately from volumes to handle forgotten passphrases | `43_toolkits` |

### filesystem

Filesystem utilities, FUSE, and network or encrypted filesystems. 20 ports, under
`ports/core/filesystem/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `btrfs-progs` | 7.1 | Btrfs filesystem utilities | `41_system` |
| `cifs-utils` | 7.7 | mount.cifs and the SMB/CIFS userspace helpers | `41_system` |
| `dosfstools` | 4.2 | Various utilities for use with the FAT family of file systems | `41_system` |
| `e2fsprogs` | 1.47.4 | utilities for handling the ext2, ext3 and ext4 file system | `41_system` |
| `erofs-utils` | 1.9.4 | Tools for the EROFS read-only filesystem KDOS packs are built as | `41_system` |
| `exfatprogs` | 1.4.3 | exFAT filesystem utilities (mkfs, fsck, label) | `41_system` |
| `f2fs-tools` | 1.16.0 | Tools to create, check and resize an F2FS filesystem | `41_system` |
| `fuse` | 3.18.3 | libfuse 3 — Filesystem in Userspace library and the setuid fusermount3 mount helper | `41_system` |
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

### crypto

Cryptographic libraries, PKCS#11, smartcards and TPM. 31 ports, under `ports/core/crypto/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `argon2` | 20190702 | Argon2 password hashing library and command-line tool | `41_system` |
| `botan` | 3.13.0 | Botan 3 — C++ cryptography and TLS library | `41_system` |
| `ccid` | 1.8.4 | USB CCID and ICCD smart card reader driver for pcsc-lite | `41_system` |
| `gnutls` | 3.8.13 | Libraries and userspace tools which provide a secure layer over a reliable transport layer | `41_system` |
| `gpgme` | 2.2.0 | C library that allows cryptography support to be added to a program | `43_toolkits` |
| `gpgmepp` | 2.2.0 | GpgME++ — the C++ binding to GnuPG Made Easy | `43_toolkits` |
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
| `openssl` | 4.0.2 | Management tools and libraries relating to cryptography | `30_foundation` |
| `openssl3` | 3.5.8 | OpenSSL 3 runtime libraries for programs built against libssl.so.3 | `30_foundation` |
| `p11-kit` | 0.26.5 | Provides a way to load and enumerate PKCS #11 (a Cryptographic Token Interface Standard) modules | `41_system` |
| `pcsc-lite` | 2.5.2 | PC/SC smart card middleware — the pcscd daemon and libpcsclite | `41_system` |
| `qca` | 2.3.12 | Qt Cryptographic Architecture for Qt 6, with the OpenSSL, GnuPG and SASL providers | `43_toolkits` |
| `qca-qt5` | 2.3.12 | Qt Cryptographic Architecture for Qt 5, with the OpenSSL, GnuPG and SASL providers | `43_toolkits` |
| `qgpgme` | 2.2.0 | QGpgME — the Qt 6 binding to GnuPG Made Easy | `43_toolkits` |
| `swtpm` | 0.10.2 | A software TPM for virtual machines — qemu's -tpmdev emulator backend | `41_system` |
| `tpm2-tools` | 5.8 | The tpm2_* commands — seal a LUKS key to the chip and unseal it at boot | `41_system` |
| `tpm2-tss` | 4.2.0 | The TPM2 software stack — what talks to the chip that can hold a disk key | `41_system` |

### security

Security applications: GnuPG and front-ends, password and secret managers, certificates, scanners,
forensics and audit. 20 ports, under `ports/core/security/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `age` | 1.3.2 | File encryption tool | `41_system` |
| `aircrack-ng` | 1.7 | 802.11 Wi-Fi auditing and diagnosis tools | `42_graphics` |
| `clamav` | 1.5.4 | Anti-virus toolkit — clamscan, the clamd daemon, sigtool and the libclamav scanning engine | `41_system` |
| `gnupg` | 2.5.24 | GNU Privacy Guard — OpenPGP encryption and signing | `43_toolkits` |
| `john` | 1.9.0.jumbo1 | John the Ripper password hash cracker | `41_system` |
| `keepassxc` | 2.7.12 | Password manager — KeePass databases, TOTP, SSH agent, browser integration and the Secret Service | `44_apps` |
| `kleopatra` | 26.08.1 | Certificate manager and graphical front end to GnuPG — OpenPGP and S/MIME keys, signing and encryption | `44_apps` |
| `libewf` | 20240506 | Expert Witness Format (E01, Ex01) forensic image library, the ewf tools and ewfmount | `41_system` |
| `libkleo` | 26.08.1 | KDE PIM library for key management and cryptography widgets over GnuPG | `43_toolkits` |
| `libsecret` | 0.21.8.2 | Secret Service client library — stores and looks up passwords over D-Bus | `41_system` |
| `lynis` | 3.1.7 | Host security auditing tool | `41_system` |
| `nmap` | 7.991 | nmap — Network exploration tool and security scanner | `41_system` |
| `pass` | 1.7.4 | The git-versioned, gpg-encrypted password store | `43_toolkits` |
| `pass-otp` | 1.2.0 | A one-time password in the password store — pass otp | `44_apps` |
| `pinentry` | 1.3.3 | Collection of simple PIN or pass-phrase entry dialogs which utilize the Assuan protocol | `43_toolkits` |
| `qtkeychain` | 0.17.0 | Qt 6 API for storing passwords in the Secret Service, KWallet or a plain file | `43_toolkits` |
| `sleuthkit` | 4.15.0 | Filesystem forensics tools that read disk images directly | `41_system` |
| `step-ca` | 0.30.2 | Private certificate authority with ACME | `41_system` |
| `step-cli` | 0.30.6 | The step command — creates a CA for step-ca, and requests, renews and inspects certificates | `41_system` |
| `yara` | 4.5.8 | Rule-based pattern matching for classifying files | `41_system` |

### containers

Container runtimes, image tools, container networking and sandbox primitives. 18 ports, under
`ports/core/containers/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `aardvark-dns` | 2.1.0 | Authoritative DNS server for container networks (Rust) | `41_system` |
| `bubblewrap` | 0.13.0 | Unprivileged sandboxing tool (bwrap), the sandbox xdg-desktop-portal decodes untrusted icons and sounds in | `41_system` |
| `buildah` | 1.45.1 | Build OCI / Docker container images | `44_apps` |
| `catatonit` | 0.2.1 | Static container init, for podman pods and --init | `41_system` |
| `conmon` | 2.2.1 | An OCI container runtime monitor (used by Podman) | `41_system` |
| `containers-common` | 1 | /etc/containers — the engine, storage, registry and image-trust configuration podman, buildah and skopeo share | `41_system` |
| `crun` | 1.29.1 | OCI container runtime written in C, with a small memory footprint | `41_system` |
| `distrobox` | 1.8.2.5 | Wrapper around podman or docker that creates mutable development containers | `43_toolkits` |
| `fuse-overlayfs` | 1.18 | An implementation of overlay+shiftfs in FUSE for rootless containers | `41_system` |
| `lazydocker` | 0.25.2 | lazydocker — terminal UI for containers, images and logs | `44_apps` |
| `libseccomp` | 2.6.1 | High-level userspace seccomp interface | `41_system` |
| `netavark` | 2.1.0 | Container network stack (network backend for Podman, Rust) | `41_system` |
| `passt` | 2026_07_28.f8df3f1 | User-mode networking for VMs and containers, unprivileged and without a tap device | `41_system` |
| `podman` | 6.1.2 | Daemonless container engine for OCI containers | `43_toolkits` |
| `podman-tui` | 2.0.0 | Terminal UI for podman | `44_apps` |
| `skopeo` | 1.24.1 | Inspect, copy, and sign container images and image registries | `44_apps` |
| `slirp4netns` | 1.3.5 | User-mode networking for unprivileged network namespaces | `41_system` |
| `xdg-dbus-proxy` | 0.1.9 | Filtering D-Bus proxy — the bus a sandboxed WebKitGTK web process is given | `41_system` |

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
| `libslirp` | 4.9.5 | libslirp — user-mode networking library (TCP/IP emulator) used by slirp4netns/qemu | `41_system` |
| `libvirt` | 12.7.0 | Virtualisation API and the libvirtd daemon, driving QEMU/KVM guests, networks and storage; virsh | `43_toolkits` |
| `libvirt-glib` | 5.0.0 | GLib, GObject and GConfig wrappers of the libvirt API, with introspection data and Vala bindings | `43_toolkits` |
| `libvncserver` | 0.9.15 | VNC server and client libraries | `41_system` |
| `nbdkit` | 1.48.1 | NBD server toolkit with pluggable plugins and filters; serves disks to libvirt and QEMU | `41_system` |
| `neatvnc` | 1.0.1 | VNC server library | `42_graphics` |
| `osinfo-db` | 20260812 | The osinfo database: every operating system, installer and virtual device libosinfo knows | `41_system` |
| `osinfo-db-tools` | 1.12.0 | Tools that import, export, validate and locate the osinfo operating-system database | `41_system` |
| `phodav` | 3.0 | WebDAV server library on libsoup, with chezdav — the folder sharing behind spice-gtk's shared directory | `41_system` |
| `qemu` | 11.1.1 | Machine emulator and virtualiser | `43_toolkits` |
| `remmina` | 1.4.43 | Remmina (GTK 3) — remote desktop client for RDP, VNC, SPICE and SSH | `44_apps` |
| `spice` | 0.16.0 | SPICE server library — what QEMU links to offer a guest's display, sound and USB to a SPICE client | `42_graphics` |
| `spice-gtk` | 0.43 | SPICE client library and GTK 3 widget — the VM console of virt-manager — with spicy and the USB redirection | `43_toolkits` |
| `spice-protocol` | 0.14.5 | SPICE protocol headers — the message definitions spice-gtk and QEMU's SPICE display share | `41_system` |
| `usbredir` | 0.15.0 | USB-over-network redirection protocol libraries and usbredirect, for SPICE and QEMU | `41_system` |
| `virt-manager` | 5.1.0 | Virtual Machine Manager — create, run and connect to libvirt/QEMU virtual machines, with virt-install, virt-clone and virt-xml | `44_apps` |
| `virtiofsd` | 1.14.0 | The virtio-fs daemon — shares a host directory with a qemu guest | `41_system` |
| `wayvnc` | 0.10.1 | VNC server for wlroots compositors, serving the session over the network | `42_graphics` |
| `wlvncc` | 0.1.0.20260429 | Wayland-native VNC client drawing through EGL/GLES2, with H.264 decoding by FFmpeg | `42_graphics` |

### net-libs

Networking and protocol libraries. 33 ports, under `ports/core/net-libs/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `asio` | 1.38.2 | Asio — the standalone, header-only C++ networking and asynchronous I/O library, without Boost | `41_system` |
| `avahi` | 0.9rc5 | mDNS/DNS-SD stack — what makes network printers discoverable | `41_system` |
| `c-ares` | 1.34.8 | C library for asynchronous DNS requests | `30_foundation` |
| `cppzmq` | 4.11.0 | Header-only C++ binding for ZeroMQ — Horizon EDA's pool updater and GNU Radio's gr-zeromq blocks | `41_system` |
| `curl` | 8.22.0 | Utility and a library used for transferring files | `30_foundation` |
| `glib-networking` | 2.90.0 | GIO modules for TLS and proxy resolution (the TLS backend every GIO and libsoup https connection needs) | `41_system` |
| `libevent` | 2.1.13 | Asynchronous event notification software library | `41_system` |
| `libidn` | 1.44 | Implementation of the Stringprep, Punycode and IDNA specifications | `41_system` |
| `libidn2` | 2.3.8 | Free software implementation of IDNA2008, Punycode and TR46 | `30_foundation` |
| `libmicrohttpd` | 1.0.10 | Embeddable HTTP server — what kiwix-serve listens with | `41_system` |
| `libmnl` | 1.0.5 | Minimal user-space library for Netlink | `41_system` |
| `libndp` | 1.9 | IPv6 Neighbor Discovery Protocol library (NetworkManager dep) | `41_system` |
| `libnice` | 0.1.24 | ICE — how two WebRTC peers find a path to each other through NAT | `42_graphics` |
| `libnl` | 3.12.0 | Netlink protocol library (kernel <-> userspace networking) | `41_system` |
| `libpcap` | 1.11.0 | A system-independent interface for user-level packet capture | `41_system` |
| `libpsl` | 0.23.3 | Library for accessing and resolving information from the Public Suffix List | `30_foundation` |
| `libsoup3` | 3.6.6 | GNOME HTTP client/server library, the 3.x API (GStreamer's souphttpsrc and adaptivedemux2, GeoClue) | `41_system` |
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
| `nng` | 1.12.4 | nng — nanomsg-next-gen, brokerless pub/sub, request/reply and pipeline messaging | `41_system` |
| `poco` | 1.15.4 | POCO C++ libraries — foundation, XML, JSON, Zip, networking and TLS classes | `41_system` |
| `rpcsvc-proto` | 1.4.4 | rpcsvc protocol .x files and headers | `41_system` |
| `uriparser` | 1.0.2 | Strict RFC 3986 URI parsing and normalising library, with the uriparse tool | `41_system` |
| `zeromq` | 4.3.5 | ZeroMQ — the message sockets Jupyter kernels talk over | `41_system` |

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
| `nftables` | 1.1.7 | Netfilter packet filtering and classification framework, and the `nft` tool the firewall is written in | `41_system` |
| `openconnect` | 9.21 | Client for AnyConnect, GlobalProtect, Fortinet and Pulse VPNs | `41_system` |
| `openresolv` | 3.17.4 | resolvconf: one writer of /etc/resolv.conf for NetworkManager, dhcpcd and wg-quick | `41_system` |
| `openvpn` | 2.7.7 | SSL/TLS VPN daemon | `41_system` |
| `ppp` | 2.5.4 | Point-to-Point Protocol daemon (PPPoE and serial modems for NetworkManager) | `41_system` |
| `vpnc-script` | 20240208 | Routing and DNS script openconnect runs when a tunnel connects | `41_system` |
| `wireguard-tools` | 1.0.20260223 | wireguard-tools — userspace tooling (wg, wg-quick) for WireGuard VPN | `41_system` |
| `wpa_supplicant` | 2.12 | IEEE 802.1X / WPA/WPA2/WPA3 supplicant (Wi-Fi authentication backend for NetworkManager) | `41_system` |

### net-diag

Network diagnostics, capture, measurement and low-level socket tools. 15 ports, under
`ports/core/net-diag/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `bandwhich` | 0.23.1 | Shows bandwidth use by process and connection | `41_system` |
| `doggo` | 1.4.0 | Command-line DNS client | `41_system` |
| `iftop` | 1.0pre4 | iftop — display bandwidth usage on a network interface | `41_system` |
| `iperf3` | 3.21 | iperf3 — network bandwidth measurement tool | `41_system` |
| `mqttui` | 0.24.0 | Subscribe to an MQTT broker and watch the topic tree, in the terminal | `41_system` |
| `mtr` | 0.96 | mtr — combined ping and traceroute | `41_system` |
| `net-snmp` | 5.9.5.2 | SNMP library, tools and agent — read network UPS cards, switches and printers (snmpget, snmpwalk, snmpd) | `41_system` |
| `netcat` | 1.238 | openbsd-netcat — reads and writes data over TCP and UDP connections | `41_system` |
| `sniffnet` | 1.5.1 | Watch network traffic by host, service, program and country, in a GPU-drawn window | `42_graphics` |
| `socat` | 1.8.1.3 | socat — multipurpose relay (TCP/UNIX/SSL/etc.) | `41_system` |
| `tcpdump` | 4.99.7 | tcpdump — command-line packet analyzer | `41_system` |
| `termshark` | 2.4.0 | Terminal UI for tshark | `44_apps` |
| `trippy` | 0.13.0 | Network diagnostic combining traceroute and ping, with per-hop loss and jitter | `41_system` |
| `whois` | 5.6.6 | WHOIS client for domains, IP blocks and AS numbers | `41_system` |
| `wireshark` | 4.7.3 | Wireshark — the network analyser's Qt window, tshark and dumpcap | `43_toolkits` |

### servers

Daemons that serve other machines: remote shell, web, mail, XMPP, IRC, CalDAV/CardDAV, SMB and DLNA.
10 ports, under `ports/core/servers/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `caddy` | 2.11.4 | Web server with built-in TLS certificate management | `41_system` |
| `maddy` | 0.9.5 | A mail server in one program — SMTP, submission and IMAP for a LAN | `41_system` |
| `minidlna` | 1.3.3 | ReadyMedia — a DLNA media server for the televisions and players on a LAN | `42_graphics` |
| `mosh` | 1.4.0 | mosh — mobile shell, a replacement for SSH over unreliable networks | `41_system` |
| `mosquitto` | 2.1.2 | MQTT broker and the mosquitto_pub and mosquitto_sub clients | `41_system` |
| `ngircd` | 28 | An IRC server small enough for one room — chat on a LAN with no internet | `41_system` |
| `openssh` | 10.5p1 | OpenSSH client and server | `41_system` |
| `prosody` | 13.0.6 | XMPP server | `41_system` |
| `radicale` | 3.8.1 | A CalDAV and CardDAV server — shared calendars and contacts on a LAN with no internet | `41_system` |
| `samba` | 4.24.7 | SMB/CIFS file and print server and client tools | `41_system` |

### transfer

File transfer, synchronisation, backup and BitTorrent. 14 ports, under `ports/core/transfer/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `croc` | 11.5.3 | One-off file transfer by code phrase, kept on this network | `41_system` |
| `deja-dup` | 50.2 | Déjà Dup — scheduled, encrypted backups to a disk or folder over restic | `44_apps` |
| `deluge` | 2.2.0 | Deluge — BitTorrent client with a daemon, a GTK 3 interface, a web UI and a console UI | `44_apps` |
| `filezilla` | 3.71.0 | FileZilla (wxWidgets, GTK 3) — FTP, FTPS and SFTP client with a two-pane site manager | `44_apps` |
| `fzssh` | 1.4.0 | FileZilla's SSH and SFTP client library | `41_system` |
| `libfilezilla` | 0.57.0 | FileZilla's C++ platform library: events, sockets, TLS over GnuTLS, hashing and time | `41_system` |
| `libtorrent-rasterbar` | 2.1.2 | Arvid Norberg's BitTorrent library, with its Python binding — the engine of qBittorrent and Deluge | `41_system` |
| `qbittorrent` | 5.2.3 | qBittorrent (Qt 6) — BitTorrent client with a web UI, plus qbittorrent-nox for a machine with no display | `44_apps` |
| `rclone` | 1.75.1 | Syncs files between local disks and remote storage, and verifies copies with rclone check | `41_system` |
| `restic` | 0.19.1 | Deduplicating backup program, a single static binary | `41_system` |
| `rsync` | 3.5.1 | Utilities for synchronizing large file archives over a network | `41_system` |
| `syncthing` | 2.1.5 | Continuous peer-to-peer file synchronisation | `41_system` |
| `syncthingtray` | 2.1.7 | Syncthing Tray (Qt 6) — status, folders, devices and control of a local Syncthing from the panel tray | `44_apps` |
| `transmission` | 4.1.3 | Transmission — BitTorrent daemon with a web UI, command-line tools and a Qt 6 client | `44_apps` |

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
| `newsboat` | 2.44 | newsboat — an RSS and Atom feed reader for the terminal | `41_system` |
| `notmuch` | 0.40 | Mail indexer with tag-based search | `43_toolkits` |
| `pizauth` | 1.1.0 | Obtains and refreshes OAuth2 tokens for mail clients | `41_system` |
| `sfeed` | 2.4 | RSS and Atom feeds as a tab-separated file, and the programs that read it | `44_apps` |
| `thunderbird` | 153.3.1 | Mail, calendar, contacts and feeds, offline-first, on GTK 3 for Wayland and X11 | `44_apps` |

### chat

Instant messaging, IRC, XMPP, Matrix, fediverse and voice clients, with their protocol and E2E
libraries. 16 ports, under `ports/core/chat/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `baresip` | 4.11.0 | SIP softphone for voice calls | `44_apps` |
| `dino` | 0.5.1 | Dino (GTK 4, libadwaita) — XMPP chat with OMEMO and OpenPGP encryption, file transfer, and voice and video calls | `44_apps` |
| `iamb` | 0.0.12 | Terminal Matrix client with vim key bindings | `41_system` |
| `ii` | 2.0 | An IRC client that is a directory of FIFOs and text files | `41_system` |
| `konversation` | 26.08.1 | Konversation — IRC client (Qt 6, KDE Frameworks) with TLS, DCC transfers and Blowfish-encrypted channels | `44_apps` |
| `libomemo-c` | 0.5.1 | The Signal double ratchet with OMEMO's wire format — end-to-end encryption for XMPP clients | `41_system` |
| `libre` | 4.11.0 | The SIP, RTP and STUN protocol stack baresip is built on | `41_system` |
| `libstrophe` | 0.14.0 | The XMPP client library profanity speaks the protocol with | `41_system` |
| `mtxclient` | 0.10.1 | Client library for the Matrix protocol, with olm end-to-end encryption | `41_system` |
| `mumble` | 1.5.915 | Low-latency voice chat — the Mumble client and mumble-server, for group voice on a LAN | `44_apps` |
| `nheko` | 0.12.1 | Matrix chat client (Qt 6) — rooms, end-to-end encryption, voice and video calls | `44_apps` |
| `olm` | 3.2.16 | The olm and megolm end-to-end encryption ratchets of Matrix | `41_system` |
| `profanity` | 0.18.2 | Console XMPP client | `44_apps` |
| `quassel` | 0.14.0 | Quassel IRC (Qt 5) — the monolithic client, the core that stays connected, and the client for it | `44_apps` |
| `toot` | 0.52.1 | toot — the Fediverse from a terminal, as a TUI and as a command | `41_system` |
| `weechat` | 4.10.1 | Extensible IRC client for the terminal, with a relay and scripting | `41_system` |

### browsers

Web, Gemini and text-mode browsers. 10 ports, under `ports/core/browsers/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `chromium` | 154.0.8037.57 | Chromium web browser, on GTK 3 with Wayland and X11 | `44_apps` |
| `falkon` | 26.08.1 | Qt WebEngine web browser from KDE, with KWallet and KIO integration | `44_apps` |
| `firefox-esr` | 153.3.0 | Firefox Extended Support Release web browser, on GTK 3 with Wayland and X11 | `44_apps` |
| `inlyne` | 0.5.3 | GPU-drawn Markdown and HTML viewer with live reload and no browser engine | `44_apps` |
| `lagrange` | 1.21.1 | Gemini, Gopher, Spartan and Finger browser, with a terminal build (clagrange) beside the SDL one | `44_apps` |
| `librewolf` | 156.0.1.1 | LibreWolf, the privacy-hardened Firefox fork, on GTK 3 with Wayland and X11 | `44_apps` |
| `lynx` | 2.9.3 | Text-based web browser | `41_system` |
| `netsurf` | 3.11 | NetSurf — a web browser with its own layout engine, on GTK 3 | `44_apps` |
| `qutebrowser` | 3.7.0 | Keyboard-driven web browser with vim-like bindings, on Qt WebEngine | `44_apps` |
| `w3m` | 0.5.6 | Text-mode web browser and pager that renders tables | `42_graphics` |

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

### x11

X11 protocol, client libraries and data, plus Xwayland, the one X server. 42 ports, under
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
| `xwayland` | 24.1.13 | Rootless X server that runs X11 clients on a Wayland compositor | `42_graphics` |

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
| `wayland-utils` | 1.3.0 | wayland-info — list the globals, outputs, seats and formats a compositor offers | `42_graphics` |
| `waylandpp` | 1.0.1 | Wayland C++ bindings and the wayland-scanner++ protocol generator | `42_graphics` |
| `waypipe` | 0.11.2 | Network proxy for Wayland clients — run a program on another machine and show it here, over SSH | `42_graphics` |
| `wev` | 1.1.0 | Wayland event viewer — print every input event a window receives | `42_graphics` |
| `wl-clipboard` | 2.3.0 | Wayland clipboard utilities (wl-copy, wl-paste) | `42_graphics` |
| `wl-kbptr` | 0.4.1 | Move and click the pointer from the keyboard — labelled screen regions over a layer-shell overlay | `44_apps` |
| `wl-mirror` | 0.18.5 | Mirror a Wayland output into a window, for a projector or a second screen | `42_graphics` |
| `wlroots` | 0.20.2 | Modular Wayland compositor library — the base kdos-comp is built on | `50_desktop` |
| `wtype` | 0.4 | xdotool type for Wayland — types text and presses keys through the virtual-keyboard protocol | `42_graphics` |

### gpu

The GPU stack: Mesa, DRM, GL and Vulkan loaders and headers, shader compilers, VA-API, OpenCL. 25
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
| `opencl-headers` | 2026.05.29 | Khronos OpenCL C API headers | `41_system` |
| `shaderc` | 2026.4 | glslc and libshaderc — GLSL and HLSL to SPIR-V, on the system glslang and SPIRV-Tools | `41_system` |
| `spirv-headers` | 1.202609.0 | SPIR-V Headers | `41_system` |
| `spirv-tools` | 2026.4.rc2 | API and commands for processing SPIR-V modules | `41_system` |
| `vulkan-headers` | 1.4.363 | Vulkan header files | `41_system` |
| `vulkan-loader` | 1.4.363 | Khronos Vulkan ICD loader (libvulkan.so.1) — dispatches to Mesa venus/lavapipe ICDs | `42_graphics` |
| `vulkan-tools` | 1.4.363 | Khronos Vulkan tools — vkcube, vkcubepp and vulkaninfo, Wayland and KMS display WSI | `42_graphics` |

### graphics-libs

2D rendering, vector and SVG, text layout, colour management and graph drawing libraries. 25 ports,
under `ports/core/graphics-libs/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `babl` | 0.1.128 | Dynamic any-to-any pixel format conversion library, used by GEGL and GIMP | `41_system` |
| `blend2d` | 0.21.2 | 2D vector graphics engine with a JIT-compiled pipeline | `41_system` |
| `cairo` | 1.18.6 | 2D graphics library, with its Xlib and XCB surfaces | `42_graphics` |
| `cairomm` | 1.18.1 | C++ bindings for cairo, the 1.16 API (cairomm-1.16) that gtkmm 4 builds on | `42_graphics` |
| `cairomm1.14` | 1.14.6 | C++ bindings for cairo, the 1.0 API (cairomm-1.0) that gtkmm 3 builds on | `42_graphics` |
| `gdk-pixbuf` | 2.44.8 | gdk-pixbuf — image-loading library used by GTK and many notification daemons | `41_system` |
| `gegl` | 0.4.72 | Graph-based image processing framework, the engine under GIMP | `43_toolkits` |
| `goocanvas` | 3.0.0 | GooCanvas — a cairo canvas widget for GTK 3, with introspection data | `44_apps` |
| `graphene` | 1.10.8 | Thin layer of vector and matrix types for 2D and 3D graphics, used by GTK 4 | `41_system` |
| `graphviz` | 16.1.0 | Graph layout and rendering — dot, neato, sfdp and the cgraph/gvc libraries | `43_toolkits` |
| `harfbuzz` | 14.5.0 | OpenType text shaping engine | `42_graphics` |
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

### image-libs

Raster image formats, codecs, metadata, processing, barcodes, and terminal image output. 43 ports,
under `ports/core/image-libs/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `aalib` | 1.4rc5 | ASCII art graphics library — renders true-color images to text | `41_system` |
| `chafa` | 1.18.3 | chafa — images as terminal graphics: sixel, kitty, iTerm2 or Unicode | `42_graphics` |
| `exempi` | 2.6.6 | XMP metadata library and the exempi tool, from Adobe's XMP SDK | `41_system` |
| `exiftool` | 13.59 | Reads and writes metadata in image, audio and video files | `41_system` |
| `exiv2` | 0.28.9 | Read and write image metadata — EXIF, IPTC and XMP | `41_system` |
| `farbfeld` | 4 | Lossless image format designed for piping, and its converters | `41_system` |
| `gexiv2` | 0.14.7 | GObject wrapper around exiv2 image metadata, the 0.14 API GIMP links | `42_graphics` |
| `giflib` | 6.1.3 | Libraries for reading and writing GIFs as well as programs for converting and working with GIF files | `41_system` |
| `graphicsmagick` | 1.3.48 | GraphicsMagick — image processing and Magick++, behind Octave's imread and imwrite | `41_system` |
| `imagemagick` | 7.1.2.31 | ImageMagick — convert, edit, and compose raster images (provides `convert`) | `43_toolkits` |
| `imath` | 3.2.3 | C++ and python library of 2D and 3D vector, matrix, and math operations for computer graphics | `41_system` |
| `jbigkit` | 2.1 | JBIG1 (ITU-T T.82 and T.85) bi-level image compression library and tools | `41_system` |
| `lensfun` | 0.3.4 | Lens correction library and its camera and lens database | `41_system` |
| `leptonica` | 1.87.0 | The image processing tesseract does its page analysis with | `41_system` |
| `libavif` | 1.4.2 | AVIF reader and writer — the AV1 still-image format, with avifenc and avifdec | `41_system` |
| `libdmtx` | 0.7.8 | Data Matrix two-dimensional barcode reading and writing library | `41_system` |
| `libexif` | 0.6.26 | Library for parsing, editing, and saving EXIF data | `41_system` |
| `libgd` | 2.3.3 | GD graphics library — draws lines, shapes, text and images into PNG, JPEG, GIF, WebP, TIFF, HEIF and BMP | `42_graphics` |
| `libheif` | 1.23.5 | HEIF and AVIF image reader and writer | `41_system` |
| `libimagequant` | 4.4.1 | Palette quantisation that turns a 32-bit image into a small 8-bit PNG or GIF | `41_system` |
| `libiptcdata` | 1.0.5 | IPTC photo metadata library and the iptc tool | `41_system` |
| `libjpeg-turbo` | 3.2.0 | A fork of the original IJG libjpeg which uses SIMD to accelerate baseline JPEG compression and decompression | `41_system` |
| `libjxl` | 0.12.0 | JPEG XL reference implementation — libjxl, cjxl, djxl and the gdk-pixbuf loader | `41_system` |
| `libkdcraw` | 26.08.1 | Qt 6 wrapper around LibRaw for decoding camera raw files | `43_toolkits` |
| `libkexiv2` | 26.08.1 | Qt 6 wrapper around exiv2 for reading and writing picture metadata | `43_toolkits` |
| `libmypaint` | 1.6.1 | MyPaint brush engine library, used by GIMP and Krita | `41_system` |
| `libpng` | 1.6.58 | A collection of routines used to create PNG format graphics files | `41_system` |
| `libraw` | 0.22.2 | Camera raw file decoding library | `41_system` |
| `libsixel` | 1.10.5 | Sixel encoder and decoder library, for pictures in a terminal | `42_graphics` |
| `libtiff` | 4.7.2 | TIFF libraries and associated utilities | `41_system` |
| `libvips` | 8.18.6 | Streaming image-processing library with low memory use | `44_apps` |
| `libwebp` | 1.6.0 | Library and support programs to encode and decode images in WebP format | `41_system` |
| `libyuv` | 0.0.1971 | YUV conversion, scaling and rotation — libavif converts colour through it | `41_system` |
| `mypaint-brushes` | 2.0.2 | MyPaint brush collection, version 2 data set | `41_system` |
| `opencolorio` | 2.5.2 | OpenColorIO colour management for visual effects and animation | `42_graphics` |
| `openexr` | 3.5.0 | High dynamic range image format library | `41_system` |
| `openimageio` | 3.1.17.0 | OpenImageIO: image reading, writing and processing for film pipelines — Blender's image I/O, with oiiotool | `42_graphics` |
| `openjpeg` | 2.5.4 | JPEG 2000 codec — what scanners, archives and PDFs put images in | `41_system` |
| `qrencode` | 4.1.1 | Write a QR code, including as terminal characters | `41_system` |
| `zbar` | 0.23.93 | Read a QR or barcode from an image or a camera | `43_toolkits` |
| `zimg` | 3.0.6 | zimg image scaling, colour space and depth conversion library | `41_system` |
| `zint` | 2.16.0 | Zint — barcode encoder for over 50 symbologies, with a CLI and the Zint Barcode Studio GUI | `44_apps` |
| `zxing-cpp` | 2.3.0 | Barcode and QR code reading and writing — what `mutool barcode` is built on | `41_system` |

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
| `timg` | 1.6.3 | timg — a terminal image and video viewer | `44_apps` |

### 3d

3D modelling, animation, voxel and mesh tools and libraries, and scientific visualisation. 14 ports,
under `ports/core/3d/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `admesh` | 0.98.5 | STL mesh checking and repair tool | `41_system` |
| `alembic` | 1.8.12 | Alembic: baked geometry and animation interchange (Ogawa .abc) — Blender's import and export | `41_system` |
| `assimp` | 6.0.5 | Open Asset Import Library — reads and writes some 40 3D model formats (glTF, OBJ, FBX, Collada, STL) into one scene graph | `41_system` |
| `blender` | 5.2.2 | Blender: 3D modelling, sculpting, animation, rendering (Cycles, EEVEE), compositing and video editing | `44_apps` |
| `draco` | 1.5.7 | Draco — Google's compressed geometry and point-cloud format, library and codec tools | `41_system` |
| `gl2ps` | 1.4.2 | gl2ps — OpenGL scenes to PostScript, PDF and SVG, for Octave's figure printing | `42_graphics` |
| `goxel` | 0.15.1 | 3D voxel editor, drawn with OpenGL through GLFW and no toolkit | `44_apps` |
| `lib3mf` | 2.5.0 | lib3mf — reference implementation of the 3D Manufacturing Format (3MF) for reading and writing 3D print files | `41_system` |
| `libspnav` | 1.2 | libspnav — client library for 6-DoF 3D mice (3Dconnexion SpaceNavigator and friends) through spacenavd | `42_graphics` |
| `manifold` | 3.5.4 | Guaranteed-manifold mesh boolean library, the geometry kernel OpenSCAD renders with | `42_graphics` |
| `opencsg` | 1.8.2 | Image-based constructive solid geometry rendering with OpenGL | `42_graphics` |
| `opensubdiv` | 3.7.0 | Pixar's OpenSubdiv: subdivision surface evaluation on the CPU and in GLSL — Blender's Subdivision modifier | `42_graphics` |
| `openvdb` | 13.0.0 | OpenVDB and NanoVDB: sparse volumes — Blender's smoke, fire and volume objects | `42_graphics` |
| `vtk` | 9.5.2 | The Visualization Toolkit: meshes, filters and OpenGL rendering, with Python bindings | `42_graphics` |

### cad

Mechanical, architectural and 2D CAD, geometry kernels, meshing and FEM. 17 ports, under
`ports/core/cad/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `calculix` | 2.23 | CalculiX CrunchiX (ccx) — three-dimensional structural and thermal finite element solver with an Abaqus-style input deck | `41_system` |
| `cgal` | 6.2.1 | Computational Geometry Algorithms Library (header-only C++) | `41_system` |
| `clipper2` | 2.0.1 | Polygon clipping and offsetting library | `41_system` |
| `coin` | 4.0.10 | Open Inventor 3D scene graph library, the viewer under FreeCAD | `42_graphics` |
| `freecad` | 1.1.3 | Parametric 3D CAD modeller: Part Design, Sketcher, TechDraw, FEM, CAM, BIM and Assembly | `44_apps` |
| `gmsh` | 4.15.2 | Gmsh — 3D finite element mesh generator with CAD, meshing and post-processing (FLTK) | `44_apps` |
| `ifcopenshell` | 0.8.5 | IfcOpenShell — IFC (BIM) toolkit: the C++ parser and OpenCASCADE geometry, IfcConvert and the ifcopenshell Python module FreeCAD's BIM workbench imports | `42_graphics` |
| `librecad` | 2.2.1.5 | 2D CAD drafting with DXF and DWG import (Qt 5) | `44_apps` |
| `netgen` | 6.2.2604 | Netgen — automatic 3D tetrahedral mesh generator (nglib, OpenCASCADE geometry and the Python module FreeCAD's FEM drives) | `42_graphics` |
| `opencascade` | 7.9.3 | Open CASCADE Technology, the B-rep CAD kernel with STEP, IGES and glTF exchange | `42_graphics` |
| `openscad` | 2026.09.27 | The programmer's solid 3D CAD modeller: models written as scripts, rendered with Manifold and CGAL | `44_apps` |
| `pivy` | 0.6.11 | Python bindings for Coin and SoQt, the scene-graph API FreeCAD's workbenches script | `43_toolkits` |
| `polyclipping` | 6.4.2 | Clipper 1 — polygon clipping and offsetting library, the API libnest2d and CuraEngine link | `41_system` |
| `qcad` | 3.33.1.0 | 2D CAD with DXF, script add-ons and a large part library (community edition, Qt 6) | `44_apps` |
| `qhull` | 8.0.2 | Convex hulls, Delaunay triangulation and Voronoi diagrams | `41_system` |
| `solvespace` | 3.2 | Parametric 2D and 3D CAD with a constraint solver (Qt 6 interface) | `44_apps` |
| `soqt` | 1.6.4 | Qt 6 bindings for the Coin 3D scene graph (viewers and render areas) | `43_toolkits` |

### fabrication

3D-printing slicers and hosts, and CNC control. 13 ports, under `ports/core/fabrication/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `bcnc` | 0.9.16 | bCNC — GRBL CNC controller and G-code sender with an editor, probing, auto-levelling and CAM tools (Tk) | `44_apps` |
| `candle2` | 2.4 | Candle2 — GRBL CNC and laser controller with a G-code visualiser, height maps and jogging (Qt 5) | `44_apps` |
| `cncjs` | 1.11.5 | CNCjs — web-based controller for Grbl, Marlin, Smoothieware and TinyG CNC machines, served to the browser | `41_system` |
| `cura` | 5.13.0 | UltiMaker Cura — prepares 3D models for FDM printing, with printer definitions and material profiles | `44_apps` |
| `curaengine` | 5.13.0 | CuraEngine — the slicer behind Cura, also usable on its own from the command line | `42_graphics` |
| `libarcus` | 5.11.1 | Arcus — the protobuf message socket between Cura and CuraEngine | `41_system` |
| `libnest2d` | 5.11.0.alpha0 | libnest2d — header-only 2D bin packing, which arranges parts on Cura's build plate | `41_system` |
| `libsavitar` | 5.12.0 | Savitar — the 3MF scene reader and writer under Cura | `41_system` |
| `linuxcnc` | 2.9.10 | LinuxCNC — CNC machine controller for mills, lathes, routers and plasma: HAL, G-code interpreter and the AXIS GUI | `44_apps` |
| `orcaslicer` | 2.4.2 | OrcaSlicer — G-code slicer for FDM printers with calibration tools and a wide printer-profile library | `44_apps` |
| `printrun` | 2.2.0 | pronsole and printcore — send a sliced job to a printer over USB serial | `41_system` |
| `prusaslicer` | 2.9.6 | PrusaSlicer — turns 3D models into G-code for FDM and resin printers, with the bundled vendor profiles | `44_apps` |
| `uranium` | 5.13.0 | Uranium — the Python and Qt Quick application framework Cura is built on | `43_toolkits` |

### fonts

Font packages, and font libraries and tooling. 24 ports, under `ports/core/fonts/`.

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

### themes

Icon, cursor, sound and widget themes, and theme-configuration tools. 7 ports, under
`ports/core/themes/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `adwaita-icon-theme` | 51.0 | GNOME's Adwaita icon and cursor theme — the symbolic-icon fallback GTK applications expect | `44_apps` |
| `breeze` | 6.7.5 | Breeze widget style and colour schemes for Qt 6 applications | `43_toolkits` |
| `hicolor-icon-theme` | 0.18 | Freedesktop.org Hicolor icon theme | `41_system` |
| `icon-naming-utils` | 0.8.90 | Perl script used for maintaining backwards compatibility with current desktop icon themes | not installed |
| `qt5ct` | 1.9 | Qt 5 appearance settings — the palette, style, fonts and icon theme of every Qt 5 program, as a platform theme plugin | `44_apps` |
| `qt6ct` | 0.11 | Qt 6 appearance settings — the palette, style, fonts and icon theme of every Qt 6 program, as a platform theme plugin | `44_apps` |
| `sound-theme-freedesktop` | 0.8 | The freedesktop default sound theme — event sounds libcanberra plays | `41_system` |

### xdg

freedesktop desktop integration: portals, MIME, desktop entries, AppStream, tray and indicators,
notifications, event sounds, script dialogs. 15 ports, under `ports/core/xdg/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `appstream` | 1.2.0 | AppStream metadata library and appstreamcli — reads and validates software component data | `42_graphics` |
| `ayatana-ido` | 0.10.4 | Ayatana Indicator Display Objects — the GTK 3 menu widgets indicator menus are built from | `43_toolkits` |
| `desktop-file-utils` | 0.28 | Command line utilities for working with Desktop entries | `41_system` |
| `libayatana-appindicator` | 0.6.0 | Ayatana application indicators — a GTK 3 program's tray icon and menu as a StatusNotifierItem | `43_toolkits` |
| `libayatana-indicator` | 0.9.5 | Ayatana indicator library — the shared object behind application and system indicators | `43_toolkits` |
| `libcanberra` | 0.30 | XDG sound theme event sounds over ALSA, PulseAudio and GStreamer, with the GTK 3 module | `43_toolkits` |
| `libdbusmenu` | 16.04.0 | Menus passed over D-Bus — libdbusmenu-glib and the GTK 3 renderer libdbusmenu-gtk3 | `43_toolkits` |
| `libnotify` | 0.8.8 | Desktop notification client library and notify-send | `41_system` |
| `libportal` | 0.11.0 | Client library for the XDG desktop portals, with GTK 3 and GTK 4 parent windows | `43_toolkits` |
| `shared-mime-info` | 2.5.1 | A MIME database | `41_system` |
| `xdg-desktop-portal` | 1.22.1 | Desktop portal D-Bus framework (Wayland screencast / file-picker integration) | `42_graphics` |
| `xdg-desktop-portal-gtk` | 1.15.3 | GTK backend for xdg-desktop-portal — the Print and Email portals | `43_toolkits` |
| `xdg-desktop-portal-wlr` | 0.8.4 | wlroots backend for xdg-desktop-portal (ScreenCast and Screenshot) | `50_desktop` |
| `xdg-utils` | 1.2.1 | Command line tools that assist applications with a variety of desktop integration tasks | `41_system` |
| `zenity` | 4.2.2 | Zenity — GTK dialog boxes from the command line, and the file chooser other programs call | `43_toolkits` |

### accessibility

Screen readers, speech, braille and alternative input. 9 ports, under `ports/core/accessibility/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `at-spi2-core` | 2.62.0.1 | Assistive Technology Service Provider Interface — the accessibility bus, ATK and the ATK bridge | `42_graphics` |
| `brltty` | 6.9.1 | Braille display and console speech — the screen reader | `41_system` |
| `dasher` | 5.0.0.beta.20200420 | Dasher — text entry by steering a pointer, eye tracker or switch through a predictive language model | `44_apps` |
| `espeak-ng` | 1.52.0 | Speech synthesis for over 100 languages, in about 15 MB | `41_system` |
| `liblouis` | 3.39.0 | Braille translation — contracted and uncontracted tables for well over a hundred languages | `41_system` |
| `orca` | 51.0 | Orca — the screen reader: speech and braille over the AT-SPI accessibility tree | `44_apps` |
| `pcaudiolib` | 1.3 | The audio output layer espeak-ng speaks through | `41_system` |
| `piper` | 1.8.0 | Piper: neural text to speech on the CPU, with the voice model on the disk and nothing sent anywhere | `42_graphics` |
| `speech-dispatcher` | 0.12.1 | Speech synthesis server for screen readers, with espeak-ng as its voice | `41_system` |

### i18n

Unicode, locale, text shaping and segmentation, conversion, and translation. 14 ports, under
`ports/core/i18n/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `fribidi` | 1.0.17 | Implementation of the Unicode Bidirectional Algorithm (BIDI) | `41_system` |
| `gettext` | 1.0 | Utilities for internationalization and localization | `30_foundation`, named again by `40_lang` |
| `icu` | 78.3 | International Components for Unicode library | `30_foundation` |
| `iso-codes` | 4.20.1 | List of country, language and currency names | `41_system` |
| `libdatrie` | 0.2.14 | Double-array trie — the dictionary structure libthai looks words up in | `41_system` |
| `libgrapheme` | 3.0.0 | Unicode string segmentation — grapheme clusters, words, sentences and line breaks | `41_system` |
| `libthai` | 0.1.30 | Thai word segmentation — where pango may wrap a line of Thai | `41_system` |
| `libunibreak` | 8.0 | Unicode line, word and grapheme breaking — where libass may wrap a subtitle | `41_system` |
| `libunistring` | 1.4.2 | Library for manipulating Unicode strings and C strings | `30_foundation` |
| `opencc` | 1.4.2 | Conversion between Traditional and Simplified Chinese | `41_system` |
| `snowball` | 3.1.1 | Snowball stemming compiler and libstemmer, the stemmers for 30 languages | `41_system` |
| `uchardet` | 0.0.8 | Guesses the character encoding of a text file — how mpv reads a subtitle that is not UTF-8 | `41_system` |
| `utf8proc` | 2.11.3 | Unicode normalization, casefolding, etc. (fcft dep) | `41_system` |
| `utfcpp` | 4.2.1 | Header-only C++ library for checking and converting UTF-8, UTF-16 and UTF-32 text | `41_system` |

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
| `libpeas` | 1.36.0 | GObject plugin engine, 1.x API — Rhythmbox's plugin loader | `43_toolkits` |
| `libsigc++2` | 2.12.1 | Typesafe callback framework for C++, the 2.x API (sigc++-2.0) that gtkmm 3 builds on | `41_system` |
| `libsigc++3` | 3.6.0 | Typesafe callback framework for C++, the 3.x API (sigc++-3.0) that gtkmm 4 builds on | `41_system` |
| `libxfce4ui` | 4.20.2 | Xfce widget library, with its X11 and Wayland paths | `43_toolkits` |
| `libxfce4util` | 4.20.1 | Xfce utility library | `41_system` |
| `vte3` | 0.84.1 | Terminal emulator widget for GTK 3 and GTK 4 | `43_toolkits` |
| `xfconf` | 4.20.0 | Xfce configuration store — libxfconf and the xfconfd daemon | `41_system` |

### qt6

The Qt 6 modules, and everything in `group = qt6`. 29 ports, under `ports/core/qt6/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `python3-pyside6` | 6.11.2 | PySide6 and Shiboken6, the official Qt 6 bindings for Python (FreeCAD's scripting GUI) | `43_toolkits` |
| `qt6-qt3d` | 6.11.2 | Qt 6 3D — scene graph, render, input, logic, animation and extras aspects, C++ and QML | `43_toolkits` |
| `qt6-qt5compat` | 6.11.2 | Qt 6 Core5Compat — QTextCodec, QRegExp and the Qt 5 graphical effects for code ported from Qt 5 | `43_toolkits` |
| `qt6-qtbase` | 6.11.2 | Qt 6 core, GUI, widgets, network, SQL, D-Bus and printing, with the Wayland and X11 platform plugins | `43_toolkits` |
| `qt6-qtcharts` | 6.11.2 | Qt 6 Charts — chart widgets and QML chart types | `43_toolkits` |
| `qt6-qtconnectivity` | 6.11.2 | Qt 6 Bluetooth and NFC — QtBluetooth over BlueZ and QtNfc over PC/SC | `43_toolkits` |
| `qt6-qtdeclarative` | 6.11.2 | Qt 6 QML and Qt Quick — the language, the engine, Qt Quick Controls and the qml tools | `43_toolkits` |
| `qt6-qtgraphs` | 6.11.2 | Qt 6 Graphs — 2D and 3D data visualisation for QML and widgets | `43_toolkits` |
| `qt6-qtimageformats` | 6.11.2 | Qt 6 image format plugins — TIFF, WebP, ICNS, TGA and WBMP | `43_toolkits` |
| `qt6-qtlanguageserver` | 6.11.2 | Qt 6 Language Server Protocol and JSON-RPC library, used by qmlls | `43_toolkits` |
| `qt6-qtlocation` | 6.11.2 | Qt 6 Location — maps, places and routing for QML | `43_toolkits` |
| `qt6-qtmqtt` | 6.11.2 | Qt 6 MQTT — MQTT 3.1, 3.1.1 and 5 client, for LabPlot's live data sources | `43_toolkits` |
| `qt6-qtmultimedia` | 6.11.2 | Qt 6 Multimedia — audio, video playback and capture through FFmpeg and PipeWire | `43_toolkits` |
| `qt6-qtnetworkauth` | 6.11.2 | Qt 6 Network Authorization — OAuth 1 and OAuth 2 clients | `43_toolkits` |
| `qt6-qtpositioning` | 6.11.2 | Qt 6 Positioning — position sources from GeoClue and NMEA | `43_toolkits` |
| `qt6-qtquick3d` | 6.11.2 | Qt 6 Quick 3D — 3D scenes in QML | `43_toolkits` |
| `qt6-qtremoteobjects` | 6.11.2 | Qt 6 Remote Objects — QObjects shared between processes | `43_toolkits` |
| `qt6-qtscxml` | 6.11.2 | Qt 6 SCXML and StateMachine — state charts compiled or interpreted | `43_toolkits` |
| `qt6-qtsensors` | 6.11.2 | Qt 6 Sensors — accelerometer, gyroscope and light sensors through iio-sensor-proxy (no sensor daemon is ported) | `43_toolkits` |
| `qt6-qtserialport` | 6.11.2 | Qt 6 SerialPort — serial and USB-serial device access | `43_toolkits` |
| `qt6-qtshadertools` | 6.11.2 | Qt 6 shader tools — qsb, the GLSL/HLSL/MSL/SPIR-V baker Qt Quick and Qt RHI shaders are compiled with | `43_toolkits` |
| `qt6-qtspeech` | 6.11.2 | Qt 6 TextToSpeech — speech synthesis through speech-dispatcher | `43_toolkits` |
| `qt6-qtsvg` | 6.11.2 | Qt 6 SVG rendering — QSvgRenderer, QSvgWidget and the SVG image and icon engine plugins | `43_toolkits` |
| `qt6-qttools` | 6.11.2 | Qt 6 tools — Designer, Linguist, Assistant, Qt Help, UiTools, lrelease, qdbus and qtdiag | `43_toolkits` |
| `qt6-qttranslations` | 6.11.2 | Qt 6 translation catalogues for the Qt libraries and tools | `43_toolkits` |
| `qt6-qtwayland` | 6.11.2 | Qt 6 Wayland compositor library and the client decoration and shell plugins | `43_toolkits` |
| `qt6-qtwebchannel` | 6.11.2 | Qt 6 WebChannel — QObjects shared with HTML and JavaScript clients | `43_toolkits` |
| `qt6-qtwebengine` | 6.11.2 | Qt 6 WebEngine and Qt PDF — the Chromium web engine as Qt widgets and Qt Quick types | `43_toolkits` |
| `qt6-qtwebsockets` | 6.11.2 | Qt 6 WebSockets — RFC 6455 client and server | `43_toolkits` |

### qt5

The Qt 5 modules, `group = qt5`. 11 ports, under `ports/core/qt5/`.

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

### qt-extra

Third-party and KDE-hosted Qt add-on libraries that are not in KF6. 18 ports, under
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
| `mpvqt` | 1.2.0 | Qt Quick item that renders libmpv, used by Haruna | `43_toolkits` |
| `phonon` | 4.12.0 | KDE multimedia API for Qt 6; playback goes through a backend port | `43_toolkits` |
| `phonon-backend-vlc` | 0.12.0 | Phonon backend that plays through libvlc | `43_toolkits` |
| `polkit-qt6` | 0.201.1 | Qt 6 wrapper around the polkit client and agent libraries | `43_toolkits` |
| `python3-qscintilla` | 2.14.1 | PyQt6.Qsci — QScintilla for Python, for QGIS's console and script editor | `43_toolkits` |
| `qscintilla` | 2.14.1 | QScintilla — the Scintilla editor widget for Qt 6, in Octave and QGIS | `43_toolkits` |
| `qtforkawesome` | 0.3.4 | The Fork Awesome icon font as a Qt 6 library, icon engine plugin and Qt Quick image provider | `43_toolkits` |
| `qtutilities` | 6.22.2 | Martchus's Qt 6 utility library: settings dialogs, about dialog, notifications and resources | `43_toolkits` |
| `quazip` | 1.7.2 | Qt 6 C++ wrapper for zip archives | `43_toolkits` |
| `qwt` | 6.3.0 | Qwt — Qt widgets for plots, dials and scales, for QGIS | `43_toolkits` |
| `qwt-qt5` | 6.3.0 | Qwt for Qt 5 — plot, dial and scale widgets for GNU Radio's gr-qtgui and Qt 5 SDR tools | `43_toolkits` |

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
| `ktexttemplate` | 6.30.0 | KTextTemplate — the KDE Frameworks text template engine, for KDevelop's templates | `43_toolkits` |
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

### kde

KDE Plasma and Gear platform pieces outside Frameworks that have no other domain: KIO workers,
thumbnailers, Plasma integration, KDE Wayland glue. 8 ports, under `ports/core/kde/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `baloo-widgets` | 26.08.1 | Baloo widgets — the file metadata panel, tag and rating editors used by Dolphin | `43_toolkits` |
| `ffmpegthumbs` | 26.08.1 | KIO thumbnailer for video files, through FFmpeg | `43_toolkits` |
| `kdegraphics-thumbnailers` | 26.08.1 | KIO thumbnailers for PostScript, PDF, Blender and camera raw files | `43_toolkits` |
| `kio-extras` | 26.08.1 | Extra KIO workers and thumbnailers: archives, man pages, MTP and iOS devices, file previews | `43_toolkits` |
| `kwayland` | 6.7.5 | Qt-style client library for the Wayland and Plasma Wayland protocols | `43_toolkits` |
| `libksysguard` | 6.7.5 | libksysguard — process list, sensors and system statistics libraries, for KDevelop's attach to process | `43_toolkits` |
| `plasma-integration` | 6.7.5 | Qt platform theme that gives every Qt 6 and KF6 application the KDE palette, fonts, icons and file dialogs | `43_toolkits` |
| `plasma-wayland-protocols` | 1.22.0 | KDE Plasma Wayland protocol definitions | `41_system` |

### toolkits

Other GUI and TUI toolkits for applications. 4 ports, under `ports/core/toolkits/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `fltk` | 1.4.5 | The Fast Light Toolkit — a small C++ GUI library, Wayland first with an X11 fallback, and FLUID | `43_toolkits` |
| `motif` | 2.5.2 | The Motif widget toolkit (libXm, libMrm, uil) for X11 programs run under Xwayland | `43_toolkits` |
| `stfl` | 0.24 | STFL — a curses-based structured terminal forms library | `41_system` |
| `wxwidgets` | 3.2.9 | wxWidgets on GTK 3 — the native-widget C++ toolkit under KiCad, PrusaSlicer, FileZilla, VeraCrypt and wxPython | `43_toolkits` |

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
| `portaudio` | 19.7.0 | Portable real-time audio I/O library, with its C++ binding, over ALSA | `41_system` |
| `pulseaudio-qt` | 1.9.0 | Qt 6 bindings for the PulseAudio client API, served here by pipewire-pulse | `43_toolkits` |
| `rtaudio` | 6.0.1 | C++ real-time audio I/O classes over ALSA and PulseAudio | `41_system` |
| `shairplay` | 0.9.0.git20180824 | AirPlay (RAOP) audio receiver library — Kodi's AirTunes target | `41_system` |
| `wiremix` | 0.11.0 | wiremix — a PipeWire mixer and routing panel for the terminal | `42_graphics` |
| `wireplumber` | 0.5.17 | PipeWire's session manager — device policy, routing and Bluetooth profiles | `42_graphics` |

### audio-codecs

Audio codecs, containers and tagging, including the Bluetooth audio codecs. 28 ports, under
`ports/core/audio-codecs/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `a52dec` | 0.8.0 | liba52, the ATSC A/52 (AC-3) audio decoder library, and the a52dec tool | `41_system` |
| `audiofile` | 0.3.6 | SGI Audio File Library — reads and writes AIFF, WAVE, NeXT/Sun, IRCAM, AVR, CAF and FLAC through one API | `41_system` |
| `codec2` | 1.2.0 | Codec 2 — the open low-bitrate speech codec and FreeDV modem library, with freedv_tx and freedv_rx | `41_system` |
| `faad2` | 2.11.3 | FAAD2 MPEG-2/MPEG-4 AAC decoder — libfaad and the faad command | `41_system` |
| `fdk-aac` | 2.0.3 | Fraunhofer AAC codec, used for AAC over Bluetooth | `41_system` |
| `flac` | 1.5.0 | Free Lossless Audio Codec library and tools | `41_system` |
| `gsm` | 1.0.24 | GSM 06.10 lossy speech codec — libgsm and the toast, untoast and tcat tools | `41_system` |
| `id3lib` | 3.8.3 | ID3v1 and ID3v2 tag library, with the id3tag, id3info, id3convert and id3cp commands | `41_system` |
| `lame` | 4.0 | LAME MP3 encoder | `41_system` |
| `ldacbt` | 2.0.2.6 | Sony LDAC encoder, for the Bluetooth headsets that negotiate it | `41_system` |
| `libfreeaptx` | 0.2.2 | Free aptX and aptX HD codec, for Bluetooth headsets that speak them | `41_system` |
| `libid3tag` | 0.16.4 | ID3 tag manipulation library (optional dep of imlib2 image audio metadata) | `41_system` |
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

### audio-dsp

Audio DSP, resampling, analysis, spatial audio, and the LV2/LADSPA/Vamp plug-in APIs with their RDF
libraries. 27 ports, under `ports/core/audio-dsp/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `aubio` | 0.4.9 | Audio labelling library — onset, pitch, beat and tempo detection | `41_system` |
| `chromaprint` | 1.6.1 | AcoustID audio fingerprinting library and the fpcalc tool | `42_graphics` |
| `iir1` | 1.10.0 | IIR1 — realtime C++ IIR filter library (Butterworth, Chebyshev, RBJ) | `41_system` |
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
| `rnnoise` | 0.2 | RNNoise — recurrent neural network noise suppression for speech, with its trained model | `41_system` |
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

### music-libs

Music-format libraries: tracker modules, chip emulation, MIDI, OSC, synthesis and soundfonts. 17
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
| `libmt32emu` | 2.8.3 | libmt32emu — Roland MT-32, CM-32L and LAPC-I synthesiser emulation (needs the original control and PCM ROMs) | `41_system` |
| `libopenmpt` | 0.8.9 | Tracker module playback library (MOD, S3M, XM, IT, MPTM and more) and openmpt123 | `41_system` |
| `libresidfp` | 1.2.2 | Cycle-exact MOS 6581/8580 SID chip emulation library, the reSIDfp engine libsidplayfp plays through | `41_system` |
| `libsidplayfp` | 3.1.1 | C64 SID music player library — libsidplayfp and libstilview, playing through reSIDfp | `41_system` |
| `libxmp` | 4.7.3 | Play tracker modules: MOD, S3M, XM, IT and some ninety other formats | `41_system` |
| `portmidi` | 2.0.8 | Portable real-time MIDI input and output library over ALSA sequencer | `41_system` |
| `portsmf` | 239 | Standard MIDI File and Allegro score reader and writer library | `41_system` |
| `rtmidi` | 6.0.0 | C++ real-time MIDI input and output classes over the ALSA sequencer | `41_system` |

### studio

Music production: DAWs, audio editors, trackers, drum machines and notation. 12 ports, under
`ports/core/studio/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `ardour` | 9.8.0 | Ardour — digital audio workstation: multitrack recording, editing, mixing and LV2 plugins (YTK, X11) | `44_apps` |
| `audacity` | 4.0.0 | Multitrack audio editor and recorder — the Qt 6 Audacity 4, with LV2, LADSPA and Nyquist effects | `44_apps` |
| `ft2-clone` | 2.24 | Fasttracker II clone — the XM tracker, pixel for pixel, on SDL2 | `44_apps` |
| `furnace` | 0.6.8.3 | Chiptune tracker for over fifty sound chips of consoles, computers and arcades | `44_apps` |
| `hydrogen` | 1.2.6 | Pattern-based drum machine and sequencer with sample drum kits | `44_apps` |
| `kwave` | 26.08.1 | KDE sound editor — record, cut, filter and convert WAV, FLAC, MP3, Ogg Vorbis and Opus | `44_apps` |
| `lmms` | 1.2.2 | LMMS — pattern-based music production: sequencer, synthesizers, samples and LADSPA effects (Qt 5) | `44_apps` |
| `milkytracker` | 1.06 | Fast Tracker II–style music tracker for XM and MOD files, on SDL2 | `44_apps` |
| `musescore` | 4.7.5 | MuseScore Studio — music notation: write, play back, print and export scores, with the MS Basic soundfont | `44_apps` |
| `pt2-clone` | 1.92 | ProTracker 2 clone — the Amiga MOD tracker, pixel for pixel, on SDL2 | `44_apps` |
| `schismtracker` | 20260524 | Impulse Tracker clone — compose and play IT, S3M, XM and MOD music, on SDL3 | `44_apps` |
| `tenacity` | 1.3.5 | Multitrack audio editor on wxWidgets — the Audacity 3 lineage with the network code removed | `44_apps` |

### music-players

Music players, library managers and taggers. 10 ports, under `ports/core/music-players/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `audacious` | 4.6.1 | Playlist-oriented music player — Qt 6 interface, MPRIS on the session bus | `43_toolkits` |
| `audacious-plugins` | 4.6.1 | Audacious decoders, outputs, effects and the Qt interface — without them the player opens no window and plays nothing | `44_apps` |
| `cmus` | 2.12.0 | Console music player with a library view | `44_apps` |
| `elisa` | 26.08.1 | KDE music player — a local music collection, played through libVLC | `44_apps` |
| `mpd` | 0.24.15 | Music Player Daemon — plays music for separate client programs | `42_graphics` |
| `picard` | 3.0.0rc4 | MusicBrainz Picard — tag and rename music files from the MusicBrainz database | `44_apps` |
| `rhythmbox` | 3.5.1 | GNOME music player and library organiser | `44_apps` |
| `rmpc` | 0.11.0 | An MPD client drawn in ratatui, with album art in the terminal | `44_apps` |
| `strawberry` | 1.2.30 | Music player and collection organiser — tags, playlists, loudness normalisation, CD and MTP devices | `44_apps` |
| `termusic` | 0.13.2 | A terminal music player drawn in ratatui, with its own playback daemon | `41_system` |

### video-libs

Video codecs, containers, subtitles, disc playback and effects libraries. 22 ports, under
`ports/core/video-libs/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `dav1d` | 1.5.4 | AV1 cross-platform decoder | `41_system` |
| `frei0r-plugins` | 3.6.0 | frei0r video effect plugins and their plugin API | `42_graphics` |
| `gavl` | 1.4.0 | Gmerlin audio and video library — colourspace, scaling and sample-format conversion for frei0r's plugins | `41_system` |
| `libass` | 0.17.5 | SSA/ASS subtitle renderer — what lets ffmpeg burn subtitles in | `42_graphics` |
| `libbluray` | 1.5.0 | Blu-ray playlists and titles — mpv's bd:// and ffmpeg's bluray: protocol | `41_system` |
| `libde265` | 1.1.3 | HEVC decoder — libheif's decode half | `41_system` |
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
| `svt-av1` | 4.2.0 | AV1 encoder (BSD-3 licence) | `41_system` |
| `vidstab` | 1.1.2 | Video stabilisation library, used by ffmpeg and MLT | `41_system` |
| `x264` | 20250608.1624 | H.264 encoder — GPL-2+, and it relicenses the ffmpeg that links it | `41_system` |
| `x265` | 4.2 | HEVC encoder — GPL-2+ | `41_system` |

### media-frameworks

Multimedia frameworks, capture, and media inspection. 17 ports, under
`ports/core/media-frameworks/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `ffmpeg` | 9.0.2 | FFmpeg — record, convert, and stream audio/video | `42_graphics` |
| `ffmpegthumbnailer` | 2.3.1 | Video thumbnail generator for file managers | `42_graphics` |
| `gst-libav` | 1.28.7 | GStreamer FFmpeg-based codec plugins | `42_graphics` |
| `gst-plugins-bad` | 1.28.7 | GStreamer "bad" plugin set (less mature plugins; auto-detects deps) | `43_toolkits` |
| `gst-plugins-base` | 1.28.7 | GStreamer base plugin set (provides gstreamer-pbutils, audio/video conversion, file I/O) | `42_graphics` |
| `gst-plugins-good` | 1.28.7 | GStreamer "good" plugin set (well-maintained LGPL plugins) | `43_toolkits` |
| `gst-plugins-ugly` | 1.28.7 | GStreamer "ugly" plugin set (patent/license-encumbered codecs) | `42_graphics` |
| `gstreamer` | 1.28.7 | GStreamer streaming media framework — core library | `42_graphics` |
| `libcamera` | 0.7.2 | Camera stack that puts ISP pipelines and sensor control behind one API | `42_graphics` |
| `libmediainfo` | 26.05 | Media file inspection library — codec, profile, bit depth and every track | `41_system` |
| `libzen` | 0.4.41 | ZenLib, the portability layer libmediainfo is written on | `41_system` |
| `mediainfo` | 26.05 | The command line over libmediainfo | `41_system` |
| `mlt` | 7.40.0 | MLT multimedia framework: the video editing engine under Kdenlive and Shotcut | `43_toolkits` |
| `opentimelineio` | 0.18.1 | OpenTimelineIO: interchange format and C++ API for editorial timelines | `41_system` |
| `orc` | 0.4.44 | Oil Runtime Compiler — JIT for the SIMD inner loops of GStreamer's plugins | `41_system` |
| `totem-pl-parser` | 3.26.7 | Playlist parser library — m3u, pls, xspf and podcast feeds | `41_system` |
| `v4l-utils` | 1.32.0 | Video4Linux userspace — libv4l2 and libv4lconvert, libdvbv5, v4l2-ctl, media-ctl, ir-keytable, cec-ctl | `41_system` |

### video-players

Video players and media centres, and their front-end add-ons. 8 ports, under
`ports/core/video-players/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `celluloid` | 0.30 | GTK 4 video player over libmpv | `44_apps` |
| `haruna` | 1.8.1 | Haruna: a Qt 6 and Kirigami video player over libmpv | `44_apps` |
| `kodi` | 21.3 | Kodi — ten-foot media centre for local and network video, music and pictures, with add-ons | `44_apps` |
| `mpv` | 0.41.0 | Command-line media player for Wayland, with no toolkit | `42_graphics` |
| `mpv-mpris` | 1.3 | Puts mpv on the session bus, so the media keys and the panel reach it | `44_apps` |
| `smplayer` | 26.8.29 | Qt front end for mpv that remembers where every file stopped | `44_apps` |
| `uosc` | 5.13.0 | Minimalist, proximity-based on-screen controls, menus and playlist for mpv | `44_apps` |
| `vlc` | 3.0.24 | VLC media player 3 with its Qt 5 interface — plays files, discs, devices and streams | `43_toolkits` |

### video-tools

Video editing, transcoding, muxing, recording, streaming and webcam apps. 9 ports, under
`ports/core/video-tools/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `avidemux` | 2.8.1 | Avidemux: a Qt 6 video cutter, filter and encoder | `44_apps` |
| `blind` | 1.1 | Video editing as a pipeline of small programs over raw frames | `44_apps` |
| `handbrake` | 1.11.2 | HandBrake video transcoder: the GTK 4 application and HandBrakeCLI | `44_apps` |
| `kamoso` | 26.08.1 | KDE webcam booth — take pictures and record videos from a camera | `44_apps` |
| `kdenlive` | 26.08.1 | Kdenlive: the KDE multi-track video editor, on MLT | `44_apps` |
| `mkvtoolnix` | 102.0 | MKVToolNix: create, split, edit and inspect Matroska files, with the Qt 6 GUI | `44_apps` |
| `obs-studio` | 32.2.2 | OBS Studio: screen recording and live streaming, with PipeWire screen capture on Wayland | `44_apps` |
| `shotcut` | 26.8.1 | Shotcut: a Qt 6 video editor on MLT, with no KDE Frameworks | `44_apps` |
| `wf-recorder` | 0.6.0 | Screen recorder for wlroots compositors, through wlr-screencopy | `44_apps` |

### game-libs

Game engines and game-development libraries, SDL in all versions, and shared game libraries. 18
ports, under `ports/core/game-libs/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `enet` | 1.3.18 | Reliable UDP networking for games | `41_system` |
| `glm` | 1.0.3 | OpenGL Mathematics: header-only C++ vectors and matrices in GLSL's terms | `41_system` |
| `libkdegames` | 26.08.1 | Common code, card decks and sound for the KDE games | `43_toolkits` |
| `libkmahjongg` | 26.08.1 | Tile sets, backgrounds and the rendering code shared by the KDE Mahjongg games | `43_toolkits` |
| `love` | 11.5 | LÖVE — the Lua 2D game framework, and the runtime that opens a .love game | `42_graphics` |
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

### games-action

Action, arcade, racing, platform and sandbox games. 9 ports, under `ports/core/games-action/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `ddnet` | 20.1 | DDraceNetwork — the cooperative Teeworlds racing game, its client, server and map tools | `44_apps` |
| `luanti` | 5.17.0 | Luanti (formerly Minetest) — voxel game engine, with Minetest Game for offline play | `44_apps` |
| `moon-buggy` | 1.1.0 | Drive a buggy across the moon, jumping craters and shooting meteors | `41_system` |
| `neverball` | 1.6.0 | Neverball and Neverputt — tilt the floor to roll a ball through 3D obstacle courses, and minigolf on the same engine | `44_apps` |
| `powder-toy` | 100.1.400 | The Powder Toy — a falling-sand physics sandbox of air pressure, heat, gravity and electronics, with local saves | `44_apps` |
| `srb2` | 2.2.15 | Sonic Robo Blast 2 — a 3D Sonic fan game on a heavily modified Doom Legacy engine, with its game data | `44_apps` |
| `stk-assets` | 1.5 | SuperTuxKart game data — karts, tracks, arenas, music, sounds, models and textures | `41_system` |
| `supertux` | 0.7.0 | SuperTux — classic 2D side-scrolling platformer with Tux, with a level editor | `44_apps` |
| `supertuxkart` | 1.5 | SuperTuxKart — 3D kart racing with story mode, grand prix, battles and soccer | `44_apps` |

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
| `sauerbraten` | 2020.12.29 | Cube 2: Sauerbraten — the Cube 2 engine shooter with in-game co-operative map editing, and its data | `44_apps` |
| `xonotic` | 0.8.6 | Xonotic — the arena first-person shooter on the DarkPlaces engine: SDL client and dedicated server | `44_apps` |
| `xonotic-data` | 0.8.6 | Xonotic's game data — maps, models, textures, sounds and music, about 1.2 GB | `41_system` |
| `yamagi-quake2` | 8.70 | Quake II engine kept faithful to the original — OpenGL 1.4, 3.2, GLES3 and software renderers | `44_apps` |

### games-strategy

Strategy, simulation and space-trading games, and their data. 12 ports, under
`ports/core/games-strategy/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `endless-sky` | 0.11.2 | Endless Sky — 2D space trading, exploration and combat in the spirit of Escape Velocity | `44_apps` |
| `fheroes2` | 1.1.17 | fheroes2 — the Heroes of Might and Magic II engine, for a player's own copy of the game data | `44_apps` |
| `freeciv` | 3.2.6 | Freeciv — turn-based empire-building strategy in the Civilization tradition, SDL2 client and server | `44_apps` |
| `openrct2` | 0.5.5 | OpenRCT2 — the RollerCoaster Tycoon 2 engine, for a player's own copy of the RCT2 or RCT Classic data | `44_apps` |
| `openttd` | 15.3 | Transport Tycoon Deluxe engine — build rail, road, air and sea networks | `44_apps` |
| `openttd-opengfx` | 8.0 | OpenGFX: the free graphics base set for OpenTTD | `41_system` |
| `openttd-openmsx` | 0.4.2 | OpenMSX: the free music base set for OpenTTD | `41_system` |
| `openttd-opensfx` | 1.0.3 | OpenSFX: the free sound base set for OpenTTD | `41_system` |
| `pioneer` | 20260907 | Pioneer — open-ended space trading and combat across a procedurally generated Milky Way, with Newtonian flight | `44_apps` |
| `warzone2100` | 4.7.0 | Warzone 2100 — post-apocalyptic real-time strategy with campaigns, skirmish and a research tree | `44_apps` |
| `wesnoth` | 1.18.8 | Battle for Wesnoth — turn-based fantasy strategy with campaigns, skirmishes and a map editor | `44_apps` |
| `widelands` | 1.3.1 | Widelands — real-time strategy about building an economy and its roads, in the line of The Settlers II | `44_apps` |

### games-rpg

RPGs, roguelikes and interactive fiction. 8 ports, under `ports/core/games-rpg/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `brogue-ce` | 1.15.1 | Brogue Community Edition — a roguelike of 26 dungeon levels, drawn in SDL2 tiles or glyphs | `44_apps` |
| `cataclysm-dda` | 0.I.1 | Cataclysm: Dark Days Ahead — turn-based survival in a procedurally generated post-apocalyptic world, in its tiles build | `44_apps` |
| `crawl-tiles` | 0.34.1 | Dungeon Crawl Stone Soup — the open-ended roguelike of the Orb of Zot, in its graphical tiles build | `44_apps` |
| `devilutionx` | 1.5.5 | DevilutionX — the Diablo and Hellfire engine, for a player's own copy of the game data | `44_apps` |
| `flare-engine` | 1.15 | FLARE — the engine of the Flare isometric action RPG, run by the game data of flare-game or any other mod | `42_graphics` |
| `flare-game` | 1.15 | Flare: Empyrean Campaign — the art, music, maps and story that make the FLARE engine a game | `44_apps` |
| `frotz` | 2.55 | Z-machine interpreter — Infocom-era interactive fiction in a terminal | `41_system` |
| `nethack` | 5.0.0 | Roguelike dungeon game in characters, with saves under the player's own home | `41_system` |

### games-board

Board, card, puzzle, chess and classic small games, whatever toolkit or desktop project they come
from. 18 ports, under `ports/core/games-board/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `aisleriot` | 3.22.35 | AisleRiot — over eighty solitaire card games, each written in Scheme | `44_apps` |
| `black-hole-solver` | 1.14.0 | Solver for the Black Hole, All in a Row and Golf patience games | `41_system` |
| `bsd-games` | 2.17 | Three of the BSD games — Colossal Cave, tetris and worm | `41_system` |
| `chess-tui` | 2.7.1 | Chess in a terminal, against Stockfish or a second player | `41_system` |
| `freecell-solver` | 6.16.0 | Solver for Freecell, Simple Simon and related patience games, as a library and a command | `41_system` |
| `gnome-mahjongg` | 51.1 | GNOME Mahjongg — match pairs of tiles until the board is clear | `44_apps` |
| `gnome-mines` | 50.0 | GNOME Mines — clear hidden mines from a minefield | `44_apps` |
| `gnome-sudoku` | 51.0.1 | GNOME Sudoku — the number-grid puzzle, generated at four difficulties | `44_apps` |
| `katomic` | 26.08.1 | KAtomic — slide atoms into place to build the molecule | `44_apps` |
| `kblocks` | 26.08.1 | KBlocks — the falling-blocks game, against the clock or a computer player | `44_apps` |
| `kbounce` | 26.08.1 | KBounce — fence off the field while the balls bounce around it | `44_apps` |
| `kmahjongg` | 26.08.1 | KMahjongg — mahjong solitaire: clear the board by matching pairs of free tiles | `44_apps` |
| `kmines` | 26.08.1 | KMines — the classic minesweeper, on a themed board | `44_apps` |
| `kpat` | 26.08.1 | KPatience — fourteen solitaire card games, with a solver for each deal | `44_apps` |
| `kreversi` | 26.08.1 | KReversi — the reversi board game against the computer or a second player | `44_apps` |
| `ksudoku` | 26.08.1 | KSudoku — sudoku, jigsaw, killer and 3-D Roxdoku puzzles, generated and solved | `44_apps` |
| `qqwing` | 1.3.4 | Sudoku generator and solver, as a C++ library and a command | `41_system` |
| `stockfish` | 19 | Stockfish — the UCI chess engine, with its evaluation network built in | `41_system` |

### emulators

Standalone emulators of computers and consoles, and compatibility layers. 18 ports, under
`ports/core/emulators/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `amiberry` | 8.3.0 | Amiberry — Amiga emulator from the A500 to the A4000 and CD32, with WHDLoad booting and the AROS ROMs | `44_apps` |
| `dosbox-staging` | 0.83.0 | DOSBox Staging — a DOS PC emulator for games and old software | `44_apps` |
| `dosbox-x` | 2026.08.31 | DOSBox-X — x86 PC emulator for DOS, Windows 3.x/9x, PC-98 and PCjr/Tandy software | `44_apps` |
| `fuse-emulator` | 1.10.0 | Fuse — the Free Unix Spectrum Emulator, 16K to +3, Pentagon and Timex, with its ROMs | `44_apps` |
| `hatari` | 2.6.1 | Hatari — Atari ST, STE, TT and Falcon emulator, with the Hatari UI configuration front end | `44_apps` |
| `libretro-mgba` | 0.10.5 | mGBA as a libretro core — Game Boy Advance, Game Boy and Game Boy Color in RetroArch | `41_system` |
| `libspectrum` | 1.7.0 | ZX Spectrum emulator file formats — snapshots, tapes, disks and RZX recordings | `41_system` |
| `mednafen` | 1.32.1 | Mednafen — command-line multi-system emulator: NES, SNES, Game Boy, GBA, Mega Drive, PC Engine, PlayStation, Saturn and more | `44_apps` |
| `mgba` | 0.10.5 | mGBA — Game Boy Advance, Game Boy and Game Boy Color emulator, SDL front end | `44_apps` |
| `mupen64plus` | 2.6.0 | Mupen64Plus — Nintendo 64 emulator: core, console front end, SDL audio and input, HLE RSP, Rice and Glide64mk2 video | `44_apps` |
| `openmsx` | 21.0 | openMSX — MSX home computer emulator with C-BIOS, a built-in debugger and GUI | `44_apps` |
| `ppsspp` | 1.20.4 | PPSSPP — PlayStation Portable emulator, OpenGL and Vulkan, drawn through SDL | `44_apps` |
| `scummvm` | 2026.3.0 | ScummVM — runs classic point-and-click adventure and role-playing games from their original data files | `44_apps` |
| `stella` | 7.0c | Stella — Atari 2600 VCS emulator with a built-in debugger | `44_apps` |
| `vice` | 3.10 | VICE — Commodore 64, 128, VIC-20, PET, Plus/4 and CBM-II emulators, with their ROMs | `44_apps` |
| `wine` | 11.0 | Wine — runs Windows programs on Linux, 64- and 32-bit, on Wayland or X11 | `44_apps` |
| `wine-gecko` | 2.47.4 | Wine Gecko — the HTML engine Wine's MSHTML uses, 32- and 64-bit, as upstream builds it | `41_system` |
| `wine-mono` | 10.4.1 | Wine Mono — the .NET Framework runtime Wine installs into a prefix, as upstream builds it | `41_system` |

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
| `retroarch` | 1.22.2 | RetroArch — the libretro frontend: one window, one menu and one set of controls for every emulator core | `44_apps` |
| `retroarch-assets` | 1.22.0 | RetroArch menu artwork — the XMB, Ozone, RGUI and GLUI themes, their icons, fonts and sounds | `41_system` |
| `retroarch-joypad-autoconfig` | 1.22.0 | RetroArch controller profiles — a game pad is mapped the moment it is plugged in | `41_system` |

### education

Learning software, offline encyclopedias and reference dictionaries. 16 ports, under
`ports/core/education/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `gcompris` | 26.2 | Educational activities for children aged 2 to 10 — reading, counting, science, games and art | `44_apps` |
| `goldendict-ng` | 26.8.0 | Dictionary lookup — StarDict, DSL, MDict, ZIM, Babylon and hunspell morphology, offline | `44_apps` |
| `kalzium` | 26.08.1 | KDE periodic table of the elements — properties, isotopes, spectra and a molar mass calculator | `44_apps` |
| `kgeography` | 26.08.1 | KDE geography trainer — maps, capitals and flags of countries and regions, as quizzes | `44_apps` |
| `kiwix-desktop` | 2.5.1 | Kiwix: an offline reader for ZIM libraries such as Wikipedia, with search, tabs and a reading list | `44_apps` |
| `kiwix-tools` | 3.8.2 | kiwix-serve, kiwix-manage and kiwix-search over ZIM archives | `41_system` |
| `kolibri` | 0.19.5 | Offline learning platform — courses, lessons and quizzes from content channels, served on the LAN | `41_system` |
| `ktouch` | 26.08.1 | KDE touch typing tutor — graded courses, keyboard layouts and progress statistics | `44_apps` |
| `kturtle` | 26.08.1 | KDE educational programming environment — steer a turtle with TurtleScript and learn to program | `44_apps` |
| `kwordquiz` | 26.08.1 | KDE flashcard trainer — flashcards, multiple choice and question-and-answer quizzes on KVTML decks | `44_apps` |
| `libkeduvocdocument` | 26.08.1 | KDE vocabulary document library — reads and writes the KVTML files of Parley and KWordQuiz | `43_toolkits` |
| `libkiwix` | 14.2.1 | The library kiwix-serve searches and serves a ZIM with | `41_system` |
| `libzim` | 9.8.2 | Read and write the ZIM archives Wikipedia offline ships as | `41_system` |
| `parley` | 26.08.1 | KDE vocabulary trainer — spaced repetition over KVTML word lists, with themes and HTML export | `44_apps` |
| `sdcv` | 0.5.5 | Look a word up in a StarDict dictionary, offline | `41_system` |
| `tuxpaint` | 0.9.35 | Drawing program for children — brushes, stamps, shapes, text and magic tools | `44_apps` |

### office

Office suites, word processors, spreadsheets, presentations, accounting and finance, and
office-format import libraries. 22 ports, under `ports/core/office/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `abiword` | 3.0.8 | AbiWord word processor — reads and writes Word, OpenDocument, RTF and HTML | `44_apps` |
| `freexl` | 2.0.0 | FreeXL — reads Excel .xls, .xlsx and LibreOffice .ods sheets, for SpatiaLite and GDAL | `41_system` |
| `gnucash` | 5.17 | Double-entry accounting — personal and small-business books, invoices, reports | `44_apps` |
| `gnumeric` | 1.12.62 | GNOME spreadsheet — accurate statistics, and reads Excel, OpenDocument and Lotus files | `44_apps` |
| `goffice` | 0.10.62 | GLib/GTK office library — charts, formats and canvas shared by Gnumeric and AbiWord | `43_toolkits` |
| `homebank` | 5.10.3 | Personal finance manager — accounts, budgets, reports, QIF and CSV import | `44_apps` |
| `ledger` | 3.4.1 | Double-entry accounting over a plain-text journal | `44_apps` |
| `libcdr` | 0.1.9 | CorelDRAW file import library | `41_system` |
| `libgsf` | 1.14.59 | GNOME Structured File library — OLE2, ZIP and ODF containers for office formats | `41_system` |
| `libixion` | 0.20.0 | ixion — threaded spreadsheet formula engine, the calculation half of orcus | `41_system` |
| `liborcus` | 0.20.2 | orcus — import filters for ODS, XLSX, Excel 2003 XML, Gnumeric, CSV, JSON and YAML | `41_system` |
| `libreoffice` | 26.8.0.3 | Office suite — Writer, Calc, Impress, Draw, Math and Base, with the gtk3 and qt6 front ends | `44_apps` |
| `librevenge` | 0.0.6 | Base library for document import filters: the drawing, text and spreadsheet interfaces | `41_system` |
| `libvisio` | 0.1.11 | Microsoft Visio diagram import library | `41_system` |
| `libwpd` | 0.10.3 | WordPerfect document import library | `41_system` |
| `libwpg` | 0.3.4 | WordPerfect Graphics import library | `41_system` |
| `libxlsxwriter` | 1.2.4 | C library for writing Excel XLSX files | `41_system` |
| `mdds` | 3.2.1 | mdds — header-only multi-dimensional data structures for ixion and orcus | `41_system` |
| `presenterm` | 0.16.1 | Slides from a markdown file, drawn in the terminal | `41_system` |
| `sc-im` | 0.8.5 | Terminal spreadsheet with vi keys | `44_apps` |
| `tryton` | 7.0.44 | Tryton — desktop client for the Tryton business platform and GNU Health, in GTK 3 | `44_apps` |
| `wv` | 1.2.9 | Library and converters for Microsoft Word 2000, 97, 95 and 6 documents | `41_system` |

### documents

PDF, PostScript, DjVu, EPUB, CHM and DOCX viewers, libraries and tools, and ebook managers. 20
ports, under `ports/core/documents/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `calibre` | 9.15.0 | E-book library manager, converter, viewer and editor | `44_apps` |
| `chmlib` | 0.40a | Library for reading Microsoft Compiled HTML Help (CHM) files, with extract_chmLib | `41_system` |
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

### pim

Personal information: calendars, contacts, tasks, time tracking, notes and genealogy. 12 ports,
under `ports/core/pim/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `calcurse` | 4.8.2 | A calendar and todo list whose storage is two text files | `41_system` |
| `gramps` | 6.0.8 | Gramps — genealogy: family trees, people, places, sources and reports | `44_apps` |
| `khal` | 0.14.1 | Terminal calendar over a vdir | `41_system` |
| `khard` | 0.21.0 | Terminal address book over a vdir | `41_system` |
| `libical` | 4.0.5 | Reference implementation of the iCalendar and vCard formats | `41_system` |
| `nb` | 7.25.5 | Notes and bookmarks in a git repository, from one command | `41_system` |
| `qownnotes` | 26.9.13 | Plain-text Markdown notes with a live preview, to-do lists and optional Nextcloud sync | `44_apps` |
| `taskwarrior` | 3.5.0 | Tasks with dates, dependencies and a query language | `41_system` |
| `taskwarrior-tui` | 0.27.0 | Terminal UI for taskwarrior | `41_system` |
| `timewarrior` | 1.10.0 | Time tracking in a text file | `41_system` |
| `vdirsyncer` | 0.21.0 | Makes a CalDAV or CardDAV collection and a local vdir equal | `41_system` |
| `zim` | 0.77.2 | Zim desktop wiki — linked notebook pages, journal, tasks and attachments in plain text | `44_apps` |

### printing

Printing, printer drivers, labels, scanning and OCR. 26 ports, under `ports/core/printing/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `brlaser` | 6.2.8 | CUPS driver for Brother monochrome laser printers | `41_system` |
| `cups` | 2.4.19 | Print spooler and associated utilities | `41_system` |
| `cups-browsed` | 2.1.1 | Daemon that turns printers announced on the network into local CUPS queues | `43_toolkits` |
| `cups-filters` | 2.0.1 | The filters CUPS needs to turn a job into something a printer prints | `43_toolkits` |
| `foomatic-db` | 20260926 | Foomatic printer database — printer and driver XML plus manufacturer PPDs for CUPS | `44_apps` |
| `foomatic-db-engine` | 4.0.13 | Foomatic PPD generator — builds CUPS PPDs from the foomatic-db printer database | `43_toolkits` |
| `ghostscript` | 10.08.0 | PostScript and PDF interpreter | `41_system` |
| `gimagereader` | 3.4.3 | OCR front end for Tesseract — scan or open images and PDFs, recognise and edit text | `44_apps` |
| `glabels` | 3.4.1 | gLabels — labels, business cards and envelopes, with barcodes and mail merge | `44_apps` |
| `gutenprint` | 5.3.5 | Printer drivers for inkjet and dye-sublimation printers | `41_system` |
| `hplip` | 3.26.4 | HP printer and scanner drivers — hpcups, the hp backend, PPDs and the hpaio SANE backend | `44_apps` |
| `ipp-usb` | 0.9.34 | A USB printer that speaks IPP-over-USB, presented to cups as a network one | `41_system` |
| `ksanecore` | 26.08.1 | Qt 6 library that drives SANE scanners, without a user interface | `43_toolkits` |
| `libcupsfilters` | 2.2.1 | The filter functions behind every CUPS print filter, as a library | `43_toolkits` |
| `libksane` | 26.08.1 | Qt 6 scanner widget over ksanecore, used by Skanlite and KolourPaint | `43_toolkits` |
| `libpaper` | 2.3.0 | Library and paper(1) tool for the system's default paper size and the known paper sizes | `41_system` |
| `libppd` | 2.1.1 | The PPD handling CUPS 3 drops, as a library for its filters and daemons | `43_toolkits` |
| `ocrmypdf` | 17.12.1 | Adds an invisible OCR text layer to a scanned PDF, in place | `42_graphics` |
| `ptouch-print` | 1.9 | ptouch-print — print text and images on Brother P-touch label printers over USB | `42_graphics` |
| `sane-airscan` | 0.99.38 | Driverless scanning over eSCL and WSD | `44_apps` |
| `sane-backends` | 1.4.0 | The scanner driver set, and the udev rules 70-kdos-*.rules already grant | `43_toolkits` |
| `skanlite` | 26.08.1 | Image scanning application — preview, select and save scans through SANE | `44_apps` |
| `splix` | 2.0.2 | CUPS driver for Samsung, Xerox, Dell and Lexmark SPL2/SPLc laser printers | `44_apps` |
| `system-config-printer` | 1.5.18 | Printer configuration tool for CUPS — add, configure and manage print queues (GTK 3) | `44_apps` |
| `tessdata-eng` | 4.1.0 | English and script-detection models for the tesseract OCR engine | `41_system` |
| `tesseract` | 5.5.3 | OCR engine — text from scanned images | `41_system` |

### math

Mathematics applications and libraries: computer algebra, arbitrary precision, solvers, calculators,
statistics and plotting apps. 24 ports, under `ports/core/math/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `analitza` | 26.08.1 | KDE mathematical expression library — the parser, evaluator and 2D/3D plotter behind KAlgebra | `43_toolkits` |
| `cantor` | 26.08.1 | Cantor — worksheet front end for Python, R, Octave, Maxima, Qalculate, KAlgebra and Lua | `43_toolkits` |
| `ceres-solver` | 2.2.0.git20251109 | Ceres Solver: non-linear least squares — Blender's camera and motion tracker | `41_system` |
| `coolprop` | 8.0.0 | Thermophysical properties for 122 fluids, IAPWS-95 water/steam and humid air | `41_system` |
| `glpk` | 5.0 | Linear and mixed-integer programming, with its own modelling language | `41_system` |
| `gmp` | 6.3.0 | Arbitrary-precision arithmetic library | `20_selfhost` (as a dependency) |
| `gnuplot` | 6.0.5 | Plotting program driven by scripts, drawing to the terminal, to a Qt window or to PNG | `43_toolkits` |
| `highs` | 1.15.1 | Linear and mixed-integer programming solver | `41_system` |
| `kalgebra` | 26.08.1 | KDE graph calculator — expressions, 2D and 3D plots, and a console calculator | `44_apps` |
| `labplot` | 2.12.1 | LabPlot — data visualisation and analysis: plots, fits, FFT, spreadsheets and live data | `44_apps` |
| `libqalculate` | 5.12.0 | Calculator library with unit and expression parsing | `41_system` |
| `mpc` | 1.4.1 | A library for the arithmetic of complex numbers with arbitrarily high precision | `20_selfhost` (as a dependency) |
| `mpfr` | 4.2.2 | Functions for multiple precision math | `20_selfhost` (as a dependency) |
| `nlopt` | 2.11.0 | NLopt — nonlinear optimisation library used by slicers and nesting tools | `41_system` |
| `numbat` | 1.24.0 | Calculator with physical units and dimension checking | `41_system` |
| `octave` | 11.3.0 | GNU Octave — MATLAB-compatible numerical computing, with its Qt 6 desktop | `44_apps` |
| `pari` | 2.17.4 | Number theory and arbitrary-precision arithmetic, at a prompt | `41_system` |
| `qalculate-qt` | 5.12.0 | Qalculate! desktop calculator (Qt) — units, currencies, symbolic algebra, plots | `44_apps` |
| `rkward` | 0.8.3 | RKWard — a KDE front end to R: data editor, plots, dialogs and R Markdown | `44_apps` |
| `sympy` | 1.14.0 | Symbolic mathematics for Python | `41_system` |
| `units` | 2.27 | Unit conversion program and its units database | `41_system` |
| `veusz` | 4.2.1 | Veusz — publication-quality 2D and 3D scientific plots, in Python and PyQt6 | `44_apps` |
| `yices2` | 2.7.0 | The SMT solver SymbiYosys proves properties with | `41_system` |
| `z3` | 5.1.0 | Microsoft Research's SMT solver and theorem prover — libz3, the z3 command and its Python bindings | `41_system` |

### sci-libs

Numerical and scientific-data libraries, and the unprefixed core Python science stack. 21 ports,
under `ports/core/sci-libs/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `arpack-ng` | 3.9.1 | ARPACK-NG — large sparse eigenvalue problems, for Octave's eigs and SciPy-style solvers | `41_system` |
| `eigen` | 5.0.1 | Header-only C++ linear algebra library (nextpnr's timing analysis uses it) | `41_system` |
| `fftw` | 3.3.11 | Fast Fourier transform library | `41_system` |
| `gsl` | 2.8 | GNU Scientific Library — numerical routines outside linear algebra | `41_system` |
| `hdf5` | 2.2.0 | Hierarchical Data Format library for scientific data | `41_system` |
| `libcerf` | 3.8 | libcerf — complex error, Faddeeva, Voigt and Dawson functions | `41_system` |
| `libmed` | 5.0.0 | MED-file, the Salome mesh and field format library over HDF5 (FreeCAD FEM meshes) | `41_system` |
| `matio` | 1.6.0 | matio — reads and writes MATLAB MAT files, v4, v5 and the HDF5-based v7.3 | `41_system` |
| `matplotlib` | 3.11.2 | 2D plotting library for Python, Agg backend only | `42_graphics` |
| `nco` | 5.4.0 | Subset, regrid and do arithmetic on gridded data without loading it | `41_system` |
| `netcdf-c` | 4.10.1 | Library for netCDF, the self-describing array format of climate, ocean and atmosphere data | `41_system` |
| `numpy` | 2.5.3 | N-dimensional array library for Python | `41_system` |
| `openblas` | 0.3.34 | Optimised BLAS and LAPACK library | `41_system` |
| `pandas` | 3.0.6 | Data frames for Python — tables, joins and group-bys | `42_graphics` |
| `qrupdate` | 1.2.0 | QR and Cholesky factorisation updates — Octave's qrupdate, cholupdate and friends | `41_system` |
| `scipy` | 1.18.1 | Scientific computing for Python — optimisation, integration, signal, sparse, statistics and physical constants | `41_system` |
| `spooles` | 2.2 | SPOOLES — sparse direct solver library (LU and Cholesky factorisation, serial and threaded), the default solver of CalculiX | `41_system` |
| `suitesparse` | 7.14.1 | Sparse matrix algebra — CHOLMOD, UMFPACK, KLU, SPQR and CXSparse, under Octave | `41_system` |
| `sundials` | 7.9.0 | SUNDIALS — ODE and DAE solvers, behind Octave's ode15s and ode15i | `41_system` |
| `udunits` | 2.2.28 | Unit conversion library for scientific data | `41_system` |
| `xsimd` | 14.3.0 | C++ wrappers for SIMD intrinsics (headers) | `41_system` |

### astronomy

Astronomy, planetaria, ephemerides, FITS and satellite prediction. 14 ports, under
`ports/core/astronomy/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `astropy` | 8.0.1 | Coordinates, times, FITS, WCS and cosmology | `42_graphics` |
| `celestia` | 1.7.0.git20260926 | Real-time 3D space simulator — fly through the solar system, the stars and the galaxies | `44_apps` |
| `celestia-content` | 1.7.0.git20260923 | Celestia's universe — star and deep-sky catalogues, planet textures, models and orbits | `43_toolkits` |
| `cfitsio` | 4.7.0 | Library for reading and writing FITS, the standard astronomical data format | `41_system` |
| `cspice` | 67.0.0.git20260416 | CSPICE N0067 — NASA NAIF's SPICE toolkit for planetary ephemerides and observation geometry, as a shared C library | `41_system` |
| `erfa` | 2.0.1 | Essential Routines for Fundamental Astronomy — the IAU standard, relicensed | `41_system` |
| `gnuastro` | 0.24 | The GNU astronomy toolset — arithmetic, cropping, photometry, on FITS | `41_system` |
| `gpredict` | 2.6 | Gpredict — real-time satellite tracking and pass prediction with radio and rotator control | `44_apps` |
| `libnova` | 0.15.0 | Celestial mechanics and astronomical calculation library — sun and moon rise, set and phase for Viking | `41_system` |
| `predict` | 3.0.2 | Curses satellite tracker — passes, doppler and footprints from a TLE | `41_system` |
| `sgp4` | 3.0 | SGP4 — C++ library for satellite orbit propagation from two-line element sets | `41_system` |
| `skyfield` | 1.55 | Planet, moon and satellite positions from JPL kernels | `41_system` |
| `stellarium` | 26.2 | Planetarium — a realistic sky in 3D, as seen with the eye, binoculars or a telescope | `44_apps` |
| `wcslib` | 8.9 | The FITS World Coordinate System — pixel to sky, and back | `41_system` |

### bioscience

Bioinformatics, chemistry and medical informatics or imaging. 12 ports, under
`ports/core/bioscience/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `avogadrolibs` | 2.0.0 | Avogadro libraries — molecular data, file formats, OpenGL rendering and Qt widgets for chemistry programs | `43_toolkits` |
| `bcftools` | 1.24 | Call and filter variants | `41_system` |
| `dcmtk` | 3.7.0 | DCMTK — the OFFIS DICOM toolkit: medical image conversion, network storage and query tools | `41_system` |
| `diamond` | 2.2.8 | Protein and translated-DNA sequence aligner, a faster BLASTP/BLASTX | `41_system` |
| `gnuhealth` | 5.0.7 | GNU Health — hospital and clinic information system: patients, labs, pharmacy, imaging and stock, on Tryton | `42_graphics` |
| `hmmer` | 3.4 | Profile hidden Markov models — the sequence search Pfam is built on | `41_system` |
| `htslib` | 1.24 | The SAM/BAM/CRAM/VCF reader samtools and bcftools are built on | `41_system` |
| `mafft` | 7.526 | Multiple sequence alignment | `41_system` |
| `minimap2` | 2.31 | Align long reads to a reference | `41_system` |
| `openbabel` | 3.2.1 | Chemistry toolbox — reads, writes and converts over a hundred molecular file formats | `42_graphics` |
| `samtools` | 1.24 | Sort, index and view alignments | `41_system` |
| `seqkit` | 2.13.0 | FASTA/FASTQ manipulation toolkit | `41_system` |

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
| `josm` | 19613 | JOSM — the Java OpenStreetMap editor: map data, GPS traces, imagery and validation (Java Swing) | `42_graphics` |
| `libgeotiff` | 1.7.4 | libgeotiff — reads and writes the georeferencing tags of GeoTIFF rasters, with listgeo and geotifcp | `41_system` |
| `librttopo` | 1.1.0 | RT Topology Library — PostGIS-style topology and validity functions, for SpatiaLite | `41_system` |
| `libspatialindex` | 2.1.0 | libspatialindex — R-tree spatial indexing, under QGIS | `41_system` |
| `marble` | 26.08.1 | Marble: virtual globe and world atlas, with offline Atlas, satellite and OpenStreetMap vector views | `44_apps` |
| `opencpn` | 5.14.0 | OpenCPN — chart plotter and navigation for S-57/S-63 ENC and raster charts, AIS and GPS (wxWidgets) | `44_apps` |
| `organicmaps` | 2025.09.05.1 | Organic Maps: offline maps with search and turn-by-turn routing for walking, cycling and driving, from OpenStreetMap data | `44_apps` |
| `pdal` | 2.10.2 | PDAL — point cloud translation and processing: LAS/LAZ, E57, Draco, HDF and the pdal tool | `43_toolkits` |
| `proj` | 9.9.0 | Cartographic projection and coordinate transformation library | `41_system` |
| `qgis` | 3.44.15 | QGIS — the desktop geographic information system, with PyQGIS and Processing | `44_apps` |
| `qmapshack` | 1.21.1 | QMapShack: offline topographic maps, elevation models, GPS tracks and route planning with Routino | `44_apps` |
| `routino` | 3.4.4 | Offline route planning over an OSM extract | `41_system` |
| `shapelib` | 1.6.3 | Shapelib — read and write ESRI shapefiles and their dBase attribute tables | `41_system` |
| `viking` | 1.11 | Viking — GPS track, waypoint and route manager on maps, with MBTiles and gpsd (GTK 3) | `44_apps` |

### eda

Electronics: schematic and PCB, circuit simulation, antenna modelling, FPGA synthesis and
simulation, logic analysers and bench instruments. 29 ports, under `ports/core/eda/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `digital` | 0.31 | Digital — digital logic designer and circuit simulator for teaching, with FSM and HDL export (Java Swing) | `42_graphics` |
| `horizon-eda` | 2.7.2 | Horizon EDA — schematic capture and PCB layout around a pool of parts with unique identities | `44_apps` |
| `icestorm` | 1.1 | The iCE40 bitstream database, packer and programmer | `41_system` |
| `iverilog` | 13_0 | Event-driven Verilog simulation with real delays and X propagation | `41_system` |
| `kicad` | 10.0.6 | KiCad — schematic capture, PCB layout, Gerber viewer and SPICE simulation for electronics design | `44_apps` |
| `kicad-footprints` | 10.0.6 | KiCad's official PCB footprint library, with the global fp-lib-table template | `41_system` |
| `kicad-symbols` | 10.0.6 | KiCad's official schematic symbol library, with the global sym-lib-table template | `41_system` |
| `kicad-templates` | 10.0.6 | KiCad's official project templates — board outlines and starting points for common form factors | `41_system` |
| `liblxi` | 1.22 | Discover and talk to an LXI instrument | `41_system` |
| `libmodbus` | 3.2.0 | Modbus RTU and TCP library | `41_system` |
| `librepcb` | 2.1.1 | LibrePCB — schematic and PCB design with managed libraries, STEP 3D export and Gerber output (Qt 6, Slint) | `44_apps` |
| `libsigrok` | 0.5.2 | Hardware drivers for logic analysers, oscilloscopes and meters | `41_system` |
| `libsigrokdecode` | 0.5.3 | Protocol decoder library, with the decoders written in Python | `41_system` |
| `logisim-evolution` | 5.0.0 | Logisim-evolution — digital logic designer and simulator with FPGA synthesis export (Java Swing) | `42_graphics` |
| `lxi-tools` | 2.8 | Drive a bench instrument over ethernet with SCPI, from the lxi command or the lxi-gui window | `44_apps` |
| `mbpoll` | 1.5.4 | Read and write Modbus registers from a prompt, over RTU or TCP | `41_system` |
| `nec2c` | 1.3.3 | NEC-2 antenna simulation from a card deck | `41_system` |
| `nextpnr` | 0.11.1 | Place and route — the second stage of the open FPGA flow | `44_apps` |
| `ngspice` | 47 | SPICE circuit simulation from a prompt — transient, AC, DC, noise, Monte Carlo | `41_system` |
| `openfpgaloader` | 1.1.1 | FPGA programming tool for the boards the FPGA flow targets | `41_system` |
| `prjtrellis` | 1.4.git20260920 | The ECP5 bitstream database and packer | `41_system` |
| `qucs-s` | 26.1.1 | Qucs-S — circuit schematic capture and simulation front end for ngspice, with RF filter, attenuator and line calculators | `44_apps` |
| `sigrok-cli` | 0.7.2 | Command-line capture and protocol decoding for sigrok hardware | `41_system` |
| `sigrok-firmware-fx2lafw` | 0.1.7 | The firmware libsigrok uploads to a Cypress FX2 logic analyser, compiled here with sdcc | `41_system` |
| `surfer` | 0.7.0 | Waveform viewer for VCD, FST and GHW simulation traces, drawn with egui | `42_graphics` |
| `symbiyosys` | 0.69 | Formal property verification over yosys — sby, and the solvers behind it | `44_apps` |
| `verilator` | 5.052 | Verilog simulator and linter | `41_system` |
| `xnec2c` | 4.4.18 | xnec2c — GTK antenna modelling with NEC2: patterns, impedance, gain and SWR over frequency | `44_apps` |
| `yosys` | 0.69 | Verilog synthesis — the first stage of the open FPGA flow | `43_toolkits` |

### embedded

Microcontroller and bare-metal development: cross toolchains, C libraries, flashers, debug probes
and serial consoles. 27 ports, under `ports/core/embedded/`.

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
| `libjaylink` | 0.5.0 | Library to drive SEGGER J-Link debug probes over USB and TCP | `41_system` |
| `libstdcxx-arm-none-eabi` | 16.2.0 | The C++ standard library for arm-none-eabi, built against picolibc | `41_system` |
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
| `stm32flash` | 0.7 | Flash an STM32 over its own ROM bootloader, with no debugger attached | `41_system` |
| `tio` | 3.9 | Serial terminal that reconnects when the device returns | `41_system` |
| `xa` | 2.4.1 | xa65 cross-assembler for the 6502, 65816 and R65C02, with the o65 relocation tools | `41_system` |

### sdr-hw

SDR hardware drivers and the SoapySDR device layer. 18 ports, under `ports/core/sdr-hw/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `airspy` | 1.0.10 | libairspy and the airspy_* tools for the Airspy R2 and Mini receivers | `41_system` |
| `airspyhf` | 1.8.1.git20260722 | libairspyhf and the airspyhf_* tools for the Airspy HF+ Discovery and Dual Port receivers | `41_system` |
| `bladerf` | 2025.10 | libbladeRF and bladeRF-cli for the Nuand bladeRF and bladeRF 2.0 micro transceivers | `41_system` |
| `hackrf` | 2026.01.3 | libhackrf and the hackrf_* tools for the HackRF One, Jawbreaker and rad1o transceivers | `41_system` |
| `libad9361` | 0.4.0 | AD936x transceiver helpers over libiio — filter design and multichip sync for ADALM-Pluto in gr-iio | `41_system` |
| `libfobos` | 2.4.1.git20260618 | libfobos — driver library and fobos_* tools for the RigExpert Fobos SDR receiver | `41_system` |
| `libiio` | 0.26 | Linux Industrial I/O client library — ADALM-Pluto and other ADI SDRs over USB and the network, for gr-iio and SDRangel | `41_system` |
| `libmirisdr` | 2.0.0 | libmirisdr-4 — driver library and miri_sdr and miri_fm tools for Mirics MSi2500/MSi001 receivers | `41_system` |
| `libperseus-sdr` | 0.8.2 | libperseus-sdr — driver library and perseustest for the Microtelecom Perseus HF receiver | `41_system` |
| `librfnm` | 0.2.0.git20240716 | librfnm — host library and rfnm_info for RFNM software-defined radio boards | `41_system` |
| `limesuite` | 23.11.0.git20260603 | LimeSuite — driver library, LimeUtil and the SoapySDR module for LimeSDR boards | `41_system` |
| `rtl-sdr` | 2.0.3 | RTL2832U SDR driver library and the rtl_* tools | `41_system` |
| `soapyairspy` | 0.2.0 | The SoapySDR module for Airspy R2 and Mini receivers, through libairspy | `41_system` |
| `soapybladerf` | 0.4.2 | The SoapySDR module for the bladeRF family, through libbladeRF | `41_system` |
| `soapyhackrf` | 0.3.4 | The SoapySDR module for the HackRF family, through libhackrf | `41_system` |
| `soapyrtlsdr` | 0.3.3 | The SoapySDR module for RTL2832U sticks, through librtlsdr | `41_system` |
| `soapysdr` | 0.8.1 | Vendor-neutral API over SDR hardware | `41_system` |
| `uhd` | 4.10.0.0 | USRP Hardware Driver — Ettus/NI USRP radios for GNU Radio's gr-uhd, with the uhd_* utilities and Python API | `41_system` |

### sdr

SDR DSP libraries, GNU Radio and its blocks, receivers, decoders and signal analysis. 24 ports,
under `ports/core/sdr/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `aptdec` | 1.7.0.git20250920 | libapt — NOAA APT weather-satellite image decoding library, the libaptdec branch SDRangel's APT demodulator links | `41_system` |
| `cm256cc` | 1.1.2 | cm256cc — Cauchy MDS erasure codes over GF(256); SDRangel's remote sink and source | `41_system` |
| `cubicsdr` | 0.2.7 | CubicSDR — software radio receiver with an OpenGL spectrum and waterfall, over SoapySDR and liquid-dsp | `44_apps` |
| `dsdcc` | 1.9.6 | DSDcc — digital voice decoder for D-STAR, DMR, dPMR, NXDN and YSF, and the dsdccx tool | `41_system` |
| `gnuradio` | 3.10.12.0 | GNU Radio — signal-processing blocks for software radio, and GNU Radio Companion to wire them into flow graphs | `43_toolkits` |
| `gqrx` | 2.17.7 | Gqrx — software radio receiver with a spectrum and waterfall, over GNU Radio and gr-osmosdr | `44_apps` |
| `gr-funcube` | 3.10.0.git20260208 | gr-funcube — GNU Radio source and control blocks for the FUNcube Dongle Pro and Pro+ | `43_toolkits` |
| `gr-iqbal` | 0.38.3 | gr-iqbal — GNU Radio blocks that estimate and correct I/Q imbalance, used by gr-osmosdr | `43_toolkits` |
| `gr-osmosdr` | 0.2.6 | GNU Radio source and sink blocks for RTL-SDR, HackRF, Airspy, bladeRF, SoapySDR and network radios | `43_toolkits` |
| `inspectrum` | 0.4.0 | inspectrum — inspect recorded radio signals: spectrogram, cursors, and demodulated traces | `44_apps` |
| `libdab` | 0.8.git20260323 | libdab — the DAB and DAB+ decoder library from dab-cmdline; SDRangel's DAB demodulator | `41_system` |
| `libinmarsatc` | 20260112 | inmarsatc — Inmarsat-C demodulator, decoder and message parser libraries, the fork SDRangel's Inmarsat demodulator links | `41_system` |
| `libosmo-dsp` | 0.5.0 | libosmo-dsp — Osmocom complex-vector DSP and I/Q imbalance estimation | `41_system` |
| `libsigmf` | 20260606 | libsigmf — C++ reader and writer for SigMF signal recordings, with the SDRangel namespace | `41_system` |
| `libvolk` | 3.3.0 | Vector-Optimized Library of Kernels — runtime-dispatched SIMD kernels for GNU Radio and SDR tools | `41_system` |
| `liquid-dsp` | 1.8.3 | liquid-dsp — filters, modems, resamplers and FEC for software radio, in C | `41_system` |
| `mbelib` | 1.3.0 | mbelib — AMBE and IMBE vocoder decoding for digital voice radio | `41_system` |
| `multimon-ng` | 1.6.1 | POCSAG, FLEX, AFSK, DTMF and the rest, demodulated from a stream of samples | `42_graphics` |
| `rtl-433` | 25.12 | Decodes 433/868 MHz transmissions from weather stations, sensors and tyre monitors | `41_system` |
| `satdump` | 1.2.2 | SatDump — receive, demodulate and decode weather and science satellites into images and products | `44_apps` |
| `sdrangel` | 7.27.2 | SDRangel — software radio receiver, transmitter and analyser with decoders for ADS-B, AIS, APRS, FT8, DATV and more | `44_apps` |
| `sdrpp` | 1.3.0.dev.20260704 | SDR++ — a software radio receiver with its own GPU-drawn interface, no toolkit | `44_apps` |
| `serialdv` | 1.1.5 | SerialDV — AMBE3000 hardware vocoder dongles over serial and UDP, and the dvtest tool | `41_system` |
| `urh` | 2.10.0 | Universal Radio Hacker — record, demodulate, decode and replay wireless protocols | `44_apps` |

### hamradio

Amateur-radio operation: rig control, digital modes, logging, packet and APRS, propagation, and mesh
radio. 25 ports, under `ports/core/hamradio/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `ardopcf` | 1.0.4.1.3 | ardopcf — the ARDOP HF sound-card modem that Pat drives for Winlink email over radio | `41_system` |
| `chirp` | 20260927 | CHIRP — program the memory channels of hundreds of amateur radios, with a wxPython interface and the chirpc CLI | `44_apps` |
| `contact` | 1.7.1 | A terminal client for a Meshtastic radio — chat, nodes and settings in curses | `41_system` |
| `direwolf` | 1.8.1 | Software TNC — AX.25, APRS and KISS over a sound card | `41_system` |
| `flamp` | 2.2.14 | flamp — Amateur Multicast Protocol file transfer to many stations at once through fldigi | `44_apps` |
| `fldigi` | 4.2.13 | fldigi — sound-card digital modes for amateur radio (PSK, RTTY, Olivia, MFSK, CW) with flarq | `44_apps` |
| `flmsg` | 4.0.24 | flmsg — amateur radio message forms (ICS, Radiogram, Red Cross) sent through fldigi | `44_apps` |
| `flrig` | 2.0.12 | flrig — transceiver control over CAT for amateur radio, and the rig server fldigi talks to | `44_apps` |
| `flxmlrpc` | 1.0.1 | flxmlrpc — the XML-RPC library shared by fldigi, flmsg and flamp | `41_system` |
| `freedv-gui` | 2.4.0 | FreeDV — HF digital voice for amateur radio: RADE, 700D, 700E and 1600 modes over a sound card | `44_apps` |
| `ggmorse` | 0.1.0.git20250920 | ggmorse — Morse code decoding library, the fork SDRangel's Morse decoder links | `41_system` |
| `hamlib` | 4.7.2 | Radio and rotator control library | `41_system` |
| `js8call` | 3.0.3 | JS8Call — weak-signal keyboard-to-keyboard and store-and-forward messaging for HF amateur radio | `44_apps` |
| `klog` | 2.6 | KLog — amateur radio logbook with DXCC and award tracking, ADIF, LoTW and a DX cluster | `44_apps` |
| `meshtastic-firmware` | 2.7.26 | Meshtastic radio firmware images, to flash a LoRa node with no network | `41_system` |
| `nomadnet` | 1.4.3 | Nomad Network — messages, pages and files over a Reticulum mesh, in a terminal | `42_graphics` |
| `pat` | 1.0.0 | Pat — a Winlink email client for amateur radio, over ARDOP, AX.25 packet and telnet | `41_system` |
| `qsstv` | 9.5.8 | QSSTV — slow-scan television and digital image modes (HamDRM) for amateur radio | `44_apps` |
| `rnode-firmware` | 1.86 | RNode LoRa radio firmware images, for rnodeconf to flash with no network | `41_system` |
| `sideband` | 2.1.0 | Sideband — LXMF messaging, voice, telemetry and maps over a Reticulum mesh | `42_graphics` |
| `splat` | 1.4.2 | Terrain-aware VHF/UHF path loss and coverage over Longley-Rice | `41_system` |
| `tlf` | 1.4.1 | Curses contest logger with hamlib and Cabrillo output | `41_system` |
| `voacapl` | 0.7.7 | HF propagation prediction (VOACAP) | `41_system` |
| `wsjtx` | 3.0.1 | WSJT-X — FT8, FT4, JT65, Q65, WSPR and other weak-signal digital modes for amateur radio | `44_apps` |
| `xastir` | 2.2.4 | Xastir — APRS mapping and messaging over TNCs, AX.25 and APRS-IS (Motif, under Xwayland) | `44_apps` |

### ai

Machine learning inference, speech recognition, computer vision and neural translation. 7 ports,
under `ports/core/ai/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `libggml` | 0.25.3 | ggml, the tensor library under llama.cpp and whisper.cpp: CPU variants, OpenBLAS and Vulkan backends as loadable modules | `42_graphics` |
| `llama.cpp` | 0.5.0 | Local language models on the CPU or a Vulkan GPU: llama-cli, llama-server and the GGUF tools, with nothing sent anywhere | `42_graphics` |
| `onnxruntime` | 1.30.0 | ONNX Runtime: inference for ONNX models on the CPU, as a C/C++ library and a Python module | `41_system` |
| `opencv` | 4.14.0 | OpenCV — computer vision and machine-learning library, with the tracking, optical-flow and extended image-processing contrib modules and Python bindings | `43_toolkits` |
| `translatelocally` | 0.0.2.20250330 | translateLocally: machine translation on the CPU with Bergamot models, with the text never leaving the machine | `44_apps` |
| `whisper-model-base-en` | 20241029 | The Whisper base.en speech model in ggml form, for whisper.cpp and kdos-rec | `41_system` |
| `whisper.cpp` | 1.9.4 | Speech to text on the CPU or a Vulkan GPU, with the model on the disk and nothing sent anywhere | `42_graphics` |

### python

The python3 interpreter, tkinter, and Python build, packaging and extension tooling. 36 ports, under
`ports/core/python/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `python3` | 3.14.7 | The Python 3 interpreter and standard library; `tkinter` is `python3-tkinter` | `30_foundation`, named again by `40_lang` |
| `python3-calver` | 2025.10.20 | Date-based versions for setuptools — trove-classifiers builds with it | `30_foundation` |
| `python3-cffi` | 2.1.1 | Foreign function interface for calling C from Python | `40_lang` |
| `python3-cppy` | 1.3.1 | C++ headers for writing CPython extensions — kiwisolver builds with it | `40_lang` |
| `python3-cython` | 3.3.0 | The C extension compiler numpy is written against | `40_lang` |
| `python3-extension-helpers` | 1.5.0 | The setuptools glue astropy compiles its C extensions through | `40_lang` |
| `python3-flit-core` | 4.1.0 | The minimal PEP 517 backend packaging itself builds with | `30_foundation` |
| `python3-hatch-jupyter-builder` | 0.10.0 | hatch-jupyter-builder — the hatchling hook that builds and checks Jupyter front-end assets | `40_lang` |
| `python3-hatch-nodejs-version` | 0.4.0 | hatch-nodejs-version — hatchling plugins that read version and metadata from package.json | `40_lang` |
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
| `python3-pyqt-builder` | 1.19.1 | PyQt-builder — the sip-build backend every PyQt6 binding is configured with | `40_lang` |
| `python3-scikit-build-core` | 1.0.3 | The PEP 517 backend for CMake projects — pybind11, nanobind, pikepdf and CoolProp build through it | `40_lang` |
| `python3-semantic-version` | 2.10.0 | Semantic-version parsing — what setuptools-rust compares against | `40_lang` |
| `python3-setuptools` | 84.0.0 | Library for building, packaging and installing Python projects | `30_foundation` |
| `python3-setuptools-rust` | 1.13.0 | Builds a Rust extension from setup.py — how maturin bootstraps itself | `40_lang` |
| `python3-setuptools-scm` | 10.3.4 | Version from VCS — matplotlib requires it at build time | `30_foundation` |
| `python3-sip` | 6.16.1 | SIP — the binding generator PyQt6, QScintilla and QGIS are built with | `40_lang` |
| `python3-tkinter` | 3.14.7 | tkinter — Python's binding to Tk, the _tkinter module and the tkinter package (X11 under Xwayland) | `42_graphics` |
| `python3-tomlkit` | 0.15.1 | Style-preserving TOML reading and writing — hatchling requires it | `30_foundation` |
| `python3-trove-classifiers` | 2026.9.21.13 | The canonical PyPI classifier list hatchling validates metadata against | `30_foundation` |
| `python3-vcs-versioning` | 2.5.0 | The VCS version machinery setuptools-scm is built on | `30_foundation` |
| `python3-versioneer` | 0.29 | Derives a package version from git metadata — a build-time import pandas needs | `40_lang` |
| `python3-wheel` | 0.48.0 | The reference implementation of the Python wheel format | `40_lang` |

### python-libs

General-purpose python3-* runtime libraries: text, templating, markup, parsing, config, CLI, system,
files, crypto, imaging. 63 ports, under `ports/core/python-libs/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `python3-attrs` | 26.1.0 | Classes without boilerplate: declared attributes, validators and converters for Python | `40_lang` |
| `python3-bcrypt` | 5.0.0 | bcrypt — the bcrypt password hash for Python, in Rust | `40_lang` |
| `python3-click` | 8.5.0 | Composable command-line interfaces for python programs | `40_lang` |
| `python3-click-log` | 0.4.0 | Logging integration for click command-line programs | `40_lang` |
| `python3-configobj` | 5.0.9 | INI-style configuration files with validation, for python programs | `40_lang` |
| `python3-cryptography` | 50.0.1 | Cryptographic primitives and recipes for Python | `40_lang` |
| `python3-cssselect` | 1.5.0 | CSS selectors translated to XPath — Inkscape's extension library selects with it | `40_lang` |
| `python3-cups` | 2.0.4 | pycups — libcups from Python, for system-config-printer | `41_system` |
| `python3-dateutil` | 2.9.0.post0 | Date parsing, relative deltas and time zones — pandas and matplotlib import it | `40_lang` |
| `python3-decorator` | 5.3.1 | decorator — signature-preserving function decorators for Python | `40_lang` |
| `python3-defusedxml` | 0.7.1 | defusedxml — XML parsing hardened against entity expansion attacks | `40_lang` |
| `python3-distro` | 1.9.0 | distro — Linux distribution identification from os-release for Python | `40_lang` |
| `python3-docutils` | 0.23 | Set of tools for processing plaintext docs into formats such as HTML, XML, or LaTeX | `30_foundation` |
| `python3-ecdsa` | 0.19.2 | Pure-python ECDSA and EdDSA signatures — adafruit-nrfutil signs nRF52 firmware packages with it | `40_lang` |
| `python3-glad2` | 2.0.8 | OpenGL and EGL loader generator — writes libplacebo's GL loader at build time | `40_lang` |
| `python3-isodate` | 0.7.2 | isodate — ISO 8601 date, time and duration parsing and formatting for Python | `40_lang` |
| `python3-jellyfish` | 1.2.1 | jellyfish — approximate and phonetic string matching, in Rust | `40_lang` |
| `python3-jinja2` | 3.1.6 | The template engine libplacebo generates its shader sources with | `30_foundation` |
| `python3-jsonschema` | 4.26.0 | JSON Schema validation for Python | `40_lang` |
| `python3-jsonschema-specifications` | 2025.9.1 | The JSON Schema meta-schemas and vocabularies, packaged for jsonschema | `40_lang` |
| `python3-keyring` | 25.7.0 | keyring — Python access to the desktop's Secret Service, with the keyring command | `40_lang` |
| `python3-lark` | 1.3.1 | Lark — a parsing toolkit for context-free grammars, under JupyterLab's RFC 3987 validator | `40_lang` |
| `python3-libvirt` | 12.7.0 | Python binding of the libvirt API — what virt-manager and virt-install drive libvirt through | `43_toolkits` |
| `python3-lxml` | 6.1.3 | libxml2 and libxslt bindings for Python | `40_lang` |
| `python3-mako` | 1.4.3 | Templating library for Python | `40_lang` |
| `python3-markdown` | 3.11 | Python-Markdown — Markdown to HTML conversion for Python | `40_lang` |
| `python3-markdown-it-py` | 4.2.0 | CommonMark parser for python — what rich renders Markdown with | `40_lang` |
| `python3-markupsafe` | 3.0.3 | Markup-safe string type for XML, HTML and XHTML in Python | `30_foundation` |
| `python3-mdurl` | 0.1.2 | URL parsing and formatting for markdown-it-py | `40_lang` |
| `python3-openpyxl` | 3.1.5 | Read and write xlsx from python — what visidata opens one with | `42_graphics` |
| `python3-orjson` | 3.12.0 | orjson — JSON for Python, written in Rust | `40_lang` |
| `python3-pathvalidate` | 3.3.1 | Validates and sanitises file names and paths for every platform's rules | `40_lang` |
| `python3-pefile` | 2024.8.26 | Read and rewrite the headers of a PE executable, the format of every EFI binary | `40_lang` |
| `python3-pikepdf` | 10.13.0.post1 | qpdf bindings — read and rewrite a PDF without rasterising it | `42_graphics` |
| `python3-pillow` | 12.3.0 | Python imaging library | `42_graphics` |
| `python3-pillow-heif` | 1.8.0 | HEIF and AVIF for Pillow, through libheif | `42_graphics` |
| `python3-platformdirs` | 4.11.12 | Where a python program's config, cache and data directories are | `40_lang` |
| `python3-ply` | 3.11 | Lex and yacc for python — libcamera generates its control tables with it | `40_lang` |
| `python3-protobuf` | 7.36.2 | Protocol Buffers runtime for Python, with the upb C extension | `40_lang` |
| `python3-psutil` | 7.2.2 | Processes, CPU, memory, disks and network from python | `40_lang` |
| `python3-psycopg2` | 2.9.13 | psycopg2 — the PostgreSQL adapter for Python, over libpq | `41_system` |
| `python3-pydantic-core` | 2.46.5 | pydantic's validation core, in Rust — pinned to the version pydantic names | `40_lang` |
| `python3-pyemf3` | 3.3 | pyemf3 — pure-Python Enhanced Metafile writer, for Veusz's EMF export | `40_lang` |
| `python3-pygments` | 2.21.0 | Python syntax highlighter | `30_foundation` |
| `python3-pyparsing` | 3.3.3 | Grammar-based text parsing — matplotlib's mathtext and fontconfig patterns | `40_lang` |
| `python3-pypdfium2` | 5.13.0 | Python bindings to PDFium — render, read and edit a PDF from Python | `42_graphics` |
| `python3-pytz` | 2026.4 | The Olson time zone database for python datetime | `40_lang` |
| `python3-pyxdg` | 0.28 | pyxdg — freedesktop.org base directories, desktop entries, menus and icon themes in Python | `41_system` |
| `python3-qrcode` | 8.2 | QR code generator for python, as text or as an image | `42_graphics` |
| `python3-referencing` | 0.37.0 | JSON reference resolution across JSON Schema dialects, under jsonschema | `40_lang` |
| `python3-rich` | 15.0.0 | Colour, tables and progress bars in a terminal, for python programs | `40_lang` |
| `python3-rpds-py` | 2026.6.3 | rpds-py — persistent data structures in Rust, under jsonschema's referencing | `40_lang` |
| `python3-scour` | 0.38.2 | SVG optimiser — the scour command and Inkscape's Optimized SVG output | `40_lang` |
| `python3-send2trash` | 2.1.0 | Send files to the freedesktop.org trash instead of deleting them | `40_lang` |
| `python3-six` | 1.17.0 | Python 2 and 3 compatibility shims — python-dateutil imports it | `40_lang` |
| `python3-sphinx` | 9.1.0 | Documentation generator — the manual pages of LLVM, CMake, mpd and flashrom | `30_foundation` |
| `python3-tinycss2` | 1.5.1 | CSS parser for Python — Inkscape's extension library reads style with it | `40_lang` |
| `python3-typing-extensions` | 4.16.0 | Backported and experimental type hints for Python's typing module | `40_lang` |
| `python3-uharfbuzz` | 0.56.2 | Cython bindings to HarfBuzz — text shaping from Python | `42_graphics` |
| `python3-wcwidth` | 0.9.1 | The column width of a Unicode string in a terminal | `40_lang` |
| `python3-yaml` | 6.0.3 | Python bindings for YAML, using libyaml | `30_foundation` |
| `python3-yapps` | 2.2.0 | Yapps — Yet Another Python Parser System, the yapps2 LL(1) parser generator | `40_lang` |
| `python3-zstandard` | 0.25.0 | python-zstandard — Python bindings for the zstd library | `40_lang` |

### python-net

python3-* HTTP, web-framework, TLS-trust and network-stack modules. 22 ports, under
`ports/core/python-net/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `python3-asgiref` | 3.12.1 | ASGI specification helpers and sync/async adapters for Python | `40_lang` |
| `python3-beautifulsoup4` | 4.15.0 | Beautiful Soup — lenient HTML and XML parsing, for nbconvert and toot | `40_lang` |
| `python3-blinker` | 1.9.0 | In-process object-to-object and broadcast signalling for Python | `40_lang` |
| `python3-certifi` | 2026.7.22 | The CA bundle python libraries look for by name | `30_foundation` |
| `python3-charset-normalizer` | 3.5.1 | Character encoding detection for undeclared text | `40_lang` |
| `python3-flask` | 3.1.3 | Flask, a WSGI web application framework for Python | `40_lang` |
| `python3-flask-cors` | 6.0.5 | Cross-origin resource sharing (CORS) headers for Flask applications | `40_lang` |
| `python3-html5lib` | 1.1 | An HTML parser following the WHATWG HTML specification, for Python | `40_lang` |
| `python3-httplib2` | 0.32.0 | An HTTP client with caching, keep-alive and digest authentication | `40_lang` |
| `python3-idna` | 3.20 | Internationalised domain names in applications (IDNA 2008 and UTS #46) | `40_lang` |
| `python3-itsdangerous` | 2.2.0 | Cryptographically signed serialisation of data for passing through untrusted channels | `40_lang` |
| `python3-lxmf` | 1.1.1 | LXMF — store-and-forward messaging over Reticulum, and the lxmd propagation node | `40_lang` |
| `python3-meshtastic` | 2.7.11 | Meshtastic LoRa mesh client — text and position messages | `40_lang` |
| `python3-pysocks` | 1.7.1 | SOCKS4 and SOCKS5 proxy client module for Python sockets and urllib | `41_system` |
| `python3-requests` | 2.34.2 | HTTP client library for Python | `40_lang` |
| `python3-rns` | 1.5.4 | Reticulum — encrypted mesh networking over LoRa, packet radio, WiFi or anything that carries bytes | `40_lang` |
| `python3-soupsieve` | 2.10 | Soup Sieve — CSS selectors for Beautiful Soup | `40_lang` |
| `python3-truststore` | 0.10.4 | Verify TLS against the system certificate store through the ssl module | `40_lang` |
| `python3-urllib3` | 2.8.0 | HTTP client with connection pooling, retries and TLS verification | `40_lang` |
| `python3-waitress` | 3.0.2 | Waitress, a pure-Python production WSGI server | `40_lang` |
| `python3-webencodings` | 0.6.1 | The WHATWG Encoding standard's labels and legacy decoders, for Python | `40_lang` |
| `python3-werkzeug` | 3.1.9 | Werkzeug, the WSGI toolkit under Flask: request, response, routing and a development server | `40_lang` |

### python-sci

python3-* science, numerics, astronomy and geo modules. 13 ports, under `ports/core/python-sci/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `python3-astropy-iers-data` | 0.2026.9.21.0.56.25 | The IERS Earth-orientation and leap-second tables astropy reads, frozen at release | `40_lang` |
| `python3-contourpy` | 1.4.0 | Contour lines and filled contours over a 2D grid — matplotlib's contouring | `41_system` |
| `python3-cycler` | 0.12.1 | Composable style cycles — matplotlib's property cycle | `40_lang` |
| `python3-gmpy2` | 2.3.1 | Multiple-precision integers, rationals, reals and complex numbers from python, through GMP, MPFR and MPC | `40_lang` |
| `python3-h5py` | 3.16.0 | h5py — HDF5 files from Python as numpy arrays, for Veusz's HDF5 import | `41_system` |
| `python3-iminuit` | 2.33.0 | iminuit — Minuit2 minimiser and error analysis from Python, for Veusz's fitting | `41_system` |
| `python3-jplephem` | 2.24 | Reads JPL ephemeris kernels for planetary positions | `41_system` |
| `python3-kiwisolver` | 1.5.1 | Cassowary constraint solver — matplotlib's layout engine | `40_lang` |
| `python3-mpmath` | 1.4.1 | Arbitrary-precision floating point — sympy computes numerically through it | `40_lang` |
| `python3-owslib` | 0.36.0 | OWSLib — OGC web service client (WMS, WFS, WCS, CSW, OGC API), for QGIS MetaSearch | `40_lang` |
| `python3-pyerfa` | 2.0.1.5 | The python binding for ERFA — fundamental astronomy, and astropy's floor | `41_system` |
| `python3-sgp4` | 2.27 | SGP4 satellite orbit propagation from a TLE | `41_system` |
| `python3-shapely` | 2.1.2 | Shapely — planar geometry (points, lines, polygons and their set operations) over GEOS, for Python | `41_system` |

### python-gui

python3-* GUI toolkit, desktop and D-Bus bindings. 15 ports, under `ports/core/python-gui/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `python3-cairo` | 1.29.1 | pycairo — cairo drawing for Python, and the cairo half of PyGObject | `42_graphics` |
| `python3-dasbus` | 1.7 | dasbus — a D-Bus library for Python on top of GLib's GDBus, which Orca talks through | `42_graphics` |
| `python3-dbus` | 1.5.0 | dbus-python — the libdbus bindings system-config-printer and PyQt6's D-Bus main loop use | `41_system` |
| `python3-gobject` | 3.58.0 | PyGObject — GLib, GTK and every introspected library, from Python | `42_graphics` |
| `python3-kivy` | 2.3.1 | Kivy — a python UI framework drawing with OpenGL through SDL2 | `42_graphics` |
| `python3-opengl` | 3.1.10 | PyOpenGL — the OpenGL, GLU and GLUT bindings for Python, loaded through ctypes | `42_graphics` |
| `python3-pyqt5` | 5.15.11 | PyQt5 — the Qt 5 bindings GNU Radio's gr-qtgui and the Qt 5 Python tools are written in | `43_toolkits` |
| `python3-pyqt5-sip` | 12.19.0 | PyQt5.sip — the runtime module every PyQt5 extension is linked through | `41_system` |
| `python3-pyqt6` | 6.11.0 | PyQt6 — the Qt 6 bindings Calibre, Anki, QGIS, Picard and git-cola are written in | `43_toolkits` |
| `python3-pyqt6-sip` | 13.12.0 | PyQt6.sip — the runtime module every PyQt6 extension is linked through | `41_system` |
| `python3-pyqt6-webengine` | 6.11.0 | PyQt6-WebEngine — QtWebEngine from Python, for the Calibre viewer, Anki and Spyder | `43_toolkits` |
| `python3-qtpy` | 2.4.3 | QtPy — one Qt import layer over PyQt6, for git-cola and Spyder | `43_toolkits` |
| `python3-urwid` | 4.1.7 | Console user interface library for python | `41_system` |
| `python3-wxpython` | 4.2.5 | wxPython — the wxWidgets GTK 3 toolkit for Python programs such as CHIRP and Pronterface | `43_toolkits` |
| `python3-xlib` | 0.33 | python-xlib — the X11 client protocol implemented in pure Python | `41_system` |

### python-dev

python3-* Jupyter, kernel, language-server and debugger modules. 13 ports, under
`ports/core/python-dev/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `python3-cloudpickle` | 3.1.2 | cloudpickle — pickling of functions and classes, which Spyder's kernel sends variables with | `40_lang` |
| `python3-comm` | 0.2.3 | comm — the Jupyter comm protocol for kernel-side widgets | `41_system` |
| `python3-debugpy` | 1.8.22 | debugpy — the Debug Adapter Protocol server for Python, behind the debuggers of Spyder and the IPython kernel | `40_lang` |
| `python3-ipykernel` | 6.31.0 | ipykernel — the IPython kernel for Jupyter, under JupyterLab and Spyder | `41_system` |
| `python3-jupyter-client` | 8.10.0 | jupyter_client — the Jupyter messaging protocol and kernel manager | `41_system` |
| `python3-jupyter-core` | 5.9.1 | jupyter_core — Jupyter's paths, configuration and the jupyter command | `41_system` |
| `python3-jupyterlab-pygments` | 0.3.0 | jupyterlab_pygments — Pygments highlighting in JupyterLab's colours, for nbconvert | `40_lang` |
| `python3-lsp-ruff` | 2.3.4 | python-lsp-ruff — Ruff's diagnostics, fixes and formatting inside the Python language server | `41_system` |
| `python3-lsp-server` | 1.15.0 | python-lsp-server — the Python language server, with Pylint, flake8, Rope, YAPF and Black | `41_system` |
| `python3-nbconvert` | 7.17.1 | nbconvert — notebooks to HTML, PDF, Markdown and scripts, with nbformat and nbclient | `41_system` |
| `python3-nest-asyncio` | 1.6.0 | nest_asyncio — re-entrant asyncio event loops, which ipykernel runs cells in | `40_lang` |
| `python3-pyzmq` | 27.2.0 | pyzmq — Python bindings for ZeroMQ, the transport of Jupyter kernels | `41_system` |
| `python3-tornado` | 6.5.10 | Tornado — the asynchronous web server and networking library under Jupyter | `40_lang` |

### python-hw

python3-* hardware, serial, USB, instrument, CAN and device-protocol modules. 12 ports, under
`ports/core/python-hw/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `python3-adafruit-nrfutil` | 0.5.3.post16 | adafruit-nrfutil — serial DFU flasher for nRF52 boards; rnodeconf flashes RAK4631, T-Echo and Heltec T114 RNodes with it | `40_lang` |
| `python3-cantools` | 44.1.0 | Decode a live candump into named signals, and generate the MCU-side C | `42_graphics` |
| `python3-esptool` | 5.4.0 | Talk to the ESP8266 and ESP32 ROM bootloader over serial | `40_lang` |
| `python3-obd` | 0.7.3 | Reads and clears engine fault codes over an ELM327 OBD-II adapter | `40_lang` |
| `python3-pyarcus` | 5.12.0 | pyArcus — Python bindings for the Arcus socket Cura talks to CuraEngine through | `41_system` |
| `python3-pynest2d` | 5.11.0.alpha0 | pynest2d — Python bindings for libnest2d, the build-plate arranger in Cura | `41_system` |
| `python3-pysavitar` | 5.12.0 | pySavitar — Python bindings for Savitar, Cura's 3MF reader and writer | `41_system` |
| `python3-pyserial` | 3.5 | Serial port access from Python | `40_lang` |
| `python3-pyusb` | 1.3.1 | USB access from python through libusb — what pyvisa-py opens a USB instrument with | `41_system` |
| `python3-pyuvula` | 1.1.0 | pyUvula — UV unwrapping and projection for Cura's paint tool, with its Python module | `41_system` |
| `python3-pyvisa` | 1.16.2 | VISA instrument-automation API for Python, with no NI-VISA dependency | `40_lang` |
| `python3-pyvisa-py` | 0.8.1 | The pure-python VISA backend — USB, TCPIP and serial with no vendor library | `41_system` |

### perl-cpan

The perl interpreter and every perl-* CPAN module. 18 ports, under `ports/core/perl-cpan/`.

| Port | Version | Description | Phase |
|---|---|---|---|
| `perl` | 5.44.0 | The Perl programming language | `30_foundation` |
| `perl-authen-sasl` | 2.2100 | SASL authentication framework for perl | `40_lang` |
| `perl-class-inspector` | 1.36 | Get information about a perl class and its structure | `30_foundation` |
| `perl-crypt-urandom` | 0.55 | The operating system's cryptographic random source for perl | `40_lang` |
| `perl-digest-hmac` | 1.05 | Keyed-hashing for message authentication (HMAC) in perl | `40_lang` |
| `perl-file-homedir` | 1.006 | Find the home and XDG user directories of the current or another user, in pure perl | `40_lang` |
| `perl-file-sharedir` | 1.118 | Locate per-distribution and per-module shared files of perl modules | `30_foundation` |
| `perl-file-sharedir-install` | 0.14 | Install shared files of a perl distribution — the configure-time half of File::ShareDir | `30_foundation` |
| `perl-file-which` | 1.27 | Find the full path of an executable on PATH, in pure perl | `40_lang` |
| `perl-font-ttf` | 1.06 | Read, edit and write TrueType and OpenType font tables from perl | `40_lang` |
| `perl-io-socket-ssl` | 2.099 | TLS sockets for perl, over Net::SSLeay | `40_lang` |
| `perl-io-string` | 1.08 | Emulate a file handle over an in-memory string in perl | `40_lang` |
| `perl-net-ssleay` | 1.96 | OpenSSL bindings for perl | `40_lang` |
| `perl-parse-yapp` | 1.21 | An LALR parser generator in perl — samba builds its IDL compiler with it | `40_lang` |
| `perl-uri` | 5.37 | URI class for Perl | `40_lang` |
| `perl-xml-parser` | 2.59 | Expat-based XML parser module for perl | `30_foundation` |
| `perl-xml-simple` | 2.25 | Perl module that reads and writes XML as nested data structures (config files especially) | not installed |
| `perl-yaml-tiny` | 1.76 | Read and write a subset of YAML in pure perl | `40_lang` |

## KDOS's own programs

These ports are KDOS's own code, filed under `src/` by the part of the system they belong to rather
than on a shelf. Three programs under `src/` have no recipe and are not catalogued:
`src/system/kdos-kpkg`, the package manager, which `10_bootstrap` compiles by script; and
`src/devtools/kdosbuild` and `src/devtools/kdos-portup`, the build orchestrator and the upstream
version checker behind `ports/update`, which run on the build host rather than in the image.
`src/libs` holds the `libk*` libraries these programs compile in; it holds no ports.

### src/system

The system layer: the `kdos` command and its services, packs and boxes, and the installer. On the
`PORT_REPO` of every phase from `40_lang` to `60_kernel`. 5 ports.

| Port | Version | Description | Phase |
|---|---|---|---|
| `kdos-appbox` | 1.2.0 | KDOS alien app runtime and appbox manager | `44_apps` |
| `kdos-boxinit` | 0.1.0 | pid 1 inside a pack box, in place of distrobox-init | `41_system` |
| `kdos-installer` | 4.0 | KDOS installer — the disk wizard, C with no libraries | `10_bootstrap` (by script) |
| `kdos-pack` | 0.1.0 | Build, sign, index and delta packs | `41_system` |
| `kdos-tools` | 1.2.0 | KDOS system tools — kdos, services, getty, screenshots, fetch | `42_graphics` |

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

### src/daemons

The root daemons. On the `PORT_REPO` of `50_desktop` only. 5 ports.

| Port | Version | Description | Phase |
|---|---|---|---|
| `kdos-energyd` | 0.1.0 | Relative per-app Energy Impact from RAPL, attributed by container | `50_desktop` |
| `kdos-mountd` | 0.1.0 | Removable media for a desktop that is not root | `50_desktop` |
| `kdos-oomd` | 0.1.0 | PSI-triggered memory-pressure killer that spares the desktop | `50_desktop` |
| `kdos-packd` | 0.1.0 | Mounts packs, composes box root filesystems, verifies signatures | `50_desktop` |
| `kdos-powerd` | 0.1.0 | Suspend, poweroff and reboot for a desktop that is not root | `50_desktop` |

## Ports named in more than one list

A phase's list may name a port an earlier phase already installed. `kpkg` skips a port whose recipe
is unchanged since it was installed, wherever it is listed, so the later listing rebuilds the port
only when its recipe has changed; the tables above give the phase that installs it first. Most of
these are toolchain and base ports. `20_selfhost` installs the compilers and their tools, and
`30_foundation` names them again so that a changed recipe is rebuilt before anything uses it.
`toybox` is a single binary that provides many small commands, some of which other packages also
provide (`cmp`, `readelf`, `strings`, `gunzip` and others). `40_lang` names it first and the owners
of those names straight after it. A change that compiles a name out of `toybox` bumps the release of
the port that owns it, so the owner rebuilds after `toybox` and the name belongs to the full tool
again (see
[toybox and the tools it overlaps](../03-architecture/packaging.md#toybox-and-the-tools-it-overlaps)).
No other list names a port an earlier phase installs.

| Port | Named by | Installed by |
|---|---|---|
| `attr` | `30_foundation`, `40_lang` | `30_foundation` |
| `autoconf` | `30_foundation`, `40_lang` | `30_foundation` |
| `automake` | `30_foundation`, `40_lang` | `30_foundation` |
| `bash` | `30_foundation`, `40_lang` | `30_foundation` |
| `bc` | `30_foundation`, `40_lang` | `30_foundation` |
| `binutils` | `20_selfhost`, `30_foundation`, `40_lang` | `20_selfhost` |
| `bison` | `30_foundation`, `40_lang` | `30_foundation` |
| `bzip2` | `30_foundation`, `40_lang` | `30_foundation` |
| `diffutils` | `20_selfhost`, `30_foundation`, `40_lang` | `20_selfhost` |
| `elfutils` | `30_foundation`, `40_lang` | `30_foundation` |
| `eudev` | `30_foundation`, `40_lang` | `30_foundation` |
| `findutils` | `30_foundation`, `40_lang` | `30_foundation` |
| `flex` | `30_foundation`, `40_lang` | `30_foundation` |
| `gawk` | `20_selfhost`, `30_foundation` | `20_selfhost` |
| `gcc` | `20_selfhost`, `30_foundation` | `20_selfhost` |
| `gettext` | `30_foundation`, `40_lang` | `30_foundation` |
| `gzip` | `30_foundation`, `40_lang` | `30_foundation` |
| `intltool` | `30_foundation`, `40_lang` | `30_foundation` |
| `kmod` | `30_foundation`, `40_lang` | `30_foundation` |
| `libtool` | `30_foundation`, `40_lang` | `30_foundation` |
| `m4` | `20_selfhost`, `30_foundation` | `20_selfhost` |
| `make` | `30_foundation`, `40_lang` | `30_foundation` |
| `musl` | `20_selfhost`, `30_foundation` | `20_selfhost` |
| `ncurses` | `30_foundation`, `40_lang` | `20_selfhost` |
| `pkgconf` | `30_foundation`, `40_lang` | `30_foundation` |
| `python3` | `30_foundation`, `40_lang` | `30_foundation` |
| `readline` | `30_foundation`, `40_lang` | `20_selfhost` |
| `sed` | `30_foundation`, `40_lang` | `30_foundation` |
| `tar` | `20_selfhost`, `30_foundation` | `20_selfhost` |
| `texinfo` | `30_foundation`, `40_lang` | `30_foundation` |
| `toybox` | `30_foundation`, `40_lang` | `30_foundation` |
| `util-linux` | `30_foundation`, `40_lang` | `30_foundation` |
| `xz` | `30_foundation`, `40_lang` | `30_foundation` |
| `zlib` | `20_selfhost`, `30_foundation` | `20_selfhost` |

## Not installed

Nothing in any list reaches these 5 recipes through `depends =`, and no phase script builds them, so
none is on the image. Their sources are fetched and checked like any other port's, and each builds
on request with `kpkg install <name>` wherever the ports tree is on `PORT_REPO`, as it is inside the
build chroot.

| Port | Shelf | Version | Description |
|---|---|---|---|
| `helix` | `editors` | 25.07.1 | helix — modal text editor (Rust) |
| `icon-naming-utils` | `themes` | 0.8.90 | Perl script used for maintaining backwards compatibility with current desktop icon themes |
| `musl-locales` | `toolchain` | 20260425 | A locale command and message catalogues for musl |
| `perl-xml-simple` | `perl-cpan` | 2.25 | Perl module that reads and writes XML as nested data structures (config files especially) |
| `setconf` | `buildtools` | 0.7.7 | Utility for changing settings in configuration files |

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
