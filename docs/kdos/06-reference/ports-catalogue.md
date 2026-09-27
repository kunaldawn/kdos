# The ports catalogue

This chapter lists every port KDOS can build: each of the 1,014 recipes in `ports/core` and the
24 recipes KDOS writes itself under `src/`, 1,038 in all, each exactly once. It is a reference for
anyone who needs to know whether a piece of software is in the tree, which version it is at, and
where in the build it arrives. Read [Packaging](../03-architecture/packaging.md) first for what a
port and a package are, and [How KDOS is built](../05-developer/how-kdos-is-built.md) for what the
phases do and how their package lists are organised;
[The build system](../05-developer/build-system.md#phases) covers how a phase runs.

## How the catalogue is organised

A **port** is a directory holding a recipe: a `kpkgbuild` file of declarative metadata (name,
version, sources, their hashes, a one-line description, dependencies) and a `build.sh` script that
compiles the unpacked source. `kpkg` turns a port into a **package**, the archive it installs.
Upstream software lives under `ports/core/<name>/`. KDOS's own programs live under
`src/packages/<name>/` and `src/desktop/<name>/`, and their code sits in the port directory itself;
[Packaging](../03-architecture/packaging.md#the-three-repositories) explains the three
repositories and the order they are searched in. To find a port's recipe, open
`ports/core/<name>/kpkgbuild`, or `src/packages/<name>/kpkgbuild` or `src/desktop/<name>/kpkgbuild`
for a name that begins with `kdos-` or is `xdg-desktop-portal-kdos`. On a running system,
`kpkg info <name>` prints an installed package's version, release and file count. Where the ports
tree is present (inside the build chroot, or with `PORT_REPO` pointed at a checkout's
`ports/core`), it also prints an uninstalled port's recipe description and dependencies.

The build installs ports in **phases**, directories under `script/` run in sorted order. Five of
them install from a **package list**, a `packages.txt` naming the ports that phase wants, one per
line. Inside a list, after the file's banner, a three-line comment block (a dashed line, a
title, another dashed line) starts a **package-list group**, or list group for short: a named
section of related ports such as "Core Services" or "Modern CLI tools (Rust / Go)" (see *group* in
the [glossary](glossary.md#terms)). List groups exist for the reader; the build ignores them. Each
port's `depends =` line names the ports it needs, and the build installs those first, so a list
names far fewer ports than a phase installs.

The recipe key `group =` is unrelated to list groups. It is read only by `ports/update`, the
upstream version checker, which offers the version bumps of ports in one group together, all or
none. Four recipes set it: `glib` and `glib-introspection` carry `group = glib`, and
`gcc-arm-none-eabi` and `libstdcxx-arm-none-eabi` carry `group = gcc-arm-none-eabi`. For any other
port the checker derives a group only when its first source is on GitHub, Codeberg, sr.ht or a
GitLab host. That group is keyed on the forge organisation and the version, so sibling projects
that release together are offered together. A port from any other host has no group and is
reviewed on its own. See
[The group key](../03-architecture/packaging.md#the-group-key) and
[Checking for new versions](../05-developer/writing-ports.md#checking-for-new-versions).

The catalogue follows the build:

1. Phases 0 and 1 build from scripts rather than lists; this chapter catalogues one port under
   them, `kdos-installer`.
2. Each list phase follows, in build order, with one table per list group, in the order the groups
   appear in the file. Each group heading is the group's title exactly as the list writes it, so it
   can be found with grep. Ports that come before a list's first group sit under a descriptive
   heading that begins "Unheaded", or under the phase heading alone. A port named by more than
   one list is catalogued under the first list and group that names it;
   [Ports named in more than one list](#ports-named-in-more-than-one-list) gives every place
   each such port appears.
3. [Installed as dependencies](#installed-as-dependencies) catalogues the ports no list names that
   the build installs because a listed port depends on them, by what they are for.
4. [Not installed](#not-installed) catalogues the recipes nothing reaches.

Within each table, ports are in alphabetical order. The version is the recipe's `version =` value
and the description is its `description =` value, reworded where that line praises the software,
carries a typo or an aside, or does not plainly say what the software is.

## Counts

Counted from the five `packages.txt` files and the 1,038 `kpkgbuild` files. "Named" counts the
non-comment lines of a list; "catalogued here" counts the ports whose first listing is in that
phase.

| Where | Named | Catalogued here |
|---|---|---|
| Phases 0 and 1 (scripts) | none | 1 |
| [Phase 2: the self-hosting bootstrap](#phase-2-the-self-hosting-bootstrap) | 8 | 8 |
| [Phase 3: toolchain and core libraries](#phase-3-toolchain-and-core-libraries) | 98 | 90 |
| [Phase 4: userland and the Wayland base](#phase-4-userland-and-the-wayland-base) | 697 | 666 |
| [The desktop phase](#the-desktop-phase) | 22 | 22 |
| [The kernel phase](#the-kernel-phase) | 1 | 1 |
| [Installed as dependencies](#installed-as-dependencies) | none | 236 |
| [Not installed](#not-installed) | none | 14 |
| Total | 826 | 1,038 |

The five lists name 787 distinct ports in 826 lines; 37 ports are named in more than
one list. Following `depends =` from those 787 names reaches 1,023 ports: all
787 listed ports and 236 more. With `kdos-installer`, which phase 1 builds by script,
that leaves 14 recipes in `ports/core` that no phase installs.

Ports per list group, counting each port under its first listing only:

| Phase | List group | Ports |
|---|---|---|
| Phase 2 | (unheaded) | 8 |
| Phase 3 | Build Toolchain | 7 |
| Phase 3 | Modern Build Systems | 3 |
| Phase 3 | Languages & Runtimes | 23 |
| Phase 3 | Alternative compilers / linkers | 9 |
| Phase 3 | Essential Utilities | 12 |
| Phase 3 | Network & Security | 6 |
| Phase 3 | Base Libraries & Database | 7 |
| Phase 3 | System Infrastructure | 18 |
| Phase 3 | Documentation & Spec Tooling | 5 |
| Phase 4 | Core Build Utilities (host-side) | 24 |
| Phase 4 | Core Services | 9 |
| Phase 4 | Network / SSH / Audio / Bluetooth / Print | 9 |
| Phase 4 | Power-User CLI base | 35 |
| Phase 4 | Modern CLI tools (Rust / Go) | 33 |
| Phase 4 | Archives, duplicates and recovery | 10 |
| Phase 4 | The offline library: index it, search it, serve it | 8 |
| Phase 4 | Secrets, another operating system, and the memory under all of it | 15 |
| Phase 4 | Putting firmware on a chip, and talking to it while it runs | 4 |
| Phase 4 | Cross toolchains — native, because flashing a board must not need podman | 21 |
| Phase 4 | Buses — I2C, CAN and MQTT | 9 |
| Phase 4 | Software radio | 11 |
| Phase 4 | Position | 2 |
| Phase 4 | The bench — instruments, protocols and a calculator that parses units | 8 |
| Phase 4 | Raw pixels, curves, and sound with no window | 13 |
| Phase 4 | Words, notes, and where the time went | 4 |
| Phase 4 | Playing, reading, scanning and keeping a record | 17 |
| Phase 4 | Mail — fetched, indexed once, read by anything | 28 |
| Phase 4 | Sequences, coordinates, and exact arithmetic | 13 |
| Phase 4 | The numeric foundation — everything in C2 stands on these | 6 |
| Phase 4 | The python numeric stack | 43 |
| Phase 4 | Astronomy — the sky, in the formats telescopes write | 4 |
| Phase 4 | Sequences — the bioinformatics core | 4 |
| Phase 4 | Synthesise, place, route and flash — the open FPGA flow, natively | 31 |
| Phase 4 | Grammars compiled here, a Rust server that can see std, and a private CA | 4 |
| Phase 4 | Two people talking, and two machines sharing a filesystem | 5 |
| Phase 4 | Inside the kernel, on the air, and against your own hashes | 7 |
| Phase 4 | The wire, the database, the web server, and reading the screen aloud | 9 |
| Phase 4 | Language servers, a diff you can read, and a repository you can undo | 7 |
| Phase 4 | Statistics, census, HTTP, bandwidth, and example-first help | 5 |
| Phase 4 | One index over the corpus | 6 |
| Phase 4 | Keeping the bytes, and moving them between two machines | 10 |
| Phase 4 | The machine's own hardware, its GPUs and its damaged disks | 6 |
| Phase 4 | Is the hardware dying, and can this machine reach it | 25 |
| Phase 4 | Reading a repository, a log, and a code on a screen | 8 |
| Phase 4 | Editors / TUI | 4 |
| Phase 4 | Debug / trace / profile | 7 |
| Phase 4 | Network tools | 13 |
| Phase 4 | Wayland base | 48 |
| Phase 4 | Media & Imaging | 48 |
| Phase 4 | Audio from a prompt | 5 |
| Phase 4 | PostScript and PDF | 10 |
| Phase 4 | Scanning, and what is inside a media file | 8 |
| Phase 4 | ASCII art (aa-project) | 5 |
| Phase 4 | Container Layer (Podman + distrobox) | 14 |
| Phase 4 | X11 compatibility (Xwayland only — no Xorg server) | 5 |
| Phase 4 | Firmware (regulatory database, Intel SOF audio) | 2 |
| Phase 4 | Time zones | 1 |
| Phase 4 | Device access libraries (HID, USB) | 2 |
| Phase 4 | Colour management and codecs | 41 |
| Desktop | (unheaded) | 7 |
| Desktop | The resource monitor | 15 |
| Kernel | (unheaded) | 1 |

## Phases 0 and 1: built by script

Phase 0 (`script/00_toolchain`) builds the cross binutils and gcc in the build container, and phase 1
(`script/01_phase1`) cross-compiles the base userland into the new root filesystem. Neither phase
reads a package list or runs `kpkg`: each step is a script that unpacks a port's source with
`extract_port_source` and builds it by hand. What those steps build is therefore not recorded as a
package, and the same ports are installed again as packages by a later phase. Each is catalogued
under the list that names it, or under [Installed as dependencies](#installed-as-dependencies)
(`gmp`, `mpfr`, `mpc`) when no list does.

| Step | Sources used |
|---|---|
| `00_toolchain/00_binutils.sh` | `binutils` |
| `00_toolchain/01_gcc.sh` | `gcc`, `gmp`, `mpfr`, `mpc` |
| `01_phase1/01_linux_headers.sh` | `linux` |
| `01_phase1/02_musl_libc.sh` | `musl` |
| `01_phase1/03_libstdc++.sh` | `gcc` |
| `01_phase1/04_ncurses.sh` | `ncurses` |
| `01_phase1/05_xz.sh` | `xz` |
| `01_phase1/06_gzip.sh` | `gzip` |
| `01_phase1/06_tar.sh` | `tar` |
| `01_phase1/06_toybox.sh` | `toybox` |
| `01_phase1/07_readline.sh` | `readline` |
| `01_phase1/08_bash.sh` | `bash` |
| `01_phase1/09_binutils.sh` | `binutils` |
| `01_phase1/10_gcc.sh` | `gcc`, `gmp`, `mpfr`, `mpc` |
| `01_phase1/11_make.sh` | `make` |

Two of KDOS's own programs are built here from `src/`, because nothing can read a recipe before
`kpkg` exists. `12_kpkg.sh` compiles the package manager from `src/packages/kdos-kpkg`, which has no
recipe and is not catalogued. `13_kinstall.sh` compiles the installer from the sources of the one
port this section catalogues:

| Port | Version | Description |
|---|---|---|
| `kdos-installer` | 4.0 | KDOS installer — the disk wizard, C with no libraries |

## Phase 2: the self-hosting bootstrap

`script/02_phase2/packages.txt` has no list groups. It rebuilds, inside the chroot and as
packages, the tools the rest of the build compiles with. After it the system is **self-hosting**:
its compiler and C library were compiled on KDOS, and every later port is built with them. Every
port it names is named again by phase 3 or 4.

| Port | Version | Description |
|---|---|---|
| `binutils` | 2.47 | A collection of binary tools |
| `diffutils` | 3.12 | Utility programs for comparing files |
| `gawk` | 5.4.1 | GNU awk pattern scanning and processing language |
| `gcc` | 16.2.0 | The GNU compiler collection |
| `m4` | 1.4.21 | GNU implementation of the traditional Unix macro processor |
| `musl` | 1.2.6 | Musl C library |
| `tar` | 1.35 | Tar utility |
| `zlib` | 1.3.2 | Compression library implementing the deflate compression method |

## Phase 3: toolchain and core libraries

`script/03_phase3/packages.txt` builds, on the self-hosting base phase 2 leaves, every compiler,
build system, language runtime and library needed to rebuild KDOS from KDOS. It resolves against
`ports/core` only.

### Build Toolchain

| Port | Version | Description |
|---|---|---|
| `autoconf` | 2.73 | Programs for producing shell scripts that can automatically configure source code |
| `automake` | 1.19 | Programs for generating Makefiles for use with Autoconf |
| `bison` | 3.8.2 | The GNU Bison parser generator |
| `flex` | 2.6.4 | Lexical analyser generator |
| `libtool` | 2.6.2 | The GNU generic library support script |
| `make` | 4.4.1 | GNU make utility |
| `pkgconf` | 3.0.7 | Package compiler and linker metadata toolkit |

### Modern Build Systems

| Port | Version | Description |
|---|---|---|
| `cmake` | 4.4.3 | Cross-platform build-system generator |
| `meson` | 1.12.1 | Build system that generates Ninja build files |
| `ninja` | 1.13.2 | Low-level build system that runs generated build files |

### Languages & Runtimes

| Port | Version | Description |
|---|---|---|
| `bindgen` | 0.73.2 | Generates Rust FFI bindings from C and C++ headers |
| `cabal-install` | 3.18.1.0 | The cabal command — builds and installs Haskell packages |
| `cargo-c` | 0.10.25 | Cargo subcommand to build and install C-ABI-compatible dynamic and static libraries |
| `cbindgen` | 0.29.4 | A project for generating C bindings from Rust code |
| `cython3` | 3.3.0 | C-Extensions for Python3 |
| `ghc` | 9.12.4 | The Glasgow Haskell Compiler, its libraries and GHCi |
| `go` | 1.27.1 | The Go programming language toolchain (with bootstrap) |
| `help2man` | 1.49.3 | Turns a program's own --help into a man page |
| `lua` | 5.5.1 | Lightweight programming language designed for extending applications |
| `lua54` | 5.4.9 | Lua 5.4, installed beside Lua 5.5 for software that requires 5.4 |
| `lua54-luaexpat` | 1.5.2 | Expat XML parser binding for Lua 5.4 — Prosody reads XMPP stanzas with it |
| `lua54-luafilesystem` | 1.9.0 | stat, mkdir and readdir for Lua 5.4 |
| `lua54-luasec` | 1.3.2 | TLS for LuaSocket on Lua 5.4 — Prosody encrypts its connections with it |
| `lua54-luasocket` | 3.1.0 | TCP, UDP and DNS for Lua 5.4 — Prosody's transport |
| `nasm` | 3.02 | The Netwide Assembler, a portable 80x86 assembler |
| `nodejs` | 26.10.0 | JavaScript runtime built on Chrome's V8 JavaScript engine |
| `pandoc` | 3.11 | Converts between markup formats — Markdown, HTML, LaTeX, DOCX, ODT, EPUB and more |
| `perl` | 5.44.0 | The Perl programming language |
| `python3` | 3.14.7 | The Python 3 interpreter and standard library |
| `rust` | 1.98.1 | The Rust compiler, with cargo, clippy, rustdoc and rustfmt |
| `swig` | 4.5.1 | Generate scripting interfaces to C/C++ code |
| `yasm` | 1.3.0 | Assembler for the x86 and AMD64 instruction sets, a rewrite of NASM |
| `zig` | 0.16.0 | Zig — general-purpose programming language and toolchain |

### Alternative compilers / linkers

| Port | Version | Description |
|---|---|---|
| `clang` | 23.1.2 | The C/C++ compiler, plus clangd, clang-tidy and clang-format |
| `clang21` | 21.1.8 | Clang 21 libraries and compiler, installed under /usr/lib/llvm21 for zig |
| `compiler-rt` | 23.1.2 | clang's runtime libraries — builtins, the profile runtime and UndefinedBehaviorSanitizer |
| `libunwind` | 23.1.2 | LLVM libunwind, installed under /usr/lib/llvm-libunwind beside the system libunwind |
| `lld` | 23.1.2 | Linker from the LLVM project |
| `lld21` | 21.1.8 | LLD 21 libraries and linker, installed under /usr/lib/llvm21 for zig |
| `llvm` | 23.1.2 | Compiler infrastructure libraries and tools from the LLVM project |
| `llvm21` | 21.1.8 | LLVM 21, installed under /usr/lib/llvm21 beside the system LLVM for zig |
| `openmp` | 23.1.2 | LLVM's OpenMP runtime, libomp — what clang -fopenmp links |

### Essential Utilities

| Port | Version | Description |
|---|---|---|
| `bash` | 5.3 | The Bourne-Again SHell |
| `bzip2` | 1.0.8 | Programs for compressing and decompressing files |
| `findutils` | 4.11.0 | GNU utilities to locate files |
| `gzip` | 1.15 | GNU compression utility |
| `libarchive` | 3.8.9 | Reading/writing various compression formats |
| `lz4` | 1.10.0 | LZ4 compression library and command-line tool |
| `lzo` | 2.10 | Data compression library |
| `patch` | 2.8 | GNU patch — apply a diff file to an original |
| `sed` | 4.10 | GNU sed — the extensions upstream build systems assume |
| `toybox` | 0.8.14 | All-in-one Linux command-line utility |
| `xz` | 5.8.4 | XZ compression utilities |
| `zstd` | 1.5.7 | Zstandard compression library and tools |

### Network & Security

| Port | Version | Description |
|---|---|---|
| `ca-certificates` | 3.130 | Bundle of CA Root Certificates from Mozilla |
| `curl` | 8.22.0 | Utility and a library used for transferring files |
| `git` | 2.55.0 | Distributed version control system |
| `openldap` | 2.6.15 | The LDAP client library and the ldapsearch family of tools |
| `openssl` | 4.0.2 | Management tools and libraries relating to cryptography |
| `openssl3` | 3.5.8 | OpenSSL 3 runtime libraries for programs built against libssl.so.3 |

### Base Libraries & Database

| Port | Version | Description |
|---|---|---|
| `expat` | 2.8.5 | A stream oriented C library for parsing XML |
| `libffi` | 3.8.0 | Portable foreign function interface library |
| `libxml2` | 2.15.4 | Contains libraries and utilities used for parsing XML files |
| `libxslt` | 1.1.45 | XSLT libraries used for extending libxml2 libraries to support XSLT files |
| `ncurses` | 6.6 | System V Release 4.0 curses emulation library |
| `readline` | 8.3 | GNU readline library |
| `sqlite` | 3530400 | Self-contained, serverless, transactional SQL database engine |

### System Infrastructure

| Port | Version | Description |
|---|---|---|
| `acl` | 2.4.0 | Utilities to administer Access Control Lists, which are used to define more fine-grained discretionary access rights for files and directories |
| `attr` | 2.6.0 | Utilities to administer the extended attributes on filesystem objects |
| `bc` | 1.08.2 | An arbitrary precision calculator language |
| `ccache` | 4.14 | Compiler cache |
| `elfutils` | 0.196 | utilities and libraries for handling ELF files |
| `eudev` | 3.2.14 | Programs for dynamic creation of device nodes |
| `gettext` | 1.0 | Utilities for internationalization and localization |
| `gperf` | 3.3 | Generates a perfect hash function from a key set |
| `intltool` | 0.51.0 | An internationalization tool used for extracting translatable strings from source files |
| `itstool` | 2.0.7 | Translates XML documents with PO files |
| `kmod` | 34.2 | Libraries and utilities for loading kernel modules |
| `libcap` | 2.78 | Implements the user-space interfaces to the POSIX 1003.1e capabilities |
| `patchelf` | 0.19.1 | Utility to modify ELF executables |
| `perl-class-inspector` | 1.36 | Get information about a perl class and its structure |
| `perl-file-sharedir` | 1.118 | Locate per-distribution and per-module shared files of perl modules |
| `perl-file-sharedir-install` | 0.14 | Install shared files of a perl distribution — the configure-time half of File::ShareDir |
| `shadow` | 4.20.3 | Password and user-account management programs |
| `util-linux` | 2.42.4 | Miscellaneous system utilities for Linux |

### Documentation & Spec Tooling

| Port | Version | Description |
|---|---|---|
| `asciidoc` | 10.2.1 | Text document format for short documents, articles, books and UNIX man pages |
| `docbook-xml` | 4.5 | Document type definitions for verification of XML data files against the DocBook rule set |
| `docbook-xsl` | 1.79.2 | XML stylesheets for Docbook-xml transformations |
| `scdoc` | 1.11.5 | Simple man page generator for POSIX systems written in C99 |
| `xmlto` | 0.0.29 | Front-end to an XSL toolchain |

## Phase 4: userland and the Wayland base

`script/04_phase4/packages.txt` installs the rest of the userland: services, the network stack,
the command-line workspace, the scientific and hardware tools, the Wayland base, Xwayland, the
container layer, the codecs, and KDOS's own theme and tools from `src/packages`. It is the largest
list by far.

### Core Build Utilities (host-side)

| Port | Version | Description |
|---|---|---|
| `bash-completion` | 2.18.0 | Programmable completion for the bash shell |
| `btrfs-progs` | 7.1 | Btrfs filesystem utilities |
| `dosfstools` | 4.2 | Various utilities for use with the FAT family of file systems |
| `e2fsprogs` | 1.47.4 | utilities for handling the ext2, ext3 and ext4 file system |
| `exfatprogs` | 1.4.3 | exFAT filesystem utilities (mkfs, fsck, label) |
| `f2fs-tools` | 1.16.0 | Tools to create, check and resize an F2FS filesystem |
| `glib` | 2.90.0 | Low-level libraries useful for providing data structure handling for C, portability wrappers and interfaces |
| `gptfdisk` | 1.0.10 | A text-mode partitioning tool that works on GUID Partition Table (GPT) disks |
| `iana-etc` | 20260911 | /etc/services, /etc/protocols and /etc/rpc, generated from IANA's registries |
| `intel-ucode` | 20260812 | Intel processor microcode, bundled for early loading |
| `iproute2` | 7.2.0 | Programs for basic and advanced IPV4-based networking |
| `libburn` | 1.5.8 | Library for writing data to optical media |
| `libisoburn` | 1.5.8.pl02 | Frontend for libburn and libisofs which enables creation and expansion of ISO-9660 filesystems |
| `libisofs` | 1.5.8.pl02 | Library for creating and manipulating ISO 9660 filesystem images |
| `liburcu` | 0.15.7 | Userspace RCU — required by xfsprogs, no --disable option exists |
| `limine` | 12.9.0 | BIOS and UEFI bootloader and boot manager |
| `linux-firmware` | 20260916 | Device firmware blobs — upstream's complete tree, unpruned |
| `mtools` | 4.0.49 | Collection of utilities for DOS disks in Unix |
| `ntfs-3g` | 2026.9.18 | NTFS driver and the ntfsprogs filesystem utilities |
| `rsync` | 3.5.1 | Utilities for synchronizing large file archives over a network |
| `squashfs-tools` | 4.7.5 | Tools to create and extract squashfs filesystems |
| `sudo` | 1.9.17p2 | allows a system administrator to give certain users (or groups of users) the ability to run some (or all) commands as root or another user while logging the commands and arguments |
| `texinfo` | 7.3 | Programs for reading, writing, and converting info pages |
| `xfsprogs` | 7.2.0 | XFS filesystem utilities |

### Core Services

| Port | Version | Description |
|---|---|---|
| `avahi` | 0.9rc5 | mDNS/DNS-SD stack — what makes network printers discoverable |
| `chrony` | 4.9 | NTP client and server (the clock TLS depends on) |
| `dbus` | 1.16.2 | Message bus system for communication between processes |
| `libdaemon` | 0.14 | C library for writing UNIX daemons (avahi dependency) |
| `man-pages` | 6.19 | Linux kernel and C library manual pages — system calls, library functions, devices, file formats, overviews |
| `mandoc` | 1.14.6 | Compact suite of tools for BSD mdoc and man formatting |
| `snooze` | 0.6 | Runs a command at a given time — one process per job in place of a cron daemon |
| `sysklogd` | 2.7.2 | System logging daemon with built-in rotation |
| `xmltoman` | 0.6 | Convert manual pages written in XML to groff or HTML |

### Network / SSH / Audio / Bluetooth / Print

| Port | Version | Description |
|---|---|---|
| `alsa-utils` | 1.2.16 | Various utilities for controlling sound card |
| `bluez` | 5.87 | Bluetooth protocol stack for Linux |
| `cups` | 2.4.19 | Print spooler and associated utilities |
| `dhcpcd` | 10.5.2 | An implementation of the DHCP client specified in RFC2131 |
| `openssh` | 10.5p1 | OpenSSH client and server |
| `powertop` | 2.16 | Power consumption diagnosis and tuning tool |
| `tlp` | 1.10.2 | Laptop power saving — pure shell over /sys, no daemon |
| `wiremix` | 0.11.0 | wiremix — a PipeWire mixer and routing panel for the terminal |
| `wireplumber` | 0.5.17 | PipeWire's session manager — device policy, routing and Bluetooth profiles |

### Power-User CLI base

| Port | Version | Description |
|---|---|---|
| `btop` | 1.4.7 | A monitor for system resources |
| `byobu` | 7.19 | Text-based window manager and terminal multiplexer |
| `ccid` | 1.8.4 | USB CCID and ICCD smart card reader driver for pcsc-lite |
| `docx2txt` | 1.4 | Extracts the text of a .docx to standard output |
| `doxx` | 0.1.4 | A .docx reader in the terminal |
| `epy` | 2023.6.11 | An ebook reader in the terminal — epub, mobi, azw3, fb2 |
| `file` | 5.48 | Identify a file by its content, and the magic database behind it |
| `htop` | 3.5.3 | Interactive process viewer |
| `kbd` | 2.10.0 | Key-table files, console utilities and the vlock console lock |
| `less` | 710 | A text file viewer |
| `lesspipe` | 2.28 | An input filter for less that shows what is inside a file |
| `libcbor` | 0.14.0 | CBOR parser — libfido2's wire format for talking to a security key |
| `libfido2` | 1.17.0 | FIDO2/U2F over USB and NFC — what an ed25519-sk SSH key talks to |
| `libunwind-nongnu` | 1.8.3 | The libunwind library — call-chain unwinding of this process, another one over ptrace, or a core file |
| `nano` | 9.2 | Simple text editor which aims to replace Pico, the default editor in the Pine package |
| `nerd-fonts-symbols` | 3.5.1 | Symbols Nerd Font — the icon codepoints a patched font carries, as a fallback face |
| `noto-cjk` | 2.004 | Noto Sans CJK — Chinese, Japanese and Korean coverage in one collection |
| `noto-emoji` | 2.051 | Noto Color Emoji — the desktop's colour emoji face |
| `noto-fonts` | 2.015 | Noto Sans and Noto Sans Mono — the desktop's proportional and fixed faces |
| `noto-fonts-extra` | 1 | Noto Sans coverage for Arabic, Hebrew, Indic, Sinhala, Thai, Lao, Khmer, Myanmar, Ethiopic, Georgian and Armenian |
| `opensc` | 0.27.1 | The PKCS#11 module for PIV, CAC, OpenPGP and national eID smart cards, and the tools to manage them |
| `pciutils` | 3.15.0 | Set of programs for listing PCI devices |
| `perl-authen-sasl` | 2.2100 | SASL authentication framework for perl |
| `perl-crypt-urandom` | 0.55 | The operating system's cryptographic random source for perl |
| `perl-digest-hmac` | 1.05 | Keyed-hashing for message authentication (HMAC) in perl |
| `perl-io-socket-ssl` | 2.099 | TLS sockets for perl, over Net::SSLeay |
| `perl-net-ssleay` | 1.96 | OpenSSL bindings for perl |
| `procps-ng` | 4.0.7 | Programs for monitoring processes |
| `psmisc` | 23.7 | Programs for displaying information about running processes |
| `terminus-font` | 4.49.1 | Monospace bitmap console font |
| `terminus-ttf` | 4.49.3 | Scalable TTF build of Terminus, for the consumers that cannot draw a bitmap font |
| `tmux` | 3.7c | tmux - Terminal multiplexer |
| `tree` | 2.3.2 | display directory contents, including directories, files, links, in a terminal |
| `ttf-dejavu` | 2.37 | Font family based on the Bitstream Vera Fonts with a wider range of characters |
| `usbutils` | 019 | Utilities used to display information about USB devices |

### Modern CLI tools (Rust / Go)

| Port | Version | Description |
|---|---|---|
| `atuin` | 18.23.0 | Shell history in a searchable SQLite database, local only |
| `bat` | 0.26.1 | bat — a cat clone with syntax highlighting and Git integration |
| `bottom` | 0.14.9 | bottom (btm) — graphical process and system monitor for the terminal |
| `chezmoi` | 2.72.2 | Manage dotfiles across machines from one source of truth |
| `delta` | 0.19.2 | delta — a syntax-highlighting pager for git, diff, and grep output |
| `direnv` | 2.37.1 | direnv — loads and unloads environment variables per directory |
| `duf` | 0.9.1 | duf — disk usage and free space, in the manner of df |
| `dust` | 1.2.6 | dust — disk usage as a tree, a du alternative (Rust) |
| `entr` | 5.8 | Run an arbitrary command when files change |
| `eza` | 0.23.5 | eza — an ls replacement |
| `fd` | 10.5.0 | fd — a file finder, an alternative to find |
| `fontforge` | 20251009 | Outline and bitmap font editor, driven from scripts and Python — no GUI |
| `fonttools` | 4.66.0 | Inspect, subset and convert font files from a prompt |
| `fx` | 39.2.0 | fx — an interactive viewer and processor for JSON and YAML |
| `fzf` | 0.74.4 | fzf — general-purpose command-line fuzzy finder |
| `gh` | 2.101.0 | gh — official GitHub CLI |
| `glow` | 3.0.0 | Render markdown on the command line |
| `hexyl` | 0.17.0 | Hex viewer that colours bytes by class |
| `jq` | 1.8.2 | jq — command-line JSON processor |
| `just` | 1.58.0 | just — a command runner for project-specific recipes |
| `lazygit` | 0.65.1 | lazygit — terminal UI for git |
| `miller` | 6.21.0 | awk for CSV, TSV and JSON — named fields instead of column numbers |
| `ncdu` | 2.11.1 | ncdu — NCurses Disk Usage analyzer |
| `ouch` | 0.8.3 | Compresses and decompresses many archive formats with one command |
| `plocate` | 1.1.25 | Finds a file by name in a prebuilt index, per user and without a setgid bit |
| `procs` | 0.14.12 | procs — a ps replacement |
| `resvg` | 0.48.1 | SVG rendering library and SVG-to-PNG converter |
| `ripgrep` | 15.2.0 | ripgrep — recursively search directories for a regex pattern |
| `sd` | 1.1.0 | Find-and-replace, a sed alternative for the common case |
| `starship` | 1.26.0 | starship — configurable shell prompt |
| `visidata` | 3.4 | Terminal spreadsheet for tabular files |
| `yq` | 4.53.6 | yq — a portable YAML / JSON / TOML / XML processor (Go, Mike Farah) |
| `zoxide` | 0.10.0 | zoxide — a cd command that ranks directories by use |

### Archives, duplicates and recovery

| Port | Version | Description |
|---|---|---|
| `caligula` | 0.5.0 | Writes an image to a removable disk, with a progress display and a verify pass |
| `ddrescue` | 1.30 | Image a failing disk with a resumable mapfile |
| `lzip` | 1.26 | Lossless data compressor for the .lz format |
| `par2cmdline-turbo` | 1.5.0 | PAR2 recovery files with SIMD-accelerated Reed-Solomon coding |
| `parallel` | 20260922 | Run shell command lines in parallel across cores and hosts |
| `partclone` | 0.3.50 | Filesystem-aware imaging — copies the used blocks and skips the rest |
| `pv` | 1.12.0 | Pipe viewer — progress, throughput and ETA for any stream |
| `rdfind` | 1.8.0 | Find duplicate files and replace them with hardlinks or symlinks |
| `testdisk` | 7.2 | Recover lost partitions and, as photorec, deleted files |
| `zip` | 3.0 | Info-ZIP archive creation utilities |

### The offline library: index it, search it, serve it

| Port | Version | Description |
|---|---|---|
| `docopt.cpp` | 0.6.3 | Command-line parser that derives the interface from the usage text |
| `kiwix-tools` | 3.8.2 | kiwix-serve, kiwix-manage and kiwix-search over ZIM archives |
| `libkiwix` | 14.2.1 | The library kiwix-serve searches and serves a ZIM with |
| `libmicrohttpd` | 1.0.10 | Embeddable HTTP server — what kiwix-serve listens with |
| `libzim` | 9.8.2 | Read and write the ZIM archives Wikipedia offline ships as |
| `mustache` | 4.1 | Kainjow Mustache, a header-only Mustache template engine for C++11 |
| `pugixml` | 1.16 | Light XML parser — libkiwix reads its OPDS catalogues with it |
| `xapian-core` | 2.1.0 | The search engine library recoll indexes into |

### Secrets, another operating system, and the memory under all of it

| Port | Version | Description |
|---|---|---|
| `acpica` | 20260408 | ACPI Component Architecture tools — the iasl ASL compiler, acpidump and acpixtract |
| `dtc` | 1.8.1 | The device tree compiler, and libfdt — what qemu needs for arm and riscv |
| `gocryptfs` | 2.6.1 | An encrypted directory that syncs as ordinary files |
| `iamb` | 0.0.12 | Terminal Matrix client with vim key bindings |
| `libtpms` | 0.10.2 | A software TPM 1.2 and 2.0 as a library — the TPM inside swtpm |
| `memtest86plus` | 8.10 | Standalone memory tester that runs outside the operating system |
| `oath-toolkit` | 2.6.14 | One-time passwords from the command line — HOTP and TOTP |
| `pass` | 1.7.4 | The git-versioned, gpg-encrypted password store |
| `pass-otp` | 1.2.0 | A one-time password in the password store — pass otp |
| `pizauth` | 1.1.0 | Obtains and refreshes OAuth2 tokens for mail clients |
| `presenterm` | 0.16.1 | Slides from a markdown file, drawn in the terminal |
| `python3-openpyxl` | 3.1.5 | Read and write xlsx from python — what visidata opens one with |
| `qemu` | 11.1.1 | Machine emulator and virtualiser |
| `swtpm` | 0.10.2 | A software TPM for virtual machines — qemu's -tpmdev emulator backend |
| `virtiofsd` | 1.14.0 | The virtio-fs daemon — shares a host directory with a qemu guest |

### Putting firmware on a chip, and talking to it while it runs

| Port | Version | Description |
|---|---|---|
| `avrdude` | 8.3 | Programmer for AVR microcontrollers — flash, EEPROM and fuse bytes |
| `dfu-util` | 0.11 | Flashes a device over USB DFU, with no separate programmer |
| `openocd` | 0.12.0 | On-chip debugging and flashing over JTAG or SWD |
| `stlink` | 1.8.0 | The ST-Link tools — flash and debug an STM32 directly |

### Cross toolchains — native, because flashing a board must not need podman

| Port | Version | Description |
|---|---|---|
| `avr-libc` | 2.3.2 | The C library for AVR microcontrollers |
| `binutils-arm-none-eabi` | 2.47 | The assembler, linker and object tools for arm-none-eabi |
| `binutils-avr` | 2.47 | The assembler, linker and object tools for avr |
| `binutils-riscv64-unknown-elf` | 2.47 | The assembler, linker and object tools for riscv64-unknown-elf |
| `espflash` | 4.6.0 | Flash an ESP32 or ESP8266 over its own ROM bootloader, and watch its serial output |
| `flashrom` | 1.8.0 | Read and write a SPI flash chip — including this machine's BIOS |
| `gcc-arm-none-eabi` | 16.2.0 | The bare-metal Cortex-M and Cortex-R compiler |
| `gcc-avr` | 16.2.0 | GCC for AVR microcontrollers (ATmega and ATtiny) |
| `gcc-riscv64-unknown-elf` | 16.2.0 | The RISC-V bare-metal compiler, for MCUs and the soft cores the FPGA flow synthesises |
| `libgpiod` | 2.3.1 | The chardev GPIO ABI — gpioset, gpioget and gpiomon |
| `libstdcxx-arm-none-eabi` | 16.2.0 | The C++ standard library for arm-none-eabi, built against picolibc |
| `lrzsz` | 0.12.20 | X, Y and ZMODEM file transfer over a serial line |
| `minicom` | 2.11.1 | Serial terminal program |
| `picolibc-arm-none-eabi` | 1.8.12 | The C library for arm-none-eabi — newlib trimmed for a microcontroller |
| `picolibc-riscv64-unknown-elf` | 1.8.12 | The C library for riscv64-unknown-elf — newlib trimmed for a microcontroller |
| `python3-esptool` | 5.4.0 | Talk to the ESP8266 and ESP32 ROM bootloader over serial |
| `python3-pyusb` | 1.3.1 | USB access from python through libusb — what pyvisa-py opens a USB instrument with |
| `python3-pyvisa` | 1.16.2 | VISA instrument-automation API for Python, with no NI-VISA dependency |
| `python3-pyvisa-py` | 0.8.1 | The pure-python VISA backend — USB, TCPIP and serial with no vendor library |
| `stm32flash` | 0.7 | Flash an STM32 over its own ROM bootloader, with no debugger attached |
| `tio` | 3.9 | Serial terminal that reconnects when the device returns |

### Buses — I2C, CAN and MQTT

| Port | Version | Description |
|---|---|---|
| `can-utils` | 2025.01 | candump, cansend, cangen and the rest of the SocketCAN command set |
| `cjson` | 1.7.19 | cJSON, a small ANSI C JSON parser (mosquitto links it) |
| `ddcutil` | 3.0.2 | Brightness, contrast and input source of an external monitor over DDC/CI |
| `i2c-tools` | 4.4 | i2cdetect, i2cget, i2cset and the SMBus library, over /dev/i2c-* |
| `libmodbus` | 3.2.0 | Modbus RTU and TCP library |
| `mosquitto` | 2.1.2 | MQTT broker and the mosquitto_pub and mosquitto_sub clients |
| `mqttui` | 0.24.0 | Subscribe to an MQTT broker and watch the topic tree, in the terminal |
| `python3-cantools` | 44.1.0 | Decode a live candump into named signals, and generate the MCU-side C |
| `python3-meshtastic` | 2.7.11 | Meshtastic LoRa mesh client — text and position messages |

### Software radio

| Port | Version | Description |
|---|---|---|
| `airspy` | 1.0.10 | libairspy and the airspy_* tools for the Airspy R2 and Mini receivers |
| `bladerf` | 2025.10 | libbladeRF and bladeRF-cli for the Nuand bladeRF and bladeRF 2.0 micro transceivers |
| `hackrf` | 2026.01.3 | libhackrf and the hackrf_* tools for the HackRF One, Jawbreaker and rad1o transceivers |
| `multimon-ng` | 1.6.1 | POCSAG, FLEX, AFSK, DTMF and the rest, demodulated from a stream of samples |
| `rtl-433` | 25.12 | Decodes 433/868 MHz transmissions from weather stations, sensors and tyre monitors |
| `rtl-sdr` | 2.0.3 | RTL2832U SDR driver library and the rtl_* tools |
| `soapyairspy` | 0.2.0 | The SoapySDR module for Airspy R2 and Mini receivers, through libairspy |
| `soapybladerf` | 0.4.2 | The SoapySDR module for the bladeRF family, through libbladeRF |
| `soapyhackrf` | 0.3.4 | The SoapySDR module for the HackRF family, through libhackrf |
| `soapyrtlsdr` | 0.3.3 | The SoapySDR module for RTL2832U sticks, through librtlsdr |
| `soapysdr` | 0.8.1 | Vendor-neutral API over SDR hardware |

### Position

| Port | Version | Description |
|---|---|---|
| `gpsd` | 3.27.5 | GPS daemon that presents receivers as a socket |
| `scons` | 4.11.1 | Python-based build tool (gpsd builds with it) |

### The bench — instruments, protocols and a calculator that parses units

| Port | Version | Description |
|---|---|---|
| `liblxi` | 1.22 | Discover and talk to an LXI instrument |
| `libqalculate` | 5.12.0 | Calculator library with unit and expression parsing |
| `libserialport` | 0.1.2 | Enumerate and open a serial port, portably |
| `libsigrok` | 0.5.2 | Hardware drivers for logic analysers, oscilloscopes and meters |
| `libsigrokdecode` | 0.5.3 | Protocol decoder library, with the decoders written in Python |
| `lxi-tools` | 2.8 | Drive a bench instrument over ethernet with SCPI |
| `sigrok-cli` | 0.7.2 | Command-line capture and protocol decoding for sigrok hardware |
| `sigrok-firmware-fx2lafw` | 0.1.7 | The firmware libsigrok uploads to a Cypress FX2 logic analyser, compiled here with sdcc |

### Raw pixels, curves, and sound with no window

| Port | Version | Description |
|---|---|---|
| `fluidsynth` | 2.6.1 | SoundFont MIDI synthesiser |
| `fmt` | 12.2.0 | The C++ formatting library std::format was standardised from |
| `libopusenc` | 0.3 | The high-level Opus encoder opusenc is built on |
| `libraw` | 0.22.2 | Camera raw file decoding library |
| `mpd` | 0.24.15 | Music Player Daemon — plays music for separate client programs |
| `opus-tools` | 0.2 | Encode, decode and inspect Opus from a prompt |
| `opusfile` | 0.12 | Decode and seek .opus files — the read half opus-tools needs |
| `oxipng` | 10.2.1 | Lossless PNG optimiser |
| `pngquant` | 2.18.0 | Lossy PNG compressor — reduces a truecolour PNG to a palette |
| `potrace` | 1.16 | Bitmap to vector tracer |
| `rmpc` | 0.11.0 | An MPD client drawn in ratatui, with album art in the terminal |
| `termusic` | 0.13.2 | A terminal music player drawn in ratatui, with its own playback daemon |
| `zopfli` | 1.0.3 | Deflate compressor that trades time for size, with zopflipng for PNG files |

### Words, notes, and where the time went

| Port | Version | Description |
|---|---|---|
| `nb` | 7.25.5 | Notes and bookmarks in a git repository, from one command |
| `ripgrep-all` | 0.10.10 | ripgrep that also searches inside PDFs, archives and spreadsheets |
| `sdcv` | 0.5.5 | Look a word up in a StarDict dictionary, offline |
| `timewarrior` | 1.10.0 | Time tracking in a text file |

### Playing, reading, scanning and keeping a record

| Port | Version | Description |
|---|---|---|
| `chafa` | 1.18.3 | chafa — images as terminal graphics: sixel, kitty, iTerm2 or Unicode |
| `exiftool` | 13.59 | Reads and writes metadata in image, audio and video files |
| `exiv2` | 0.28.9 | Read and write image metadata — EXIF, IPTC and XMP |
| `libbluray` | 1.5.0 | Blu-ray playlists and titles — mpv's bd:// and ffmpeg's bluray: protocol |
| `libcdio` | 2.4.0 | CD-ROM and ISO 9660 access — audio CDs for mpv, mpd and GStreamer |
| `libcdio-paranoia` | 10.2+2.0.2 | Audio CD reading that verifies and repairs — cd-paranoia, and CD audio in mpv and mpd |
| `libdvdnav` | 7.0.0 | DVD-Video navigation — mpv's dvd:// and GStreamer's rsndvdbin, whose menus it drives |
| `libdvdread` | 7.1.1 | Reads the file system and title structure of a DVD-Video disc |
| `libplacebo` | 7.360.1 | The GPU video rendering pipeline mpv is built on — colour, scaling, tone mapping |
| `libudfread` | 1.2.0 | Reads the UDF file system of a Blu-ray disc or image |
| `mpv` | 0.41.0 | Command-line media player for Wayland, with no toolkit |
| `mpv-mpris` | 1.3 | Puts mpv on the session bus, so the media keys and the panel reach it |
| `python3-glad2` | 2.0.8 | OpenGL and EGL loader generator — writes libplacebo's GL loader at build time |
| `python3-jinja2` | 3.1.6 | The template engine libplacebo generates its shader sources with |
| `qpdf` | 12.4.1 | Restructure a PDF — split, merge, decrypt, repair |
| `timg` | 1.6.3 | timg — a terminal image and video viewer |
| `uchardet` | 0.0.8 | Guesses the character encoding of a text file — how mpv reads a subtitle that is not UTF-8 |

### Mail — fetched, indexed once, read by anything

| Port | Version | Description |
|---|---|---|
| `aerc` | 0.22.0 | A terminal mail client that speaks IMAP, Maildir and notmuch |
| `autoconf-archive` | 2024.10.16 | The AX_* m4 macros a configure.ac reaches for and autoconf does not carry |
| `bdwgc` | 8.2.12 | The Boehm conservative garbage collector — w3m allocates through it |
| `calcurse` | 4.8.2 | A calendar and todo list whose storage is two text files |
| `corrosion` | 0.6.1 | The CMake bridge that builds a cargo crate as a CMake target |
| `cyrus-sasl` | 2.1.28 | SASL authentication library — the mechanism loader mbsync authenticates through |
| `cyrus-sasl-xoauth2` | 0.2 | The XOAUTH2 SASL mechanism — what mbsync authenticates with where a provider has withdrawn app passwords |
| `gmime` | 3.2.15 | MIME parsing and generation — notmuch reads every message through it |
| `gtk-doc` | 1.36.1 | The m4 macro and gtk-doc.make that GNOME-lineage autotools projects include |
| `ii` | 2.0 | An IRC client that is a directory of FIFOs and text files |
| `isync` | 1.5.1 | mbsync — synchronises IMAP mailboxes with a local Maildir, XOAUTH2 included |
| `khal` | 0.14.1 | Terminal calendar over a vdir |
| `khard` | 0.21.0 | Terminal address book over a vdir |
| `ledger` | 3.4.1 | Double-entry accounting over a plain-text journal |
| `leptonica` | 1.87.0 | The image processing tesseract does its page analysis with |
| `libintl` | 1.0 | GNU libintl — the gettext runtime musl does not carry in libc |
| `msmtp` | 1.8.34 | SMTP client — sends a message from standard input to an SMTP server |
| `newsboat` | 2.44 | newsboat — an RSS and Atom feed reader for the terminal |
| `notmuch` | 0.40 | Mail indexer with tag-based search |
| `sfeed` | 2.4 | RSS and Atom feeds as a tab-separated file, and the programs that read it |
| `smu` | 1.5 | Markdown to HTML in one C file |
| `stfl` | 0.24 | STFL — a curses-based structured terminal forms library |
| `taskwarrior` | 3.5.0 | Tasks with dates, dependencies and a query language |
| `taskwarrior-tui` | 0.27.0 | Terminal UI for taskwarrior |
| `tesseract` | 5.5.3 | OCR engine — text from scanned images |
| `typst` | 0.15.1 | Markup-based typesetting system |
| `vdirsyncer` | 0.21.0 | Makes a CalDAV or CardDAV collection and a local vdir equal |
| `w3m` | 0.5.6 | Text-mode web browser and pager that renders tables |

### Sequences, coordinates, and exact arithmetic

| Port | Version | Description |
|---|---|---|
| `bcftools` | 1.24 | Call and filter variants |
| `duckdb` | 1.5.5 | In-process SQL database that queries CSV, JSON and Parquet files directly |
| `gdal` | 3.13.3 | Raster and vector geospatial data format library |
| `geos` | 3.15.0 | Computational geometry library for GIS software |
| `go-pmtiles` | 1.31.2 | Create, inspect and serve PMTiles map archives |
| `htslib` | 1.24 | The SAM/BAM/CRAM/VCF reader samtools and bcftools are built on |
| `libxlsxwriter` | 1.2.4 | C library for writing Excel XLSX files |
| `minimap2` | 2.31 | Align long reads to a reference |
| `pari` | 2.17.4 | Number theory and arbitrary-precision arithmetic, at a prompt |
| `proj` | 9.9.0 | Cartographic projection and coordinate transformation library |
| `routino` | 3.4.4 | Offline route planning over an OSM extract |
| `samtools` | 1.24 | Sort, index and view alignments |
| `sc-im` | 0.8.5 | Terminal spreadsheet with vi keys |

### The numeric foundation — everything in C2 stands on these

"C2" in the list's title names the scientific stack later in this phase. These are the numerical
libraries it links against; `numpy`, for example, depends on `openblas`, and `ngspice` on `fftw`.

| Port | Version | Description |
|---|---|---|
| `fftw` | 3.3.11 | Fast Fourier transform library |
| `glpk` | 5.0 | Linear and mixed-integer programming, with its own modelling language |
| `gsl` | 2.8 | GNU Scientific Library — numerical routines outside linear algebra |
| `hdf5` | 2.2.0 | Hierarchical Data Format library for scientific data |
| `highs` | 1.15.1 | Linear and mixed-integer programming solver |
| `openblas` | 0.3.34 | Optimised BLAS and LAPACK library |

### The python numeric stack

| Port | Version | Description |
|---|---|---|
| `astropy` | 8.0.1 | Coordinates, times, FITS, WCS and cosmology |
| `bitwise` | 0.70 | Bit-level manipulation and base conversion, watched live in the terminal |
| `coolprop` | 8.0.0 | Thermophysical properties for 122 fluids, IAPWS-95 water/steam and humid air |
| `gnuplot` | 6.0.5 | Plotting program driven by scripts, drawing to the terminal or to PNG |
| `ipython` | 9.17.1 | Interactive Python shell |
| `libraqm` | 0.11.0 | Complex text layout — bidi, shaping and script itemisation over harfbuzz |
| `matplotlib` | 3.11.2 | 2D plotting library for Python, Agg backend only |
| `mbpoll` | 1.5.4 | Read and write Modbus registers from a prompt, over RTU or TCP |
| `nco` | 5.4.0 | Subset, regrid and do arithmetic on gridded data without loading it |
| `netcdf-c` | 4.10.1 | Library for netCDF, the self-describing array format of climate, ocean and atmosphere data |
| `numbat` | 1.24.0 | Calculator with physical units and dimension checking |
| `numpy` | 2.5.3 | N-dimensional array library for Python |
| `pandas` | 3.0.6 | Data frames for Python — tables, joins and group-bys |
| `python3-calver` | 2025.10.20 | Date-based versions for setuptools — trove-classifiers builds with it |
| `python3-certifi` | 2026.7.22 | The CA bundle python libraries look for by name |
| `python3-cython` | 3.3.0 | The C extension compiler numpy is written against |
| `python3-extension-helpers` | 1.5.0 | The setuptools glue astropy compiles its C extensions through |
| `python3-flit-core` | 4.1.0 | The minimal PEP 517 backend packaging itself builds with |
| `python3-hatch-vcs` | 0.5.0 | Version from VCS for hatchling projects, through setuptools-scm |
| `python3-hatchling` | 1.32.4 | Hatch's PEP 517 backend, which bootstraps itself — scikit-build-core builds with it |
| `python3-jplephem` | 2.24 | Reads JPL ephemeris kernels for planetary positions |
| `python3-meson-python` | 0.21.1 | The PEP 517 backend numpy, scipy and matplotlib build through |
| `python3-mpmath` | 1.4.1 | Arbitrary-precision floating point — sympy computes numerically through it |
| `python3-nanobind` | 3.1.0 | C++17 binding library for Python — pikepdf and CoolProp generate through it |
| `python3-packaging` | 26.3 | Version and marker parsing every build backend uses |
| `python3-pathspec` | 1.1.1 | Gitignore-style path matching — how hatchling and scikit-build-core choose files |
| `python3-pluggy` | 1.6.0 | The plugin hook system hatchling and ocrmypdf are extended through |
| `python3-poetry-core` | 2.5.0 | Poetry's PEP 517 backend, which bootstraps itself — tomlkit builds with it |
| `python3-pybind11` | 3.1.0 | C++11 bindings — matplotlib and scipy generate through it |
| `python3-pyerfa` | 2.0.1.5 | The python binding for ERFA — fundamental astronomy, and astropy's floor |
| `python3-pyproject-metadata` | 0.12.1 | PEP 621 metadata, which meson-python reads |
| `python3-scikit-build-core` | 1.0.3 | The PEP 517 backend for CMake projects — pybind11, nanobind, pikepdf and CoolProp build through it |
| `python3-setuptools-scm` | 10.3.4 | Version from VCS — matplotlib requires it at build time |
| `python3-sgp4` | 2.27 | SGP4 satellite orbit propagation from a TLE |
| `python3-tomlkit` | 0.15.1 | Style-preserving TOML reading and writing — hatchling requires it |
| `python3-trove-classifiers` | 2026.9.21.13 | The canonical PyPI classifier list hatchling validates metadata against |
| `python3-vcs-versioning` | 2.5.0 | The VCS version machinery setuptools-scm is built on |
| `python3-versioneer` | 0.29 | Derives a package version from git metadata — a build-time import pandas needs |
| `qhull` | 8.0.2 | Convex hulls, Delaunay triangulation and Voronoi diagrams |
| `scipy` | 1.18.1 | Scientific computing for Python — optimisation, integration, signal, sparse, statistics and physical constants |
| `skyfield` | 1.55 | Planet, moon and satellite positions from JPL kernels |
| `sympy` | 1.14.0 | Symbolic mathematics for Python |
| `udunits` | 2.2.28 | Unit conversion library for scientific data |

### Astronomy — the sky, in the formats telescopes write

| Port | Version | Description |
|---|---|---|
| `cfitsio` | 4.7.0 | Library for reading and writing FITS, the standard astronomical data format |
| `erfa` | 2.0.1 | Essential Routines for Fundamental Astronomy — the IAU standard, relicensed |
| `gnuastro` | 0.24 | The GNU astronomy toolset — arithmetic, cropping, photometry, on FITS |
| `wcslib` | 8.9 | The FITS World Coordinate System — pixel to sky, and back |

### Sequences — the bioinformatics core

| Port | Version | Description |
|---|---|---|
| `diamond` | 2.2.8 | Protein and translated-DNA sequence aligner, a faster BLASTP/BLASTX |
| `hmmer` | 3.4 | Profile hidden Markov models — the sequence search Pfam is built on |
| `mafft` | 7.526 | Multiple sequence alignment |
| `seqkit` | 2.13.0 | FASTA/FASTQ manipulation toolkit |

### Synthesise, place, route and flash — the open FPGA flow, natively

| Port | Version | Description |
|---|---|---|
| `ax25-tools` | 1.1.0 | kissattach, NET/ROM and connected-mode AX.25 links |
| `capstone` | 5.0.9 | Disassembly engine library (rizin disassembles with it) |
| `direwolf` | 1.8.1 | Software TNC — AX.25, APRS and KISS over a sound card |
| `eigen` | 5.0.1 | Header-only C++ linear algebra library (nextpnr's timing analysis uses it) |
| `gef` | 2026.01 | GDB extension with context, heap and ROP views |
| `hamlib` | 4.7.2 | Radio and rotator control library |
| `icestorm` | 1.1 | The iCE40 bitstream database, packer and programmer |
| `iverilog` | 13_0 | Event-driven Verilog simulation with real delays and X propagation |
| `libax25` | 1.2.2 | The AX.25 socket library the tools and apps link against |
| `libftdi` | 1.5 | Library for FTDI USB serial and JTAG bridge chips |
| `libtommath` | 1.3.0 | Multiple-precision integers — tcl bundles a renamed copy and yosys needs the real one |
| `nec2c` | 1.3.3 | NEC-2 antenna simulation from a card deck |
| `nextpnr` | 0.11.1 | Place and route — the second stage of the open FPGA flow |
| `ngspice` | 47 | SPICE circuit simulation from a prompt — transient, AC, DC, noise, Monte Carlo |
| `openfpgaloader` | 1.1.1 | FPGA programming tool for the boards the FPGA flow targets |
| `picotool` | 2.3.1 | RP2040 and RP2350 UF2 inspection and USB loading |
| `predict` | 3.0.2 | Curses satellite tracker — passes, doppler and footprints from a TLE |
| `prjtrellis` | 1.4 | The ECP5 bitstream database and packer |
| `probe-rs` | 0.32.0 | ARM and RISC-V flash and debug over CMSIS-DAP, ST-Link and J-Link, with RTT |
| `rizin` | 0.9.1 | Reverse-engineering framework — disassembler and binary analysis in the terminal |
| `sdcc` | 4.6.0 | A C compiler for the 8-bit families AVR does not cover — 8051, STM8, Z80, PIC |
| `splat` | 1.4.2 | Terrain-aware VHF/UHF path loss and coverage over Longley-Rice |
| `srecord` | 1.65 | HEX, S-record and TI-TXT conversion, fill, CRC and offset |
| `symbiyosys` | 0.69 | Formal property verification over yosys — sby, and the solvers behind it |
| `tcl` | 9.0.4 | The Tcl scripting language — the interpreter yosys and weechat embed |
| `tlf` | 1.4.1 | Curses contest logger with hamlib and Cabrillo output |
| `units` | 2.27 | Unit conversion program and its units database |
| `verilator` | 5.052 | Verilog simulator and linter |
| `voacapl` | 0.7.7 | HF propagation prediction (VOACAP) |
| `yices2` | 2.7.0 | The SMT solver SymbiYosys proves properties with |
| `yosys` | 0.69 | Verilog synthesis — the first stage of the open FPGA flow |

### Grammars compiled here, a Rust server that can see std, and a private CA

| Port | Version | Description |
|---|---|---|
| `rust-analyzer` | 2026.09.21 | The Rust language server |
| `step-ca` | 0.30.2 | Private certificate authority with ACME |
| `step-cli` | 0.30.6 | The step command — creates a CA for step-ca, and requests, renews and inspects certificates |
| `tree-sitter` | 0.27.0 | The parser library and CLI — grammars compiled here, never fetched |

### Two people talking, and two machines sharing a filesystem

| Port | Version | Description |
|---|---|---|
| `baresip` | 4.11.0 | SIP softphone for voice calls |
| `libre` | 4.11.0 | The SIP, RTP and STUN protocol stack baresip is built on |
| `nfs-utils` | 3.1.1 | NFS client and server utilities |
| `perl-parse-yapp` | 1.21 | An LALR parser generator in perl — samba builds its IDL compiler with it |
| `samba` | 4.24.7 | SMB/CIFS file and print server and client tools |

### Inside the kernel, on the air, and against your own hashes

| Port | Version | Description |
|---|---|---|
| `aircrack-ng` | 1.7 | 802.11 Wi-Fi auditing and diagnosis tools |
| `bcc` | 0.37.0 | The BPF Compiler Collection library, USDT probe support and the libbpf-tools tracers |
| `bpftrace` | 0.27.0 | High-level tracing language for eBPF, for tracing inside the kernel |
| `cereal` | 1.3.2 | Header-only C++ serialisation — bpftrace's BTF cache format |
| `john` | 1.9.0.jumbo1 | John the Ripper password hash cracker |
| `libbpf` | 1.7.0 | The userspace side of BPF — load a program, read a map |
| `usql` | 0.21.6 | Command-line SQL client for PostgreSQL, SQLite and other databases |

### The wire, the database, the web server, and reading the screen aloud

| Port | Version | Description |
|---|---|---|
| `brltty` | 6.9.1 | Braille display and console speech — the screen reader |
| `caddy` | 2.11.4 | Web server with built-in TLS certificate management |
| `coreutils` | 9.12 | The GNU core utilities, for the commands toybox implements too narrowly, such as expr |
| `hurl` | 8.0.1 | Runs HTTP requests and their assertions from a plain-text file |
| `liblouis` | 3.39.0 | Braille translation — contracted and uncontracted tables for well over a hundred languages |
| `postgresql` | 18.6 | PostgreSQL relational database server |
| `speexdsp` | 1.2.1 | Resampling, echo cancellation and jitter buffering — wireshark decodes RTP with it |
| `termshark` | 2.4.0 | Terminal UI for tshark |
| `wireshark` | 4.7.3 | tshark and dumpcap — Wireshark's dissection engine, no GUI |

### Language servers, a diff you can read, and a repository you can undo

| Port | Version | Description |
|---|---|---|
| `7zip` | 26.03 | 7-Zip's 7zz — packs and unpacks 7z, zip, rar, tar, iso, cab, deb, rpm and disk images |
| `difftastic` | 0.71.0 | Syntax-aware structural diff |
| `gopls` | 0.23.0 | The Go language server |
| `jujutsu` | 0.45.1 | Git-compatible version control system with an operation log |
| `trippy` | 0.13.0 | Network diagnostic combining traceroute and ping, with per-hop loss and jitter |
| `yazi` | 26.9.1 | Asynchronous terminal file manager with image previews |
| `zls` | 0.16.0 | The Zig language server |

### Statistics, census, HTTP, bandwidth, and example-first help

| Port | Version | Description |
|---|---|---|
| `bandwhich` | 0.23.1 | Shows bandwidth use by process and connection |
| `hyperfine` | 1.20.0 | Statistical command-line benchmarking with warmup and outliers |
| `tealdeer` | 1.9.0 | Example-first help — the tldr pages, pre-seeded and never fetched |
| `tokei` | 15.0.0 | Counts lines of code by language |
| `xh` | 0.26.2 | Command-line HTTP client |

### One index over the corpus

| Port | Version | Description |
|---|---|---|
| `aspell` | 0.60.8.2 | GNU Aspell spell checker, library and command |
| `aspell-en` | 2026.02.25.0 | English dictionaries for Aspell (en_US, en_GB, en_CA, en_AU) |
| `enchant` | 2.8.21 | Spell-checking library over several engines, built with its Aspell provider |
| `jsoncpp` | 1.9.8 | JSON for C++ — recoll stores its index metadata through it |
| `poppler` | 26.09.0 | PDF rendering library, and pdftotext with the other poppler utilities |
| `recoll` | 1.44.1 | Full-text search index across PDFs, ODT, EPUB, mail and source |

### Keeping the bytes, and moving them between two machines

| Port | Version | Description |
|---|---|---|
| `age` | 1.3.2 | File encryption tool |
| `croc` | 11.5.3 | One-off file transfer by code phrase, kept on this network |
| `doggo` | 1.4.0 | Command-line DNS client |
| `git-lfs` | 3.8.0 | Git extension that stores large files out of band |
| `podman-tui` | 2.0.0 | Terminal UI for podman |
| `rclone` | 1.75.1 | Syncs files between local disks and remote storage, and verifies copies with rclone check |
| `restic` | 0.19.1 | Deduplicating backup program, a single static binary |
| `shellcheck` | 0.11.0 | Static analysis for sh and bash scripts — quoting, word splitting and portability |
| `shfmt` | 3.14.1 | Shell parser, formatter and linter |
| `syncthing` | 2.1.5 | Continuous peer-to-peer file synchronisation |

### The machine's own hardware, its GPUs and its damaged disks

| Port | Version | Description |
|---|---|---|
| `espeak-ng` | 1.52.0 | Speech synthesis for over 100 languages, in about 15 MB |
| `nvtop` | 3.3.2 | GPU utilisation from DRM fdinfo — top for every vendor's GPU |
| `pcaudiolib` | 1.3 | The audio output layer espeak-ng speaks through |
| `sleuthkit` | 4.15.0 | Filesystem forensics tools that read disk images directly |
| `valkey` | 9.1.2 | In-memory key-value store — the BSD continuation of Redis |
| `yara` | 4.5.8 | Rule-based pattern matching for classifying files |

### Is the hardware dying, and can this machine reach it

| Port | Version | Description |
|---|---|---|
| `dmidecode` | 3.7 | Prints the SMBIOS table in readable form |
| `dotconf` | 1.4.1 | Configuration file parser (speech-dispatcher dependency) |
| `efibootmgr` | 18 | Lists, adds and reorders EFI boot entries |
| `efivar` | 39 | Read and write EFI variables — the library efibootmgr needs |
| `ethtool` | 7.1 | Network interface settings — link state, ring sizes, offloads |
| `f3` | 10.0 | Tests a flash drive's real capacity, detecting counterfeit drives |
| `fprintd` | 1.94.5 | The fingerprint daemon and its PAM module, so a reader can unlock a session |
| `gusb` | 0.4.9 | GObject wrapper over libusb — libfprint's device layer |
| `hdparm` | 9.65 | Set and read ATA power management, spindown and secure erase |
| `hostapd` | 2.12 | Wireless access point daemon |
| `iw` | 6.17 | Wireless device configuration over nl80211, including monitor mode |
| `libfprint` | 1.94.100 | Fingerprint reader drivers |
| `lm-sensors` | 3.6.2 | Hardware sensor readings — temperatures, fans and voltages from hwmon |
| `lynis` | 3.1.7 | Host security auditing tool |
| `mdadm` | 4.6 | Linux software RAID management |
| `nvme-cli` | 3.1 | NVMe management — the drive's log pages, SMART data and self-tests |
| `openconnect` | 9.21 | Client for AnyConnect, GlobalProtect, Fortinet and Pulse VPNs |
| `picocom` | 3.1 | Minimal serial terminal |
| `smartmontools` | 7.5 | SMART attribute reporting and disk self-tests |
| `speech-dispatcher` | 0.12.1 | Speech synthesis server for screen readers, with espeak-ng as its voice |
| `thermald` | 2.5.13 | Intel thermal daemon — keeps a laptop off its throttle point under load |
| `thin-provisioning-tools` | 1.3.4 | thin_check and its family, without which LVM refuses to activate a thin pool |
| `tpm2-tools` | 5.8 | The tpm2_* commands — seal a LUKS key to the chip and unseal it at boot |
| `tpm2-tss` | 4.2.0 | The TPM2 software stack — what talks to the chip that can hold a disk key |
| `vpnc-script` | 20240208 | Routing and DNS script openconnect runs when a tunnel connects |

### Reading a repository, a log, and a code on a screen

| Port | Version | Description |
|---|---|---|
| `lazydocker` | 0.25.2 | lazydocker — terminal UI for containers, images and logs |
| `lnav` | 0.14.1 | Log navigator with SQL over log lines |
| `qrencode` | 4.1.1 | Write a QR code, including as terminal characters |
| `tig` | 2.6.1 | ncurses interface for reading a git repository |
| `toot` | 0.52.1 | toot — the Fediverse from a terminal, as a TUI and as a command |
| `universal-ctags` | 6.2.1 | Source-code tag index generator |
| `zbar` | 0.23.93 | Read a QR or barcode from an image or a camera |
| `zsh` | 5.9.2 | Z shell, with an extensive completion system |

### Editors / TUI

| Port | Version | Description |
|---|---|---|
| `lf` | 42 | lf — terminal file manager (Go, single binary) |
| `mc` | 4.8.33 | GNU Midnight Commander — Norton-style TUI file manager |
| `micro` | 2.0.15 | micro — terminal text editor (Go) |
| `neovim` | 0.12.5 | neovim — extensible Vim-based text editor |

### Debug / trace / profile

| Port | Version | Description |
|---|---|---|
| `gdb` | 17.2 | GNU Debugger |
| `iftop` | 1.0pre4 | iftop — display bandwidth usage on a network interface |
| `iotop` | 1.31 | iotop — top-like utility for I/O (C rewrite) |
| `lsof` | 4.99.7 | lsof — lists open files held by running processes |
| `ltrace` | 0.8.1 | ltrace — library call tracer |
| `strace` | 7.2 | strace — system call tracer |
| `valgrind` | 3.27.1 | valgrind — instrumentation framework for memory debugging and profiling |

### Network tools

| Port | Version | Description |
|---|---|---|
| `cifs-utils` | 7.7 | mount.cifs and the SMB/CIFS userspace helpers |
| `iperf3` | 3.21 | iperf3 — network bandwidth measurement tool |
| `krb5` | 1.22.2 | Kerberos 5 authentication libraries and tools |
| `ldns` | 1.9.2 | DNS library, the drill query tool and the ldns DNSSEC tools |
| `mosh` | 1.4.0 | mosh — mobile shell, a replacement for SSH over unreliable networks |
| `mtr` | 0.96 | mtr — combined ping and traceroute |
| `netcat` | 1.238 | openbsd-netcat — reads and writes data over TCP and UDP connections |
| `nmap` | 7.991 | nmap — Network exploration tool and security scanner |
| `openresolv` | 3.17.4 | resolvconf: one writer of /etc/resolv.conf for NetworkManager, dhcpcd and wg-quick |
| `socat` | 1.8.1.3 | socat — multipurpose relay (TCP/UNIX/SSL/etc.) |
| `sshfs` | 3.7.6 | Mount a remote directory over SSH with FUSE |
| `tcpdump` | 4.99.7 | tcpdump — command-line packet analyzer |
| `wireguard-tools` | 1.0.20260223 | wireguard-tools — userspace tooling (wg, wg-quick) for WireGuard VPN |

### Wayland base

| Port | Version | Description |
|---|---|---|
| `blind` | 1.1 | Video editing as a pipeline of small programs over raw frames |
| `bubblewrap` | 0.13.0 | Unprivileged sandboxing tool (bwrap), the sandbox xdg-desktop-portal decodes untrusted icons and sounds in |
| `cmus` | 2.12.0 | Console music player with a library view |
| `cryptsetup` | 2.8.8 | Used to set up transparent encryption of block devices using the kernel crypto API |
| `dav1d` | 1.5.4 | AV1 cross-platform decoder |
| `dnsmasq` | 2.93 | DNS forwarder, DHCP server and TFTP server (NetworkManager hotspot mode) |
| `duktape` | 2.7.0 | Embeddable JavaScript engine (used by polkit for rule processing) |
| `farbfeld` | 4 | Lossless image format designed for piping, and its converters |
| `foot` | 1.28.0 | Wayland terminal emulator |
| `geoclue` | 2.8.2 | D-Bus geolocation service (what xdg-desktop-portal's Location portal asks) |
| `glib-networking` | 2.90.0 | GIO modules for TLS and proxy resolution (the TLS backend every GIO and libsoup https connection needs) |
| `glmark2` | 2023.01 | Benchmark, in wayland, drm and gbm flavours of GL and GLESv2, all on EGL |
| `grim` | 1.5.0 | Screenshot tool for wlroots-based Wayland compositors |
| `gst-libav` | 1.28.7 | GStreamer FFmpeg-based codec plugins |
| `gst-plugins-bad` | 1.28.7 | GStreamer "bad" plugin set (less mature plugins; auto-detects deps) |
| `gst-plugins-base` | 1.28.7 | GStreamer base plugin set (provides gstreamer-pbutils, audio/video conversion, file I/O) |
| `gst-plugins-good` | 1.28.7 | GStreamer "good" plugin set (well-maintained LGPL plugins) |
| `gst-plugins-ugly` | 1.28.7 | GStreamer "ugly" plugin set (patent/license-encumbered codecs) |
| `gstreamer` | 1.28.7 | GStreamer streaming media framework — core library |
| `imv` | 5.0.1 | Image viewer for Wayland (Wayland-only build, meson) |
| `libdisplay-info` | 0.4.0 | EDID and DisplayID parser used by Wayland compositors |
| `libgrapheme` | 3.0.0 | Unicode string segmentation — grapheme clusters, words, sentences and line breaks |
| `libmad` | 0.16.4 | MPEG audio decoder library |
| `libnftnl` | 1.3.2 | Userspace library for low-level interaction with nf_tables (nftables dep) |
| `libnice` | 0.1.24 | ICE — how two WebRTC peers find a path to each other through NAT |
| `libnsbmp` | 0.1.7 | NetSurf BMP and ICO decoding library (imv decodes BMP with it) |
| `libsoup3` | 3.6.6 | GNOME HTTP client/server library, the 3.x API (GStreamer's souphttpsrc and adaptivedemux2, GeoClue) |
| `libsrtp` | 2.8.1 | Secure RTP library — the encrypted media transport of WebRTC calls |
| `mesa-demos` | 9.0.0 | Mesa EGL, GLES2 and Vulkan demos for Wayland — es2gears_wayland, eglinfo, vkgears |
| `mobile-broadband-provider-info` | 20251101 | Carrier APN database for mobile broadband (NetworkManager's WWAN connection wizard) |
| `modemmanager` | 1.24.2 | Mobile broadband modem daemon (3G/4G/5G over QMI, MBIM, QRTR and AT), mmcli and libmm-glib |
| `networkmanager-openvpn` | 1.12.5 | NetworkManager VPN plugin for OpenVPN |
| `nftables` | 1.1.7 | Netfilter packet filtering and classification framework (replaces iptables) |
| `openvpn` | 2.7.7 | SSL/TLS VPN daemon |
| `pam` | 1.7.2 | Linux Pluggable Authentication Modules (libpam + pam_unix against /etc/shadow) |
| `polkit` | 127 | Authorization framework for unprivileged processes (NetworkManager, fwupd, bolt, fprintd) |
| `ppp` | 2.5.4 | Point-to-Point Protocol daemon (PPPoE and serial modems for NetworkManager) |
| `sdl2-compat` | 2.32.72 | The SDL2 library, headers and build files, answering every SDL2 call through SDL3 |
| `sdl3` | 3.4.16 | Simple DirectMedia Layer 3 — windows, audio and input; SDL2 programs reach it through sdl2-compat |
| `seatd` | 0.9.3 | Minimal seat management daemon for non-systemd Wayland setups |
| `slurp` | 1.5.0 | Region selector for Wayland compositors (pairs with grim for area screenshots) |
| `vulkan-loader` | 1.4.363 | Khronos Vulkan ICD loader (libvulkan.so.1) — dispatches to Mesa venus/lavapipe ICDs |
| `vulkan-tools` | 1.4.363 | Khronos Vulkan tools — vkcube, vkcubepp and vulkaninfo, Wayland and KMS display WSI |
| `wayland` | 1.26.0 | Core Wayland protocol library (libwayland-server / libwayland-client) |
| `wayland-protocols` | 1.49 | Wayland protocol XML definitions (xdg-shell, layer-shell, etc.) |
| `wl-clipboard` | 2.3.0 | Wayland clipboard utilities (wl-copy, wl-paste) |
| `wpa_supplicant` | 2.12 | IEEE 802.1X / WPA/WPA2/WPA3 supplicant (Wi-Fi authentication backend for NetworkManager) |
| `xdg-desktop-portal` | 1.22.1 | Desktop portal D-Bus framework (Wayland screencast / file-picker integration) |

### Media & Imaging

| Port | Version | Description |
|---|---|---|
| `admesh` | 0.98.5 | STL mesh checking and repair tool |
| `android-file-transfer` | 4.5 | aft-mtp-mount — an Android phone's storage as a FUSE directory — and the aft-mtp-cli shell |
| `bolt` | 0.9.11 | Thunderbolt device manager, so a dock behind firmware security is authorised |
| `bsd-games` | 2.17 | Three of the BSD games — Colossal Cave, tetris and worm |
| `fastfetch` | 2.68.1 | fastfetch — system information tool, ships the KDOS banner config |
| `ffmpeg` | 9.0.2 | FFmpeg — record, convert, and stream audio/video |
| `ffmpegthumbnailer` | 2.3.1 | Video thumbnail generator for file managers |
| `frotz` | 2.55 | Z-machine interpreter — Infocom-era interactive fiction in a terminal |
| `fwupd` | 2.1.7 | Daemon and client for updating device firmware |
| `gn` | 0.2480 | Chromium's meta-build system, which writes ninja files from BUILD.gn |
| `gphoto2` | 2.5.32 | Tethered capture and card download from a prompt |
| `imagemagick` | 7.1.2.31 | ImageMagick — convert, edit, and compose raster images (provides `convert`) |
| `libcamera` | 0.7.2 | Camera stack that puts ISP pipelines and sensor control behind one API |
| `libgphoto2` | 2.5.34 | Drive a camera over PTP — the udev rules already grant the class |
| `libmtp` | 1.1.23 | Talk MTP to a phone or a player — the class gphoto2 cannot reach |
| `libnotify` | 0.8.8 | Desktop notification client library and notify-send |
| `libomemo-c` | 0.5.1 | The Signal double ratchet with OMEMO's wire format — end-to-end encryption for XMPP clients |
| `libsixel` | 1.10.5 | Sixel encoder and decoder library, for pictures in a terminal |
| `libstrophe` | 0.14.0 | The XMPP client library profanity speaks the protocol with |
| `moon-buggy` | 1.1.0 | Drive a buggy across the moon, jumping craters and shooting meteors |
| `nethack` | 5.0.0 | Roguelike dungeon game in characters, with saves under the player's own home |
| `ocrmypdf` | 17.12.1 | Adds an invisible OCR text layer to a scanned PDF, in place |
| `pdfium` | 7988 | Chromium's PDF renderer, as a shared library with its C headers |
| `printrun` | 2.2.0 | pronsole and printcore — send a sliced job to a printer over USB serial |
| `profanity` | 0.18.2 | Console XMPP client |
| `prosody` | 13.0.6 | XMPP server |
| `protobuf-c` | 1.5.2 | Protocol Buffers for C — the runtime library and the protoc-gen-c code generator |
| `python3-cffi` | 2.1.1 | Foreign function interface for calling C from Python |
| `python3-cryptography` | 50.0.1 | Cryptographic primitives and recipes for Python |
| `python3-lxml` | 6.1.3 | libxml2 and libxslt bindings for Python |
| `python3-maturin` | 1.15.0 | Build backend for Rust-based Python extensions |
| `python3-obd` | 0.7.3 | Reads and clears engine fault codes over an ELM327 OBD-II adapter |
| `python3-pikepdf` | 10.13.0.post1 | qpdf bindings — read and rewrite a PDF without rasterising it |
| `python3-pillow` | 12.3.0 | Python imaging library |
| `python3-pillow-heif` | 1.8.0 | HEIF and AVIF for Pillow, through libheif |
| `python3-pkgconfig` | 1.6.0 | Python interface to pkg-config — how uharfbuzz finds the system harfbuzz |
| `python3-platformdirs` | 4.11.12 | Where a python program's config, cache and data directories are |
| `python3-ply` | 3.11 | Lex and yacc for python — libcamera generates its control tables with it |
| `python3-pycparser` | 3.0 | A C parser in pure python — what cffi reads a header with |
| `python3-pydantic-core` | 2.46.5 | pydantic's validation core, in Rust — pinned to the version pydantic names |
| `python3-pypdfium2` | 5.13.0 | Python bindings to PDFium — render, read and edit a PDF from Python |
| `python3-pyserial` | 3.5 | Serial port access from Python |
| `python3-semantic-version` | 2.10.0 | Semantic-version parsing — what setuptools-rust compares against |
| `python3-setuptools-rust` | 1.13.0 | Builds a Rust extension from setup.py — how maturin bootstraps itself |
| `python3-uharfbuzz` | 0.56.2 | Cython bindings to HarfBuzz — text shaping from Python |
| `shaderc` | 2026.4 | glslc and libshaderc — GLSL and HLSL to SPIR-V, on the system glslang and SPIRV-Tools |
| `ttyper` | 1.6.0 | Terminal touch-typing practice with per-key statistics |
| `whisper.cpp` | 1.9.4 | Speech to text on the CPU or a Vulkan GPU, with the model on the disk and nothing sent anywhere |

### Audio from a prompt

| Port | Version | Description |
|---|---|---|
| `libao` | 1.2.2 | Audio output library over ALSA and PulseAudio |
| `libsamplerate` | 0.2.2 | Secret Rabbit Code — audio sample rate converter library |
| `rubberband` | 4.0.0 | Audio time-stretching and pitch-shifting library |
| `sox` | 14.4.2 | Audio conversion, resampling, filtering and analysis from the command line |
| `vorbis-tools` | 1.4.3 | oggenc, oggdec, ogginfo and vorbiscomment |

### PostScript and PDF

| Port | Version | Description |
|---|---|---|
| `cups-browsed` | 2.1.1 | Daemon that turns printers announced on the network into local CUPS queues |
| `cups-filters` | 2.0.1 | The filters CUPS needs to turn a job into something a printer prints |
| `ghostscript` | 10.08.0 | PostScript and PDF interpreter |
| `gutenprint` | 5.3.5 | Printer drivers for inkjet and dye-sublimation printers |
| `jbig2dec` | 0.20 | JBIG2 image decoder, the scanned-page codec in PDF |
| `libcupsfilters` | 2.2.1 | The filter functions behind every CUPS print filter, as a library |
| `libppd` | 2.1.1 | The PPD handling CUPS 3 drops, as a library for its filters and daemons |
| `mupdf` | 1.28.4 | MuPDF — the mutool PDF command-line tool and a viewer with no toolkit |
| `pdfio` | 1.6.5 | C library for reading and writing PDF files |
| `zxing-cpp` | 2.3.0 | Barcode and QR code reading and writing — what `mutool barcode` is built on |

### Scanning, and what is inside a media file

| Port | Version | Description |
|---|---|---|
| `ipp-usb` | 0.9.34 | A USB printer that speaks IPP-over-USB, presented to cups as a network one |
| `libmediainfo` | 26.05 | Media file inspection library — codec, profile, bit depth and every track |
| `libvips` | 8.18.6 | Streaming image-processing library with low memory use |
| `libzen` | 0.4.41 | ZenLib, the portability layer libmediainfo is written on |
| `mediainfo` | 26.05 | The command line over libmediainfo |
| `sane-airscan` | 0.99.38 | Driverless scanning over eSCL and WSD |
| `sane-backends` | 1.4.0 | The scanner driver set, and the udev rules 70-kdos-*.rules already grant |
| `wf-recorder` | 0.6.0 | Screen recorder for wlroots compositors, through wlr-screencopy |

### ASCII art (aa-project)

| Port | Version | Description |
|---|---|---|
| `aa3d` | 1.0 | ASCII art stereogram generator — renders a depth map as a magic eye |
| `aalib` | 1.4rc5 | ASCII art graphics library — renders true-color images to text |
| `aview` | 1.3.0rc1 | ASCII art image browser and FLI/FLC player for the terminal |
| `kdos-bb` | 1.3.0 | The AAlib demo, hard-forked and rebranded, with a threaded mixer |
| `libmikmod` | 3.3.14 | Tracker module player library — S3M, XM, IT, MOD |

### Container Layer (Podman + distrobox)

| Port | Version | Description |
|---|---|---|
| `aardvark-dns` | 2.1.0 | Authoritative DNS server for container networks (Rust) |
| `buildah` | 1.45.1 | Build OCI / Docker container images |
| `catatonit` | 0.2.1 | Static container init, for podman pods and --init |
| `conmon` | 2.2.1 | An OCI container runtime monitor (used by Podman) |
| `containers-common` | 1 | /etc/containers — the engine, storage, registry and image-trust configuration podman, buildah and skopeo share |
| `crun` | 1.29.1 | OCI container runtime written in C, with a small memory footprint |
| `distrobox` | 1.8.2.5 | Wrapper around podman or docker that creates mutable development containers |
| `fuse` | 3.18.3 | libfuse 3 — Filesystem in Userspace library and the setuid fusermount3 mount helper |
| `fuse-overlayfs` | 1.18 | An implementation of overlay+shiftfs in FUSE for rootless containers |
| `libslirp` | 4.9.5 | libslirp — user-mode networking library (TCP/IP emulator) used by slirp4netns/qemu |
| `netavark` | 2.1.0 | Container network stack (network backend for Podman, Rust) |
| `podman` | 6.1.2 | Daemonless container engine for OCI containers |
| `skopeo` | 1.24.1 | Inspect, copy, and sign container images and image registries |
| `slirp4netns` | 1.3.5 | User-mode networking for unprivileged network namespaces |

### X11 compatibility (Xwayland only — no Xorg server)

| Port | Version | Description |
|---|---|---|
| `font-adobe-75dpi` | 1.0.4 | X.org core bitmap fonts, served by Xwayland to its X11 clients |
| `font-cursor-misc` | 1.0.4 | X.org core bitmap fonts, served by Xwayland to its X11 clients |
| `font-misc-misc` | 1.1.3 | X.org core bitmap fonts, served by Xwayland to its X11 clients |
| `hicolor-icon-theme` | 0.18 | Freedesktop.org Hicolor icon theme |
| `xwayland` | 24.1.13 | Rootless X server that runs X11 clients on a Wayland compositor |

### Firmware (regulatory database, Intel SOF audio)

| Port | Version | Description |
|---|---|---|
| `sof-firmware` | 2026.09.1 | Intel Sound Open Firmware DSP firmware and topologies |
| `wireless-regdb` | 2026.09.03 | Signed wireless regulatory database — without it the kernel applies the restrictive world domain |

### Time zones

| Port | Version | Description |
|---|---|---|
| `tzdata` | 2026d | IANA time zone database — musl reads /usr/share/zoneinfo natively |

### Device access libraries (HID, USB)

| Port | Version | Description |
|---|---|---|
| `hidapi` | 0.15.0 | HID device access — CMSIS-DAP debuggers and instruments speak it |
| `libzip` | 1.11.4 | C library for reading and writing zip archives |

### Colour management and codecs

The list's last group holds more than its title says: after the codecs it carries the VNC server
and its libraries, `weechat`, `whois`, `passt`, `libedit`, `lldb`, `perf` and KDOS's own phase-4
ports (`kdos-tools`, `kdos-appbox`, `kdos-boxinit`, `kdos-splash` and the theme packages).

| Port | Version | Description |
|---|---|---|
| `aml` | 1.0.0 | Event loop library for main-loop driven Wayland services |
| `flac` | 1.5.0 | Free Lossless Audio Codec library and tools |
| `intel-gmmlib` | 22.10.2 | Intel Graphics Memory Management Library for the media driver |
| `intel-media-driver` | 26.3.5 | Intel iHD VA-API driver for Broadwell and newer graphics |
| `kdos-appbox` | 1.2.0 | KDOS alien app runtime and appbox manager |
| `kdos-boxinit` | 0.1.0 | pid 1 inside a pack box, in place of distrobox-init |
| `kdos-cursors` | 2.0 | KDOS phosphor cursor theme |
| `kdos-gtk-theme` | 6.5 | KDOS phosphor GTK theme — a recoloured adw-gtk3 |
| `kdos-icons` | 20250501 | KDOS phosphor icon theme — a recoloured Papirus |
| `kdos-splash` | 1.1 | Boot splash — CRT power-on animation drawn on the framebuffer |
| `kdos-theme` | 1.0.0 | KDOS theme generators — GTK stylesheet, icons, cursors |
| `kdos-tools` | 1.2.0 | KDOS system tools — kdos, services, getty, screenshots, fetch |
| `lame` | 4.0 | LAME MP3 encoder |
| `lcms2` | 2.19.1 | Little CMS colour management engine — applies ICC profiles |
| `libass` | 0.17.5 | SSA/ASS subtitle renderer — what lets ffmpeg burn subtitles in |
| `libdatrie` | 0.2.14 | Double-array trie — the dictionary structure libthai looks words up in |
| `libde265` | 1.1.3 | HEVC decoder — libheif's decode half |
| `libedit` | 20260512 | NetBSD line editor with history, key bindings and tab completion |
| `libheif` | 1.23.5 | HEIF and AVIF image reader and writer |
| `liblc3` | 1.1.3 | Low Complexity Communication Codec, the mandatory codec of Bluetooth LE Audio |
| `libogg` | 1.3.6 | Ogg container library |
| `libthai` | 0.1.30 | Thai word segmentation — where pango may wrap a line of Thai |
| `libunibreak` | 8.0 | Unicode line, word and grapheme breaking — where libass may wrap a subtitle |
| `libva` | 2.24.1 | VA-API — hardware video decode and encode |
| `libva-intel-driver` | 2.4.1 | Intel i965 VA-API driver for GMA 4500 through Coffee Lake — the one Sandy Bridge, Ivy Bridge and Haswell decode with |
| `libva-utils` | 2.24.0 | VA-API utilities — vainfo and the conformance test programs |
| `libvorbis` | 1.3.7 | Vorbis audio codec |
| `libvpx` | 1.17.0 | VP8 and VP9 codecs — BSD, so this one does not relicense ffmpeg |
| `lldb` | 23.1.2 | Debugger from the LLVM project, for C, C++, Rust and Zig |
| `neatvnc` | 1.0.1 | VNC server library |
| `openexr` | 3.5.0 | High dynamic range image format library |
| `openjpeg` | 2.5.4 | JPEG 2000 codec — what scanners, archives and PDFs put images in |
| `opus` | 1.6.1 | Opus audio codec for voice and music |
| `passt` | 2026_07_28.f8df3f1 | User-mode networking for VMs and containers, unprivileged and without a tap device |
| `perf` | 7.2.7 | The kernel's own profiler: sampling, counters, tracepoints and BPF |
| `svt-av1` | 4.2.0 | AV1 encoder (BSD-3 licence) |
| `wayvnc` | 0.10.1 | VNC server for wlroots compositors, serving the session over the network |
| `weechat` | 4.10.1 | Extensible IRC client for the terminal, with a relay and scripting |
| `whois` | 5.6.6 | WHOIS client for domains, IP blocks and AS numbers |
| `x264` | 20250608.1624 | H.264 encoder — GPL-2+, and it relicenses the ffmpeg that links it |
| `x265` | 4.2 | HEVC encoder — GPL-2+ |

## The desktop phase

`script/05_desktop/packages.txt` installs the desktop: wlroots, the compositor, the shell, the
terminal, the lock screen, the root daemons, the pack tools, the input method and the portals. It is
the only phase that resolves against `src/desktop`. The list opens with an unheaded block before
its one named group.

### Unheaded: wlroots, the compositor, box socket, shell, terminal and lock screen

The block holds more than its title says: the list names `xcb-util-wm` first, the ICCCM and
EWMH helpers that wlroots' Xwayland window manager needs.

| Port | Version | Description |
|---|---|---|
| `kdos-boxsock` | 0.1.0 | Per-box tagged Wayland socket — the security-context-v1 engine |
| `kdos-comp` | 0.20.0 | The KDOS compositor — a hard fork of labwc 0.20.0 |
| `kdos-lock` | 0.2.0 | The KDOS lock screen and its setuid password checker |
| `kdos-shell` | 0.2.0 | The KDOS shell — a character-cell panel on layer-shell |
| `kdos-term` | 0.1.0 | KDOS terminal emulator |
| `wlroots` | 0.20.2 | Modular Wayland compositor library — the base kdos-comp is built on |
| `xcb-util-wm` | 0.4.2 | ICCCM and EWMH helpers for XCB — wlroots' Xwayland xwm needs both |

### The resource monitor

The file's one named group holds more than its title says: besides `kdos-res` it holds the root
daemons, the pack tools, the screen recorder, the input method and both portals.

| Port | Version | Description |
|---|---|---|
| `erofs-utils` | 1.9.4 | Tools for the EROFS read-only filesystem KDOS packs are built as |
| `fcitx5` | 5.1.22 | Input method framework, Wayland only on KDOS |
| `fcitx5-anthy` | 5.1.11 | Japanese input for fcitx5, via anthy-unicode |
| `fcitx5-chinese-addons` | 5.1.14 | Pinyin, shuangpin and table input for fcitx5 |
| `fcitx5-hangul` | 5.1.11 | Korean input for fcitx5, via libhangul |
| `kdos-energyd` | 0.1.0 | Relative per-app Energy Impact from RAPL, attributed by container |
| `kdos-mountd` | 0.1.0 | Removable media for a desktop that is not root |
| `kdos-oomd` | 0.1.0 | PSI-triggered memory-pressure killer that spares the desktop |
| `kdos-pack` | 0.1.0 | Build, sign, index and delta packs |
| `kdos-packd` | 0.1.0 | Mounts packs, composes box root filesystems, verifies signatures |
| `kdos-powerd` | 0.1.0 | Suspend, poweroff and reboot for a desktop that is not root |
| `kdos-record` | 0.1.0 | Screen recorder: the ScreenCast portal into a GStreamer pipeline |
| `kdos-res` | 0.2.0 | KDOS Resources — per-device pages, a process table and an application rollup |
| `xdg-desktop-portal-kdos` | 0.3.0 | FileChooser, Settings, AppChooser and Access portal backend for KDOS |
| `xdg-desktop-portal-wlr` | 0.8.4 | wlroots backend for xdg-desktop-portal (ScreenCast and Screenshot) |

## The kernel phase

`script/05_phase5/packages.txt` names one port, the kernel. Phases run in sorted directory order,
so `05_phase5` runs after `05_desktop` (whose own list header calls it "Phase 5: The desktop") and
immediately before `06_packaging`.

| Port | Version | Description |
|---|---|---|
| `linux` | 7.2.7 | Linux kernel |

## Ports named in more than one list

A port named by more than one list is catalogued above under its first listing. Most of these are
toolchain and base ports the bootstrap installs early and a later phase installs again against the
finished toolchain. `toybox` is a single binary that provides many small commands, some of which
other packages also provide (`cmp`, `readelf`, `strings`, `gunzip` and others). The block after
`toybox` in phase 4 reinstalls those packages, so that after a `toybox` upgrade the names belong to
the full tools again (see
[toybox and the tools it overlaps](../03-architecture/packaging.md#toybox-and-the-tools-it-overlaps)).

| Port | Named in |
|---|---|
| `attr` | phase 3 (“System Infrastructure”), phase 4 (“Core Build Utilities (host-side)”) |
| `autoconf` | phase 3 (“Build Toolchain”), phase 4 (“Core Build Utilities (host-side)”) |
| `automake` | phase 3 (“Build Toolchain”), phase 4 (“Core Build Utilities (host-side)”) |
| `bash` | phase 3 (“Essential Utilities”), phase 4 (“Core Build Utilities (host-side)”) |
| `bc` | phase 3 (“System Infrastructure”), phase 4 (“Core Build Utilities (host-side)”) |
| `binutils` | phase 2 (unheaded), phase 3 (“Build Toolchain”), phase 4 (“Core Build Utilities (host-side)”) |
| `bison` | phase 3 (“Build Toolchain”), phase 4 (“Core Build Utilities (host-side)”) |
| `bzip2` | phase 3 (“Essential Utilities”), phase 4 (“Core Build Utilities (host-side)”) |
| `diffutils` | phase 2 (unheaded), phase 3 (“Essential Utilities”), phase 4 (“Core Build Utilities (host-side)”) |
| `elfutils` | phase 3 (“System Infrastructure”), phase 4 (“Core Build Utilities (host-side)”) |
| `eudev` | phase 3 (“System Infrastructure”), phase 4 (“Core Build Utilities (host-side)”) |
| `findutils` | phase 3 (“Essential Utilities”), phase 4 (“Core Build Utilities (host-side)”) |
| `flex` | phase 3 (“Build Toolchain”), phase 4 (“Core Build Utilities (host-side)”) |
| `gawk` | phase 2 (unheaded), phase 3 (“Essential Utilities”) |
| `gcc` | phase 2 (unheaded), phase 3 (“Build Toolchain”) |
| `gettext` | phase 3 (“System Infrastructure”), phase 4 (“Core Build Utilities (host-side)”) |
| `git` | phase 3 (“Network & Security”), phase 4 (“Power-User CLI base”) |
| `gzip` | phase 3 (“Essential Utilities”), phase 4 (“Core Build Utilities (host-side)”) |
| `intltool` | phase 3 (“System Infrastructure”), phase 4 (“Core Build Utilities (host-side)”) |
| `kmod` | phase 3 (“System Infrastructure”), phase 4 (“Core Build Utilities (host-side)”) |
| `libcap` | phase 3 (“System Infrastructure”), phase 4 (“Core Build Utilities (host-side)”) |
| `libtool` | phase 3 (“Build Toolchain”), phase 4 (“Core Build Utilities (host-side)”) |
| `m4` | phase 2 (unheaded), phase 3 (“Build Toolchain”) |
| `make` | phase 3 (“Build Toolchain”), phase 4 (“Core Build Utilities (host-side)”) |
| `musl` | phase 2 (unheaded), phase 3 (“Build Toolchain”) |
| `ncurses` | phase 3 (“Base Libraries & Database”), phase 4 (“Core Build Utilities (host-side)”) |
| `pkgconf` | phase 3 (“Build Toolchain”), phase 4 (“Core Build Utilities (host-side)”) |
| `python3` | phase 3 (“Languages & Runtimes”), phase 4 (“Core Build Utilities (host-side)”) |
| `readline` | phase 3 (“Base Libraries & Database”), phase 4 (“Core Build Utilities (host-side)”) |
| `sed` | phase 3 (“Essential Utilities”), phase 4 (“Core Build Utilities (host-side)”) |
| `shadow` | phase 3 (“System Infrastructure”), phase 4 (“Core Build Utilities (host-side)”) |
| `sqlite` | phase 3 (“Base Libraries & Database”), phase 4 (“Core Build Utilities (host-side)”) |
| `tar` | phase 2 (unheaded), phase 3 (“Essential Utilities”) |
| `toybox` | phase 3 (“Essential Utilities”), phase 4 (“Core Build Utilities (host-side)”) |
| `util-linux` | phase 3 (“System Infrastructure”), phase 4 (“Core Build Utilities (host-side)”) |
| `xz` | phase 3 (“Essential Utilities”), phase 4 (“Core Build Utilities (host-side)”) |
| `zlib` | phase 2 (unheaded), phase 3 (“Base Libraries & Database”) |

## Installed as dependencies

No list names these 236 ports. Each is installed because a port that is installed names it in
`depends =`, directly or through another dependency. They are grouped here by what they are for;
the grouping is this chapter's, not the tree's.

### C and C++ support libraries

| Port | Version | Description |
|---|---|---|
| `abseil-cpp` | 20260817.0 | Abseil — Google's C++ common libraries (strings, containers, synchronization, time) |
| `argp-standalone` | 1.4.1 | Standalone version of arguments parsing functions from GLIBC |
| `basu` | 0.2.1 | sd-bus library extracted from systemd (no-systemd sd-bus provider) |
| `boost` | 1.92.0 | Boost C++ libraries — the headers and eight compiled components |
| `gdbm` | 1.26 | The GNU Database Manager |
| `highway` | 1.4.0 | Performance-portable SIMD library with runtime CPU dispatch |
| `imath` | 3.2.3 | C++ and python library of 2D and 3D vector, matrix, and math operations for computer graphics |
| `libbsd` | 0.12.2 | Functions commonly found on BSD systems, such as strlcpy() |
| `libevent` | 2.1.13 | Asynchronous event notification software library |
| `libmd` | 1.2.0 | Message Digest functions from BSD systems |
| `libuv` | 1.52.1 | Multi-platform support library with a focus on asynchronous I/O |
| `musl-fts` | 1.2.7 | Implementation of fts(3) for musl libc |
| `musl-obstack` | 1.2.3 | Obstack standalone library |
| `musl-rpmatch` | 1.0 | Implementation of rpmatch(3) for musl libc |
| `nlohmann-json` | 3.12.0 | JSON for Modern C++, header-only |
| `popt` | 1.19 | Popt libraries which are used by some programs to parse command-line options |
| `protobuf` | 36.2 | Protocol Buffers — Google's language-neutral data interchange format |
| `talloc` | 2.5.0 | Hierarchical reference-counted memory pool library from Samba |
| `tllist` | 1.1.0 | C header-only typed linked list library (foot/fcft dep) |
| `xxhash` | 0.8.4 | Non-cryptographic hash algorithm library |
| `z3` | 5.1.0 | Microsoft Research's SMT solver and theorem prover — libz3, the z3 command and its Python bindings |

### Arbitrary-precision arithmetic

| Port | Version | Description |
|---|---|---|
| `gmp` | 6.3.0 | Arbitrary-precision arithmetic library |
| `mpc` | 1.4.1 | A library for the arithmetic of complex numbers with arbitrarily high precision |
| `mpfr` | 4.2.2 | Functions for multiple precision math |

### Text, Unicode and data formats

| Port | Version | Description |
|---|---|---|
| `fribidi` | 1.0.17 | Implementation of the Unicode Bidirectional Algorithm (BIDI) |
| `hwdata` | 0.411 | Hardware identification and configuration data |
| `icu` | 78.3 | International Components for Unicode library |
| `inih` | r62 | Tiny C library for parsing INI files (used by xdg-desktop-portal-wlr config) |
| `iso-codes` | 4.20.1 | List of country, language and currency names |
| `jansson` | 2.15.1 | C library for encoding/decoding/manipulating JSON (NetworkManager dep) |
| `json-c` | 0.19 | JSON implementation in C |
| `json-glib` | 1.10.8 | JSON parser/serializer library built on GLib (used by xdg-desktop-portal) |
| `libical` | 4.0.5 | Reference implementation of the iCalendar and vCard formats |
| `libpaper` | 2.3.0 | Library and paper(1) tool for the system's default paper size and the known paper sizes |
| `libunistring` | 1.4.2 | Library for manipulating Unicode strings and C strings |
| `libxmlb` | 0.3.29 | Library to help create and query binary XML blobs |
| `pcre2` | 10.48 | Perl Compatible Regular Expressions library, version 2 |
| `shared-mime-info` | 2.5.1 | A MIME database |
| `utf8proc` | 2.11.3 | Unicode normalization, casefolding, etc. (fcft dep) |
| `yaml` | 0.2.5 | C library for parsing and emitting YAML |

### Compression

| Port | Version | Description |
|---|---|---|
| `brotli` | 1.2.0 | Brotli compression library |
| `libaec` | 1.1.7 | Adaptive Entropy Coding library and its libsz drop-in replacement for SZIP |
| `libdeflate` | 1.26 | DEFLATE/zlib/gzip compression library |
| `unzip` | 6.0 | ZIP extraction utilities |

### Build, documentation and introspection tools

| Port | Version | Description |
|---|---|---|
| `asciidoctor` | 2.0.26 | AsciiDoc processor in Ruby — HTML, DocBook and manual pages from .adoc |
| `bmake` | 20260912 | NetBSD make, portable — the make that BSD-style makefiles need |
| `buildsystem` | 1.10 | NetSurf shared build framework (Makefile fragments used by libnsgif and friends) |
| `doxygen` | 1.18.0 | Documentation system for C++, C, Java, Objective-C, Python, IDL, PHP, C# |
| `extra-cmake-modules` | 6.30.0 | Extra modules and scripts for CMake (KDE) |
| `glib-introspection` | 2.90.0 | GLib's introspection data: the GLib, GObject, GModule, Gio and GIRepository GIR and typelib files |
| `gnu-efi` | 4.0.4 | GNU EFI library |
| `go-md2man` | 2.0.7 | Converts Markdown to roff manual pages |
| `gobject-introspection` | 1.86.0 | GObject introspection: g-ir-scanner, g-ir-compiler, libgirepository-1.0 and the base GIR/typelib data (cairo, freetype2, fontconfig, libxml2, GL, DBus) |
| `musl-ldd` | 1.2.5 | LDD script for Musl |
| `lowdown` | 3.2.1 | Markdown translator to roff manual pages, HTML and LaTeX |
| `sgml-common` | 0.6.3 | Creating and maintaining centralized SGML catalogs |

### Language runtimes

| Port | Version | Description |
|---|---|---|
| `luajit` | 20260914 | Just-in-time compiler and drop-in replacement for Lua. |
| `ruby` | 4.0.7 | The Ruby programming language |

### Python modules

| Port | Version | Description |
|---|---|---|
| `python3-astropy-iers-data` | 0.2026.9.21.0.56.25 | The IERS Earth-orientation and leap-second tables astropy reads, frozen at release |
| `python3-charset-normalizer` | 3.5.1 | Character encoding detection for undeclared text |
| `python3-click` | 8.5.0 | Composable command-line interfaces for python programs |
| `python3-click-log` | 0.4.0 | Logging integration for click command-line programs |
| `python3-configobj` | 5.0.9 | INI-style configuration files with validation, for python programs |
| `python3-contourpy` | 1.4.0 | Contour lines and filled contours over a 2D grid — matplotlib's contouring |
| `python3-cppy` | 1.3.1 | C++ headers for writing CPython extensions — kiwisolver builds with it |
| `python3-cycler` | 0.12.1 | Composable style cycles — matplotlib's property cycle |
| `python3-dateutil` | 2.9.0.post0 | Date parsing, relative deltas and time zones — pandas and matplotlib import it |
| `python3-docutils` | 0.23 | Set of tools for processing plaintext docs into formats such as HTML, XML, or LaTeX |
| `python3-gmpy2` | 2.3.1 | Multiple-precision integers, rationals, reals and complex numbers from python, through GMP, MPFR and MPC |
| `python3-httplib2` | 0.32.0 | An HTTP client with caching, keep-alive and digest authentication |
| `python3-idna` | 3.20 | Internationalised domain names in applications (IDNA 2008 and UTS #46) |
| `python3-kiwisolver` | 1.5.1 | Cassowary constraint solver — matplotlib's layout engine |
| `python3-mako` | 1.4.3 | Templating library for Python |
| `python3-markdown-it-py` | 4.2.0 | CommonMark parser for python — what rich renders Markdown with |
| `python3-markupsafe` | 3.0.3 | Markup-safe string type for XML, HTML and XHTML in Python |
| `python3-mdurl` | 0.1.2 | URL parsing and formatting for markdown-it-py |
| `python3-pefile` | 2024.8.26 | Read and rewrite the headers of a PE executable, the format of every EFI binary |
| `python3-pip` | 26.2.1 | Python package installer |
| `python3-psutil` | 7.2.2 | Processes, CPU, memory, disks and network from python |
| `python3-pygments` | 2.21.0 | Python syntax highlighter |
| `python3-pyparsing` | 3.3.3 | Grammar-based text parsing — matplotlib's mathtext and fontconfig patterns |
| `python3-pytz` | 2026.4 | The Olson time zone database for python datetime |
| `python3-requests` | 2.34.2 | HTTP client library for Python |
| `python3-rich` | 15.0.0 | Colour, tables and progress bars in a terminal, for python programs |
| `python3-setuptools` | 84.0.0 | Library for building, packaging and installing Python projects |
| `python3-six` | 1.17.0 | Python 2 and 3 compatibility shims — python-dateutil imports it |
| `python3-sphinx` | 9.1.0 | Documentation generator — the manual pages of LLVM, CMake, mpd and flashrom |
| `python3-typing-extensions` | 4.16.0 | Backported and experimental type hints for Python's typing module |
| `python3-urllib3` | 2.8.0 | HTTP client with connection pooling, retries and TLS verification |
| `python3-urwid` | 4.1.7 | Console user interface library for python |
| `python3-wcwidth` | 0.9.1 | The column width of a Unicode string in a terminal |
| `python3-wheel` | 0.48.0 | The reference implementation of the Python wheel format |
| `python3-yaml` | 6.0.3 | Python bindings for YAML, using libyaml |

### Perl modules

| Port | Version | Description |
|---|---|---|
| `perl-font-ttf` | 1.06 | Read, edit and write TrueType and OpenType font tables from perl |
| `perl-io-string` | 1.08 | Emulate a file handle over an in-memory string in perl |
| `perl-uri` | 5.37 | URI class for Perl |
| `perl-xml-parser` | 2.59 | Expat-based XML parser module for perl |

### Cryptography, keys and smart cards

| Port | Version | Description |
|---|---|---|
| `gnupg` | 2.5.24 | GNU Privacy Guard — OpenPGP encryption and signing |
| `gnutls` | 3.8.13 | Libraries and userspace tools which provide a secure layer over a reliable transport layer |
| `gpgme` | 2.2.0 | C library that allows cryptography support to be added to a program |
| `gpgmepp` | 2.2.0 | GpgME++ — the C++ binding to GnuPG Made Easy |
| `keyutils` | 1.6.3 | Tools to control the Linux key management system |
| `libassuan` | 3.0.2 | Inter process communication library used by some of the other GnuPG related packages |
| `libcap-ng` | 0.9.6 | Alternate POSIX capabilities library (required by openvpn 2.6+) |
| `libgcrypt` | 1.12.4 | A general purpose crypto library based on the code used in GnuPG |
| `libgpg-error` | 1.61 | Library that defines common error values for all GnuPG components |
| `libksba` | 1.8.1 | Library for X.509 certificates and CMS (Cryptographic Message Syntax) |
| `libseccomp` | 2.6.1 | High-level userspace seccomp interface |
| `libtasn1` | 4.21.0 | Portable C library that encodes and decodes DER/BER data following an ASN.1 schema |
| `nettle` | 4.0 | Low-level cryptographic library |
| `npth` | 1.8 | Portable POSIX/ANSI-C based library for Unix platforms which provides non-preemptive priority-based scheduling for multiple threads of execution (multithreading) inside event-driven applications |
| `p11-kit` | 0.26.5 | Provides a way to load and enumerate PKCS #11 (a Cryptographic Token Interface Standard) modules |
| `pcsc-lite` | 2.5.2 | PC/SC smart card middleware — the pcscd daemon and libpcsclite |
| `pinentry` | 1.3.3 | Collection of simple PIN or pass-phrase entry dialogs which utilize the Assuan protocol |

### Networking

| Port | Version | Description |
|---|---|---|
| `c-ares` | 1.34.8 | C library for asynchronous DNS requests |
| `libidn` | 1.44 | Implementation of the Stringprep, Punycode and IDNA specifications |
| `libidn2` | 2.3.8 | Free software implementation of IDNA2008, Punycode and TR46 |
| `libmbim` | 1.34.0 | MBIM mobile broadband modem protocol library, mbimcli and mbim-proxy |
| `libmnl` | 1.0.5 | Minimal user-space library for Netlink |
| `libndp` | 1.9 | IPv6 Neighbor Discovery Protocol library (NetworkManager dep) |
| `libnl` | 3.12.0 | Netlink protocol library (kernel <-> userspace networking) |
| `libpcap` | 1.11.0 | A system-independent interface for user-level packet capture |
| `libpsl` | 0.23.3 | Library for accessing and resolving information from the Public Suffix List |
| `libqmi` | 1.38.0 | QMI mobile broadband modem protocol library, qmicli, qmi-proxy and qmi-firmware-update |
| `libqrtr-glib` | 1.4.0 | Qualcomm IPC Router (QRTR) protocol library on GLib (used by libqmi and ModemManager) |
| `libssh2` | 1.11.1 | Client-side C library implementing the SSH2 protocol |
| `libtirpc` | 1.3.8 | Libraries that support programs that use the Remote Procedure Call (RPC) API |
| `lynx` | 2.9.3 | Text-based web browser |
| `networkmanager` | 1.58.1 | Network connection manager (Wi-Fi, Ethernet, mobile broadband, VPN; libnm for the panel's network surface) |
| `newt` | 0.52.25 | Programming library for color text-mode widget UIs (provides libnewt for nmtui) |
| `nghttp2` | 1.70.0 | Hypertext Transfer Protocol version 2 implementation |
| `nghttp3` | 1.18.0 | HTTP/3 and QPACK library |
| `ngtcp2` | 1.25.0 | QUIC protocol library, with its OpenSSL crypto helper |
| `rpcsvc-proto` | 1.4.4 | rpcsvc protocol .x files and headers |

### Kernel, storage and system

| Port | Version | Description |
|---|---|---|
| `bpftool` | 7.7.0 | Inspect and manage BPF programs and maps, and generate BPF skeletons |
| `dwarves` | 1.32 | pahole and the DWARF tools — encodes the kernel's BTF type information |
| `fwupd-efi` | 1.8 | The EFI program fwupd boots into to hand a firmware capsule to the machine's firmware |
| `libaio` | 0.3.113 | The Linux-native asynchronous I/O facility (aio) library |
| `libewf` | 20240506 | Expert Witness Format (E01, Ex01) forensic image library, the ewf tools and ewfmount |
| `libnvme` | 1.16.2 | C library for NVM Express on Linux |
| `libtraceevent` | 1.9.0 | Parser for the kernel's ftrace event format |
| `libtracefs` | 1.8.3 | API for the tracefs filesystem |
| `liburing` | 2.15 | Linux kernel io_uring access library |
| `lvm2` | 2.03.42 | Logical volume management tools |
| `parted` | 3.8 | Disk partitioning and partition resizing tool |
| `slang` | 2.3.3 | S-Lang library — multi-platform programmer's library for text-mode UIs (newt dep) |
| `upower` | 1.91.4 | D-Bus daemon for power-related stats and policy (battery, AC, suspend hints) |
| `xdg-utils` | 1.2.1 | Command line tools that assist applications with a variety of desktop integration tasks |

### Devices and input

| Port | Version | Description |
|---|---|---|
| `libevdev` | 1.13.7 | Wrapper library for kernel evdev input devices |
| `libgudev` | 238 | GObject bindings for libudev (used by upower and NetworkManager) |
| `libinput` | 1.32.0 | library that handles input devices for display servers and other applications that need to directly deal with input devices |
| `libjaylink` | 0.5.0 | Library to drive SEGGER J-Link debug probes over USB and TCP |
| `libusb` | 1.0.30 | Library used by some applications for USB device access |
| `libwacom` | 2.20.0 | The tablet database libinput reads to pair a stylus with its pad and tell a pen display from a tablet |
| `libxkbcommon` | 1.13.2 | Keymap compiler and support library (Wayland-only build) |
| `mtdev` | 1.1.7 | Multitouch Protocol Translation Library which is used to transform all variants of kernel MT (Multitouch) events to the slotted type B protocol |
| `v4l-utils` | 1.32.0 | Video4Linux userspace — libv4l2 and libv4lconvert, libdvbv5, v4l2-ctl, media-ctl, ir-keytable, cec-ctl |
| `xkeyboard-config` | 2.48 | Keyboard configuration database (keymap data, used by libxkbcommon) |

### Graphics, GPU and text rendering

| Port | Version | Description |
|---|---|---|
| `cairo` | 1.18.6 | 2D graphics library (Wayland-only build, no X11) |
| `fcft` | 3.3.3 | Small library for font loading and glyph rasterization (foot/fuzzel dep) |
| `fontconfig` | 2.18.3 | A library and support programs used for configuring and customizing font access |
| `freetype2` | 2.14.3 | Font rasterization library |
| `glslang` | 16.6.0 | OpenGL and OpenGL ES shader front end and validator |
| `glu` | 9.0.3 | The OpenGL Utility library — GL/glu.h and libGLU |
| `harfbuzz` | 14.5.0 | OpenType text shaping engine |
| `libclc` | 23.1.2 | Library requirements of the OpenCL C programming language |
| `libdecor` | 0.2.5 | Client-side window decorations for Wayland clients |
| `libdrm` | 2.4.134 | Provides a user space library for accessing the DRM, direct rendering manager, on operating systems that support the ioctl interface |
| `libepoxy` | 1.5.10 | Library for handling OpenGL function pointer management |
| `libglvnd` | 1.7.0 | GL Vendor-Neutral Dispatch: EGL/GLES plus a GLX-less libGL.so |
| `libpciaccess` | 0.19 | X11 PCI access library |
| `mesa` | 26.2.3 | OpenGL compatible 3D graphics library (Wayland-only build) |
| `ocl-icd` | 2.3.5 | OpenCL ICD loader — libOpenCL.so dispatching to the drivers listed in /etc/OpenCL/vendors |
| `opencl-headers` | 2026.05.29 | Khronos OpenCL C API headers |
| `pango` | 1.58.2 | Text layout and rendering library (Wayland-only build, no X11/Xft) |
| `pixman` | 0.46.4 | Library that provides low-level pixel manipulation features such as image compositing and trapezoid rasterization |
| `plasma-wayland-protocols` | 1.22.0 | KDE Plasma Wayland protocol definitions |
| `spirv-headers` | 1.202609.0 | SPIR-V Headers |
| `spirv-llvm-translator` | 23.1.1 | Tool and library for translation between LLVM IR and SPIR-V |
| `spirv-tools` | 2026.4.rc2 | API and commands for processing SPIR-V modules |
| `vulkan-headers` | 1.4.363 | Vulkan header files |

### Image formats

| Port | Version | Description |
|---|---|---|
| `gdk-pixbuf` | 2.44.8 | gdk-pixbuf — image-loading library used by GTK and many notification daemons |
| `giflib` | 6.1.3 | Libraries for reading and writing GIFs as well as programs for converting and working with GIF files |
| `libavif` | 1.4.2 | AVIF reader and writer — the AV1 still-image format, with avifenc and avifdec |
| `libexif` | 0.6.26 | Library for parsing, editing, and saving EXIF data |
| `libgd` | 2.3.3 | GD graphics library — draws lines, shapes, text and images into PNG, JPEG, GIF, WebP, TIFF, HEIF and BMP |
| `libimagequant` | 4.4.1 | Palette quantisation that turns a 32-bit image into a small 8-bit PNG or GIF |
| `libjpeg-turbo` | 3.2.0 | A fork of the original IJG libjpeg which uses SIMD to accelerate baseline JPEG compression and decompression |
| `libjxl` | 0.12.0 | JPEG XL reference implementation — libjxl, cjxl, djxl and the gdk-pixbuf loader |
| `libnsgif` | 1.0.0 | NetSurf GIF decoding library (libkimg and imv decode GIF with it) |
| `libpng` | 1.6.58 | A collection of routines used to create PNG format graphics files |
| `librsvg` | 2.63.2 | SVG rendering library (Rust-based; used by imv for SVG support) |
| `libtiff` | 4.7.2 | TIFF libraries and associated utilities |
| `libwebp` | 1.6.0 | Library and support programs to encode and decode images in WebP format |
| `libyuv` | 0.0.1971 | YUV conversion, scaling and rotation — libavif converts colour through it |
| `zimg` | 3.0.6 | zimg image scaling, colour space and depth conversion library |

### Audio

| Port | Version | Description |
|---|---|---|
| `alsa-lib` | 1.2.16.1 | ALSA library used by programs (including ALSA Utilities) requiring access to the ALSA sound interface |
| `alsa-topology-conf` | 1.2.5.1 | ALSA topology configuration files |
| `alsa-ucm-conf` | 1.2.16.1 | ALSA Use Case Manager configuration (and topologies) |
| `faad2` | 2.11.3 | FAAD2 MPEG-2/MPEG-4 AAC decoder — libfaad and the faad command |
| `fdk-aac` | 2.0.3 | Fraunhofer AAC codec, used for AAC over Bluetooth |
| `ldacbt` | 2.0.2.6 | Sony LDAC encoder, for the Bluetooth headsets that negotiate it |
| `libfreeaptx` | 0.2.2 | Free aptX and aptX HD codec, for Bluetooth headsets that speak them |
| `libid3tag` | 0.16.4 | ID3 tag manipulation library (optional dep of imlib2 image audio metadata) |
| `libpulse` | 17.0 | PulseAudio client libraries and pactl/pacat — the server is pipewire-pulse |
| `libsndfile` | 1.2.2 | Library for reading and writing files containing sampled sound (WAV, AIFF, FLAC, etc.) |
| `mpg123` | 1.33.7 | MPEG audio decoder — libmpg123, libout123, libsyn123 and the mpg123 and out123 commands |
| `orc` | 0.4.44 | Oil Runtime Compiler — JIT for the SIMD inner loops of GStreamer's plugins |
| `pipewire` | 1.6.9 | Audio and video stream router (replaces PulseAudio; carries Wayland screencast) |
| `sbc` | 2.2 | Bluetooth Sub-Band Codec library (mandatory codec for A2DP audio) |
| `soxr` | 0.1.3 | SoX Resampler library — one-dimensional sample-rate conversion |
| `wavpack` | 5.9.0 | WavPack hybrid lossless audio codec — library and the wavpack, wvunpack, wvgain and wvtag commands |
| `webrtc-audio-processing` | 2.1 | WebRTC's audio processing module — echo cancellation, noise suppression, gain control |

### X11 client libraries and fonts for Xwayland

| Port | Version | Description |
|---|---|---|
| `bdftopcf` | 1.1 | Convert X font from Bitmap Distribution Format to Portable Compiled Format |
| `encodings` | 1.1.0 | X.org font encoding files |
| `font-util` | 1.4.2 | X.Org font utilities |
| `libfontenc` | 1.1.9 | Font encoding library |
| `libX11` | 1.8.13 | Core X11 client-side library |
| `libXau` | 1.0.12 | X11 authorisation protocol library |
| `libxcb` | 1.17.0 | X protocol C-language binding |
| `libxcvt` | 0.1.3 | Library providing a standalone version of the X server implementation of the VESA CVT standard timing modelines generator |
| `libXdmcp` | 1.1.5 | X Display Manager Control Protocol library |
| `libXfont2` | 2.0.9 | X font rasterisation library |
| `libxkbfile` | 1.2.0 | XKB file handling library |
| `libxshmfence` | 1.3.3 | Shared memory fences library |
| `mkfontscale` | 1.2.4 | Create an index of scalable font files for X |
| `util-macros` | 1.20.2 | X.Org Autotools macros |
| `xcb-proto` | 1.17.0 | X protocol XML descriptions (build-time) |
| `xcb-util-renderutil` | 0.3.10 | XCB convenience functions for the Render extension |
| `xkbcomp` | 1.5.0 | XKB keymap compiler, invoked by Xwayland at runtime |
| `xorgproto` | 2025.1 | X.Org protocol headers |
| `xtrans` | 1.6.0 | X transport library (headers only, build-time) |

### Input method engines

| Port | Version | Description |
|---|---|---|
| `anthy-unicode` | 1.0.0.20260213 | Japanese kana-kanji conversion engine, the maintained Anthy fork |
| `libhangul` | 0.2.0 | Hangul input processing library |
| `libime` | 1.1.16 | Pinyin and table input method engine library for fcitx5 |
| `opencc` | 1.4.2 | Conversion between Traditional and Simplified Chinese |

## Not installed

Nothing in any list reaches these 14 recipes through `depends =`, and no phase script builds
them, so none is on the image. Their sources are fetched and checked like any other port's, and
each builds on request with `kpkg install <name>` wherever the ports tree is on `PORT_REPO`, as it
is inside the build chroot.

| Port | Version | Description |
|---|---|---|
| `desktop-file-utils` | 0.28 | Command line utilities for working with Desktop entries |
| `double-conversion` | 3.4.0 | Binary-decimal and decimal-binary routines for IEEE doubles (Qt6 dep) |
| `helix` | 25.07.1 | helix — modal text editor (Rust) |
| `icon-naming-utils` | 0.8.90 | Perl script used for maintaining backwards compatibility with current desktop icon themes |
| `musl-locales` | 20260425 | A locale command and message catalogues for musl |
| `perl-xml-simple` | 2.25 | Perl module that reads and writes XML as nested data structures (config files especially) |
| `setconf` | 0.7.7 | Utility for changing settings in configuration files |
| `stemmer` | 3.1.1 | Stemming library supporting several languages |
| `uthash` | 2.4.0 | A hash table for C structures (header-only) |
| `volume_key` | 0.3.12 | Library for manipulating storage volume encryption keys and storing them separately from volumes to handle forgotten passphrases |
| `xcb-util` | 0.4.1 | XCB utility convenience functions |
| `xcb-util-cursor` | 0.1.6 | XCB cursor library (libxcb-cursor) |
| `xcb-util-image` | 0.4.1 | XCB port of Xlib XImage |
| `yajl` | 2.1.0 | Yet Another JSON Library — small, event-driven C JSON parser |

## See also

- [Packaging](../03-architecture/packaging.md) — recipes, the three repositories, phase lists and
  the `group =` key
- [The build system](../05-developer/build-system.md) — how phases are discovered, run and
  snapshotted
- [Writing ports](../05-developer/writing-ports.md) — every recipe key, and how to add a port to a
  list
- [Build troubleshooting](../05-developer/build-troubleshooting.md) — when a port in these tables
  fails to build
- [Repository layout](repository-layout.md) — where `ports/`, `script/` and `src/` sit in the tree

<!-- book-nav -->
---

*Part VI — Reference, chapter 38.* Previous: [37. Testing](../05-developer/testing.md) · [Contents](../README.md) · Next: [39. Command index](command-index.md)
