# System

<!-- kfs:phase 41_system -->

`41_system` is the largest phase of the build. It installs everything whose [dependency closure]{.idx}
reaches no graphics and no toolkit: the services the machine boots into, networking, storage and
file systems, the command line, cryptography, the audio and image codecs, fonts, and the science,
radio and hardware libraries. It also builds KDOS's own artwork and its first system programs. This
part describes the rule that fills the phase, how its list is filed, the order file that sorts
ahead of every shelf and what it does to a parallel build, and which of its ports have a full
page. The package pages follow, in chapters by shelf, in the shelves' own order, and within a shelf
in the order [kpkgdepends]{.idx} builds them.

## What belongs here

A port is installed here when nothing in its closure is graphical: no Wayland, no Xlib, no Mesa or
libdrm, no cairo or pango, no GStreamer, FFmpeg or PipeWire, and no toolkit. Subject does not
enter into it. A terminal game, a text-mode browser, the data files of a graphical game or a font
is a `41_system` port when its closure stays clear of those, even though its shelf is one of the
application shelves; the same shelf then has more of its ports in Part X or Part XII. The table of
phases and what decides each is in
[Which phase lists a port](../../kdos/05-developer/writing-ports.md#which-phase-lists-a-port).

The same rule explains a few shelves that look out of place here. The `x11` chapter holds the X
protocol headers and the XCB libraries, which draw nothing; Xlib itself, `libX11`, is one of the
libraries that put a port into `42_graphics`. The `gpu` chapter holds the Vulkan and SPIR-V
headers, the shader compilers and the OpenCL loader, and not Mesa. The `gtk` chapter holds GLib,
GObject introspection and dconf, and not GTK.

## The list, by shelf

The list is `script/phases/41_system/packages.d/`, one file per shelf named after it, plus
`src-system.txt` and `src-art.txt` for the programs of KDOS's own from those two areas of `src/`
(see [Package lists](../../kdos/05-developer/build-system.md#package-lists)). Almost every shelf
of [`ports/shelves`]{.idx} has a file here. The chapters fall into a few groups:

The machine itself
: `base` (the D-Bus daemon, the system logger, `seatd`, `snooze`, the s6 tools, procps),
  `auth` (PAM, polkit, sudo, Kerberos, fingerprint readers), `boot` (Limine, `efibootmgr`, the
  firmware and microcode), `disk` and `filesystem` (partitioning, LVM, RAID, LUKS, SMART, and the
  tools of every file system KDOS can create or read), `containers` (`crun`, `conmon`, `netavark`,
  `passt` and the other pieces podman runs on), and `network` (NetworkManager, `wpa_supplicant`,
  ModemManager, nftables, `chrony`, the VPN clients).

Libraries the later phases link
: `base-libs`, `devlibs`, `formats`, `database`, `archiver`, `crypto`, `net-libs` and `i18n`: the
  C and C++ utility libraries, XML, JSON and protobuf, SQLite's neighbours, compression, GnuTLS,
  NSS and the smartcard stack, the networking libraries, and the Unicode, bidirectional-text and
  line-breaking libraries.

The command line
: `shells`, `cli`, `files`, `sysmon`, `editors`, `mail`, `chat`, `transfer`, `servers` and
  `net-diag`: terminal programs and daemons, from `tmux` and `zsh` to Neovim, WeeChat, OpenSSH
  and the network diagnostics.

Codecs and media data
: `audio-io` (ALSA and the PulseAudio client library; PipeWire itself is Part X's),
  `audio-codecs`, `audio-dsp`, `music-libs`, `video-libs` and `image-libs`, and the `fonts`
  chapter, which carries FreeType and Fontconfig as well as the font packages.

Science and hardware
: `sci-libs`, `math`, `astronomy`, `bioscience`, `gis`, `eda`, `embedded`, `sdr-hw`, `sdr`,
  `hamradio`, `hardware`, `mobile`, `optical` and `ai`: numerical libraries, cross toolchains for
  microcontrollers, radio drivers, and the device tools.

KDOS's own
: `src-art` (the theme generator `kdos-theme`, the icon, cursor and GTK themes it generates, the
  boot splash and the `kdos-bb` demo) and `src-system` (`kdos-pack`, `kdos-boxinit`, and the two
  books, `kdos-doc` and `kdos-kfs`). They come last in the part, after every shelf.

## The order file

[`00-order.txt`]{.idx} sorts ahead of every shelf's file, so the ports it names are resolved first, in the
order it gives. Its header says what it is for:

<!-- kfs:include script/phases/41_system/packages.d/00-order.txt -->

A run belongs here when a comment can say why its order matters and `depends =` cannot express it.
The header names one such case, two ports that install the same path;
[Which phase lists a port](../../kdos/05-developer/writing-ports.md#which-phase-lists-a-port)
names another, a port that takes paths from another through [`replaces =`]{.idx term="replaces"} and so must install
after it, because the other order is refused as a [file conflict]{.idx}. It is the only place in the
phase's list where a port that an earlier phase installed may be named again.

The file is also the phase's [serial prefix]{.idx}. Under [`kdosbuild --port-jobs`]{.idx term="--port-jobs"}, the ports it
names, and every port the resolved order places among them, build and install one at a time, and
no other port of the phase starts until the last of them is installed, so every run written here
costs parallelism. A file that names no port, like the one above, leaves the prefix empty, and the
whole phase is built level by level. When a [parallel build]{.idx} commits a level and [kdosbuild]{.idx} sees a path change hands in an order the
serial run would have refused, it names the pair and asks for it to be pinned here; see
[Building a package phase by level](../../kdos/05-developer/build-system.md#building-a-package-phase-by-level).

## Which ports have full pages

The [Tier A]{.idx} ports of this phase are the parts of the KDOS spine that need no graphics: the D-Bus
daemon, `seatd`, `sysklogd`, `snooze` and polkit; Limine, `efibootmgr` and `linux-firmware`; the
file-system, image and encryption tools (`e2fsprogs`, `dosfstools`, `mtools`, `erofs-utils`,
`squashfs-tools`, `cpio`, `libisoburn` and `cryptsetup`); `crun`, the OCI runtime that
`containers-common` configures podman with; `nftables` and NetworkManager; `libinput`; and every
port of `src-art` and `src-system` apart from `kdos-kfs`. Their pages carry a hand-written note beside the recipe; see
[The three tiers](../00-front/reading-a-package-page.md#the-three-tiers).

Many more ports have [Tier B]{.idx} pages, for the reasons that page gives: a patch, a vendor bundle, a long
`build.sh`, or many dependants. In this phase the last reason picks out the libraries much of the
tree links, such as GLib, FreeType, Fontconfig, libpng, the audio codecs, `libxcb`, Boost and
NumPy. The rest of the phase is on cards.

## See also

- [The userland: `40_lang` to `44_apps`](../../kdos/05-developer/how-kdos-is-built.md#the-userland-40_lang-to-44_apps): the five phases in the whole journey
- [Every phase installs exactly its list](../../kdos/05-developer/build-system.md#every-phase-installs-exactly-its-list): the closure check and re-lists
- [The ports catalogue](../../kdos/06-reference/ports-catalogue.md#the-shelves): every shelf and its ports
- [Reading a package page](../00-front/reading-a-package-page.md): how the pages below are laid out
- [Part X](../10-graphics/intro.md): the next phase

<!-- book-nav -->
---

*Part IX. System.* Previous: [Part VIII. Languages](../08-lang/intro.md) · [Contents](../README.md) · Next: [Part X. The graphics stack](../10-graphics/intro.md)
