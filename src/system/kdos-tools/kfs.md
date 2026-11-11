## About
`kdos-tools` is one binary with many names. Installed once as `/usr/sbin/ksvc`, it is the service
supervisor (`ksvc`, and `service` for the conventional name), the `kdos` command and its
subcommands, the console getty wrapper `kdos-getty`, the A/B slot tool `kdos-bootctl`, and the
small tools `kdos-banner`, `kdos-shot`, `kdos-sfx`, `kdos-mpctl`, `kdos-share`, `kdos-fetch-app`
and `kdos-fetch-static`. `main.c` picks the tool from the name it was started under, and every
other name is a symbolic link `build.sh` makes; run as `kdos-tools`, the first argument names the
tool. The names, where each is installed and where each is documented are in
[`ksvc` — 12 names](../../../docs/kdos/04-programs/README.md#ksvc--12-names)
and [The kdos command](../../../docs/kdos/04-programs/kdos-command.md).

The package also carries data the `kdos` command reads: the reasons corpus that `kdos why` and
`kdos explain` browse, under `/usr/share/kdos/reasons`; `secdb.txt`, the vulnerability database
`kdos cve` matches installed packages against, and `bundled.txt`, its list of libraries ports
compile in from vendored crates; the *KDOS Help* menu entry, which runs `kdos help --pager` in a
terminal; `/usr/share/kglobalaccel/org.kdos.tiling.desktop`, which binds Meta+Shift+T to
`kdos tiling toggle`, and `org.kdos.nightlight.desktop` beside it, which binds Meta+Ctrl+Shift+N
to `kdos toggle night-light`; and `/etc/xdg/autostart/kdos-mpris.desktop`, which starts
`kdos-mpctl mpris` at a Plasma login where mpd is installed, so Plasma's media keys, Media Player
applet and lock screen reach mpd over MPRIS; and `/usr/share/kio/servicemenus/kdos-share.desktop`,
Dolphin's **Send with a Code Word** action, which runs `kdos-share` on the selection, and `kdos-verify.desktop`
beside it, the **Verify** action on every file, which runs `kdos verify --notify` on the selection
and links nothing new: SHA-512 is Monocypher's, already linked through `libksig` for signatures.
`/usr/sbin/service` and `/usr/local/sbin/kdos-getty`
come from this package, not from `fs/`.

The `depends` line is mostly programs it runs rather than libraries it links: `setfont` and
`loadkeys` from [kbd](#port-kbd) for `kdos-getty`, `ps` and `pkill` from
[procps-ng](#port-procps-ng), `aplay` from [alsa-utils](#port-alsa-utils) for `kdos-sfx`, and
[croc](#port-croc) and [qrencode](#port-qrencode) for `kdos-share`, and [curl](#port-curl) and
[zstd](#port-zstd) for `kdos update` and `kdos image`, which download over HTTPS and read and
write zstd streams through the two programs rather than linking either. [basu](#port-basu) is the
session and system bus every Plasma-facing verb talks over; it is opened at run time and not
linked (see Notes). kbd depends on [libxkbcommon](#port-libxkbcommon), a `42_graphics` port, so
this one is built in that phase and not with the other `src/system` programs in `41_system`. The
Plasma programs some verbs run — `kwriteconfig6`, `plasma-apply-lookandfeel`, `spectacle`,
`kdialog`, `kscreen-doctor` — are built in later phases and are not in `depends`: each verb that
needs one finds it on `PATH` when it runs and says so when it is missing.

## Flags
- `"$PORT_SRC"/*.c` — Every C file in the port directory is part of the binary, so a new tool's
  source is compiled without a change to `build.sh`. Its name still has to be added to the table
  in `main.c` and given a link in `build.sh`.
- `-I"$LIBS/libkbase"`, `-I"$LIBS/libkcolor"`, `-I"$LIBS/libkpkg"`, `-I"$LIBS/libkxdg"`,
  `-I"$LIBS/libkproc"`, `-I"$LIBS/libksig"`, `-I"$LIBS/libkpack"`, `-I"$LIBS/libkbuild"`,
  `-I"$PORT_SRC"` — The headers of the libraries it is built from, and of the program itself.
  `$LIBS` is `src/libs`.
- `"$LIBS"/libkbase/*.c`, `"$LIBS"/libkcolor/*.c`, `"$LIBS"/libkpkg/*.c`, `"$LIBS"/libkxdg/*.c`,
  `"$LIBS"/libkproc/*.c`, `"$LIBS"/libksig/*.c`, `"$LIBS"/libkpack/*.c` — Compile those libraries
  into the binary, which is how every `libk*` library is used; see
  [How the libraries are built](../../../docs/kdos/05-developer/c-libraries.md#how-the-libraries-are-built).
- `"$LIBS"/libkbuild/kb_json.c` — libkbuild's JSON reader alone, for `kdos privacy`, which reads
  the document `pw-dump` writes; the rest of libkbuild is the build system's and is not compiled
  in.
- `"$LIBS"/libksig/monocypher/*.c` — Monocypher, the Ed25519 code vendored under libksig, which
  verifies signatures; it is the `BSD-2-Clause OR CC0-1.0` in the licence line.
- `-std=gnu11`, `-D_GNU_SOURCE` — The C dialect, which the tree's `CFLAGS` already names, and the
  macro that exposes the C library's GNU and POSIX extensions.
- `-O2`, `-Wall`, `-Wextra` — The tree's optimisation level, named again, and the compiler's
  common and extra warnings.

## Notes
The table in `main.c` and the links `build.sh` installs have to agree. A name in the table with no
link is a tool nothing can start; a link with no name in the table makes the binary print the
names it knows and exit with status 1.

The bus is libbasu, opened with `dlopen` when a verb first needs it (`bus.c`), and not linked.
`kdos-bootctl` is in the initramfs, and the image step copies it through its link, so what lands
there is this whole binary with its linked closure; a linked libbasu would be one more library the
initramfs has to carry for a command that never uses a bus there. A development host has the same
API as libsystemd, which `bus.c` opens when there is no libbasu, so the self-test drives the
Plasma-facing verbs against a private bus on any host. See
[`kdos-bootctl`](../../../docs/kdos/03-architecture/boot-and-init.md#kdos-bootctl).

`kdos hey` and `kdos appid` read KWin's window list over the Wayland wire protocol, spoken in
`windows.c` itself — a registry, one bind and a request per window — rather than through a client
generated from the protocol file: that file comes from
[plasma-wayland-protocols](#port-plasma-wayland-protocols), a `43_toolkits` port built a phase
after this one, and the generated client would put libwayland-client under the binary the
initramfs carries. The opcodes are those of `plasma-window-management.xml` at interface version
21, and no version above that is bound, so an event KWin adds later is never sent.

`kdos image` (`image.c`) both makes a release's whole-slot image and writes one into the inactive
slot, so the format has one reader and one writer. An image is a signed text manifest of every
directory, file, link and hard link with its mode, owner, time and SHA-256, and zstd bundles of
file contents: one full bundle cut into parts under the 2 GiB a release asset may be, and one delta
per earlier release holding the contents that release lacks, each whole or as a
`zstd --patch-from` difference against the file the same path held in it, and every file under
`/etc` and `/var` whether it changed or not: a running machine has edited those, so its copy is
seldom the release's, and a file neither the running root nor the delta holds sends the machine to
the full image. The difference is taken
per file and not over the whole slot because `--patch-from` needs a window covering its reference
and the window stops at 2 GiB, which one slot with Plasma and the Gear applications exceeds
several times over. A bundle is never signed: every object is checked against the manifest's hash
before it is kept, so a tampered bundle costs a download and never a file. A slot is written by
synchronising it, not formatting it — what it or the running root already holds with the right hash
is copied — so it keeps the filesystem and UUID the installer gave it, whichever of the four that
is. The format refuses a file capability or an ACL rather than dropping it. Under `/var/log`,
`/var/cache` and `/var/tmp` an image holds the directories and none of the files: packages own
directories there (`/var/log/cups`), and a slot without them fails `kpkg check` and is never armed.

`secdb/secdb.txt` is generated by `secdb/vendor.py` from Alpine's security database, on a
development host with network, and committed; the build only installs it. `secdb/corrections.txt`
names the rows Alpine recorded under a version upstream never released, each with the right version
or `drop`; `vendor.py` applies it whenever it writes the table, and `vendor.py --corrections`
applies it to the committed table offline. A match in it is keyed
to Alpine's package names, so a port whose name differs from Alpine's carries `secdb =` in its
recipe. `bundled.txt` is kept by hand: a port whose version bump moves a vendored crate changes its
row in the same change.
