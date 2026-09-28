# The ports catalogue

This chapter lists every port KDOS can build: each of the 2,003 recipes in `ports/core` and the
24 recipes KDOS writes itself under `src/`, 2,027 in all, each exactly once. It is a reference for
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
none. Two kinds of recipe set it. A pair that builds one upstream source twice, or a program and
the data released beside it, shares a group: `glib` and `glib-introspection` (`glib`), `python3`
and `python3-tkinter` (`python3`), `gcc-arm-none-eabi` and `libstdcxx-arm-none-eabi`, `qca` and
`qca-qt5` (`qca`), `qwt` and `qwt-qt5` (`qwt`), `qscintilla` and `python3-qscintilla`
(`qscintilla`), `webkitgtk` and `webkitgtk6` (`webkitgtk`), `texlive` and `texlive-doc`
(`texlive`), `mgba` and `libretro-mgba` (`mgba`), and `supertuxkart` and `stk-assets`
(`supertuxkart`). A family released together from a host the derivation does not read carries one
key for all its members: the 29 Qt 6 modules carry `group = qt6` and the 11 Qt 5 modules
`group = qt5`, all fetched from `download.qt.io`. For any other
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

Counted from the five `packages.txt` files and the 2,027 `kpkgbuild` files. "Named" counts the
non-comment lines of a list; "catalogued here" counts the ports whose first listing is in that
phase.

