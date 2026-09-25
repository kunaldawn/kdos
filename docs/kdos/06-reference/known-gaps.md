# Known gaps

What KDOS does not do. This page exists so that you stop looking for something that is not there,
and so that a limitation is never discovered by assuming it is a bug.

Everything here is present tense. A gap that has been closed is not recorded; see
[Principles](../01-philosophy/principles.md#documentation-describes-the-present).

Where something is deliberately absent rather than merely missing, the reason is in
[Decisions](../01-philosophy/decisions.md). Where it is intended, it is in [Roadmap](roadmap.md).

## Desktop

Drag and drop carries text and files, and nothing else. `text/plain` and `text/uri-list` are offered
and accepted; there is no MIME negotiation, no deferred transfer and no image payload. Only the
trash accepts a drop on the desktop, which is a narrowing rather than a gap — see
[Decisions](../01-philosophy/decisions.md#narrowings). Both directions are implemented, and what is
asserted is the four verbs and their order over a socketpair: nothing has been dragged with a
pointer between a KDOS surface and a boxed application. See [Status](status.md).

There is no fractional scaling. The toolkit adopts an output's integer scale and renders glyphs at
that scale, so a high-density display gets a sharp grid rather than a stretched one. Fractional
scale is not negotiated.

Emoji are a colour bitmap face, and `tty1` cannot draw one. Noto Color Emoji ships and fontconfig
resolves the generic `emoji` family to it, so every surface with a pixel layer draws them — a
`kdos-term` window, the panel, a boxed application. The Linux VT's font is 512 monochrome glyphs, so
an emoji there is a blank cell like any other codepoint the font does not carry.

A tray menu does not update while it is open. The tree is read when the menu opens and
`LayoutUpdated` is not watched, which is a narrowing rather than a gap: a menu whose rows move under
the hand activates the wrong row. `AboutToShow` is sent first and its answer is the one update
taken. The rows, their submenus, their toggles, their icons and their chords are all drawn — see
[kdos-shell](../04-programs/kdos-shell.md). An item that publishes no menu path is sent
`ContextMenu` and opens whatever window it has of its own.

Workspace occupancy is derived rather than reported. The protocol has active, urgent and hidden but
no "there are windows here", so the panel marks a workspace occupied when a window on it is not
minimised. That is right for every workspace you have visited, and silent about the rest.

A corporate VPN is a terminal program here. `openconnect` ships and speaks AnyConnect, GlobalProtect,
Fortinet and Pulse, with `vpnc-script` giving the tunnel its routes, but there is no NetworkManager
plugin for it, so it does not appear in the network surface beside Wi-Fi and OpenVPN. Upstream's
plugin checks `webkit2gtk` unconditionally, and the dialog it builds with it is what the VPN service
delegates authentication to: on a host with no GTK the plugin cannot be built, and one built with
the dialog disabled would be a connection type the surface offers and cannot authenticate. WireGuard
and OpenVPN are the two that do appear.

A fingerprint answers `sudo` and nothing else. `pam_fprintd` is in `sudo`'s PAM stack ahead of the
password, but the lock screen checks the shadow file through `kdos-checkpass` and the console login
is shadow's, built without PAM, so neither asks for a finger. Enrolment is a terminal command run as
root — `sudo fprintd-enroll <user>` — because fprintd's polkit actions allow only an active session,
which no subject here ever is. No surface offers enrolment. For an account with a finger on file,
`sudo` over SSH waits on the machine's reader before it asks for the password, because the one remote test the module has without logind is
`PAM_RHOST`, which `sudo` never sets.

An X11 application gets no input method. Xwayland gives its X clients no text-input, and fcitx5 is
built without the XIM frontend that X clients use, so CJK input works in Wayland clients only —
host or boxed. See [the session](../03-architecture/session.md#input-methods).

There is no input-method configuration tool. The one upstream ships is built on a toolkit this host
does not have. Configuration is text files.

Nothing reads the desktop. `kdos-comp` draws pixels, so a screen reader would need the tree of
accessible objects this project does not build — and no such tree exists for a KDOS surface, which
composes its own cells and publishes them to nobody. What a surface does keep is `libktui`'s
announcement record: ten roles of control state their role there each frame, with their position in
the set and — where the widget holds them rather than the caller's own draw callback — a name and a
value, a menu included. Nothing carries that record out of the process — there is no socket,
no bus name and no bridge — and the only thing that reads it is the library's self-test, so the gap
is a route and a client rather than the material. What exists for applications is a boxed one's own
registry, opted into with `~/.config/kdos/a11y`. There is no braille route and no voice for the
desktop itself.

There is no greeter. `tty1` is handed to `agetty` by `kdos-login`, which autologins the account
`login.conf` names and otherwise asks for a password at the ordinary prompt. There is no account
chooser, no session chooser and no surface of any kind before the shell starts. Adding one means a
Wayland client that runs before the compositor does, which is a session-lock-shaped problem nobody
has taken on here.

Nothing starts the screensaver. `kdos-saver` is a program with eight effects and six committed
reference frames, and `kdos-comp`'s idle policy spawns `kdos-lock` and nothing else, so the saver is
reachable by running it and by no idle path. `comp.conf` has no `idle_saver` key to give it one.

The control centre has no pointer page. Speed, natural scroll, tap-to-click, tap-drag,
disable-while-typing, left-handed and middle emulation are all libinput settings `rc.xml` carries and
this desktop offers no surface for. The Input page says where they live and stops there.

`launcher.conf`'s `files` key is written by the control centre and read by nothing. `kdos-launcher`
searches applications only, and `kdos-palette` always includes files from three typed characters
upward, whatever the file says. Neither reads `~/.cache/kdos/plocate.db`; the palette shells out to
`fd` over `$HOME`, and the index `kdos-updatedb` builds nightly is reached only by the `locate`
command.

Nerd Font icons are blank on `tty1`, and the shipped configurations turn them off everywhere. They
are private-use codepoints and the VT font is 512 glyphs, which is a kernel limit: a glyph the font
does not carry renders as a blank cell, so an icon in front of a filename is a hole rather than a
picture. The face itself is on the image — `nerd-fonts-symbols` installs Symbols Nerd Font and Mono
under `/usr/share/fonts/nerd-fonts`, and its `66-nerd-font-symbols.conf` accepts the Mono face as a
fallback for `monospace` and `Terminus`, so anything with a pixel layer can draw one. What turns
them off is each program's own configuration: `yazi`'s generated theme empties all five `[icon]`
tables, `starship`'s format uses box drawing only, the `eza` aliases say `--icons=never` rather than
relying on a default, and `lazygit` 0.65 already ships `showIcons: false`. Turning them back on is
editing those four, one program at a time, with the understanding that the same shell on `tty1` will
show holes. There is no VT-font patching, and the console is where this cannot be fixed.

## Applications and boxes

An accent switch reaches a running GTK3 application and not a running libadwaita or Qt one. GTK
rebuilds its style cascade when `gtk-theme-name` moves, so the settings portal's change signal plus a
stylesheet directory named after the accent is enough for GTK3, which is most of the catalogue.
libadwaita ignores GTK themes entirely and reads `~/.config/gtk-4.0/gtk.css`, which GTK loads once at
startup; Qt under the KDE platform theme reads `~/.config/kdeglobals`, which KDE re-reads only on its
own global-settings signal, and there is no KDE daemon here to raise it. Both wear the new accent
when they are next started. libadwaita 1.6 follows the portal's `accent-color` on its own, which
moves the accent and not the rest of the palette.

There are no per-box protocol grants beyond the profile's list. The compositor's sandbox filter is a
fixed allowlist: a client is sandboxed or it is not. A profile can open named globals; teaching the
filter to consult a box's profile for anything finer is deliberate work that is not done.

A boxed application is given its position without being asked. The portal front end asks before
Location only for an application in a sandbox it recognises — Flatpak, Snap or Linyaps — and a KDOS
box is none of them, so it counts as an unsandboxed host program. GeoClue runs with no consent agent,
its whitelist empty, and answers any client on the system bus. For the same reason the Camera and Screenshot questions
are asked once for every box together, since the front end sees no application id on any of them,
and no surface shows or changes a stored answer: each portal's answers are a file under
`~/.local/share/flatpak/db/`, and removing it asks again. See
[The session](../03-architecture/session.md#access-and-what-it-unlocks).

A catalogue edit reaches a box only after the image is rebuilt. The base row carries `libva` and the
VA-API driver set, but an installed application is a built image: editing the row changes nothing a
running system can see until `kdos app install <id>` builds it again. A boxed browser built before
that row reports no hardware decoder and decodes every frame on the CPU. There is no notification
that a row moved — the catalogue has no version per row, so an image is current by definition until
somebody removes it.

X11 clients get no OpenGL. The X server is built without the GL extension, because the graphics stack
is built without X11 platform support. Enabling it means rebuilding the graphics stack and adding
several X libraries. Wayland-native applications are unaffected.

A live session cannot create a persistent box. The home directory is on the boot overlay, and the
kernel refuses to stack a container's writable layer on an overlay. A pack is mounted from the medium
and is gone when the session ends; `kdos doctor` reports this as a property of the session rather
than as a failure.

The *Containers* entries have not been opened on a built image. `kdos-podman-api` starts
`podman system service` for `podman-tui` and `lazydocker`; what is measured is its start, lock and
idle-restart logic against a stand-in engine, not either tool connecting to a real service. A pod
and `--init` rest on `catatonit`, which is measured to build static and run a command, not yet to
start a pod.

A box is not a security boundary against you. It shares your home directory in full, and it
constrains what an application can do to the *desktop*, not to your data. See
[The security model](../03-architecture/security-model.md#what-is-not-protected).

No Rust program's yank has been photographed reaching `kdos-clip`. `iamb` and `atuin` both offer a
clipboard through `arboard`, and both ask it for `wayland-data-control` on Linux — which is
wl-clipboard-rs speaking `zwlr_data_control_v1`, in pure Rust, so the binaries stay static-pie.
`kdos-comp` creates both data-control managers, so the path exists at both ends. Measured on both
built binaries: 214 `zwlr_data_control` symbols each, and no `NEEDED` entry at all. What is measured
is the path in the binary and the manager in the compositor, not the two meeting.

A video call has never been placed. `baresip` is the SIP phone here and its interface is a terminal
menu; the far end's picture goes in an `sdl.so` window, and sending your own means turning on
`v4l2.so`, which the generated config leaves commented because which camera to send is a choice.
The rig has no second endpoint and no camera, so what is measured is that the modules load.
GStreamer's `webrtcbin` is in the same state: it is built, with `libnice`'s `nicesrc` and
`nicesink` and `libsrtp`, and no pipeline has negotiated with a peer.

`mbsync` reaches XOAUTH2 and not OAUTHBEARER. `cyrus-sasl` is the mechanism loader and ships no
XOAUTH2 of its own, so the mechanism comes from `cyrus-sasl-xoauth2` beside it, and that plugin
implements the one. A server offering only OAUTHBEARER cannot be mirrored into a local Maildir, and
with it goes `notmuch`'s index and offline search for that account. `aerc` reads it and `msmtp` sends
it: measured on the image, `msmtp --version` reports `Authentication library: built-in` and lists
`oauthbearer` and `xoauth2`, and `aerc` carries `imaps+oauthbearer`, `smtps+oauthbearer` and its own
`xoauth2Client`.

A browse list is what answered a broadcast, not a directory. Samba here is built without winbind and
without a domain controller, so there is no browse master to ask: `kdos-mount browse` is an mDNS
query and a NetBIOS one, and a machine asleep, on another subnet, or behind a router that does not
forward broadcasts is absent from it. It can still be reached by name.

Kerberos is built and has never been given a ticket. `kinit`, `cifs.upcall` and the `request-key`
rule that joins them are on the image, and `kdos-mount krb5` mounts with `sec=krb5` — but there is no
KDC here and no realm to join, since the server half of krb5 is not installed. What is measured is
the request this daemon makes, and not that a domain controller accepts it. No share has ever been
mounted with a ticket on this image. `cifs.idmap` is not built either: it needs winbind's client
library, and this desktop does not need it, because every share is mounted with an explicit `uid=`
and `gid=`.

Neither synchroniser in the mail and calendar lanes has ever run against a server. The rig has no
account, no network and no IMAP or CalDAV server on the image, so what is measured of `mbsync` and
`vdirsyncer` is that each runs, reports its version, does nothing and exits cleanly with nothing
configured. That a password account synchronises is unproven here and can only be proven against a
real account. The XOAUTH2 lane is unproven in the same way and one step further back: that
`libxoauth2.so` is in `/usr/lib/sasl2` and that `mbsync` links `libsasl2` can be measured on the
image; that a provider accepts the token `pizauth` mints cannot.

`gpg` asks for a passphrase only in a terminal. The pinentries built are `curses` and `tty`, because
every graphical one is GTK, Qt, EFL or FLTK; each interactive `bash` exports `GPG_TTY`, so anything
run from a terminal prompts there. A program started from a launcher has no terminal, and a
signature or decryption that needs a passphrase fails, with no prompt anywhere, unless `gpg-agent`
already holds it.

`rga` does not search inside `.docx`, `.odt`, `.epub`, `.fb2`, `.ipynb` or `.html`. Its adapter for
those runs `pandoc`, which is Haskell and needs a GHC bootstrap this tree does not carry, so each
such file is reported as *Could not find executable "pandoc"* and yields no match — including plain
HTML, which the adapter claims ahead of ripgrep. `rga --rga-adapters=-pandoc` drops the adapter, and
HTML is then searched as text. PDFs, media, archives and compressed files are unaffected.

## Hardware and platform

x86-64 only. There is no other build target.

Secure Boot is not supported. Limine's EFI binary is unsigned and KDOS enrols no keys, so a machine
with Secure Boot enabled refuses to load it. The installer reports the firmware state on its first
page; turning Secure Boot off in firmware setup is the only route.

32-bit UEFI is built and has never been booted. `BOOTIA32.EFI` is on the ISO's ESP tree, on an
installed machine's, and in the El Torito UEFI record, and the installer writes an NVRAM entry naming
it where `/sys/firmware/efi/fw_platform_size` says 32 — but the machine that needs it is an early
Atom tablet and nothing in this tree has one. What is verified is that the binary is built and
placed. BIOS and 64-bit UEFI are the two that have been booted.

Broad hardware enablement is not a goal. The firmware tree ships whole and unpruned, which covers a
great deal, but nothing here is tested against a wide device matrix.

`btop`'s GPU box is empty on Intel graphics unless btop runs as root. It reads the i915 PMU with
`perf_event_open`, a system-wide event that the kernel's default `perf_event_paranoid` of 2 refuses
to a process without `CAP_PERFMON`, and nothing grants that capability to the binary or lowers the
setting. `sudo btop` shows the panel.

LVM volume groups are activated only at boot: by the initramfs on a disk boot, then by `03_lvm`. A
disk carrying LVM that is plugged in later shows no logical volumes until `sudo vgchange -aay`:
lvm2's own hotplug activation runs through `systemd-run`, so it is built off. A root on a logical
volume boots, but the installer cannot put one there: it neither creates LVM nor lists a logical
volume as a root, so that layout is set up by hand. A root on a thin or cache volume does not boot,
because the initramfs does not carry `thin_check` or `cache_check` — see
[Activating volume groups](../03-architecture/boot-and-init.md#activating-volume-groups).

`lsblk` shows a filesystem's type, label and UUID to root only. util-linux is built without libudev
— eudev needs util-linux's libblkid first — so lsblk can learn them only by probing the device,
which it does only as root. `sudo lsblk -f`, or `blkid` as root, answers.

The power button and the sleep key act only inside a graphical session. They arrive as keys, which
the compositor's `rc.xml` binds; there is no acpid, and nothing below the session listens for them,
so at a text login or on the console desktop the power button does nothing. Neither binding has
been pressed on hardware.

A USB modem that first presents itself as a storage device is not switched into a modem.
`usb_modeswitch` is not a port, so such a stick shows up as a small read-only disk and ModemManager
never sees it; a built-in WWAN card, and a stick that enumerates as a modem, connect through
NetworkManager. Mobile broadband, PPPoE and Bluetooth tethering have been built and not connected on
hardware.

A UEFI capsule update has not been applied on this tree. What is verified is that `fwupd-efi`
builds its loader with the NX flag and a KDOS SBAT line, and that the ESP probe the `fwupd` recipe
patches in reads the right partition number, offset, UUID and type from a mounted ESP. No capsule
has been staged and booted: the rig's firmware publishes no ESRT, so it has no device to update.

A phone is mounted by hand. `kdos-mountd` offers block devices only, so an MTP phone never appears
in its list; `aft-mtp-mount ~/Phone` is the way in, and no phone has been mounted on this tree.
Likewise built and never exercised against the hardware: `opensc` with a card in a reader,
`libwacom`'s pairing with a tablet, and the SoapySDR modules with a radio. SDRplay receivers have a
udev grant and no driver, because SDRplay's API is a closed library.

Intel Quick Sync through oneVPL has no runtime. `libvpl` and `vpl-gpu-rt` are not ports, so
ffmpeg's and GStreamer's `qsv` paths find nothing; VA-API reaches the same decode and encode
blocks through `intel-media-driver` and `libva-intel-driver`.

An encrypted disc does not play. A CSS-encrypted DVD needs `libdvdcss` and an AACS-encrypted
Blu-ray needs `libaacs`, and neither is a port, so `mpv`, `ffmpeg` and GStreamer open only an
unencrypted disc or a backup. A `libdvdcss.so.2` installed by hand is loaded, because `libdvdread`
looks for it at run time. A Blu-ray's BD-J menus do not run either: they are Java, and `libbluray`
is built without its jar because the host has no JDK and no JVM, so a disc plays its titles without
them. An audio CD lists as numbered tracks, because the CDDB lookup `cmus` and `libcdio` can make
needs `libcddb`, which is not a port. None of the optical paths has read a disc on this tree: the
rig has no drive, and what is verified is that each library builds and each consumer links it.

Much of `kdos doctor` cannot answer in a virtual machine, which is why it has a *skip with a reason*
level rather than reporting those as passing.

No speech model ships, and the transcribed text has never been read back on this tree. What is
verified is the model gate, the `whisper-cli` argv, the spawn and the exit status. A model is fetched
rather than packaged — `kdos speech get` names one out of a table, checks its sha256 and writes it to
`~/.local/share/whisper.cpp/models`, and `kdos-rec`'s third button is *Get model* until one is there
— so what is missing is a machine with a model on it and somebody reading the result, not a way in.

Live transcription is a terminal program and not a desktop verb. `whisper-stream` is built and
transcribes a microphone, and nothing starts it. `kdos-rec`'s *Transcribe* is `whisper-cli` over the
file it has just recorded. Both want the same model, and no model ships.

The emulated HDA codec gives the guest no capture signal, so the rig cannot photograph a deflecting
meter from `--audio` alone. The recording evidence comes from `snd-aloop`, loaded by hand in a root
script.

## Security

The full statement is
[What is not protected](../03-architecture/security-model.md#what-is-not-protected). In brief: no
mandatory access control, no verified boot, no measured boot; disk encryption protects data at rest
only; `wheel` is effectively root; a registry base fetches unsigned content; an unsigned pack mounts
while a failed signature does not; and there is no automatic update path.

## Build and packaging

A LUKS key is typed and not sealed. `tpm2-tss` and the `tpm2_*` tools ship, so a key *can* be sealed
to the chip by hand, but nothing in the boot path unseals one: `unlock_root` reads a passphrase from
`tty1` and has no second road. Wiring one means a sealed blob on the ESP, a PCR policy, and an answer
for what happens when a firmware update changes the measurements — none of which is decided.

Nothing creates or first populates the second root slot. The installer lays down slot A only and
writes a state with `slot_b` empty; a second slot's filesystem has to be made, filled with a copy of
the system and recorded with `kdos-bootctl set-slot b` by hand. From there `kdos update apply`
installs into the mounted inactive slot, deploys its kernel and tries it, the initramfs selects a
slot, unlocks that slot's container and counts attempts, and a boot that reaches the end of
initialisation confirms the slot. A second *encrypted* slot has never been booted. What is measured
is on the host: `select` rolling from B back to A hands back A's container and not B's, which is the
failure the mechanism exists to prevent.

Per-slot kernels have not been booted. `deploy`, the regenerated menu, the hand-picked entry and the
rollback onto the confirmed slot's kernel are asserted on the host against a fixture ESP, and the
`linux` postinstall's appended initramfs was assembled and inspected, not started. A candidate
kernel that dies before its initramfs runs spends no attempt, because Limine counts nothing: the
menu keeps leading with it until the confirmed slot's `/KDOS (slot <x>)` entry is picked by hand —
see [One kernel per slot](../03-architecture/boot-and-init.md#one-kernel-per-slot). No update
replaces the init inside an installed initramfs, so a machine installed from an image whose init
does not read `kdos_slot=` rolls back onto the candidate's kernel until `mark-good` corrects the
menu. A root installed from an image without `/boot/initramfs.modules` builds its new kernel's
initramfs from the module set its image archive carries; that path is exercised on fixture
archives only.

Go has no race detector and no BoringCrypto. Both are objects upstream compiles and ships inside
the source tarball, and the `go` port does not install them, so `go build -race`, `go test -race`
and `GOEXPERIMENT=boringcrypto` fail to link. Every other Go build is unaffected.

nmap has no `jdwp-exec` and no `jdwp-info` script. Both inject Java classes that upstream compiles
and ships inside the source tarball, and the `nmap` port installs neither the classes nor the two
scripts, so `--script jdwp-info` names nothing and the `default` category runs without it.
`jdwp-inject`, which injects a class the user supplies, and `jdwp-version` are unaffected.

qemu carries firmware only for what its three targets boot through their defaults and their firmware
descriptors: the x86_64 PC machines, aarch64 and riscv64 `virt`, and microvm. The Nuvoton and ASPEED
BMC boards inside `qemu-system-aarch64` stop at startup with *Could not find ROM image* unless
`-bios` names one, and there is no 32-bit Arm UEFI, no 32-bit x86 OVMF and no OVMF build for microvm.
The riscv64 `virt` UEFI image is upstream's rather than compiled here — see
[what is not built from source](../01-philosophy/why-kdos.md#what-is-not-built-from-source).

A qemu guest cannot join a host bridge as an ordinary user. `-netdev bridge` runs
`qemu-bridge-helper`, which is installed without its setuid bit and with no `/etc/qemu/bridge.conf`,
so it serves only root, and only once root writes an `allow <bridge>` line there. Making it setuid
would add a root program to the [setuid list](../03-architecture/security-model.md), and that is not
decided. User-mode networking through `passt` needs no privilege.

PROJ has no transformation grids, and there is no port that carries them. A transformation that
needs a grid — NAD27 to NAD83, OSGB36, a national geoid — falls back to a ballpark one or fails
naming the grid it wanted. proj is built with `ENABLE_CURL=OFF`, so the grids are never fetched on
demand either, and the transformations that need no grid are the ones the bundled `proj.db`
answers exactly.

presenterm's seven syntect stock themes (`base16-ocean.dark`, `InspiredGitHub`, the Solarized pair
and the rest) are the serialized `default.themedump` inside the vendored `syntect` crate, and are
not compiled here. Their `.tmTheme` sources are in syntect's repository and not in the crate, and
the only way to swap in a rebuilt dump is a patch to presenterm's theme loading. Its grammars and
bat's themes are compiled here by the `bat` port.

A `tzdata` upgrade on a machine whose installed `tzdata` manifest lists `/etc/localtime` resets the
timezone to UTC, once. The new version does not own the link, so the upgrade removes it as an orphan,
`rcS` links UTC on the next boot, and `TZ=':/etc/localtime'` follows it. kpkg has no rule that keeps
a file a package gave up, so `kdos-power timezone <Area/City>` sets the zone again. Every later
upgrade leaves it alone.

Upgrading toybox by itself deletes tools on a machine whose installed toybox manifest lists names
another port owns — `mount`, `umount`, `losetup`, `kill`, `dmesg`, `readelf`, `strings`, `cmp`,
`gunzip`, `insmod`, `lsattr` and the rest. That happens when toybox was installed after util-linux,
binutils, diffutils, gzip, bzip2, attr, kmod, ncurses, e2fsprogs or procps-ng. A same-path install
takes the file into the later manifest, so the owning port's manifest does not list it, and toybox's upgrade
removes it as an orphan. On such a machine toybox is upgraded together with those ports, whose
release bumps reinstall the names. A machine is one when `/var/lib/kpkg/db/toybox`, the installed manifest, lists any of those names.

There is no public binary host. The mechanism is complete — a signed index, three equality tests,
deltas — but it is one you run yourself.

No port declares `vendoring = node`, though `ports/fetch` implements it beside the three that are
used: 58 recipes declare `rust`, 32 `go` and 24 `python`. The npm path has never been run, so what
stands behind it is the code and not a tarball it produced.

## Testing

The compositor and the shell are not compiled by the self-test on a bare host, because their Wayland
dependencies are not there. Those blocks report as skipped on most machines.

Nothing tests the build. A package manager can only really be tested by building the distribution
with it, which takes hours and a container.

The phosphor shader is not in any rig photograph. The rig's virtual display puts the compositor on
software rendering, where the pass declines. What is photographed is the cell grid underneath it.

## Documentation

Some measurements in this book are quoted rather than re-derived. Contrast ratios, launch timings,
freeze ratios and transport throughput figures were measured once and are repeated here. Where a
number is a measurement, the conditions are stated; where it has not been re-taken, that is the
honest caveat.

## See also

- [Decisions](../01-philosophy/decisions.md) — what is absent on purpose, and why
- [Roadmap](roadmap.md) — what is intended
- [Status](status.md) — maturity per subsystem, and the evidence behind each verdict
- [The security model](../03-architecture/security-model.md) — the full statement of what is not protected