| Where | Named | Catalogued here |
|---|---|---|
| Phases 0 and 1 (scripts) | none | 1 |
| [Phase 2: the self-hosting bootstrap](#phase-2-the-self-hosting-bootstrap) | 8 | 8 |
| [Phase 3: toolchain and core libraries](#phase-3-toolchain-and-core-libraries) | 98 | 90 |
| [Phase 4: userland, the Wayland base and the applications](#phase-4-userland-the-wayland-base-and-the-applications) | 1,687 | 1,656 |
| [The desktop phase](#the-desktop-phase) | 22 | 21 |
| [The kernel phase](#the-kernel-phase) | 1 | 1 |
| [Installed as dependencies](#installed-as-dependencies) | none | 244 |
| [Not installed](#not-installed) | none | 6 |
| Total | 1,816 | 2,027 |

The five lists name 1,776 distinct ports in 1,816 lines; 38 ports are named in more than
one list. Following `depends =` from those 1,776 names reaches 2,020 ports: all
1,776 listed ports and 244 more. With `kdos-installer`, which phase 1 builds by script,
that leaves 6 recipes in `ports/core` that no phase installs.

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
| Phase 4 | X11 client libraries | 25 |
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
| Phase 4 | Raw pixels, curves, and sound with no window | 15 |
| Phase 4 | Words, notes, and where the time went | 4 |
| Phase 4 | Playing, reading, scanning and keeping a record | 18 |
| Phase 4 | Mail — fetched, indexed once, read by anything | 29 |
| Phase 4 | Sequences, coordinates, and exact arithmetic | 15 |
| Phase 4 | The numeric foundation — everything in C2 stands on these | 6 |
| Phase 4 | The python numeric stack | 44 |
| Phase 4 | Astronomy — the sky, in the formats telescopes write | 4 |
| Phase 4 | Sequences — the bioinformatics core | 4 |
| Phase 4 | Synthesise, place, route and flash — the open FPGA flow, natively | 32 |
| Phase 4 | Grammars compiled here, a Rust server that can see std, and a private CA | 4 |
| Phase 4 | Two people talking, and two machines sharing a filesystem | 5 |
| Phase 4 | Inside the kernel, on the air, and against your own hashes | 8 |
| Phase 4 | The wire, the database, the web server, and reading the screen aloud | 10 |
| Phase 4 | Language servers, a diff you can read, and a repository you can undo | 7 |
| Phase 4 | Statistics, census, HTTP, bandwidth, and example-first help | 5 |
| Phase 4 | One index over the corpus | 10 |
| Phase 4 | Keeping the bytes, and moving them between two machines | 10 |
| Phase 4 | The machine's own hardware, its GPUs and its damaged disks | 6 |
| Phase 4 | Is the hardware dying, and can this machine reach it | 26 |
| Phase 4 | Reading a repository, a log, and a code on a screen | 10 |
| Phase 4 | Editors / TUI | 4 |
| Phase 4 | Debug / trace / profile | 7 |
| Phase 4 | Network tools | 13 |
| Phase 4 | Wayland base | 50 |
| Phase 4 | Media & Imaging | 54 |
| Phase 4 | Audio from a prompt | 5 |
| Phase 4 | PostScript and PDF | 11 |
| Phase 4 | Scanning, and what is inside a media file | 9 |
| Phase 4 | ASCII art (aa-project) | 5 |
| Phase 4 | Container Layer (Podman + distrobox) | 14 |
| Phase 4 | X11 compatibility (Xwayland only — no Xorg server) | 5 |
| Phase 4 | Firmware (regulatory database, Intel SOF audio) | 2 |
| Phase 4 | Time zones | 1 |
| Phase 4 | Device access libraries (HID, USB) | 2 |
| Phase 4 | Colour management and codecs | 33 |
| Phase 4 | GTK 3, GTK 4 and libadwaita | 39 |
| Phase 4 | The C++ bindings of GTK | 11 |
| Phase 4 | Hyphenation and the document fonts | 4 |
| Phase 4 | WebKitGTK | 4 |
| Phase 4 | Qt 6 | 28 |
| Phase 4 | Qt 5 | 15 |
| Phase 4 | KDE Frameworks 6 | 73 |
| Phase 4 | Shared Qt and KDE libraries | 31 |
| Phase 4 | QtWebEngine | 3 |
| Phase 4 | wxWidgets | 3 |
| Phase 4 | Python bindings to the toolkits | 11 |
| Phase 4 | Python modules | 13 |
| Phase 4 | OpenGL helpers, FLTK and Tk | 4 |
| Phase 4 | Runtimes | 2 |
| Phase 4 | Libraries the applications share | 8 |
| Phase 4 | SDL satellites and game libraries | 18 |
| Phase 4 | Audio libraries | 19 |
| Phase 4 | Document formats | 15 |
| Phase 4 | Picture and video libraries | 31 |
| Phase 4 | Phones, remote desktops and virtual machines | 25 |
| Phase 4 | Internet and communication | 55 |
| Phase 4 | Documents and office | 39 |
| Phase 4 | Pictures | 24 |
| Phase 4 | Sound, video and discs | 52 |
| Phase 4 | Files, system and safety | 29 |
| Phase 4 | Accessibility | 5 |
| Phase 4 | Knowledge and learning offline | 35 |
| Phase 4 | Software radio applications | 45 |
| Phase 4 | CAD, electronics, 3D printing and 3D | 61 |
| Phase 4 | Science, data and development | 56 |
| Phase 4 | Amateur radio and mesh | 29 |
| Phase 4 | Services for a LAN | 11 |
| Phase 4 | Meshing, charts, music production and the workshop | 23 |
| Phase 4 | Clinic, shop and family | 7 |
| Phase 4 | Windows programs | 3 |
| Phase 4 | The light tier | 18 |
| Phase 4 | Games | 58 |
| Phase 4 | Emulators | 30 |
| Phase 4 | Data the applications read | 9 |
| Desktop | (unheaded) | 6 |
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
| `python3` | 3.14.7 | The Python 3 interpreter and standard library; `tkinter` is `python3-tkinter` |
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

## Phase 4: userland, the Wayland base and the applications

`script/04_phase4/packages.txt` installs the rest of the userland: services, the network stack,
the command-line workspace, the scientific and hardware tools, the Wayland base, Xwayland, the
container layer, the codecs, and KDOS's own theme and tools from `src/packages`. It then installs
the graphical stacks: the X11 client libraries, GTK 3 and 4 with libadwaita, WebKitGTK, Qt 6 and
Qt 5 with QtWebEngine, KDE Frameworks 6, wxWidgets, FLTK and Tk, and the Python bindings to them.
On those it builds the native applications, grouped by what they are for, from the browsers and
office suites to the games, the emulators and the data packages they read. It is the largest list
by far.

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

### X11 client libraries

| Port | Version | Description |
|---|---|---|
| `libICE` | 1.1.2 | X11 Inter-Client Exchange library |
| `libSM` | 1.2.6 | X11 Session Management library |
| `libXcomposite` | 0.4.7 | X11 Composite extension client library |
| `libXcursor` | 1.2.3 | X11 cursor management library |
| `libXdamage` | 1.1.7 | X11 DAMAGE extension client library |
| `libXext` | 1.3.7 | X11 miscellaneous extensions library (SHAPE, MIT-SHM, XSync and others) |
| `libXfixes` | 6.0.2 | X11 XFIXES extension client library |
| `libXft` | 2.3.9 | X11 FreeType font rendering library over RENDER |
| `libXi` | 1.8.3 | X11 Input extension (XInput2) client library |
| `libXinerama` | 1.1.6 | X11 Xinerama extension client library |
| `libXmu` | 1.3.1 | X11 miscellaneous utility library (libXmu and libXmuu) |
| `libXpm` | 3.5.19 | X11 pixmap (XPM) image library |
| `libXpresent` | 1.0.2 | X11 Present extension client library |
| `libXrandr` | 1.5.5 | X11 RandR extension client library |
| `libXrender` | 0.9.12 | X11 RENDER extension client library |
| `libXres` | 1.2.3 | X11 X-Resource extension client library — per-client resource and pid queries |
| `libXScrnSaver` | 1.2.5 | X11 MIT-SCREEN-SAVER extension client library |
| `libXt` | 1.3.1 | X11 Toolkit Intrinsics library |
| `libXtst` | 1.2.5 | X11 XTEST and RECORD extension client library |
| `libXv` | 1.0.13 | X11 Xvideo extension client library |
| `libXxf86vm` | 1.1.7 | X11 XFree86-VidModeExtension client library |
| `motif` | 2.5.2 | The Motif widget toolkit (libXm, libMrm, uil) for X11 programs run under Xwayland |
| `xbitmaps` | 1.1.4 | X.Org bitmap files — the X11/bitmaps headers Motif compiles in |
| `xcb-util-keysyms` | 0.4.1 | XCB keysym and keycode conversion library (libxcb-keysyms) |
| `xcb-util-wm` | 0.4.2 | ICCCM and EWMH helpers for XCB — wlroots' Xwayland xwm needs both |

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
| `fontforge` | 20251009 | Outline and bitmap font editor: the GTK editor window, scripts and the Python module |
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
| `testdisk` | 7.3pre20260819 | Recover lost partitions and, as photorec and the QPhotoRec window, deleted files |
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
| `lxi-tools` | 2.8 | Drive a bench instrument over ethernet with SCPI, from the lxi command or the lxi-gui window |
| `sigrok-cli` | 0.7.2 | Command-line capture and protocol decoding for sigrok hardware |
| `sigrok-firmware-fx2lafw` | 0.1.7 | The firmware libsigrok uploads to a Cypress FX2 logic analyser, compiled here with sdcc |

### Raw pixels, curves, and sound with no window

| Port | Version | Description |
|---|---|---|
| `fluidr3-gm-sf3` | 4.7.5 | FluidR3 Mono General MIDI SoundFont, compressed — the default instrument set for fluidsynth and every MIDI player |
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
| `twolame` | 0.4.0 | MPEG Audio Layer 2 (MP2) encoder library and command-line encoder |
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
| `libdvdcss` | 1.6.0 | Reads a CSS-encrypted DVD-Video disc, for libdvdread and every DVD player above it |
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
| `tessdata-eng` | 4.1.0 | English and script-detection models for the tesseract OCR engine |
| `tesseract` | 5.5.3 | OCR engine — text from scanned images |
| `typst` | 0.15.1 | Markup-based typesetting system |
| `vdirsyncer` | 0.21.0 | Makes a CalDAV or CardDAV collection and a local vdir equal |
| `w3m` | 0.5.6 | Text-mode web browser and pager that renders tables |

### Sequences, coordinates, and exact arithmetic

| Port | Version | Description |
|---|---|---|
| `bcftools` | 1.24 | Call and filter variants |
| `duckdb` | 1.5.5 | In-process SQL database that queries CSV, JSON and Parquet files directly |
| `freexl` | 2.0.0 | FreeXL — reads Excel .xls, .xlsx and LibreOffice .ods sheets, for SpatiaLite and GDAL |
| `gdal` | 3.13.3 | Raster and vector geospatial data format library |
| `geos` | 3.15.0 | Computational geometry library for GIS software |
| `go-pmtiles` | 1.31.2 | Create, inspect and serve PMTiles map archives |
| `htslib` | 1.24 | The SAM/BAM/CRAM/VCF reader samtools and bcftools are built on |
| `libxlsxwriter` | 1.2.4 | C library for writing Excel XLSX files |
| `minimap2` | 2.31 | Align long reads to a reference |
| `minizip` | 1.3.2 | Read and write ZIP archives: the minizip library from zlib's contrib |
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
| `gnuplot` | 6.0.5 | Plotting program driven by scripts, drawing to the terminal, to a Qt window or to PNG |
| `ipython` | 9.17.1 | Interactive Python shell |
| `libcerf` | 3.8 | libcerf — complex error, Faddeeva, Voigt and Dawson functions |
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
| `codec2` | 1.2.0 | Codec 2 — the open low-bitrate speech codec and FreeDV modem library, with freedv_tx and freedv_rx |
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
| `hwloc` | 2.15.0 | Portable hardware locality — the CPU, cache, NUMA and device topology, with lstopo |
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
| `python3-psycopg2` | 2.9.13 | psycopg2 — the PostgreSQL adapter for Python, over libpq |
| `speexdsp` | 1.2.1 | Resampling, echo cancellation and jitter buffering — wireshark decodes RTP with it |
| `termshark` | 2.4.0 | Terminal UI for tshark |
| `wireshark` | 4.7.3 | Wireshark — the network analyser's Qt window, tshark and dumpcap |

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
| `enchant` | 2.8.21 | Spell-checking library over several engines, built with its Hunspell and Aspell providers |
| `hunspell` | 1.7.4 | Spell checker and morphological analyser — the library LibreOffice, Firefox and enchant check through |
| `hunspell-en` | 26.8.0.3 | English spelling dictionaries, hyphenation patterns and thesaurus from the LibreOffice dictionaries release |
| `jsoncpp` | 1.9.8 | JSON for C++ — recoll stores its index metadata through it |
| `mythes` | 1.2.6 | Thesaurus library and the index generator for its data files |
| `poppler` | 26.09.0 | PDF rendering library, and pdftotext with the other poppler utilities |
| `poppler-data` | 0.4.12 | CMaps and encoding tables poppler needs to render CJK and Cyrillic PDFs |
| `recoll` | 1.44.1 | One full-text index across PDFs, ODT, EPUB, mail and source, with a search window |

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
| `python3-pyxdg` | 0.28 | pyxdg — freedesktop.org base directories, desktop entries, menus and icon themes in Python |
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
| `python3-beautifulsoup4` | 4.15.0 | Beautiful Soup — lenient HTML and XML parsing, for nbconvert and toot |
| `python3-soupsieve` | 2.10 | Soup Sieve — CSS selectors for Beautiful Soup |
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
| `a52dec` | 0.8.0 | liba52, the ATSC A/52 (AC-3) audio decoder library, and the a52dec tool |
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
| `libmpeg2` | 0.5.1 | MPEG-1 and MPEG-2 video stream decoder library |
| `libnftnl` | 1.3.2 | Userspace library for low-level interaction with nf_tables (nftables dep) |
| `libnice` | 0.1.24 | ICE — how two WebRTC peers find a path to each other through NAT |
| `libnsbmp` | 0.1.7 | NetSurf BMP and ICO decoding library (imv decodes BMP with it) |
| `libsoup3` | 3.6.6 | GNOME HTTP client/server library, the 3.x API (GStreamer's souphttpsrc and adaptivedemux2, GeoClue) |
| `libsrtp` | 2.8.1 | Secure RTP library — the encrypted media transport of WebRTC calls |
| `mesa-demos` | 9.0.0 | Mesa EGL, GLES2 and Vulkan demos for Wayland — es2gears_wayland, eglinfo, vkgears |
| `mobile-broadband-provider-info` | 20251101 | Carrier APN database for mobile broadband (NetworkManager's WWAN connection wizard) |
| `modemmanager` | 1.24.2 | Mobile broadband modem daemon (3G/4G/5G over QMI, MBIM, QRTR and AT), mmcli and libmm-glib |
| `networkmanager-openvpn` | 1.12.5 | NetworkManager VPN plugin for OpenVPN |
| `nftables` | 1.1.7 | Netfilter packet filtering and classification framework, and the `nft` tool the firewall is written in |
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
| `android-file-transfer` | 4.5 | An Android phone's storage in a window, as a FUSE directory (aft-mtp-mount) and in the aft-mtp-cli shell |
| `android-tools` | 37.0.0 | Android platform tools — adb, fastboot, and the sparse-image and boot-image utilities |
| `bolt` | 0.9.11 | Thunderbolt device manager, so a dock behind firmware security is authorised |
| `bsd-games` | 2.17 | Three of the BSD games — Colossal Cave, tetris and worm |
| `fastfetch` | 2.68.1 | fastfetch — system information tool, ships the KDOS banner config |
| `ffmpeg` | 9.0.2 | FFmpeg — record, convert, and stream audio/video |
| `ffmpegthumbnailer` | 2.3.1 | Video thumbnail generator for file managers |
| `frotz` | 2.55 | Z-machine interpreter — Infocom-era interactive fiction in a terminal |
| `fwupd` | 2.1.7 | Daemon and client for updating device firmware |
| `gn` | 0.2480 | Chromium's meta-build system, which writes ninja files from BUILD.gn |
| `gphoto2` | 2.5.32 | Tethered capture and card download from a prompt |
| `gtest` | 1.18.0 | GoogleTest and GoogleMock — the C++ unit-test and mocking frameworks |
| `imagemagick` | 7.1.2.31 | ImageMagick — convert, edit, and compose raster images (provides `convert`) |
| `libcamera` | 0.7.2 | Camera stack that puts ISP pipelines and sensor control behind one API |
| `libggml` | 0.25.3 | ggml, the tensor library under llama.cpp and whisper.cpp: CPU variants, OpenBLAS and Vulkan backends as loadable modules |
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
| `python3-defusedxml` | 0.7.1 | defusedxml — XML parsing hardened against entity expansion attacks |
| `python3-lxml` | 6.1.3 | libxml2 and libxslt bindings for Python |
| `python3-maturin` | 1.15.0 | Build backend for Rust-based Python extensions |
| `python3-obd` | 0.7.3 | Reads and clears engine fault codes over an ELM327 OBD-II adapter |
| `python3-orjson` | 3.12.0 | orjson — JSON for Python, written in Rust |
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
| `vidstab` | 1.1.2 | Video stabilisation library, used by ffmpeg and MLT |
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
| `freeglut` | 3.8.0 | The OpenGL Utility Toolkit (libglut), X11 and GLX, for GLUT programs run under Xwayland |
| `ghostscript` | 10.08.0 | PostScript and PDF interpreter |
| `gutenprint` | 5.3.5 | Printer drivers for inkjet and dye-sublimation printers |
| `jbig2dec` | 0.20 | JBIG2 image decoder, the scanned-page codec in PDF |
| `libcupsfilters` | 2.2.1 | The filter functions behind every CUPS print filter, as a library |
| `libppd` | 2.1.1 | The PPD handling CUPS 3 drops, as a library for its filters and daemons |
| `mupdf` | 1.28.4 | mutool — the PDF the command line can take apart — and the mupdf-gl and mupdf-x11 viewers |
| `pdfio` | 1.6.5 | C library for reading and writing PDF files |
| `zxing-cpp` | 2.3.0 | Barcode and QR code reading and writing — what `mutool barcode` is built on |

### Scanning, and what is inside a media file

| Port | Version | Description |
|---|---|---|
| `ipp-usb` | 0.9.34 | A USB printer that speaks IPP-over-USB, presented to cups as a network one |
| `libmediainfo` | 26.05 | Media file inspection library — codec, profile, bit depth and every track |
| `libvips` | 8.18.6 | Streaming image-processing library with low memory use |
| `libzen` | 0.4.41 | ZenLib, the portability layer libmediainfo is written on |
| `matio` | 1.6.0 | matio — reads and writes MATLAB MAT files, v4, v5 and the HDF5-based v7.3 |
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

This group holds more than its title says: after the codecs it carries the VNC server and its
libraries (`aml`, `neatvnc`, `wayvnc`), `weechat`, `whois`, `passt`, `libedit`, `lldb` and `perf`.

| Port | Version | Description |
|---|---|---|
| `aml` | 1.0.0 | Event loop library for main-loop driven Wayland services |
| `flac` | 1.5.0 | Free Lossless Audio Codec library and tools |
| `intel-gmmlib` | 22.10.2 | Intel Graphics Memory Management Library for the media driver |
| `intel-media-driver` | 26.3.5 | Intel iHD VA-API driver for Broadwell and newer graphics |
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

### GTK 3, GTK 4 and libadwaita

| Port | Version | Description |
|---|---|---|
| `adwaita-icon-theme` | 51.0 | GNOME's Adwaita icon and cursor theme — the symbolic-icon fallback GTK applications expect |
| `at-spi2-core` | 2.62.0.1 | Assistive Technology Service Provider Interface — the accessibility bus, ATK and the ATK bridge |
| `blueprint-compiler` | 0.22.2 | Markup language and compiler for GTK 4 user interfaces |
| `dconf` | 51.0 | Low-level configuration store — the GSettings backend, its D-Bus service and the dconf tool |
| `graphene` | 1.10.8 | Thin layer of vector and matrix types for 2D and 3D graphics, used by GTK 4 |
| `gsettings-desktop-schemas` | 51.0 | GSettings schemas shared by GTK and GNOME applications: interface, fonts, proxy, privacy |
| `gspell` | 1.14.5 | Spell checking for GTK 3 text views and entries, over enchant |
| `gtk3` | 3.24.52 | GTK 3 widget toolkit, with the Wayland and X11 backends |
| `gtk4` | 4.24.0 | GTK 4 widget toolkit, with the Wayland and X11 backends, Vulkan and GStreamer media |
| `gtksourceview4` | 4.8.4 | Source code editing widget for GTK 3 — highlighting, completion, search |
| `gtksourceview5` | 5.22.0 | Source code editing widget for GTK 4 — highlighting, completion, search |
| `gvfs` | 1.62.0 | GIO virtual filesystem — sftp, smb, dav, trash, recent, MTP, camera and device volumes for GTK applications |
| `iniparser` | 4.3.0 | Small C library for parsing INI files |
| `libadwaita` | 1.10.0 | GNOME's building blocks for GTK 4 applications — adaptive widgets and styles |
| `libatasmart` | 0.19 | ATA S.M.A.R.T. reading and parsing library |
| `libblockdev` | 3.5.0 | Library for manipulating block devices: partitions, filesystems, LVM, MD RAID, crypto, NVMe |
| `libbytesize` | 2.12 | Library for working with arbitrarily large sizes in bytes |
| `libconfig` | 1.8.2 | Structured configuration file library for C and C++ |
| `libgee` | 0.20.8 | GObject collection library — lists, maps, sets and queues for Vala and C |
| `libhandy` | 1.8.3 | Adaptive widgets for GTK 3 applications |
| `libimobiledevice` | 1.4.0 | Library and idevice* tools that speak the iPhone and iPad protocols: pairing, backup, files, syslog |
| `libimobiledevice-glue` | 1.3.2 | Common socket, thread and utility code shared by the libimobiledevice libraries |
| `libplist` | 2.7.0 | Apple property list library and plistutil — binary, XML, JSON and OpenStep plists |
| `libportal` | 0.11.0 | Client library for the XDG desktop portals, with GTK 3 and GTK 4 parent windows |
| `libsecret` | 0.21.8.2 | Secret Service client library — stores and looks up passwords over D-Bus |
| `libspelling` | 0.4.10 | Spell checking for GTK 4 text views and GtkSourceView 5, over enchant |
| `libstoragemgmt` | 1.11.0 | Storage array and local disk management library, the lsmd plugin daemon and lsmcli |
| `libtatsu` | 1.0.5 | Client for Apple's Tatsu Signing Server, as used to restore and personalise iOS devices |
| `libusbmuxd` | 2.1.1 | Client library for usbmuxd, with iproxy and inetcat to tunnel TCP to an iOS device |
| `libxdg-basedir` | 1.2.3 | A small C library for the XDG Base Directory specification |
| `ndctl` | 85 | Userspace tools and libraries for NVDIMM, DAX and CXL devices (ndctl, daxctl, cxl) |
| `nspr` | 4.40 | Netscape Portable Runtime — the platform layer under NSS |
| `nss` | 3.130 | Mozilla Network Security Services — TLS, PKCS #11 and certificate libraries |
| `python3-cairo` | 1.29.1 | pycairo — cairo drawing for Python, and the cairo half of PyGObject |
| `python3-gobject` | 3.58.0 | PyGObject — GLib, GTK and every introspected library, from Python |
| `udisks2` | 2.11.2 | Disk management daemon and D-Bus API for toolkit applications |
| `vala` | 0.56.19 | Vala compiler and vapigen — C#-like language compiled to GObject C |
| `xdg-dbus-proxy` | 0.1.9 | Filtering D-Bus proxy — the bus a sandboxed WebKitGTK web process is given |
| `xdg-desktop-portal-gtk` | 1.15.3 | GTK backend for xdg-desktop-portal — the Print and Email portals |

### The C++ bindings of GTK

| Port | Version | Description |
|---|---|---|
| `atkmm2.28` | 2.28.5 | C++ bindings for ATK, the 1.6 API (atkmm-1.6) that gtkmm 3 builds on |
| `cairomm` | 1.18.1 | C++ bindings for cairo, the 1.16 API (cairomm-1.16) that gtkmm 4 builds on |
| `cairomm1.14` | 1.14.6 | C++ bindings for cairo, the 1.0 API (cairomm-1.0) that gtkmm 3 builds on |
| `glibmm` | 2.90.0 | C++ bindings for GLib and GIO, the 2.68 API (glibmm-2.68, giomm-2.68) that gtkmm 4 builds on |
| `glibmm2.66` | 2.66.10 | C++ bindings for GLib and GIO, the 2.4 API (glibmm-2.4, giomm-2.4) that gtkmm 3 builds on |
| `gtkmm3` | 3.24.11 | C++ bindings for GTK 3 (gtkmm-3.0), with the atkmm and X11 APIs |
| `gtkmm4` | 4.24.0 | C++ bindings for GTK 4 (gtkmm-4.0) |
| `libsigc++2` | 2.12.1 | Typesafe callback framework for C++, the 2.x API (sigc++-2.0) that gtkmm 3 builds on |
| `libsigc++3` | 3.6.0 | Typesafe callback framework for C++, the 3.x API (sigc++-3.0) that gtkmm 4 builds on |
| `pangomm` | 2.58.0 | C++ bindings for Pango, the 2.48 API (pangomm-2.48) that gtkmm 4 builds on |
| `pangomm2.46` | 2.46.5 | C++ bindings for Pango, the 1.4 API (pangomm-1.4) that gtkmm 3 builds on |

### Hyphenation and the document fonts

| Port | Version | Description |
|---|---|---|
| `font-caladea` | 20130214 | Caladea — a serif face metric-compatible with Cambria |
| `font-carlito` | 20130920 | Carlito — a sans face metric-compatible with Calibri |
| `font-liberation` | 2.1.5 | Liberation Sans, Serif, Mono and Sans Narrow — metric-compatible with Arial, Times New Roman, Courier New and Arial Narrow |
| `hyphen` | 2.8.9 | Hyphenation library for TeX-style pattern files |

### WebKitGTK

| Port | Version | Description |
|---|---|---|
| `unifdef` | 2.12 | Selectively remove C preprocessor conditionals |
| `webkitgtk` | 2.54.0 | WebKitGTK web engine for GTK 3: the webkit2gtk-4.1 and javascriptcoregtk-4.1 APIs over libsoup 3 |
| `webkitgtk6` | 2.54.0 | WebKitGTK web engine for GTK 4: the webkitgtk-6.0 and javascriptcoregtk-6.0 APIs |
| `woff2` | 1.0.2 | Web Open Font Format 2 reference implementation: the WOFF2 codec libraries and tools |

### Qt 6

| Port | Version | Description |
|---|---|---|
| `assimp` | 6.0.5 | Open Asset Import Library — reads and writes some 40 3D model formats (glTF, OBJ, FBX, Collada, STL) into one scene graph |
| `qt6-qt3d` | 6.11.2 | Qt 6 3D — scene graph, render, input, logic, animation and extras aspects, C++ and QML |
| `qt6-qt5compat` | 6.11.2 | Qt 6 Core5Compat — QTextCodec, QRegExp and the Qt 5 graphical effects for code ported from Qt 5 |
| `qt6-qtbase` | 6.11.2 | Qt 6 core, GUI, widgets, network, SQL, D-Bus and printing, with the Wayland and X11 platform plugins |
| `qt6-qtcharts` | 6.11.2 | Qt 6 Charts — chart widgets and QML chart types |
| `qt6-qtconnectivity` | 6.11.2 | Qt 6 Bluetooth and NFC — QtBluetooth over BlueZ and QtNfc over PC/SC |
| `qt6-qtdeclarative` | 6.11.2 | Qt 6 QML and Qt Quick — the language, the engine, Qt Quick Controls and the qml tools |
| `qt6-qtgraphs` | 6.11.2 | Qt 6 Graphs — 2D and 3D data visualisation for QML and widgets |
| `qt6-qtimageformats` | 6.11.2 | Qt 6 image format plugins — TIFF, WebP, ICNS, TGA and WBMP |
| `qt6-qtlanguageserver` | 6.11.2 | Qt 6 Language Server Protocol and JSON-RPC library, used by qmlls |
| `qt6-qtlocation` | 6.11.2 | Qt 6 Location — maps, places and routing for QML |
| `qt6-qtmqtt` | 6.11.2 | Qt 6 MQTT — MQTT 3.1, 3.1.1 and 5 client, for LabPlot's live data sources |
| `qt6-qtmultimedia` | 6.11.2 | Qt 6 Multimedia — audio, video playback and capture through FFmpeg and PipeWire |
| `qt6-qtnetworkauth` | 6.11.2 | Qt 6 Network Authorization — OAuth 1 and OAuth 2 clients |
| `qt6-qtpositioning` | 6.11.2 | Qt 6 Positioning — position sources from GeoClue and NMEA |
| `qt6-qtquick3d` | 6.11.2 | Qt 6 Quick 3D — 3D scenes in QML |
| `qt6-qtremoteobjects` | 6.11.2 | Qt 6 Remote Objects — QObjects shared between processes |
| `qt6-qtscxml` | 6.11.2 | Qt 6 SCXML and StateMachine — state charts compiled or interpreted |
| `qt6-qtsensors` | 6.11.2 | Qt 6 Sensors — accelerometer, gyroscope and light sensors through iio-sensor-proxy |
| `qt6-qtserialport` | 6.11.2 | Qt 6 SerialPort — serial and USB-serial device access |
| `qt6-qtshadertools` | 6.11.2 | Qt 6 shader tools — qsb, the GLSL/HLSL/MSL/SPIR-V baker Qt Quick and Qt RHI shaders are compiled with |
| `qt6-qtspeech` | 6.11.2 | Qt 6 TextToSpeech — speech synthesis through speech-dispatcher |
| `qt6-qtsvg` | 6.11.2 | Qt 6 SVG rendering — QSvgRenderer, QSvgWidget and the SVG image and icon engine plugins |
| `qt6-qttools` | 6.11.2 | Qt 6 tools — Designer, Linguist, Assistant, Qt Help, UiTools, lrelease, qdbus and qtdiag |
| `qt6-qttranslations` | 6.11.2 | Qt 6 translation catalogues for the Qt libraries and tools |
| `qt6-qtwayland` | 6.11.2 | Qt 6 Wayland compositor library and the client decoration and shell plugins |
| `qt6-qtwebchannel` | 6.11.2 | Qt 6 WebChannel — QObjects shared with HTML and JavaScript clients |
| `qt6-qtwebsockets` | 6.11.2 | Qt 6 WebSockets — RFC 6455 client and server |

### Qt 5

| Port | Version | Description |
|---|---|---|
| `breeze-icons` | 6.30.0 | Breeze icon theme, light and dark, and its Qt resource library |
| `qca-qt5` | 2.3.12 | Qt Cryptographic Architecture for Qt 5, with the OpenSSL, GnuPG and SASL providers |
| `qt5-qtbase` | 5.15.19 | Qt 5 core, GUI, widgets, network, SQL, D-Bus and printing, with the X11 platform plugin, for programs that build against Qt 5 only |
| `qt5-qtdeclarative` | 5.15.19 | Qt 5 QML and Qt Quick — the declarative UI language, its JavaScript engine and the Quick scene graph |
| `qt5-qtgraphicaleffects` | 5.15.19 | Qt 5 Quick graphical effects — the blur, shadow, glow and colour QML types |
| `qt5-qtmultimedia` | 5.15.19 | Qt 5 multimedia — audio, video and camera through GStreamer, PulseAudio and ALSA |
| `qt5-qtquickcontrols2` | 5.15.19 | Qt 5 Quick Controls 2 — the QML control set with its Fusion, Material, Universal and Imagine styles |
| `qt5-qtserialport` | 5.15.19 | Qt 5 serial port access, with port enumeration through udev |
| `qt5-qtsvg` | 5.15.19 | Qt 5 SVG rendering — QSvgRenderer, QSvgWidget and the SVG image and icon engine plugins |
| `qt5-qttools` | 5.15.19 | Qt 5 tools — Designer, Linguist with lrelease and lupdate, Assistant, qdbus and the Qt help and UI tools libraries |
| `qt5-qtwayland` | 5.15.19 | Qt 5 Wayland platform plugin, so Qt 5 programs run as native Wayland clients, and the QtWaylandCompositor module |
| `qt5-qtwebsockets` | 5.15.19 | Qt 5 WebSocket client and server, from C++ and QML |
| `qt5-qtx11extras` | 5.15.19 | Qt 5 X11 extras — QX11Info, which Qt 5 programs use to reach the X11 display under Xwayland |
| `qt5ct` | 1.9 | Qt 5 appearance settings — the palette, style, fonts and icon theme of every Qt 5 program, as a platform theme plugin |
| `qt6ct` | 0.11 | Qt 6 appearance settings — the palette, style, fonts and icon theme of every Qt 6 program, as a platform theme plugin |

### KDE Frameworks 6

| Port | Version | Description |
|---|---|---|
| `attica` | 6.30.0 | KDE Frameworks: Open Collaboration Services client library |
| `baloo` | 6.30.0 | KDE Frameworks: file search and tagging library, without the indexer service |
| `bluez-qt` | 6.30.0 | KDE Frameworks: Qt wrapper for the BlueZ 5 D-Bus API |
| `breeze` | 6.7.5 | Breeze widget style and colour schemes for Qt 6 applications |
| `ebook-tools` | 0.2.2 | EPUB reading library (libepub) and the einfo tool |
| `frameworkintegration` | 6.30.0 | KDE Frameworks: platform integration plugins and KPackage install handlers |
| `karchive` | 6.30.0 | KDE Frameworks: reading and writing compressed archives |
| `kauth` | 6.30.0 | KDE Frameworks: execute actions as a privileged user through polkit |
| `kbookmarks` | 6.30.0 | KDE Frameworks: bookmark storage and menus |
| `kcalendarcore` | 6.30.0 | KDE Frameworks: iCalendar data model and parser |
| `kcmutils` | 6.30.0 | KDE Frameworks: configuration modules for widget and QML settings pages |
| `kcodecs` | 6.30.0 | KDE Frameworks: string encodings and charset detection |
| `kcolorscheme` | 6.30.0 | KDE Frameworks: colour-scheme loading and roles |
| `kcompletion` | 6.30.0 | KDE Frameworks: text completion helpers and widgets |
| `kconfig` | 6.30.0 | KDE Frameworks: persistent application configuration |
| `kconfigwidgets` | 6.30.0 | KDE Frameworks: widgets for configuration dialogs |
| `kcontacts` | 6.30.0 | KDE Frameworks: vCard address book data model |
| `kcoreaddons` | 6.30.0 | KDE Frameworks: core non-GUI utilities, plugins and jobs |
| `kcrash` | 6.30.0 | KDE Frameworks: crash handler that restarts or reports a crashed application |
| `kdbusaddons` | 6.30.0 | KDE Frameworks: convenience classes for Qt D-Bus |
| `kdeclarative` | 6.30.0 | KDE Frameworks: QML integration and controls |
| `kded` | 6.30.0 | KDE Frameworks: kded6, the daemon that hosts session modules |
| `kdesu` | 6.30.0 | KDE Frameworks: running programs as another user through sudo |
| `kdnssd` | 6.30.0 | KDE Frameworks: DNS-SD service discovery through Avahi |
| `kdoctools` | 6.30.0 | KDE Frameworks: DocBook to HTML and manual page tools (meinproc6) |
| `kfilemetadata` | 6.30.0 | KDE Frameworks: file metadata and text extraction |
| `kglobalaccel` | 6.30.0 | KDE Frameworks: global keyboard shortcuts through kglobalacceld |
| `kguiaddons` | 6.30.0 | KDE Frameworks: colours, fonts, clipboard and key helpers for Qt GUIs |
| `kholidays` | 6.30.0 | KDE Frameworks: public holiday, season and astronomical calendars |
| `ki18n` | 6.30.0 | KDE Frameworks: gettext-based translation for Qt programs |
| `kiconthemes` | 6.30.0 | KDE Frameworks: icon theme lookup and icon dialogs |
| `kidletime` | 6.30.0 | KDE Frameworks: user idle-time reporting |
| `kio` | 6.30.0 | KDE Frameworks: network-transparent file access, workers and file dialogs |
| `kirigami` | 6.30.0 | KDE Frameworks: QtQuick components for convergent applications |
| `kirigami-addons` | 1.14.2 | Additional Kirigami components: form cards, dialogs and pickers |
| `kitemmodels` | 6.30.0 | KDE Frameworks: proxy and helper item models for Qt |
| `kitemviews` | 6.30.0 | KDE Frameworks: widget add-ons for Qt item views |
| `kjobwidgets` | 6.30.0 | KDE Frameworks: widgets that track the progress of KJob tasks |
| `knewstuff` | 6.30.0 | KDE Frameworks: downloading and installing add-on content |
| `knotifications` | 6.30.0 | KDE Frameworks: desktop notifications through the freedesktop D-Bus interface |
| `knotifyconfig` | 6.30.0 | KDE Frameworks: configuration dialog for desktop notifications |
| `kpackage` | 6.30.0 | KDE Frameworks: loading and installing non-binary content packages |
| `kparts` | 6.30.0 | KDE Frameworks: document-centric embeddable components |
| `kpeople` | 6.30.0 | KDE Frameworks: a unified address book of contacts |
| `kplotting` | 6.30.0 | KDE Frameworks: a plotting widget |
| `kpty` | 6.30.0 | KDE Frameworks: pseudo-terminal devices for terminal emulators |
| `kquickcharts` | 6.30.0 | KDE Frameworks: GPU-drawn charts for QtQuick |
| `kservice` | 6.30.0 | KDE Frameworks: desktop entry and plugin lookup (kbuildsycoca6) |
| `kstatusnotifieritem` | 6.30.0 | KDE Frameworks: StatusNotifierItem tray icons over D-Bus |
| `ktexteditor` | 6.30.0 | KDE Frameworks: the KatePart text editor component |
| `ktextwidgets` | 6.30.0 | KDE Frameworks: rich text editing widgets with spell checking and speech |
| `kunitconversion` | 6.30.0 | KDE Frameworks: unit conversion |
| `kwallet` | 6.30.0 | KDE Frameworks: the KWallet client API and kwalletd6, which keeps wallets in the Secret Service |
| `kwayland` | 6.7.5 | Qt-style client library for the Wayland and Plasma Wayland protocols |
| `kwidgetsaddons` | 6.30.0 | KDE Frameworks: add-on widgets and classes for Qt Widgets |
| `kwindowsystem` | 6.30.0 | KDE Frameworks: access to the windowing system, Wayland and X11 |
| `kxmlgui` | 6.30.0 | KDE Frameworks: XML-described menus, toolbars and shortcut editors |
| `libcanberra` | 0.30 | XDG sound theme event sounds over ALSA, PulseAudio and GStreamer, with the GTK 3 module |
| `libdmtx` | 0.7.8 | Data Matrix two-dimensional barcode reading and writing library |
| `lmdb` | 0.9.36 | Lightning Memory-Mapped Database: an embedded B+tree key-value store, and its mdb_* tools |
| `modemmanager-qt` | 6.30.0 | KDE Frameworks: Qt wrapper for the ModemManager D-Bus API |
| `networkmanager-qt` | 6.30.0 | KDE Frameworks: Qt wrapper for the NetworkManager D-Bus API |
| `plasma-integration` | 6.7.5 | Qt platform theme that gives every Qt 6 and KF6 application the KDE palette, fonts, icons and file dialogs |
| `polkit-qt6` | 0.201.1 | Qt 6 wrapper around the polkit client and agent libraries |
| `prison` | 6.30.0 | KDE Frameworks: barcode generation and scanning |
| `purpose` | 6.30.0 | KDE Frameworks: the share menu and its local share targets |
| `qqc2-desktop-style` | 6.30.0 | KDE Frameworks: QtQuick Controls style that follows the desktop palette and icons |
| `solid` | 6.30.0 | KDE Frameworks: hardware discovery through udev, UDisks2 and UPower |
| `sonnet` | 6.30.0 | KDE Frameworks: spell checking through Hunspell and Aspell |
| `sound-theme-freedesktop` | 0.8 | The freedesktop default sound theme — event sounds libcanberra plays |
| `syntax-highlighting` | 6.30.0 | KDE Frameworks: syntax highlighting engine for structured text |
| `taglib` | 2.3.2 | Library for reading and editing audio file metadata |
| `threadweaver` | 6.30.0 | KDE Frameworks: high-level job-based multithreading |

### Shared Qt and KDE libraries

| Port | Version | Description |
|---|---|---|
| `chromaprint` | 1.6.1 | AcoustID audio fingerprinting library and the fpcalc tool |
| `djvulibre` | 3.5.30 | DjVu document library and command-line tools — ddjvu, djvused, c44, cjb2 |
| `ffmpegthumbs` | 26.08.1 | KIO thumbnailer for video files, through FFmpeg |
| `kddockwidgets` | 2.4.1 | KDAB dock widget framework for Qt 6, with the Widgets and Qt Quick front ends |
| `kdegraphics-thumbnailers` | 26.08.1 | KIO thumbnailers for PostScript, PDF, Blender and camera raw files |
| `kdsingleapplication` | 1.2.1 | KDAB helper class for single-instance Qt 6 applications |
| `kio-extras` | 26.08.1 | Extra KIO workers and thumbnailers: archives, man pages, MTP and iOS devices, file previews |
| `ksanecore` | 26.08.1 | Qt 6 library that drives SANE scanners, without a user interface |
| `libdvbpsi` | 1.3.3 | MPEG-TS PSI/SI table decoder — VLC's transport-stream and DVB demuxing |
| `libebml` | 1.4.7 | EBML container library — the layer under libmatroska |
| `libgme` | 0.6.5 | Play video-game console music: NES, SNES, Game Boy, Genesis, PC Engine and more |
| `libkdcraw` | 26.08.1 | Qt 6 wrapper around LibRaw for decoding camera raw files |
| `libkdegames` | 26.08.1 | Common code, card decks and sound for the KDE games |
| `libkexiv2` | 26.08.1 | Qt 6 wrapper around exiv2 for reading and writing picture metadata |
| `libkmahjongg` | 26.08.1 | Tile sets, backgrounds and the rendering code shared by the KDE Mahjongg games |
| `libksane` | 26.08.1 | Qt 6 scanner widget over ksanecore, used by Skanlite and KolourPaint |
| `libmatroska` | 1.7.2 | Matroska and WebM container library — VLC's and MKVToolNix's .mkv reader |
| `libmodplug` | 0.8.9.0 | Play tracker modules: MOD, S3M, XM, IT and more, the ModPlug engine |
| `libmysofa` | 1.3.5 | Reader for AES SOFA files — head-related transfer functions for spatial audio |
| `libnfs` | 8.0.0 | Userspace NFS v3/v4 client library — Kodi's nfs:// sources, and the nfs-ls, nfs-cp and nfs-cat tools |
| `libspatialaudio` | 0.4.1 | Ambisonic and object audio rendering to speakers or headphones — the spatial audio behind MLT and VLC |
| `libtheora` | 1.2.0 | The Theora video codec, the one in Ogg video files |
| `lirc` | 0.10.2 | Infrared remote control daemon, client library and tools — lircd, irexec, irw, liblirc_client and the Python bindings |
| `openal-soft` | 1.25.2 | OpenAL 3D positional audio for games, played through PipeWire, Pulse or ALSA |
| `phonon` | 4.12.0 | KDE multimedia API for Qt 6; playback goes through a backend port |
| `phonon-backend-vlc` | 0.12.0 | Phonon backend that plays through libvlc |
| `portaudio` | 19.7.0 | Portable real-time audio I/O library, with its C++ binding, over ALSA |
| `qca` | 2.3.12 | Qt Cryptographic Architecture for Qt 6, with the OpenSSL, GnuPG and SASL providers |
| `qtkeychain` | 0.17.0 | Qt 6 API for storing passwords in the Secret Service, KWallet or a plain file |
| `speex` | 1.2.1 | Speex speech codec library, with the speexenc and speexdec tools |
| `vlc` | 3.0.24 | VLC media player 3 with its Qt 5 interface — plays files, discs, devices and streams |

### QtWebEngine

| Port | Version | Description |
|---|---|---|
| `python3-html5lib` | 1.1 | An HTML parser following the WHATWG HTML specification, for Python |
| `python3-webencodings` | 0.6.1 | The WHATWG Encoding standard's labels and legacy decoders, for Python |
| `qt6-qtwebengine` | 6.11.2 | Qt 6 WebEngine and Qt PDF — the Chromium web engine as Qt widgets and Qt Quick types |

### wxWidgets

| Port | Version | Description |
|---|---|---|
| `libmspack` | 0.11alpha | Library for Microsoft compression formats — CAB, CHM, HLP, LIT, KWAJ, SZDD |
| `python3-wxpython` | 4.2.5 | wxPython — the wxWidgets GTK 3 toolkit for Python programs such as CHIRP and Pronterface |
| `wxwidgets` | 3.2.9 | wxWidgets on GTK 3 — the native-widget C++ toolkit under KiCad, PrusaSlicer, FileZilla, VeraCrypt and wxPython |

### Python bindings to the toolkits

| Port | Version | Description |
|---|---|---|
| `python3-cups` | 2.0.4 | pycups — libcups from Python, for system-config-printer |
| `python3-dasbus` | 1.7 | dasbus — a D-Bus library for Python on top of GLib's GDBus, which Orca talks through |
| `python3-dbus` | 1.5.0 | dbus-python — the libdbus bindings system-config-printer and PyQt6's D-Bus main loop use |
| `python3-pyqt-builder` | 1.19.1 | PyQt-builder — the sip-build backend every PyQt6 binding is configured with |
| `python3-pyqt5` | 5.15.11 | PyQt5 — the Qt 5 bindings GNU Radio's gr-qtgui and the Qt 5 Python tools are written in |
| `python3-pyqt5-sip` | 12.19.0 | PyQt5.sip — the runtime module every PyQt5 extension is linked through |
| `python3-pyqt6` | 6.11.0 | PyQt6 — the Qt 6 bindings Calibre, Anki, QGIS, Picard and git-cola are written in |
| `python3-pyqt6-sip` | 13.12.0 | PyQt6.sip — the runtime module every PyQt6 extension is linked through |
| `python3-pyqt6-webengine` | 6.11.0 | PyQt6-WebEngine — QtWebEngine from Python, for the Calibre viewer, Anki and Spyder |
| `python3-qtpy` | 2.4.3 | QtPy — one Qt import layer over PyQt6, for git-cola and Spyder |
| `python3-sip` | 6.16.1 | SIP — the binding generator PyQt6, QScintilla and QGIS are built with |

### Python modules

| Port | Version | Description |
|---|---|---|
| `python3-asgiref` | 3.12.1 | ASGI specification helpers and sync/async adapters for Python |
| `python3-blinker` | 1.9.0 | In-process object-to-object and broadcast signalling for Python |
| `python3-decorator` | 5.3.1 | decorator — signature-preserving function decorators for Python |
| `python3-distro` | 1.9.0 | distro — Linux distribution identification from os-release for Python |
| `python3-flask` | 3.1.3 | Flask, a WSGI web application framework for Python |
| `python3-flask-cors` | 6.0.5 | Cross-origin resource sharing (CORS) headers for Flask applications |
| `python3-itsdangerous` | 2.2.0 | Cryptographically signed serialisation of data for passing through untrusted channels |
| `python3-markdown` | 3.11 | Python-Markdown — Markdown to HTML conversion for Python |
| `python3-protobuf` | 7.36.2 | Protocol Buffers runtime for Python, with the upb C extension |
| `python3-pysocks` | 1.7.1 | SOCKS4 and SOCKS5 proxy client module for Python sockets and urllib |
| `python3-truststore` | 0.10.4 | Verify TLS against the system certificate store through the ssl module |
| `python3-waitress` | 3.0.2 | Waitress, a pure-Python production WSGI server |
| `python3-werkzeug` | 3.1.9 | Werkzeug, the WSGI toolkit under Flask: request, response, routing and a development server |

### OpenGL helpers, FLTK and Tk

| Port | Version | Description |
|---|---|---|
| `fltk` | 1.4.5 | The Fast Light Toolkit — a small C++ GUI library, Wayland first with an X11 fallback, and FLUID |
| `glew` | 2.3.1 | The OpenGL Extension Wrangler — libGLEW, GLX mode, with glewinfo and visualinfo |
| `glfw` | 3.5.1 | A window, a GL or Vulkan context and input — Wayland first, X11 under Xwayland as the fallback |
| `tk` | 9.0.4 | Tk, the GUI toolkit for Tcl — wish and libtcl9tk, drawn through X11 under Xwayland |

### Runtimes

| Port | Version | Description |
|---|---|---|
| `openjdk` | 25.0.4.1 | OpenJDK, the Java Development Kit: the JVM, the compiler and the class library (LTS) |
| `yarn` | 4.18.1 | Yarn — the JavaScript package manager, the Berry line |

### Libraries the applications share

| Port | Version | Description |
|---|---|---|
| `libebur128` | 1.2.6 | EBU R 128 loudness measurement library |
| `minizip-ng` | 4.2.2 | Zip archive manipulation library, the zlib-ng fork of minizip |
| `onetbb` | 2023.1.0 | oneAPI Threading Building Blocks: task-parallel C++ runtime |
| `pystring` | 1.2.0 | C++ functions with the behaviour of Python's string methods |
| `rapidjson` | 1.1.0 | Header-only JSON parser and generator for C++ |
| `simde` | 0.8.2 | Portable implementations of SIMD intrinsics, header-only |
| `sparsehash` | 2.0.4 | Memory-efficient C++ hash map and set templates |
| `yaml-cpp` | 0.9.0 | YAML 1.2 parser and emitter for C++ |

### SDL satellites and game libraries

| Port | Version | Description |
|---|---|---|
| `enet` | 1.3.18 | Reliable UDP networking for games |
| `glm` | 1.0.3 | OpenGL Mathematics: header-only C++ vectors and matrices in GLSL's terms |
| `libsodium` | 1.0.22 | Cryptography library with NaCl's API, portable |
| `libxmp` | 4.7.3 | Play tracker modules: MOD, S3M, XM, IT and some ninety other formats |
| `mbedtls` | 3.6.7 | Mbed TLS: a small TLS and cryptography library, the 3.6 long-term line |
| `miniupnpc` | 2.3.3 | A UPnP IGD client, for opening a port on the home router |
| `physfs` | 3.2.0 | Read game data from directories and ZIP, 7z, WAD, GRP and other archives as one tree |
| `plutosvg` | 0.0.8 | Tiny SVG rendering library in C, with the FreeType hooks that draw OT-SVG colour glyphs |
| `plutovg` | 1.3.3 | Tiny 2D vector graphics library in C |
| `sdl2-gfx` | 1.0.4 | Lines, circles, polygons, rotation, zoom and a frame limiter for SDL2 programs |
| `sdl2-image` | 2.8.12 | Load PNG, JPEG, WebP, AVIF, JPEG XL, TIFF, SVG and older formats into SDL2 surfaces |
| `sdl2-mixer` | 2.8.2 | Play music and sound effects from SDL2 programs: Ogg, Opus, FLAC, MP3, WavPack, MOD, MIDI and chiptunes |
| `sdl2-net` | 2.4.0 | Portable TCP and UDP sockets for SDL2 programs |
| `sdl2-pango` | 2.1.5 | Draw Pango-laid-out text into SDL2 surfaces |
| `sdl2-ttf` | 2.24.0 | Render TrueType and OpenType text into SDL2 surfaces, shaped by HarfBuzz |
| `sdl3-image` | 3.4.6 | Load PNG, JPEG, WebP, AVIF, JPEG XL, TIFF, SVG and animated formats into SDL3 surfaces |
| `sdl3-mixer` | 3.2.4 | Play music and sound effects from SDL3 programs: Ogg, Opus, FLAC, MP3, WavPack, MOD, MIDI and chiptunes |
| `sdl3-ttf` | 3.2.2 | Render TrueType and OpenType text into SDL3 surfaces and GPU text, shaped by HarfBuzz |

### Audio libraries

| Port | Version | Description |
|---|---|---|
| `aubio` | 0.4.9 | Audio labelling library — onset, pitch, beat and tempo detection |
| `ladspa` | 1.17 | LADSPA audio plugin API header, example plugins and the analyseplugin, applyplugin and listplugins tools |
| `liblo` | 0.36 | Open Sound Control implementation — OSC messages over UDP and TCP |
| `libsbsms` | 2.3.0 | Subband sinusoidal modeling time stretch and pitch shift library |
| `libvolk` | 3.3.0 | Vector-Optimized Library of Kernels — runtime-dispatched SIMD kernels for GNU Radio and SDR tools |
| `lilv` | 0.28.0 | LV2 plugin host library — discovers, loads and instantiates LV2 plugins |
| `lrdf` | 0.6.1 | RDF metadata library for LADSPA plugins |
| `lv2` | 1.18.10 | LV2 audio plugin standard — the specification headers and bundle data |
| `portmidi` | 2.0.8 | Portable real-time MIDI input and output library over ALSA sequencer |
| `raptor2` | 2.0.16 | Raptor RDF syntax library — RDF/XML, Turtle and N-Triples parsers and serialisers |
| `rtaudio` | 6.0.1 | C++ real-time audio I/O classes over ALSA and PulseAudio |
| `rtmidi` | 6.0.0 | C++ real-time MIDI input and output classes over the ALSA sequencer |
| `serd` | 0.32.10 | Turtle and NTriples RDF reader and writer — what LV2 plugin data is parsed with |
| `sord` | 0.16.22 | In-memory RDF quad store over serd, used by lilv to query LV2 plugin data |
| `soundtouch` | 2.4.1 | Tempo, pitch and playback-rate changer for audio streams |
| `sratom` | 0.6.22 | Serialises LV2 atoms to and from RDF |
| `suil` | 0.10.26 | Embeds LV2 plugin user interfaces in GTK 3, Qt 6 and X11 hosts |
| `vamp-sdk` | 2.10.0 | Vamp audio analysis plugin SDK, host library and the vamp-simple-host tool |
| `zix` | 0.8.2 | C data structures and portability layer under serd, sord and lilv |

### Document formats

| Port | Version | Description |
|---|---|---|
| `blend2d` | 0.21.2 | 2D vector graphics engine with a JIT-compiled pipeline |
| `cabextract` | 1.11 | Extract Microsoft cabinet (.cab) files |
| `clipper2` | 2.0.1 | Polygon clipping and offsetting library |
| `discount` | 3.0.2.0 | Markdown to HTML library and converter — libmarkdown and markdown |
| `goffice` | 0.10.62 | GLib/GTK office library — charts, formats and canvas shared by Gnumeric and AbiWord |
| `graphviz` | 16.1.0 | Graph layout and rendering — dot, neato, sfdp and the cgraph/gvc libraries |
| `gumbo-parser` | 0.14.1 | HTML5 parsing library in C |
| `lasem` | 0.5.1 | MathML and SVG rendering library on cairo and pango, with itex-to-MathML conversion |
| `libetpan` | 1.10.1 | Mail protocol library — IMAP, SMTP, POP3, NNTP, MIME and mailbox formats |
| `libgsf` | 1.14.59 | GNOME Structured File library — OLE2, ZIP and ODF containers for office formats |
| `libpst` | 0.6.76 | Outlook PST and OST reader — readpst, lspst and the libpst library |
| `libspectre` | 0.2.12 | PostScript rendering library over the Ghostscript API |
| `numactl` | 2.0.19 | NUMA policy library (libnuma) and the numactl, numastat and migratepages tools |
| `tinyxml` | 2.6.2 | Small C++ XML parser (TinyXML 1) |
| `unshield` | 1.6.2 | Extract InstallShield cabinet (.cab) archives |

### Picture and video libraries

| Port | Version | Description |
|---|---|---|
| `babl` | 0.1.128 | Dynamic any-to-any pixel format conversion library, used by GEGL and GIMP |
| `exempi` | 2.6.6 | XMP metadata library and the exempi tool, from Adobe's XMP SDK |
| `frei0r-plugins` | 3.6.0 | frei0r video effect plugins and their plugin API |
| `gavl` | 1.4.0 | Gmerlin audio and video library — colourspace, scaling and sample-format conversion for frei0r's plugins |
| `gegl` | 0.4.72 | Graph-based image processing framework, the engine under GIMP |
| `gexiv2` | 0.14.7 | GObject wrapper around exiv2 image metadata, the 0.14 API GIMP links |
| `immer` | 0.9.1 | Immutable and persistent data structures for C++ (headers) |
| `kseexpr` | 6.0.0.0 | Krita fork of the SeExpr expression language, the Qt 6 build |
| `lager` | 0.1.3 | Value-oriented unidirectional data-flow architecture for C++ (headers) |
| `lensfun` | 0.3.4 | Lens correction library and its camera and lens database |
| `lib2geom` | 1.4 | 2D geometry library for C++, the path and curve engine of Inkscape |
| `libcdr` | 0.1.9 | CorelDRAW file import library |
| `libiptcdata` | 1.0.5 | IPTC photo metadata library and the iptc tool |
| `libmypaint` | 1.6.1 | MyPaint brush engine library, used by GIMP and Krita |
| `librevenge` | 0.0.6 | Base library for document import filters: the drawing, text and spreadsheet interfaces |
| `libvisio` | 0.1.11 | Microsoft Visio diagram import library |
| `libwpd` | 0.10.3 | WordPerfect document import library |
| `libwpg` | 0.3.4 | WordPerfect Graphics import library |
| `mlt` | 7.40.0 | MLT multimedia framework: the video editing engine under Kdenlive and Shotcut |
| `movit` | 1.7.2 | High-quality GPU video filters on OpenGL — the GLSL effect chain MLT's movit module drives |
| `mpvqt` | 1.2.0 | Qt Quick item that renders libmpv, used by Haruna |
| `mypaint-brushes` | 2.0.2 | MyPaint brush collection, version 2 data set |
| `opencolorio` | 2.5.2 | OpenColorIO colour management for visual effects and animation |
| `opencv` | 4.14.0 | OpenCV — computer vision and machine-learning library, with the tracking, optical-flow and extended image-processing contrib modules and Python bindings |
| `openh264` | 2.6.0 | Cisco's H.264 baseline encoder and decoder, compiled from source |
| `opentimelineio` | 0.18.1 | OpenTimelineIO: interchange format and C++ API for editorial timelines |
| `quazip` | 1.7.2 | Qt 6 C++ wrapper for zip archives |
| `rnnoise` | 0.2 | RNNoise — recurrent neural network noise suppression for speech, with its trained model |
| `rttr` | 0.9.6 | Run-time type reflection library for C++ |
| `xsimd` | 14.3.0 | C++ wrappers for SIMD intrinsics (headers) |
| `zug` | 0.1.2 | Transducers for C++ (headers) |

### Phones, remote desktops and virtual machines

| Port | Version | Description |
|---|---|---|
| `freerdp` | 3.32.0 | Remote Desktop Protocol library and the SDL3 client sdl-freerdp; the RDP engine of Remmina |
| `gtk-vnc` | 1.5.0 | VNC client library and GTK 3 viewer widget, with its PulseAudio bridge — virt-manager's VNC console |
| `ifuse` | 1.2.1 | FUSE filesystem that mounts an iPhone's or iPad's media and app documents |
| `iptables` | 1.8.13 | iptables, ip6tables, ebtables and arptables over the nftables kernel API |
| `libcacard` | 2.8.2 | Virtual CAC smartcard emulation — the card a SPICE or QEMU guest sees, backed by NSS or a real reader |
| `libiscsi` | 1.20.3 | Userspace iSCSI initiator library and client tools (iscsi-ls, iscsi-inq, iscsi-swp) |
| `libnbd` | 1.24.3 | NBD client library and the nbdsh, nbdcopy, nbdinfo, nbddump and nbdfuse tools |
| `libosinfo` | 1.12.0 | Library of operating-system facts for virtualisation — installer media, devices, requirements |
| `libssh` | 0.12.2 | SSH protocol library (client and server) |
| `libtorrent-rasterbar` | 2.1.2 | Arvid Norberg's BitTorrent library, with its Python binding — the engine of qBittorrent and Deluge |
| `libvirt` | 12.7.0 | Virtualisation API and the libvirtd daemon, driving QEMU/KVM guests, networks and storage; virsh |
| `libvirt-glib` | 5.0.0 | GLib, GObject and GConfig wrappers of the libvirt API, with introspection data and Vala bindings |
| `libvncserver` | 0.9.15 | VNC server and client libraries |
| `nbdkit` | 1.48.1 | NBD server toolkit with pluggable plugins and filters; serves disks to libvirt and QEMU |
| `osinfo-db` | 20260812 | The osinfo database: every operating system, installer and virtual device libosinfo knows |
| `osinfo-db-tools` | 1.12.0 | Tools that import, export, validate and locate the osinfo operating-system database |
| `phodav` | 3.0 | WebDAV server library on libsoup, with chezdav — the folder sharing behind spice-gtk's shared directory |
| `python3-libvirt` | 12.7.0 | Python binding of the libvirt API — what virt-manager and virt-install drive libvirt through |
| `spice` | 0.16.0 | SPICE server library — what QEMU links to offer a guest's display, sound and USB to a SPICE client |
| `spice-gtk` | 0.43 | SPICE client library and GTK 3 widget — the VM console of virt-manager — with spicy and the USB redirection |
| `spice-protocol` | 0.14.5 | SPICE protocol headers — the message definitions spice-gtk and QEMU's SPICE display share |
| `uriparser` | 1.0.2 | Strict RFC 3986 URI parsing and normalising library, with the uriparse tool |
| `usbmuxd` | 1.1.1.20251206 | USB multiplexing daemon that carries every connection to an iPhone or iPad over its cable |
| `usbredir` | 0.15.0 | USB-over-network redirection protocol libraries and usbredirect, for SPICE and QEMU |
| `vte3` | 0.84.1 | Terminal emulator widget for GTK 3 and GTK 4 |

### Internet and communication

| Port | Version | Description |
|---|---|---|
| `argon2` | 20190702 | Argon2 password hashing library and command-line tool |
| `ayatana-ido` | 0.10.4 | Ayatana Indicator Display Objects — the GTK 3 menu widgets indicator menus are built from |
| `chromium` | 154.0.8037.57 | Chromium web browser, on GTK 3 with Wayland and X11 |
| `cmark` | 0.31.2 | CommonMark reference implementation: libcmark and the cmark converter |
| `coeurl` | 0.3.2 | Asynchronous C++ wrapper around libcurl on a libevent loop |
| `cpp-utilities` | 5.37.0 | Martchus's C++ utility library: argument parsing, conversions, I/O and the CMake modules his programs build with |
| `deluge` | 2.2.0 | Deluge — BitTorrent client with a daemon, a GTK 3 interface, a web UI and a console UI |
| `dino` | 0.5.1 | Dino (GTK 4, libadwaita) — XMPP chat with OMEMO and OpenPGP encryption, file transfer, and voice and video calls |
| `esbuild` | 0.25.1 | JavaScript and TypeScript bundler and minifier |
| `falkon` | 26.08.1 | Qt WebEngine web browser from KDE, with KWallet and KIO integration |
| `filezilla` | 3.71.0 | FileZilla (wxWidgets, GTK 3) — FTP, FTPS and SFTP client with a two-pane site manager |
| `firefox-esr` | 153.3.0 | Firefox Extended Support Release web browser, on GTK 3 with Wayland and X11 |
| `fzssh` | 1.4.0 | FileZilla's SSH and SFTP client library |
| `kdeconnect` | 26.08.1 | KDE Connect — files, clipboard, notifications, media control and remote input between this machine and a phone, over the LAN or Bluetooth |
| `konversation` | 26.08.1 | Konversation — IRC client (Qt 6, KDE Frameworks) with TLS, DCC transfers and Blowfish-encrypted channels |
| `lagrange` | 1.21.1 | Gemini, Gopher, Spartan and Finger browser, with a terminal build (clagrange) beside the SDL one |
| `libayatana-appindicator` | 0.6.0 | Ayatana application indicators — a GTK 3 program's tray icon and menu as a StatusNotifierItem |
| `libayatana-indicator` | 0.9.5 | Ayatana indicator library — the shared object behind application and system indicators |
| `libcss` | 0.9.2 | NetSurf CSS parser and selection engine |
| `libdbusmenu` | 16.04.0 | Menus passed over D-Bus — libdbusmenu-glib and the GTK 3 renderer libdbusmenu-gtk3 |
| `libdom` | 0.4.2 | NetSurf W3C DOM implementation, with HTML (hubbub) and XML (expat) parser bindings |
| `libei` | 1.6.0 | Emulated input: libei for clients, libeis for compositors, liboeffis for the RemoteDesktop portal |
| `libfilezilla` | 0.57.0 | FileZilla's C++ platform library: events, sockets, TLS over GnuTLS, hashing and time |
| `libhubbub` | 0.3.8 | NetSurf HTML5 parser, conforming to the WHATWG tokenisation and tree-building rules |
| `libnslog` | 0.1.3 | NetSurf logging library — categorised logging with a filter language |
| `libnspsl` | 0.1.7 | NetSurf Public Suffix List library — a compiled-in copy of the list for cookie and domain checks |
| `libnsutils` | 0.1.1 | NetSurf utility library — base64, time and unistd helpers shared by the browser and its libraries |
| `libparserutils` | 0.2.5 | NetSurf parser building blocks — input streams, character-set conversion and buffers |
| `librewolf` | 156.0.1.1 | LibreWolf, the privacy-hardened Firefox fork, on GTK 3 with Wayland and X11 |
| `libsvgtiny` | 0.1.8 | NetSurf SVG Tiny renderer — parses SVG into a list of path and text shapes |
| `libwapcaplet` | 0.4.3 | NetSurf string internment library — interned, reference-counted strings |
| `lmdbxx` | 1.0.2 | C++17 header-only wrapper for the LMDB database library |
| `mtxclient` | 0.10.1 | Client library for the Matrix protocol, with olm end-to-end encryption |
| `mumble` | 1.5.915 | Low-latency voice chat — the Mumble client and mumble-server, for group voice on a LAN |
| `netsurf` | 3.11 | NetSurf — a web browser with its own layout engine, on GTK 3 |
| `nheko` | 0.12.1 | Matrix chat client (Qt 6) — rooms, end-to-end encryption, voice and video calls |
| `nsgenbind` | 0.9 | NetSurf's JavaScript binding generator — turns WebIDL and binding files into Duktape glue C |
| `olm` | 3.2.16 | The olm and megolm end-to-end encryption ratchets of Matrix |
| `perl-yaml-tiny` | 1.76 | Read and write a subset of YAML in pure perl |
| `poco` | 1.15.4 | POCO C++ libraries — foundation, XML, JSON, Zip, networking and TLS classes |
| `pulseaudio-qt` | 1.9.0 | Qt 6 bindings for the PulseAudio client API, served here by pipewire-pulse |
| `python3-attrs` | 26.1.0 | Classes without boilerplate: declared attributes, validators and converters for Python |
| `qbittorrent` | 5.2.3 | qBittorrent (Qt 6) — BitTorrent client with a web UI, plus qbittorrent-nox for a machine with no display |
| `qtforkawesome` | 0.3.4 | The Fork Awesome icon font as a Qt 6 library, icon engine plugin and Qt Quick image provider |
| `qtutilities` | 6.22.2 | Martchus's Qt 6 utility library: settings dialogs, about dialog, notifications and resources |
| `quassel` | 0.14.0 | Quassel IRC (Qt 5) — the monolithic client, the core that stays connected, and the client for it |
| `qutebrowser` | 3.7.0 | Keyboard-driven web browser with vim-like bindings, on Qt WebEngine |
| `re2` | 2025.11.05 | Google's linear-time regular expression library, with full Unicode properties from ICU |
| `remmina` | 1.4.43 | Remmina (GTK 3) — remote desktop client for RDP, VNC, SPICE and SSH |
| `spdlog` | 1.17.0 | C++ logging library, over the system fmt |
| `syncthingtray` | 2.1.7 | Syncthing Tray (Qt 6) — status, folders, devices and control of a local Syncthing from the panel tray |
| `thunderbird` | 153.3.1 | Mail, calendar, contacts and feeds, offline-first, on GTK 3 for Wayland and X11 |
| `transmission` | 4.1.3 | Transmission — BitTorrent daemon with a web UI, command-line tools and a Qt 6 client |
| `waypipe` | 0.11.2 | Network proxy for Wayland clients — run a program on another machine and show it here, over SSH |
| `wlvncc` | 0.1.0.20260429 | Wayland-native VNC client drawing through EGL/GLES2, with H.264 decoding by FFmpeg |

### Documents and office

| Port | Version | Description |
|---|---|---|
| `abiword` | 3.0.8 | AbiWord word processor — reads and writes Word, OpenDocument, RTF and HTML |
| `brlaser` | 6.2.8 | CUPS driver for Brother monochrome laser printers |
| `calibre` | 9.15.0 | E-book library manager, converter, viewer and editor |
| `chmlib` | 0.40a | Library for reading Microsoft Compiled HTML Help (CHM) files, with extract_chmLib |
| `foomatic-db` | 20260926 | Foomatic printer database — printer and driver XML plus manufacturer PPDs for CUPS |
| `foomatic-db-engine` | 4.0.13 | Foomatic PPD generator — builds CUPS PPDs from the foomatic-db printer database |
| `geany` | 2.1 | GTK programmer's editor — syntax highlighting, symbols, build commands |
| `gimagereader` | 3.4.3 | OCR front end for Tesseract — scan or open images and PDFs, recognise and edit text |
| `gnucash` | 5.17 | Double-entry accounting — personal and small-business books, invoices, reports |
| `gnumeric` | 1.12.62 | GNOME spreadsheet — accurate statistics, and reads Excel, OpenDocument and Lotus files |
| `goldendict-ng` | 26.8.0 | Dictionary lookup — StarDict, DSL, MDict, ZIM, Babylon and hunspell morphology, offline |
| `guile` | 3.0.11 | GNU Guile 3.0 — the Scheme implementation GnuCash and Aisleriot embed, with guild |
| `homebank` | 5.10.3 | Personal finance manager — accounts, budgets, reports, QIF and CSV import |
| `hplip` | 3.26.4 | HP printer and scanner drivers — hpcups, the hp backend, PPDs and the hpaio SANE backend |
| `jbigkit` | 2.1 | JBIG1 (ITU-T T.82 and T.85) bi-level image compression library and tools |
| `kate` | 26.08.1 | KDE advanced text editor — tabs, split views, LSP, projects, search, terminal |
| `konsole` | 26.08.1 | KDE terminal emulator — tabs, split views, profiles, SSH manager |
| `libreoffice` | 26.8.0.3 | Office suite — Writer, Calc, Impress, Draw, Math and Base, with the gtk3 and qt6 front ends |
| `lyx` | 2.5.3 | Document processor that writes LaTeX — structure-first editing with typeset output |
| `okular` | 26.08.1 | Universal document viewer — PDF, PostScript, DjVu, EPUB, XPS, comics, Markdown |
| `optipng` | 7.9.1 | Lossless PNG optimiser |
| `pdf4qt` | 1.6.0.0 | PDF viewer and editor — annotate, redact, sign, compare, split and merge pages |
| `perl-file-homedir` | 1.006 | Find the home and XDG user directories of the current or another user, in pure perl |
| `perl-file-which` | 1.27 | Find the full path of an executable on PATH, in pure perl |
| `podofo` | 1.1.2 | C++ library to read, create and modify PDF documents |
| `qalculate-qt` | 5.12.0 | Qalculate! desktop calculator (Qt) — units, currencies, symbolic algebra, plots |
| `qownnotes` | 26.9.13 | Plain-text Markdown notes with a live preview, to-do lists and optional Nextcloud sync |
| `qtspell` | 1.0.2 | Spell checking for Qt text widgets, through Enchant |
| `skanlite` | 26.08.1 | Image scanning application — preview, select and save scans through SANE |
| `snowball` | 3.1.1 | Snowball stemming compiler and libstemmer, the stemmers for 30 languages |
| `splix` | 2.0.2 | CUPS driver for Samsung, Xerox, Dell and Lexmark SPL2/SPLc laser printers |
| `system-config-printer` | 1.5.18 | Printer configuration tool for CUPS — add, configure and manage print queues (GTK 3) |
| `texlive` | 20260301 | TeX Live 2026 — TeX, LaTeX, pdfTeX, XeTeX, LuaTeX, MetaPost, BibTeX and dvips, with a curated texmf tree |
| `texlive-doc` | 20260301 | TeX Live 2026 documentation — the English manuals and examples of every package the texlive port ships, for texdoc |
| `texstudio` | 4.9.8 | LaTeX editor — completion, live PDF preview with SyncTeX, spelling and grammar checking |
| `tomlplusplus` | 3.4.0 | TOML parser and serializer for C++17 |
| `wv` | 1.2.9 | Library and converters for Microsoft Word 2000, 97, 95 and 6 documents |
| `zim` | 0.77.2 | Zim desktop wiki — linked notebook pages, journal, tasks and attachments in plain text |
| `zziplib` | 0.13.80 | Read files inside ZIP archives through a stdio-like API — MilkyTracker's zipped modules and TeX Live's LuaTeX |

### Pictures

| Port | Version | Description |
|---|---|---|
| `appstream` | 1.2.0 | AppStream metadata library and appstreamcli — reads and validates software component data |
| `converseen` | 0.15.2.8 | Batch image converter and resizer over ImageMagick, for Qt 6 |
| `darktable` | 5.6.1 | RAW photo developer and virtual lighttable |
| `digikam` | 9.1.0 | digiKam and showFoto: photo library, tagging, face recognition, RAW import and editing, for Qt 6 |
| `flameshot` | 14.0.0 | Screenshot tool with an annotation editor, capturing through the Screenshot portal |
| `font-manager` | 0.9.4 | Font Manager and Font Viewer: browse, compare, preview and enable fonts, for GTK 4 |
| `gimp` | 3.2.6 | GNU Image Manipulation Program — raster image editor and photo retoucher |
| `gimp-help` | 3.2.0 | The GIMP user manual in English, as local HTML that F1 opens with no network |
| `glaxnimate` | 0.6.0 | Glaxnimate: vector animation and motion design, Lottie, SVG and video export, for Qt 6 |
| `gthumb` | 4.0 | gThumb: browse, view, tag and edit photos and videos, for GTK 4 and libadwaita |
| `gwenview` | 26.08.1 | Image viewer and browser — RAW, annotation, slideshow and a camera importer |
| `inkscape` | 1.4.4 | Vector graphics editor — SVG drawing, PDF import, CorelDRAW and Visio files |
| `kcolorpicker` | 0.3.1 | Qt colour picker button with a palette popup, used by kImageAnnotator |
| `kimageannotator` | 0.7.2 | Qt image annotation widgets — arrows, text, blur and stickers — used by Gwenview |
| `kimageformats` | 6.30.0 | KDE Frameworks: QImage plugins for AVIF, HEIF, JPEG XL, JPEG 2000, EXR, RAW, PSD, XCF, Krita and more |
| `kolourpaint` | 26.08.1 | Paint program — draw, crop, recolour and scan |
| `krita` | 6.0.4 | Krita: digital painting, illustration and frame-by-frame animation, for Qt 6 |
| `libfyaml` | 0.9.6 | Complete YAML 1.2 parser and emitter library, and the fy-tool command |
| `pencil2d` | 0.7.2 | Pencil2D: hand-drawn 2D animation with bitmap and vector layers, for Qt 6 |
| `python3-cssselect` | 1.5.0 | CSS selectors translated to XPath — Inkscape's extension library selects with it |
| `python3-scour` | 0.38.2 | SVG optimiser — the scour command and Inkscape's Optimized SVG output |
| `python3-tinycss2` | 1.5.1 | CSS parser for Python — Inkscape's extension library reads style with it |
| `rawtherapee` | 5.13 | RAW photo developer with non-destructive, per-image processing profiles |
| `swayimg` | 5.6 | Image viewer for Wayland and the console — no toolkit, Lua-configured |

### Sound, video and discs

| Port | Version | Description |
|---|---|---|
| `adplug` | 2.4 | AdLib/OPL2 sound player library for DOS-era game and tracker music, with the adplugdb database tool |
| `audacious` | 4.6.1 | Playlist-oriented music player — Qt 6 interface, MPRIS on the session bus |
| `audacious-plugins` | 4.6.1 | Audacious decoders, outputs, effects and the Qt interface — without them the player opens no window and plays nothing |
| `audacity` | 4.0.0 | Multitrack audio editor and recorder — the Qt 6 Audacity 4, with LV2, LADSPA and Nyquist effects |
| `audiofile` | 0.3.6 | SGI Audio File Library — reads and writes AIFF, WAVE, NeXT/Sun, IRCAM, AVR, CAF and FLAC through one API |
| `avidemux` | 2.8.1 | Avidemux: a Qt 6 video cutter, filter and encoder |
| `brasero` | 3.12.3 | GNOME disc burner and copier — data, audio and video CDs and DVDs |
| `cdrdao` | 1.2.6 | Disc-at-once CD writer — audio CDs, CD-TEXT and exact copies |
| `celluloid` | 0.30 | GTK 4 video player over libmpv |
| `dvd+rw-tools` | 7.1 | growisofs and friends — DVD and Blu-ray writing, formatting and media inspection |
| `elisa` | 26.08.1 | KDE music player — a local music collection, played through libVLC |
| `exo` | 4.20.0 | Xfce extension library for GTK 3 applications |
| `handbrake` | 1.11.2 | HandBrake video transcoder: the GTK 4 application and HandBrakeCLI |
| `haruna` | 1.8.1 | Haruna: a Qt 6 and Kirigami video player over libmpv |
| `hydrogen` | 1.2.6 | Pattern-based drum machine and sequencer with sample drum kits |
| `id3lib` | 3.8.3 | ID3v1 and ID3v2 tag library, with the id3tag, id3info, id3convert and id3cp commands |
| `k3b` | 26.08.1 | KDE disc burner — data, audio and video CDs, DVDs and Blu-ray, copies and rips |
| `kamoso` | 26.08.1 | KDE webcam booth — take pictures and record videos from a camera |
| `kdenlive` | 26.08.1 | Kdenlive: the KDE multi-track video editor, on MLT |
| `kodi` | 21.3 | Kodi — ten-foot media centre for local and network video, music and pictures, with add-ons |
| `kwave` | 26.08.1 | KDE sound editor — record, cut, filter and convert WAV, FLAC, MP3, Ogg Vorbis and Opus |
| `libbinio` | 1.5 | Binary I/O stream class library, the file layer AdPlug reads through |
| `libbs2b` | 3.1.0 | Bauer stereophonic-to-binaural crossfeed library, with the bs2bconvert and bs2bstream commands |
| `libcec` | 8.1.7 | HDMI-CEC control library for Pulse-Eight adapters and the kernel CEC framework, with cec-client |
| `libcue` | 2.3.0 | CUE sheet parser library |
| `libdiscid` | 0.7.0 | MusicBrainz disc ID library — reads an audio CD's table of contents |
| `libgpod` | 0.8.3 | Library to read and write the music database of an iPod classic, nano and shuffle |
| `libkcddb` | 26.08.1 | KDE CDDB client library — audio CD track names for K3b |
| `libopenmpt` | 0.8.9 | Tracker module playback library (MOD, S3M, XM, IT, MPTM and more) and openmpt123 |
| `libpeas` | 1.36.0 | GObject plugin engine, 1.x API — Rhythmbox's plugin loader |
| `libresidfp` | 1.2.2 | Cycle-exact MOS 6581/8580 SID chip emulation library, the reSIDfp engine libsidplayfp plays through |
| `libsidplayfp` | 3.1.1 | C64 SID music player library — libsidplayfp and libstilview, playing through reSIDfp |
| `libxfce4ui` | 4.20.2 | Xfce widget library, with its X11 and Wayland paths |
| `libxfce4util` | 4.20.1 | Xfce utility library |
| `mkvtoolnix` | 102.0 | MKVToolNix: create, split, edit and inspect Matroska files, with the Qt 6 GUI |
| `musescore` | 4.7.5 | MuseScore Studio — music notation: write, play back, print and export scores, with the MS Basic soundfont |
| `obs-studio` | 32.2.2 | OBS Studio: screen recording and live streaming, with PipeWire screen capture on Wayland |
| `picard` | 3.0.0rc4 | MusicBrainz Picard — tag and rename music files from the MusicBrainz database |
| `portsmf` | 239 | Standard MIDI File and Allegro score reader and writer library |
| `rhythmbox` | 3.5.1 | GNOME music player and library organiser |
| `shairplay` | 0.9.0.git20180824 | AirPlay (RAOP) audio receiver library — Kodi's AirTunes target |
| `shotcut` | 26.8.1 | Shotcut: a Qt 6 video editor on MLT, with no KDE Frameworks |
| `smplayer` | 26.8.29 | Qt front end for mpv that remembers where every file stopped |
| `strawberry` | 1.2.30 | Music player and collection organiser — tags, playlists, loudness normalisation, CD and MTP devices |
| `tdb` | 1.4.15 | Trivial database library — Rhythmbox's metadata cache |
| `tenacity` | 1.3.5 | Multitrack audio editor on wxWidgets — the Audacity 3 lineage with the network code removed |
| `totem-pl-parser` | 3.26.7 | Playlist parser library — m3u, pls, xspf and podcast feeds |
| `utfcpp` | 4.2.1 | Header-only C++ library for checking and converting UTF-8, UTF-16 and UTF-32 text |
| `waylandpp` | 1.0.1 | Wayland C++ bindings and the wayland-scanner++ protocol generator |
| `xa` | 2.4.1 | xa65 cross-assembler for the 6502, 65816 and R65C02, with the o65 relocation tools |
| `xfburn` | 0.8.0 | GTK 3 disc burner over libburn — data and audio CDs, DVDs and ISO images |
| `xfconf` | 4.20.0 | Xfce configuration store — libxfconf and the xfconfd daemon |

### Files, system and safety

| Port | Version | Description |
|---|---|---|
| `ark` | 26.08.1 | Archive manager — browse, extract and create tar, zip, 7z and compressed archives |
| `baloo-widgets` | 26.08.1 | Baloo widgets — the file metadata panel, tag and rating editors used by Dolphin |
| `botan` | 3.13.0 | Botan 3 — C++ cryptography and TLS library |
| `clamav` | 1.5.4 | Anti-virus toolkit — clamscan, the clamd daemon, sigtool and the libclamav scanning engine |
| `cracklib` | 2.10.3 | Password-strength checking library, with its small English word dictionary packed |
| `deja-dup` | 50.2 | Déjà Dup — scheduled, encrypted backups to a disk or folder over restic |
| `dolphin` | 26.08.1 | File manager — tabs, split views, places, previews, a terminal panel and devices |
| `filelight` | 26.08.1 | Disk usage — folders drawn as concentric rings, sized by what they hold |
| `gnome-disk-utility` | 46.1 | GNOME Disks — partition, format, encrypt, image and benchmark drives over udisks2 (GTK 3) |
| `gparted` | 1.8.1 | GParted — create, resize, move, copy and check partitions and filesystems |
| `groff` | 1.24.1 | GNU roff — troff, nroff, eqn, tbl, pic and the PDF, PostScript and HTML output devices |
| `hfsprogs` | 540.1.3 | Apple HFS+ filesystem tools — mkfs.hfsplus and fsck.hfsplus, from Apple's diskdev_cmds |
| `imhex` | 1.38.1 | Hex editor for reverse engineering — pattern language, disassembler, diffing and data inspectors |
| `impression` | 3.8.0 | Impression — write a disk image to a USB drive or memory card |
| `jfsutils` | 1.1.15 | JFS filesystem utilities — mkfs.jfs, fsck.jfs, jfs_tune and the debugger |
| `keepassxc` | 2.7.12 | Password manager — KeePass databases, TOTP, SSH agent, browser integration and the Secret Service |
| `kleopatra` | 26.08.1 | Certificate manager and graphical front end to GnuPG — OpenPGP and S/MIME keys, signing and encryption |
| `kmbox` | 26.08.1 | KDE PIM library for reading and writing mbox mail folders |
| `kmime` | 6.30.0 | KDE Frameworks: MIME message parsing and assembly |
| `libkleo` | 26.08.1 | KDE PIM library for key management and cryptography widgets over GnuPG |
| `libpwquality` | 1.4.5 | Password quality checking and random password generation library, over cracklib |
| `mimetreeparser` | 26.08.1 | KDE PIM library that parses and renders signed and encrypted MIME messages |
| `nilfs-utils` | 2.3.1 | NILFS2 filesystem utilities — mkfs.nilfs2, the cleaner daemon, nilfs-resize and snapshot tools |
| `qdirstat` | 2.0 | QDirStat — disk usage as a tree and a treemap, with cleanup actions |
| `qgpgme` | 2.2.0 | QGpgME — the Qt 6 binding to GnuPG Made Easy |
| `resources` | 1.10.2 | Resources — system monitor for CPU, memory, GPU, NPU, disks, network, battery and processes (GTK 4) |
| `udftools` | 2.3 | UDF filesystem tools — mkudffs, udffsck, udflabel, wrudf and the packet-writing helpers |
| `veracrypt` | 1.26.29 | VeraCrypt — create and mount encrypted volumes and containers, TrueCrypt-compatible |
| `virt-manager` | 5.1.0 | Virtual Machine Manager — create, run and connect to libvirt/QEMU virtual machines, with virt-install, virt-clone and virt-xml |

### Accessibility

| Port | Version | Description |
|---|---|---|
| `dasher` | 5.0.0.beta.20200420 | Dasher — text entry by steering a pointer, eye tracker or switch through a predictive language model |
| `orca` | 51.0 | Orca — the screen reader: speech and braille over the AT-SPI accessibility tree |
| `wl-kbptr` | 0.4.1 | Move and click the pointer from the keyboard — labelled screen regions over a layer-shell overlay |
| `wtype` | 0.4 | xdotool type for Wayland — types text and presses keys through the virtual-keyboard protocol |
| `wvkbd` | 0.20 | On-screen keyboard for Wayland — layer-shell and virtual-keyboard, drawn with cairo and pango |

### Knowledge and learning offline

| Port | Version | Description |
|---|---|---|
| `analitza` | 26.08.1 | KDE mathematical expression library — the parser, evaluator and 2D/3D plotter behind KAlgebra |
| `avogadrolibs` | 2.0.0 | Avogadro libraries — molecular data, file formats, OpenGL rendering and Qt widgets for chemistry programs |
| `celestia` | 1.7.0.git20260926 | Real-time 3D space simulator — fly through the solar system, the stars and the galaxies |
| `celestia-content` | 1.7.0.git20260923 | Celestia's universe — star and deep-sky catalogues, planet textures, models and orbits |
| `fast-double-parser` | 0.8.1 | Header-only C++ parser from decimal strings to doubles, several times faster than strtod |
| `gcompris` | 26.2 | Educational activities for children aged 2 to 10 — reading, counting, science, games and art |
| `gflags` | 2.3.1 | Google's C++ command-line flags library |
| `gpxsee` | 16.16 | GPXSee: GPS track, route and waypoint viewer and analyser over offline raster and vector maps |
| `kalgebra` | 26.08.1 | KDE graph calculator — expressions, 2D and 3D plots, and a console calculator |
| `kalzium` | 26.08.1 | KDE periodic table of the elements — properties, isotopes, spectra and a molar mass calculator |
| `kgeography` | 26.08.1 | KDE geography trainer — maps, capitals and flags of countries and regions, as quizzes |
| `kiwix-desktop` | 2.5.1 | Kiwix: an offline reader for ZIM libraries such as Wikipedia, with search, tabs and a reading list |
| `kolibri` | 0.19.5 | Offline learning platform — courses, lessons and quizzes from content channels, served on the LAN |
| `kqtquickcharts` | 26.08.1 | KDE QtQuick chart components — the line and bar charts in KTouch's statistics |
| `ktouch` | 26.08.1 | KDE touch typing tutor — graded courses, keyboard layouts and progress statistics |
| `kturtle` | 26.08.1 | KDE educational programming environment — steer a turtle with TurtleScript and learn to program |
| `kwordquiz` | 26.08.1 | KDE flashcard trainer — flashcards, multiple choice and question-and-answer quizzes on KVTML decks |
| `libkeduvocdocument` | 26.08.1 | KDE vocabulary document library — reads and writes the KVTML files of Parley and KWordQuiz |
| `llama.cpp` | 0.5.0 | Local language models on the CPU or a Vulkan GPU: llama-cli, llama-server and the GGUF tools, with nothing sent anywhere |
| `marble` | 26.08.1 | Marble: virtual globe and world atlas, with offline Atlas, satellite and OpenStreetMap vector views |
| `nlopt` | 2.11.0 | NLopt — nonlinear optimisation library used by slicers and nesting tools |
| `ocaml` | 5.5.1 | OCaml — the native and bytecode compilers, runtime and standard library |
| `ocaml-facile` | 1.1.4 | FaCiLe — constraint programming library for OCaml, the solver behind Kalzium's equation balancer |
| `onnxruntime` | 1.30.0 | ONNX Runtime: inference for ONNX models on the CPU, as a C/C++ library and a Python module |
| `openbabel` | 3.2.1 | Chemistry toolbox — reads, writes and converts over a hundred molecular file formats |
| `organicmaps` | 2025.09.05.1 | Organic Maps: offline maps with search and turn-by-turn routing for walking, cycling and driving, from OpenStreetMap data |
| `parley` | 26.08.1 | KDE vocabulary trainer — spaced repetition over KVTML word lists, with themes and HTML export |
| `piper` | 1.8.0 | Piper: neural text to speech on the CPU, with the voice model on the disk and nothing sent anywhere |
| `python3-pathvalidate` | 3.3.1 | Validates and sanitises file names and paths for every platform's rules |
| `qmapshack` | 1.21.1 | QMapShack: offline topographic maps, elevation models, GPS tracks and route planning with Routino |
| `shapelib` | 1.6.3 | Shapelib — read and write ESRI shapefiles and their dBase attribute tables |
| `stellarium` | 26.2 | Planetarium — a realistic sky in 3D, as seen with the eye, binoculars or a telescope |
| `translatelocally` | 0.0.2.20250330 | translateLocally: machine translation on the CPU with Bergamot models, with the text never leaving the machine |
| `tuxpaint` | 0.9.35 | Drawing program for children — brushes, stamps, shapes, text and magic tools |
| `zeal` | 0.9.1 | Offline documentation browser — searchable API docsets for languages and libraries |

### Software radio applications

| Port | Version | Description |
|---|---|---|
| `airspyhf` | 1.8.1.git20260722 | libairspyhf and the airspyhf_* tools for the Airspy HF+ Discovery and Dual Port receivers |
| `aptdec` | 1.7.0.git20250920 | libapt — NOAA APT weather-satellite image decoding library, the libaptdec branch SDRangel's APT demodulator links |
| `cm256cc` | 1.1.2 | cm256cc — Cauchy MDS erasure codes over GF(256); SDRangel's remote sink and source |
| `cppzmq` | 4.11.0 | Header-only C++ binding for ZeroMQ — Horizon EDA's pool updater and GNU Radio's gr-zeromq blocks |
| `cspice` | 67.0.0.git20260416 | CSPICE N0067 — NASA NAIF's SPICE toolkit for planetary ephemerides and observation geometry, as a shared C library |
| `cubicsdr` | 0.2.7 | CubicSDR — software radio receiver with an OpenGL spectrum and waterfall, over SoapySDR and liquid-dsp |
| `dsdcc` | 1.9.6 | DSDcc — digital voice decoder for D-STAR, DMR, dPMR, NXDN and YSF, and the dsdccx tool |
| `ggmorse` | 0.1.0.git20250920 | ggmorse — Morse code decoding library, the fork SDRangel's Morse decoder links |
| `gnuradio` | 3.10.12.0 | GNU Radio — signal-processing blocks for software radio, and GNU Radio Companion to wire them into flow graphs |
| `gqrx` | 2.17.7 | Gqrx — software radio receiver with a spectrum and waterfall, over GNU Radio and gr-osmosdr |
| `gr-funcube` | 3.10.0.git20260208 | gr-funcube — GNU Radio source and control blocks for the FUNcube Dongle Pro and Pro+ |
| `gr-iqbal` | 0.38.3 | gr-iqbal — GNU Radio blocks that estimate and correct I/Q imbalance, used by gr-osmosdr |
| `gr-osmosdr` | 0.2.6 | GNU Radio source and sink blocks for RTL-SDR, HackRF, Airspy, bladeRF, SoapySDR and network radios |
| `gsm` | 1.0.24 | GSM 06.10 lossy speech codec — libgsm and the toast, untoast and tcat tools |
| `inspectrum` | 0.4.0 | inspectrum — inspect recorded radio signals: spectrogram, cursors, and demodulated traces |
| `jemalloc` | 5.4.0 | jemalloc — a malloc that holds fragmentation down under many threads |
| `libad9361` | 0.4.0 | AD936x transceiver helpers over libiio — filter design and multichip sync for ADALM-Pluto in gr-iio |
| `libdab` | 0.8.git20260323 | libdab — the DAB and DAB+ decoder library from dab-cmdline; SDRangel's DAB demodulator |
| `libfobos` | 2.4.1.git20260618 | libfobos — driver library and fobos_* tools for the RigExpert Fobos SDR receiver |
| `libiio` | 0.26 | Linux Industrial I/O client library — ADALM-Pluto and other ADI SDRs over USB and the network, for gr-iio and SDRangel |
| `libinmarsatc` | 20260112 | inmarsatc — Inmarsat-C demodulator, decoder and message parser libraries, the fork SDRangel's Inmarsat demodulator links |
| `libmirisdr` | 2.0.0 | libmirisdr-4 — driver library and miri_sdr and miri_fm tools for Mirics MSi2500/MSi001 receivers |
| `libosmo-dsp` | 0.5.0 | libosmo-dsp — Osmocom complex-vector DSP and I/Q imbalance estimation |
| `libperseus-sdr` | 0.8.2 | libperseus-sdr — driver library and perseustest for the Microtelecom Perseus HF receiver |
| `librfnm` | 0.2.0.git20240716 | librfnm — host library and rfnm_info for RFNM software-defined radio boards |
| `libsigmf` | 20260606 | libsigmf — C++ reader and writer for SigMF signal recordings, with the SDRangel namespace |
| `limesuite` | 23.11.0.git20260603 | LimeSuite — driver library, LimeUtil and the SoapySDR module for LimeSDR boards |
| `liquid-dsp` | 1.8.3 | liquid-dsp — filters, modems, resamplers and FEC for software radio, in C |
| `mbelib` | 1.3.0 | mbelib — AMBE and IMBE vocoder decoding for digital voice radio |
| `nng` | 1.12.4 | nng — nanomsg-next-gen, brokerless pub/sub, request/reply and pipeline messaging |
| `python3-jsonschema` | 4.26.0 | JSON Schema validation for Python |
| `python3-jsonschema-specifications` | 2025.9.1 | The JSON Schema meta-schemas and vocabularies, packaged for jsonschema |
| `python3-referencing` | 0.37.0 | JSON reference resolution across JSON Schema dialects, under jsonschema |
| `python3-rpds-py` | 2026.6.3 | rpds-py — persistent data structures in Rust, under jsonschema's referencing |
| `qwt-qt5` | 6.3.0 | Qwt for Qt 5 — plot, dial and scale widgets for GNU Radio's gr-qtgui and Qt 5 SDR tools |
| `satdump` | 1.2.2 | SatDump — receive, demodulate and decode weather and science satellites into images and products |
| `sdrangel` | 7.27.2 | SDRangel — software radio receiver, transmitter and analyser with decoders for ADS-B, AIS, APRS, FT8, DATV and more |
| `sdrpp` | 1.3.0.dev.20260704 | SDR++ — a software radio receiver with its own GPU-drawn interface, no toolkit |
| `serialdv` | 1.1.5 | SerialDV — AMBE3000 hardware vocoder dongles over serial and UDP, and the dvtest tool |
| `sgp4` | 3.0 | SGP4 — C++ library for satellite orbit propagation from two-line element sets |
| `thrift` | 0.24.0 | Apache Thrift — the IDL compiler, the C++ library and the Python module; GNU Radio's ControlPort transport |
| `uhd` | 4.10.0.0 | USRP Hardware Driver — Ettus/NI USRP radios for GNU Radio's gr-uhd, with the uhd_* utilities and Python API |
| `urh` | 2.10.0 | Universal Radio Hacker — record, demodulate, decode and replay wireless protocols |
| `zenity` | 4.2.2 | Zenity — GTK dialog boxes from the command line, and the file chooser other programs call |
| `zeromq` | 4.3.5 | ZeroMQ — the message sockets Jupyter kernels talk over |

### CAD, electronics, 3D printing and 3D

| Port | Version | Description |
|---|---|---|
| `alembic` | 1.8.12 | Alembic: baked geometry and animation interchange (Ogawa .abc) — Blender's import and export |
| `arpack-ng` | 3.9.1 | ARPACK-NG — large sparse eigenvalue problems, for Octave's eigs and SciPy-style solvers |
| `blender` | 5.2.2 | Blender: 3D modelling, sculpting, animation, rendering (Cycles, EEVEE), compositing and video editing |
| `c-blosc` | 1.21.6 | Blosc: a blocking, shuffling compressor for binary data — OpenVDB's grid codec |
| `calculix` | 2.23 | CalculiX CrunchiX (ccx) — three-dimensional structural and thermal finite element solver with an Abaqus-style input deck |
| `ceres-solver` | 2.2.0.git20251109 | Ceres Solver: non-linear least squares — Blender's camera and motion tracker |
| `cgal` | 6.2.1 | Computational Geometry Algorithms Library (header-only C++) |
| `coin` | 4.0.10 | Open Inventor 3D scene graph library, the viewer under FreeCAD |
| `cura` | 5.13.0 | UltiMaker Cura — prepares 3D models for FDM printing, with printer definitions and material profiles |
| `curaengine` | 5.13.0 | CuraEngine — the slicer behind Cura, also usable on its own from the command line |
| `draco` | 1.5.7 | Draco — Google's compressed geometry and point-cloud format, library and codec tools |
| `freecad` | 1.1.3 | Parametric 3D CAD modeller: Part Design, Sketcher, TechDraw, FEM, CAM, BIM and Assembly |
| `glog` | 0.7.1 | Google's C++ logging library, with gflags for its command-line switches |
| `horizon-eda` | 2.7.2 | Horizon EDA — schematic capture and PCB layout around a pool of parts with unique identities |
| `ifcopenshell` | 0.8.5 | IfcOpenShell — IFC (BIM) toolkit: the C++ parser and OpenCASCADE geometry, IfcConvert and the ifcopenshell Python module FreeCAD's BIM workbench imports |
| `kicad` | 10.0.6 | KiCad — schematic capture, PCB layout, Gerber viewer and SPICE simulation for electronics design |
| `kicad-footprints` | 10.0.6 | KiCad's official PCB footprint library, with the global fp-lib-table template |
| `kicad-symbols` | 10.0.6 | KiCad's official schematic symbol library, with the global sym-lib-table template |
| `kicad-templates` | 10.0.6 | KiCad's official project templates — board outlines and starting points for common form factors |
| `lib3mf` | 2.5.0 | lib3mf — reference implementation of the 3D Manufacturing Format (3MF) for reading and writing 3D print files |
| `libarcus` | 5.11.1 | Arcus — the protobuf message socket between Cura and CuraEngine |
| `libgit2` | 1.9.7 | libgit2 — portable, linkable C implementation of the Git core methods |
| `libharu` | 2.4.6 | Haru: a C library for writing PDF files — Blender's Grease Pencil PDF export |
| `libmed` | 5.0.0 | MED-file, the Salome mesh and field format library over HDF5 (FreeCAD FEM meshes) |
| `libnest2d` | 5.11.0.alpha0 | libnest2d — header-only 2D bin packing, which arranges parts on Cura's build plate |
| `librecad` | 2.2.1.5 | 2D CAD drafting with DXF and DWG import (Qt 5) |
| `libsavitar` | 5.12.0 | Savitar — the 3MF scene reader and writer under Cura |
| `libspnav` | 1.2 | libspnav — client library for 6-DoF 3D mice (3Dconnexion SpaceNavigator and friends) through spacenavd |
| `manifold` | 3.5.4 | Guaranteed-manifold mesh boolean library, the geometry kernel OpenSCAD renders with |
| `netgen` | 6.2.2604 | Netgen — automatic 3D tetrahedral mesh generator (nglib, OpenCASCADE geometry and the Python module FreeCAD's FEM drives) |
| `opencascade` | 7.9.3 | Open CASCADE Technology, the B-rep CAD kernel with STEP, IGES and glTF exchange |
| `opencsg` | 1.8.2 | Image-based constructive solid geometry rendering with OpenGL |
| `openimageio` | 3.1.17.0 | OpenImageIO: image reading, writing and processing for film pipelines — Blender's image I/O, with oiiotool |
| `openscad` | 2026.09.27 | The programmer's solid 3D CAD modeller: models written as scripts, rendered with Manifold and CGAL |
| `opensubdiv` | 3.7.0 | Pixar's OpenSubdiv: subdivision surface evaluation on the CPU and in GLSL — Blender's Subdivision modifier |
| `openvdb` | 13.0.0 | OpenVDB and NanoVDB: sparse volumes — Blender's smoke, fire and volume objects |
| `orcaslicer` | 2.4.2 | OrcaSlicer — G-code slicer for FDM printers with calibration tools and a wide printer-profile library |
| `pivy` | 0.6.11 | Python bindings for Coin and SoQt, the scene-graph API FreeCAD's workbenches script |
| `polyclipping` | 6.4.2 | Clipper 1 — polygon clipping and offsetting library, the API libnest2d and CuraEngine link |
| `prusaslicer` | 2.9.6 | PrusaSlicer — turns 3D models into G-code for FDM and resin printers, with the bundled vendor profiles |
| `python3-isodate` | 0.7.2 | isodate — ISO 8601 date, time and duration parsing and formatting for Python |
| `python3-keyring` | 25.7.0 | keyring — Python access to the desktop's Secret Service, with the keyring command |
| `python3-lark` | 1.3.1 | Lark — a parsing toolkit for context-free grammars, under JupyterLab's RFC 3987 validator |
| `python3-pyarcus` | 5.12.0 | pyArcus — Python bindings for the Arcus socket Cura talks to CuraEngine through |
| `python3-pynest2d` | 5.11.0.alpha0 | pynest2d — Python bindings for libnest2d, the build-plate arranger in Cura |
| `python3-pysavitar` | 5.12.0 | pySavitar — Python bindings for Savitar, Cura's 3MF reader and writer |
| `python3-pyside6` | 6.11.2 | PySide6 and Shiboken6, the official Qt 6 bindings for Python (FreeCAD's scripting GUI) |
| `python3-pyuvula` | 1.1.0 | pyUvula — UV unwrapping and projection for Cura's paint tool, with its Python module |
| `python3-shapely` | 2.1.2 | Shapely — planar geometry (points, lines, polygons and their set operations) over GEOS, for Python |
| `python3-zstandard` | 0.25.0 | python-zstandard — Python bindings for the zstd library |
| `qcad` | 3.33.1.0 | 2D CAD with DXF, script add-ons and a large part library (community edition, Qt 6) |
| `qscintilla` | 2.14.1 | QScintilla — the Scintilla editor widget for Qt 6, in Octave and QGIS |
| `qucs-s` | 26.1.1 | Qucs-S — circuit schematic capture and simulation front end for ngspice, with RF filter, attenuator and line calculators |
| `range-v3` | 0.12.0 | range-v3 — the header-only C++ ranges library C++20 ranges grew from |
| `solvespace` | 3.2 | Parametric 2D and 3D CAD with a constraint solver (Qt 6 interface) |
| `soqt` | 1.6.4 | Qt 6 bindings for the Coin 3D scene graph (viewers and render areas) |
| `spooles` | 2.2 | SPOOLES — sparse direct solver library (LU and Cholesky factorisation, serial and threaded), the default solver of CalculiX |
| `unixodbc` | 2.3.14 | unixODBC — the ODBC driver manager KiCad and LibreOffice Base connect to databases through |
| `uranium` | 5.13.0 | Uranium — the Python and Qt Quick application framework Cura is built on |
| `vtk` | 9.5.2 | The Visualization Toolkit: meshes, filters and OpenGL rendering, with Python bindings |
| `xerces-c` | 3.3.0 | Validating XML parser library in C++ (DOM, SAX, schema) |

### Science, data and development

| Port | Version | Description |
|---|---|---|
| `cantor` | 26.08.1 | Cantor — worksheet front end for Python, R, Octave, Maxima, Qalculate, KAlgebra and Lua |
| `git-cola` | 4.19.0 | Git Cola — a Git GUI for staging, committing and reviewing, with the git-dag history viewer |
| `gl2ps` | 1.4.2 | gl2ps — OpenGL scenes to PostScript, PDF and SVG, for Octave's figure printing |
| `graphicsmagick` | 1.3.48 | GraphicsMagick — image processing and Magick++, behind Octave's imread and imwrite |
| `jupyterlab` | 4.6.4 | JupyterLab and Jupyter Notebook — computational notebooks served locally to the web browser |
| `kdevelop` | 26.08.1 | KDevelop — the KDE IDE for C, C++ and more, with libclang code analysis |
| `kdevelop-pg-qt` | 2.4.0 | KDevelop-PG-Qt — LL(1) parser generator, for KDevelop's QMake project manager |
| `kdiff3` | 1.12.6 | KDiff3 — two- and three-way file and folder comparison and merge |
| `ktexttemplate` | 6.30.0 | KTextTemplate — the KDE Frameworks text template engine, for KDevelop's templates |
| `labplot` | 2.12.1 | LabPlot — data visualisation and analysis: plots, fits, FFT, spreadsheets and live data |
| `libgeotiff` | 1.7.4 | libgeotiff — reads and writes the georeferencing tags of GeoTIFF rasters, with listgeo and geotifcp |
| `libixion` | 0.20.0 | ixion — threaded spreadsheet formula engine, the calculation half of orcus |
| `libkomparediff2` | 26.08.1 | libkomparediff2 — diff parsing and models, for KDevelop's patch review |
| `libksysguard` | 6.7.5 | libksysguard — process list, sensors and system statistics libraries, for KDevelop's attach to process |
| `liborcus` | 0.20.2 | orcus — import filters for ODS, XLSX, Excel 2003 XML, Gnumeric, CSV, JSON and YAML |
| `librttopo` | 1.1.0 | RT Topology Library — PostGIS-style topology and validity functions, for SpatiaLite |
| `libspatialindex` | 2.1.0 | libspatialindex — R-tree spatial indexing, under QGIS |
| `libspatialite` | 5.1.0 | SpatiaLite — spatial SQL for SQLite, as a library and as mod_spatialite |
| `mdds` | 3.2.1 | mdds — header-only multi-dimensional data structures for ixion and orcus |
| `octave` | 11.3.0 | GNU Octave — MATLAB-compatible numerical computing, with its Qt 6 desktop |
| `pdal` | 2.10.2 | PDAL — point cloud translation and processing: LAS/LAZ, E57, Draco, HDF and the pdal tool |
| `python3-bcrypt` | 5.0.0 | bcrypt — the bcrypt password hash for Python, in Rust |
| `python3-cloudpickle` | 3.1.2 | cloudpickle — pickling of functions and classes, which Spyder's kernel sends variables with |
| `python3-comm` | 0.2.3 | comm — the Jupyter comm protocol for kernel-side widgets |
| `python3-debugpy` | 1.8.22 | debugpy — the Debug Adapter Protocol server for Python, behind the debuggers of Spyder and the IPython kernel |
| `python3-h5py` | 3.16.0 | h5py — HDF5 files from Python as numpy arrays, for Veusz's HDF5 import |
| `python3-hatch-jupyter-builder` | 0.10.0 | hatch-jupyter-builder — the hatchling hook that builds and checks Jupyter front-end assets |
| `python3-hatch-nodejs-version` | 0.4.0 | hatch-nodejs-version — hatchling plugins that read version and metadata from package.json |
| `python3-iminuit` | 2.33.0 | iminuit — Minuit2 minimiser and error analysis from Python, for Veusz's fitting |
| `python3-ipykernel` | 6.31.0 | ipykernel — the IPython kernel for Jupyter, under JupyterLab and Spyder |
| `python3-jellyfish` | 1.2.1 | jellyfish — approximate and phonetic string matching, in Rust |
| `python3-jupyter-client` | 8.10.0 | jupyter_client — the Jupyter messaging protocol and kernel manager |
| `python3-jupyter-core` | 5.9.1 | jupyter_core — Jupyter's paths, configuration and the jupyter command |
| `python3-jupyterlab-pygments` | 0.3.0 | jupyterlab_pygments — Pygments highlighting in JupyterLab's colours, for nbconvert |
| `python3-lsp-ruff` | 2.3.4 | python-lsp-ruff — Ruff's diagnostics, fixes and formatting inside the Python language server |
| `python3-lsp-server` | 1.15.0 | python-lsp-server — the Python language server, with Pylint, flake8, Rope, YAPF and Black |
| `python3-nbconvert` | 7.17.1 | nbconvert — notebooks to HTML, PDF, Markdown and scripts, with nbformat and nbclient |
| `python3-nest-asyncio` | 1.6.0 | nest_asyncio — re-entrant asyncio event loops, which ipykernel runs cells in |
| `python3-owslib` | 0.36.0 | OWSLib — OGC web service client (WMS, WFS, WCS, CSW, OGC API), for QGIS MetaSearch |
| `python3-pyemf3` | 3.3 | pyemf3 — pure-Python Enhanced Metafile writer, for Veusz's EMF export |
| `python3-pyzmq` | 27.2.0 | pyzmq — Python bindings for ZeroMQ, the transport of Jupyter kernels |
| `python3-qscintilla` | 2.14.1 | PyQt6.Qsci — QScintilla for Python, for QGIS's console and script editor |
| `python3-send2trash` | 2.1.0 | Send files to the freedesktop.org trash instead of deleting them |
| `python3-tornado` | 6.5.10 | Tornado — the asynchronous web server and networking library under Jupyter |
| `qgis` | 3.44.15 | QGIS — the desktop geographic information system, with PyQGIS and Processing |
| `qrupdate` | 1.2.0 | QR and Cholesky factorisation updates — Octave's qrupdate, cholupdate and friends |
| `qt-creator` | 20.0.2 | Qt Creator — the Qt IDE for C++, QML and Python, with a clangd code model |
| `qwt` | 6.3.0 | Qwt — Qt widgets for plots, dials and scales, for QGIS |
| `R` | 4.6.1 | R — the language and environment for statistics, with the recommended packages |
| `rkward` | 0.8.3 | RKWard — a KDE front end to R: data editor, plots, dialogs and R Markdown |
| `ruff` | 0.16.9 | Ruff — the Python linter and formatter, as the ruff command and the Python module that runs it |
| `spyder` | 6.1.7 | Spyder — the scientific Python IDE: editor, IPython console, variable explorer and plots |
| `sqlitebrowser` | 3.13.1 | DB Browser for SQLite — create, browse, edit and query SQLite databases |
| `suitesparse` | 7.14.1 | Sparse matrix algebra — CHOLMOD, UMFPACK, KLU, SPQR and CXSparse, under Octave |
| `sundials` | 7.9.0 | SUNDIALS — ODE and DAE solvers, behind Octave's ode15s and ode15i |
| `veusz` | 4.2.1 | Veusz — publication-quality 2D and 3D scientific plots, in Python and PyQt6 |

### Amateur radio and mesh

| Port | Version | Description |
|---|---|---|
| `ardopcf` | 1.0.4.1.3 | ardopcf — the ARDOP HF sound-card modem that Pat drives for Winlink email over radio |
| `chirp` | 20260927 | CHIRP — program the memory channels of hundreds of amateur radios, with a wxPython interface and the chirpc CLI |
| `contact` | 1.7.1 | A terminal client for a Meshtastic radio — chat, nodes and settings in curses |
| `db` | 5.3.28 | Berkeley DB — the embedded key/value database library, with its C++ binding and db_* utilities |
| `flamp` | 2.2.14 | flamp — Amateur Multicast Protocol file transfer to many stations at once through fldigi |
| `fldigi` | 4.2.13 | fldigi — sound-card digital modes for amateur radio (PSK, RTTY, Olivia, MFSK, CW) with flarq |
| `flmsg` | 4.0.24 | flmsg — amateur radio message forms (ICS, Radiogram, Red Cross) sent through fldigi |
| `flrig` | 2.0.12 | flrig — transceiver control over CAT for amateur radio, and the rig server fldigi talks to |
| `flxmlrpc` | 1.0.1 | flxmlrpc — the XML-RPC library shared by fldigi, flmsg and flamp |
| `freedv-gui` | 2.4.0 | FreeDV — HF digital voice for amateur radio: RADE, 700D, 700E and 1600 modes over a sound card |
| `goocanvas` | 3.0.0 | GooCanvas — a cairo canvas widget for GTK 3, with introspection data |
| `gpredict` | 2.6 | Gpredict — real-time satellite tracking and pass prediction with radio and rotator control |
| `js8call` | 3.0.3 | JS8Call — weak-signal keyboard-to-keyboard and store-and-forward messaging for HF amateur radio |
| `klog` | 2.6 | KLog — amateur radio logbook with DXCC and award tracking, ADIF, LoTW and a DX cluster |
| `meshtastic-firmware` | 2.7.26 | Meshtastic radio firmware images, to flash a LoRa node with no network |
| `nomadnet` | 1.4.3 | Nomad Network — messages, pages and files over a Reticulum mesh, in a terminal |
| `pat` | 1.0.0 | Pat — a Winlink email client for amateur radio, over ARDOP, AX.25 packet and telnet |
| `python3-adafruit-nrfutil` | 0.5.3.post16 | adafruit-nrfutil — serial DFU flasher for nRF52 boards; rnodeconf flashes RAK4631, T-Echo and Heltec T114 RNodes with it |
| `python3-ecdsa` | 0.19.2 | Pure-python ECDSA and EdDSA signatures — adafruit-nrfutil signs nRF52 firmware packages with it |
| `python3-kivy` | 2.3.1 | Kivy — a python UI framework drawing with OpenGL through SDL2 |
| `python3-lxmf` | 1.1.1 | LXMF — store-and-forward messaging over Reticulum, and the lxmd propagation node |
| `python3-qrcode` | 8.2 | QR code generator for python, as text or as an image |
| `python3-rns` | 1.5.4 | Reticulum — encrypted mesh networking over LoRa, packet radio, WiFi or anything that carries bytes |
| `qsstv` | 9.5.8 | QSSTV — slow-scan television and digital image modes (HamDRM) for amateur radio |
| `rnode-firmware` | 1.86 | RNode LoRa radio firmware images, for rnodeconf to flash with no network |
| `sideband` | 2.1.0 | Sideband — LXMF messaging, voice, telemetry and maps over a Reticulum mesh |
| `wsjtx` | 3.0.1 | WSJT-X — FT8, FT4, JT65, Q65, WSPR and other weak-signal digital modes for amateur radio |
| `xastir` | 2.2.4 | Xastir — APRS mapping and messaging over TNCs, AX.25 and APRS-IS (Motif, under Xwayland) |
| `xnec2c` | 4.4.18 | xnec2c — GTK antenna modelling with NEC2: patterns, impedance, gain and SWR over frequency |

### Services for a LAN

| Port | Version | Description |
|---|---|---|
| `babeld` | 1.14 | Babel routing daemon — routes across a mesh of wired and wireless links |
| `batctl` | 2026.3 | Control tool for B.A.T.M.A.N. advanced — the kernel's layer-2 mesh |
| `freeipmi` | 1.6.19 | IPMI tools and libraries — sensors, event log, power control and serial-over-LAN for server BMCs and IPMI power supplies |
| `maddy` | 0.9.5 | A mail server in one program — SMTP, submission and IMAP for a LAN |
| `minidlna` | 1.3.3 | ReadyMedia — a DLNA media server for the televisions and players on a LAN |
| `neon` | 0.37.1 | HTTP and WebDAV client library with TLS, used by NUT's netxml-ups driver and Audacious |
| `net-snmp` | 5.9.5.2 | SNMP library, tools and agent — read network UPS cards, switches and printers (snmpget, snmpwalk, snmpd) |
| `ngircd` | 28 | An IRC server small enough for one room — chat on a LAN with no internet |
| `nut` | 2.8.5 | Network UPS Tools — watch a UPS or inverter and shut down before the battery does |
| `powerman` | 2.4.4 | PowerMan — control remote power distribution units and BMCs over telnet, serial, HTTP, Redfish and SNMP |
| `radicale` | 3.8.1 | A CalDAV and CardDAV server — shared calendars and contacts on a LAN with no internet |

### Meshing, charts, music production and the workshop

| Port | Version | Description |
|---|---|---|
| `ardour` | 9.8.0 | Ardour — digital audio workstation: multitrack recording, editing, mixing and LV2 plugins (YTK, X11) |
| `bcnc` | 0.9.16 | bCNC — GRBL CNC controller and G-code sender with an editor, probing, auto-levelling and CAM tools (Tk) |
| `bwidget` | 1.10.1 | BWidget — high-level Tk widgets written in pure Tcl: trees, notebooks, combo boxes, dialogs |
| `candle2` | 2.4 | Candle2 — GRBL CNC and laser controller with a G-code visualiser, height maps and jogging (Qt 5) |
| `cncjs` | 1.11.5 | CNCjs — web-based controller for Grbl, Marlin, Smoothieware and TinyG CNC machines, served to the browser |
| `digital` | 0.31 | Digital — digital logic designer and circuit simulator for teaching, with FSM and HDL export (Java Swing) |
| `gmsh` | 4.15.2 | Gmsh — 3D finite element mesh generator with CAD, meshing and post-processing (FLTK) |
| `josm` | 19613 | JOSM — the Java OpenStreetMap editor: map data, GPS traces, imagery and validation (Java Swing) |
| `libgig` | 4.6.0 | Gigasampler, DLS, SoundFont 2 and KORG sample-library file access, with the gigextract and dlsdump tools |
| `libnova` | 0.15.0 | Celestial mechanics and astronomical calculation library — sun and moon rise, set and phase for Viking |
| `librepcb` | 2.1.1 | LibrePCB — schematic and PCB design with managed libraries, STEP 3D export and Gerber output (Qt 6, Slint) |
| `libsoundio` | 2.0.0 | Cross-backend real-time audio input and output, through PulseAudio or ALSA |
| `linuxcnc` | 2.9.10 | LinuxCNC — CNC machine controller for mills, lathes, routers and plasma: HAL, G-code interpreter and the AXIS GUI |
| `lmms` | 1.2.2 | LMMS — pattern-based music production: sequencer, synthesizers, samples and LADSPA effects (Qt 5) |
| `logisim-evolution` | 5.0.0 | Logisim-evolution — digital logic designer and simulator with FPGA synthesis export (Java Swing) |
| `opencpn` | 5.14.0 | OpenCPN — chart plotter and navigation for S-57/S-63 ENC and raster charts, AIS and GPS (wxWidgets) |
| `python3-opengl` | 3.1.10 | PyOpenGL — the OpenGL, GLU and GLUT bindings for Python, loaded through ctypes |
| `python3-tkinter` | 3.14.7 | tkinter — Python's binding to Tk, the _tkinter module and the tkinter package (X11 under Xwayland) |
| `python3-xlib` | 0.33 | python-xlib — the X11 client protocol implemented in pure Python |
| `python3-yapps` | 2.2.0 | Yapps — Yet Another Python Parser System, the yapps2 LL(1) parser generator |
| `sdl12-compat` | 1.2.76 | The SDL 1.2 library, headers and build files, answering every SDL 1.2 call through SDL2 |
| `stk` | 5.0.1 | The Synthesis ToolKit — C++ physical-model and signal-processing instruments, with their rawwave samples |
| `viking` | 1.11 | Viking — GPS track, waypoint and route manager on maps, with MBTiles and gpsd (GTK 3) |

### Clinic, shop and family

| Port | Version | Description |
|---|---|---|
| `dcmtk` | 3.7.0 | DCMTK — the OFFIS DICOM toolkit: medical image conversion, network storage and query tools |
| `glabels` | 3.4.1 | gLabels — labels, business cards and envelopes, with barcodes and mail merge |
| `gnuhealth` | 5.0.7 | GNU Health — hospital and clinic information system: patients, labs, pharmacy, imaging and stock, on Tryton |
| `gramps` | 6.0.8 | Gramps — genealogy: family trees, people, places, sources and reports |
| `ptouch-print` | 1.9 | ptouch-print — print text and images on Brother P-touch label printers over USB |
| `tryton` | 7.0.44 | Tryton — desktop client for the Tryton business platform and GNU Health, in GTK 3 |
| `zint` | 2.16.0 | Zint — barcode encoder for over 50 symbologies, with a CLI and the Zint Barcode Studio GUI |

### Windows programs

| Port | Version | Description |
|---|---|---|
| `wine` | 11.0 | Wine — runs Windows programs on Linux, 64- and 32-bit, on Wayland or X11 |
| `wine-gecko` | 2.47.4 | Wine Gecko — the HTML engine Wine's MSHTML uses, 32- and 64-bit, as upstream builds it |
| `wine-mono` | 10.4.1 | Wine Mono — the .NET Framework runtime Wine installs into a prefix, as upstream builds it |

### The light tier

| Port | Version | Description |
|---|---|---|
| `ft2-clone` | 2.24 | Fasttracker II clone — the XM tracker, pixel for pixel, on SDL2 |
| `furnace` | 0.6.8.3 | Chiptune tracker for over fifty sound chips of consoles, computers and arcades |
| `fuzzel` | 1.15.0 | Application launcher and dmenu replacement for wlroots compositors |
| `goxel` | 0.15.1 | 3D voxel editor, drawn with OpenGL through GLFW and no toolkit |
| `grafx2` | 2.9 | Pixel-art paint program in the Deluxe Paint tradition, on SDL2 with no toolkit |
| `inlyne` | 0.5.3 | GPU-drawn Markdown and HTML viewer with live reload and no browser engine |
| `koreader` | 2026.07.2 | E-book and document reader for EPUB, PDF, DjVu, FB2 and comics, on SDL3 with no toolkit |
| `lhasa` | 0.6.0 | Free LHA/LZH archive library and lha extractor — MilkyTracker's .lha module archives |
| `lite-xl` | 2.1.8 | Text editor written in Lua, drawn with SDL3 and no toolkit |
| `milkytracker` | 1.06 | Fast Tracker II–style music tracker for XM and MOD files, on SDL2 |
| `pt2-clone` | 1.92 | ProTracker 2 clone — the Amiga MOD tracker, pixel for pixel, on SDL2 |
| `schismtracker` | 20260524 | Impulse Tracker clone — compose and play IT, S3M, XM and MOD music, on SDL3 |
| `sniffnet` | 1.5.1 | Watch network traffic by host, service, program and country, in a GPU-drawn window |
| `surfer` | 0.7.0 | Waveform viewer for VCD, FST and GHW simulation traces, drawn with egui |
| `uosc` | 5.13.0 | Minimalist, proximity-based on-screen controls, menus and playlist for mpv |
| `wayland-utils` | 1.3.0 | wayland-info — list the globals, outputs, seats and formats a compositor offers |
| `wev` | 1.1.0 | Wayland event viewer — print every input event a window receives |
| `wl-mirror` | 0.18.5 | Mirror a Wayland output into a window, for a projector or a second screen |

### Games

| Port | Version | Description |
|---|---|---|
| `aisleriot` | 3.22.35 | AisleRiot — over eighty solitaire card games, each written in Scheme |
| `asio` | 1.38.2 | Asio — the standalone, header-only C++ networking and asynchronous I/O library, without Boost |
| `black-hole-solver` | 1.14.0 | Solver for the Black Hole, All in a Row and Golf patience games |
| `brogue-ce` | 1.15.1 | Brogue Community Edition — a roguelike of 26 dungeon levels, drawn in SDL2 tiles or glyphs |
| `cataclysm-dda` | 0.I.1 | Cataclysm: Dark Days Ahead — turn-based survival in a procedurally generated post-apocalyptic world, in its tiles build |
| `chess-tui` | 2.7.1 | Chess in a terminal, against Stockfish or a second player |
| `chocolate-doom` | 3.1.1 | Doom engine faithful to the DOS original — Doom, Heretic, Hexen and Strife |
| `crawl-tiles` | 0.34.1 | Dungeon Crawl Stone Soup — the open-ended roguelike of the Orb of Zot, in its graphical tiles build |
| `crispy-doom` | 7.1 | Limit-removing Doom engine — Chocolate Doom with higher resolution and quality-of-life fixes |
| `ddnet` | 20.1 | DDraceNetwork — the cooperative Teeworlds racing game, its client, server and map tools |
| `deutex` | 5.2.3 | Doom WAD composer and decomposer — builds an IWAD from lumps, PNGs and sounds |
| `devilutionx` | 1.5.5 | DevilutionX — the Diablo and Hellfire engine, for a player's own copy of the game data |
| `dsda-doom` | 0.29.4 | Speedrunning Doom engine from the PrBoom+ line — Boom, MBF21, UMAPINFO, OpenGL renderer |
| `endless-sky` | 0.11.2 | Endless Sky — 2D space trading, exploration and combat in the spirit of Escape Velocity |
| `fheroes2` | 1.1.17 | fheroes2 — the Heroes of Might and Magic II engine, for a player's own copy of the game data |
| `flare-engine` | 1.15 | FLARE — the engine of the Flare isometric action RPG, run by the game data of flare-game or any other mod |
| `flare-game` | 1.15 | Flare: Empyrean Campaign — the art, music, maps and story that make the FLARE engine a game |
| `freecell-solver` | 6.16.0 | Solver for Freecell, Simple Simon and related patience games, as a library and a command |
| `freeciv` | 3.2.6 | Freeciv — turn-based empire-building strategy in the Civilization tradition, SDL2 client and server |
| `freedoom` | 0.13.0 | Free game data for Doom engines — Phase 1, Phase 2 and FreeDM IWADs built from their lumps |
| `gnome-mahjongg` | 51.1 | GNOME Mahjongg — match pairs of tiles until the board is clear |
| `gnome-mines` | 50.0 | GNOME Mines — clear hidden mines from a minefield |
| `gnome-sudoku` | 51.0.1 | GNOME Sudoku — the number-grid puzzle, generated at four difficulties |
| `ioquake3` | 1.36.20260917 | Quake III Arena engine — the maintained id Tech 3 with SDL, OpenAL, VoIP and current renderers |
| `ironwail` | 0.8.2 | Quake engine from the QuakeSpasm line, with an OpenGL renderer that keeps the original look |
| `katomic` | 26.08.1 | KAtomic — slide atoms into place to build the molecule |
| `kblocks` | 26.08.1 | KBlocks — the falling-blocks game, against the clock or a computer player |
| `kbounce` | 26.08.1 | KBounce — fence off the field while the balls bounce around it |
| `kmahjongg` | 26.08.1 | KMahjongg — mahjong solitaire: clear the board by matching pairs of free tiles |
| `kmines` | 26.08.1 | KMines — the classic minesweeper, on a themed board |
| `kpat` | 26.08.1 | KPatience — fourteen solitaire card games, with a solver for each deal |
| `kreversi` | 26.08.1 | KReversi — the reversi board game against the computer or a second player |
| `ksudoku` | 26.08.1 | KSudoku — sudoku, jigsaw, killer and 3-D Roxdoku puzzles, generated and solved |
| `libwebsockets` | 4.5.8 | C library for WebSocket and HTTP/1 and HTTP/2 clients and servers |
| `love` | 11.5 | LÖVE — the Lua 2D game framework, and the runtime that opens a .love game |
| `luanti` | 5.17.0 | Luanti (formerly Minetest) — voxel game engine, with Minetest Game for offline play |
| `neverball` | 1.6.0 | Neverball and Neverputt — tilt the floor to roll a ball through 3D obstacle courses, and minigolf on the same engine |
| `openrct2` | 0.5.5 | OpenRCT2 — the RollerCoaster Tycoon 2 engine, for a player's own copy of the RCT2 or RCT Classic data |
| `openttd` | 15.3 | Transport Tycoon Deluxe engine — build rail, road, air and sea networks |
| `openttd-opengfx` | 8.0 | OpenGFX: the free graphics base set for OpenTTD |
| `openttd-openmsx` | 0.4.2 | OpenMSX: the free music base set for OpenTTD |
| `openttd-opensfx` | 1.0.3 | OpenSFX: the free sound base set for OpenTTD |
| `pioneer` | 20260907 | Pioneer — open-ended space trading and combat across a procedurally generated Milky Way, with Newtonian flight |
| `powder-toy` | 100.1.400 | The Powder Toy — a falling-sand physics sandbox of air pressure, heat, gravity and electronics, with local saves |
| `qqwing` | 1.3.4 | Sudoku generator and solver, as a C++ library and a command |
| `rinutils` | 0.10.3 | Header-only C utility macros shared by Shlomi Fish's solvers |
| `sauerbraten` | 2020.12.29 | Cube 2: Sauerbraten — the Cube 2 engine shooter with in-game co-operative map editing, and its data |
| `srb2` | 2.2.15 | Sonic Robo Blast 2 — a 3D Sonic fan game on a heavily modified Doom Legacy engine, with its game data |
| `stk-assets` | 1.5 | SuperTuxKart game data — karts, tracks, arenas, music, sounds, models and textures |
| `stockfish` | 19 | Stockfish — the UCI chess engine, with its evaluation network built in |
| `supertux` | 0.7.0 | SuperTux — classic 2D side-scrolling platformer with Tux, with a level editor |
| `supertuxkart` | 1.5 | SuperTuxKart — 3D kart racing with story mode, grand prix, battles and soccer |
| `warzone2100` | 4.7.0 | Warzone 2100 — post-apocalyptic real-time strategy with campaigns, skirmish and a research tree |
| `wesnoth` | 1.18.8 | Battle for Wesnoth — turn-based fantasy strategy with campaigns, skirmishes and a map editor |
| `widelands` | 1.3.1 | Widelands — real-time strategy about building an economy and its roads, in the line of The Settlers II |
| `xonotic` | 0.8.6 | Xonotic — the arena first-person shooter on the DarkPlaces engine: SDL client and dedicated server |
| `xonotic-data` | 0.8.6 | Xonotic's game data — maps, models, textures, sounds and music, about 1.2 GB |
| `yamagi-quake2` | 8.70 | Quake II engine kept faithful to the original — OpenGL 1.4, 3.2, GLES3 and software renderers |

### Emulators

| Port | Version | Description |
|---|---|---|
| `amiberry` | 8.3.0 | Amiberry — Amiga emulator from the A500 to the A4000 and CD32, with WHDLoad booting and the AROS ROMs |
| `dosbox-staging` | 0.83.0 | DOSBox Staging — a DOS PC emulator for games and old software |
| `dosbox-x` | 2026.08.31 | DOSBox-X — x86 PC emulator for DOS, Windows 3.x/9x, PC-98 and PCjr/Tandy software |
| `fuse-emulator` | 1.10.0 | Fuse — the Free Unix Spectrum Emulator, 16K to +3, Pentagon and Timex, with its ROMs |
| `hatari` | 2.6.1 | Hatari — Atari ST, STE, TT and Falcon emulator, with the Hatari UI configuration front end |
| `iir1` | 1.10.0 | IIR1 — realtime C++ IIR filter library (Butterworth, Chebyshev, RBJ) |
| `libmt32emu` | 2.8.3 | libmt32emu — Roland MT-32, CM-32L and LAPC-I synthesiser emulation (needs the original control and PCM ROMs) |
| `libretro-beetle-psx` | 20260927 | Beetle PSX as libretro cores — Mednafen's PlayStation emulator, software and OpenGL/Vulkan renderers |
| `libretro-core-info` | 1.22.2 | libretro core info files — what each core is called, which files it opens and which firmware it wants |
| `libretro-database` | 1.22.1 | libretro game databases — the checksums RetroArch's scanner names a game by, and the cursors that query them |
| `libretro-gambatte` | 20260821 | Gambatte as a libretro core — an accuracy-first Game Boy and Game Boy Color emulator |
| `libretro-genesis-plus-gx` | 20260912 | Genesis Plus GX as a libretro core — Mega Drive, Mega-CD, Master System and Game Gear (non-commercial licence) |
| `libretro-melonds` | 20260719 | melonDS as a libretro core — Nintendo DS emulator |
| `libretro-mgba` | 0.10.5 | mGBA as a libretro core — Game Boy Advance, Game Boy and Game Boy Color in RetroArch |
| `libretro-mupen64plus-next` | 20260912 | Mupen64Plus-Next as a libretro core — Nintendo 64 emulator with the GLideN64 renderer |
| `libretro-nestopia` | 20260925 | Nestopia UE as a libretro core — Nintendo Entertainment System and Famicom Disk System emulator |
| `libretro-snes9x` | 1.63 | Snes9x as a libretro core — Super Nintendo emulator (non-commercial licence) |
| `libspectrum` | 1.7.0 | ZX Spectrum emulator file formats — snapshots, tapes, disks and RZX recordings |
| `mednafen` | 1.32.1 | Mednafen — command-line multi-system emulator: NES, SNES, Game Boy, GBA, Mega Drive, PC Engine, PlayStation, Saturn and more |
| `mgba` | 0.10.5 | mGBA — Game Boy Advance, Game Boy and Game Boy Color emulator, SDL front end |
| `mupen64plus` | 2.6.0 | Mupen64Plus — Nintendo 64 emulator: core, console front end, SDL audio and input, HLE RSP, Rice and Glide64mk2 video |
| `openmsx` | 21.0 | openMSX — MSX home computer emulator with C-BIOS, a built-in debugger and GUI |
| `ppsspp` | 1.20.4 | PPSSPP — PlayStation Portable emulator, OpenGL and Vulkan, drawn through SDL |
| `retroarch` | 1.22.2 | RetroArch — the libretro frontend: one window, one menu and one set of controls for every emulator core |
| `retroarch-assets` | 1.22.0 | RetroArch menu artwork — the XMB, Ozone, RGUI and GLUI themes, their icons, fonts and sounds |
| `retroarch-joypad-autoconfig` | 1.22.0 | RetroArch controller profiles — a game pad is mapped the moment it is plugged in |
| `scummvm` | 2026.3.0 | ScummVM — runs classic point-and-click adventure and role-playing games from their original data files |
| `stella` | 7.0c | Stella — Atari 2600 VCS emulator with a built-in debugger |
| `vice` | 3.10 | VICE — Commodore 64, 128, VIC-20, PET, Plus/4 and CBM-II emulators, with their ROMs |
| `zlib-ng` | 2.3.3 | zlib-ng — zlib data compression with SIMD, under its own zng_ names beside the system zlib |

### Data the applications read

The list's last group holds more than its title says: after `whisper-model-base-en` it carries
KDOS's own phase-4 ports (`kdos-splash`, `kdos-tools`, `kdos-appbox`, `kdos-boxinit` and the theme
packages).

| Port | Version | Description |
|---|---|---|
| `kdos-appbox` | 1.2.0 | KDOS alien app runtime and appbox manager |
| `kdos-boxinit` | 0.1.0 | pid 1 inside a pack box, in place of distrobox-init |
| `kdos-cursors` | 2.0 | KDOS phosphor cursor theme |
| `kdos-gtk-theme` | 6.5 | KDOS phosphor GTK theme — a recoloured adw-gtk3 |
| `kdos-icons` | 20250501 | KDOS phosphor icon theme — a recoloured Papirus |
| `kdos-splash` | 1.1 | Boot splash — CRT power-on animation drawn on the framebuffer |
| `kdos-theme` | 1.0.0 | KDOS theme generators — GTK stylesheet, icons, cursors |
| `kdos-tools` | 1.2.0 | KDOS system tools — kdos, services, getty, screenshots, fetch |
| `whisper-model-base-en` | 20241029 | The Whisper base.en speech model in ggml form, for whisper.cpp and kdos-rec |

## The desktop phase

`script/05_desktop/packages.txt` installs the desktop: wlroots, the compositor, the shell, the
terminal, the lock screen, the root daemons, the pack tools, the input method and the portals. It is
the only phase that resolves against `src/desktop`. The list opens with an unheaded block before
its one named group.

### Unheaded: wlroots, the compositor, box socket, shell, terminal and lock screen

The list names `xcb-util-wm` first, the ICCCM and EWMH helpers that wlroots' Xwayland window
manager needs. Phase 4 names it too, under "X11 client libraries", because Qt's xcb platform and
KWindowSystem link it, so it is catalogued there.

| Port | Version | Description |
|---|---|---|
| `kdos-boxsock` | 0.1.0 | Per-box tagged Wayland socket — the security-context-v1 engine |
| `kdos-comp` | 0.20.0 | The KDOS compositor — a hard fork of labwc 0.20.0 |
| `kdos-lock` | 0.2.0 | The KDOS lock screen and its setuid password checker |
| `kdos-shell` | 0.2.0 | The KDOS shell — a character-cell panel on layer-shell |
| `kdos-term` | 0.1.0 | KDOS terminal emulator |
| `wlroots` | 0.20.2 | Modular Wayland compositor library — the base kdos-comp is built on |

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
[toybox and the tools it overlaps](../03-architecture/packaging.md#toybox-and-the-tools-it-overlaps)). `xcb-util-wm` is named twice for
another reason: phase 4 needs it for Qt and KDE Frameworks, and the desktop phase names it again
beside wlroots.

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
| `xcb-util-wm` | phase 4 (“X11 client libraries”), the desktop phase (unheaded) |
| `xz` | phase 3 (“Essential Utilities”), phase 4 (“Core Build Utilities (host-side)”) |
| `zlib` | phase 2 (unheaded), phase 3 (“Base Libraries & Database”) |

## Installed as dependencies

No list names these 244 ports. Each is installed because a port that is installed names it in
`depends =`, directly or through another dependency. They are grouped here by what they are for;
the grouping is this chapter's, not the tree's.

### C and C++ support libraries

| Port | Version | Description |
|---|---|---|
| `abseil-cpp` | 20260817.0 | Abseil — Google's C++ common libraries (strings, containers, synchronization, time) |
| `argp-standalone` | 1.4.1 | Standalone version of arguments parsing functions from GLIBC |
| `basu` | 0.2.1 | sd-bus library extracted from systemd (no-systemd sd-bus provider) |
| `boost` | 1.92.0 | Boost C++ libraries — the headers and the compiled components the catalogue links |
| `double-conversion` | 3.4.0 | Binary-decimal and decimal-binary routines for IEEE doubles (Qt6 dep) |
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
| `uthash` | 2.4.0 | A hash table for C structures (header-only) |
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
| `yajl` | 2.1.0 | Yet Another JSON Library — small, event-driven C JSON parser |
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
| `desktop-file-utils` | 0.28 | Command line utilities for working with Desktop entries |
| `doxygen` | 1.18.0 | Documentation system for C++, C, Java, Objective-C, Python, IDL, PHP, C# |
| `extra-cmake-modules` | 6.30.0 | Extra modules and scripts for CMake (KDE) |
| `glib-introspection` | 2.90.0 | GLib's introspection data: the GLib, GObject, GModule, Gio and GIRepository GIR and typelib files |
| `gnu-efi` | 4.0.4 | GNU EFI library |
| `go-md2man` | 2.0.7 | Converts Markdown to roff manual pages |
| `gobject-introspection` | 1.86.0 | GObject introspection: g-ir-scanner, g-ir-compiler, libgirepository-1.0 and the base GIR/typelib data (cairo, freetype2, fontconfig, libxml2, GL, DBus) |
| `lowdown` | 3.2.1 | Markdown translator to roff manual pages, HTML and LaTeX |
| `musl-ldd` | 1.2.5 | LDD script for Musl |
| `sgml-common` | 0.6.3 | Creating and maintaining centralized SGML catalogs |

### Language runtimes

| Port | Version | Description |
|---|---|---|
| `luajit` | 20260914 | Just-in-time compiler and drop-in replacement for Lua. |
| `ruby` | 4.0.7 | The Ruby programming language |

### Python modules installed as dependencies

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
| `volume_key` | 0.3.12 | Library for manipulating storage volume encryption keys and storing them separately from volumes to handle forgotten passphrases |
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
| `libxkbcommon` | 1.13.2 | Keymap compiler and support library, with its Wayland tools and xkbcommon-x11 |
| `mtdev` | 1.1.7 | Multitouch Protocol Translation Library which is used to transform all variants of kernel MT (Multitouch) events to the slotted type B protocol |
| `v4l-utils` | 1.32.0 | Video4Linux userspace — libv4l2 and libv4lconvert, libdvbv5, v4l2-ctl, media-ctl, ir-keytable, cec-ctl |
| `xkeyboard-config` | 2.48 | Keyboard configuration database (keymap data, used by libxkbcommon) |

### Graphics, GPU and text rendering

| Port | Version | Description |
|---|---|---|
| `cairo` | 1.18.6 | 2D graphics library, with its Xlib and XCB surfaces |
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
| `libglvnd` | 1.7.0 | GL Vendor-Neutral Dispatch: libGL, libGLX, libOpenGL, libEGL and libGLES |
| `libpciaccess` | 0.19 | X11 PCI access library |
| `mesa` | 26.2.3 | OpenGL compatible 3D graphics library: EGL and Vulkan for Wayland, GLX for X11 clients under Xwayland |
| `ocl-icd` | 2.3.5 | OpenCL ICD loader — libOpenCL.so dispatching to the drivers listed in /etc/OpenCL/vendors |
| `opencl-headers` | 2026.05.29 | Khronos OpenCL C API headers |
| `pango` | 1.58.2 | Text layout and rendering library, with pangoxft |
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

### X11 client libraries and fonts

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
| `xcb-util` | 0.4.1 | XCB utility convenience functions |
| `xcb-util-cursor` | 0.1.6 | XCB cursor library (libxcb-cursor) |
| `xcb-util-image` | 0.4.1 | XCB port of Xlib XImage |
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

Nothing in any list reaches these 6 recipes through `depends =`, and no phase script builds
them, so none is on the image. Their sources are fetched and checked like any other port's, and
each builds on request with `kpkg install <name>` wherever the ports tree is on `PORT_REPO`, as it
is inside the build chroot.

| Port | Version | Description |
|---|---|---|
| `helix` | 25.07.1 | helix — modal text editor (Rust) |
| `icon-naming-utils` | 0.8.90 | Perl script used for maintaining backwards compatibility with current desktop icon themes |
| `musl-locales` | 20260425 | A locale command and message catalogues for musl |
| `perl-xml-simple` | 2.25 | Perl module that reads and writes XML as nested data structures (config files especially) |
| `setconf` | 0.7.7 | Utility for changing settings in configuration files |
| `stemmer` | 3.1.1 | Stemming library supporting several languages |

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
