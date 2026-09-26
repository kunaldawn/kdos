# Known gaps

This page lists what KDOS does not do. It is for anyone using, administering or contributing to
KDOS who has hit something that seems missing and wants to know whether it is a bug, a limitation,
or a choice. If what you are looking for is described here, it is not there, and you can stop
looking; if it is not described here and it misbehaves, it is probably a bug worth reporting.

How to read it:

- Each gap opens with a one-line statement in bold, followed by what exactly is missing, why, and
  what to do instead where there is something.
- The sections follow the shape of the system: the desktop, applications and boxes, hardware, boot
  and updates, security, building and packaging, testing, and this documentation.
- Everything is in the present tense. A gap that has been closed is removed, not marked as closed;
  see [Principles](../01-philosophy/principles.md#documentation-describes-the-present).
- Where something is absent on purpose, the reasoning is in
  [Decisions](../01-philosophy/decisions.md). Where it is intended, it is in [Roadmap](roadmap.md).
  How mature the parts that *do* exist are is in [Status](status.md).
- Terms such as *surface*, *box*, *pack* and *rig* are defined in the [Glossary](glossary.md).

## Desktop

**Drag and drop carries text and files, and nothing else.** `text/plain` and `text/uri-list` are
offered and accepted; there is no MIME negotiation, no deferred transfer and no image payload. Only
the trash accepts a drop on the desktop, which is a narrowing rather than a gap — see
[Decisions](../01-philosophy/decisions.md#narrowings). Both directions are implemented, and what is
asserted is the four protocol verbs and their order over a socketpair: nothing has been dragged with
a pointer between a KDOS surface and a boxed application. See [Status](status.md).

**There is no fractional scaling.** The toolkit adopts an output's integer scale and renders glyphs
at that scale, so a high-density display gets a sharp grid rather than a stretched one. Fractional
scale is not negotiated.

**Emoji draw everywhere except the text console on `tty1`.** Noto Color Emoji ships, and fontconfig
resolves the generic `emoji` family to it, so every surface with a pixel layer draws them — a
`kdos-term` window, the panel, a boxed application. The Linux virtual terminal's font holds 512
monochrome glyphs, so an emoji there is a blank cell, like any other character the font does not
carry.

**A tray menu does not update while it is open.** The menu tree is read when the menu opens, and
`LayoutUpdated` is not watched. This is a narrowing rather than a gap: a menu whose rows move under
your hand activates the wrong row. `AboutToShow` is sent first and its answer is the one update
taken. The rows, their submenus, toggles, icons and shortcuts are all drawn — see
[kdos-shell](../04-programs/kdos-shell.md). An item that publishes no menu path is sent
`ContextMenu` and opens whatever window it has of its own.

**Workspace occupancy is inferred, not reported.** The workspace protocol reports active, urgent and
hidden, but has no "there are windows here", so the panel marks a workspace occupied when a window on
it is not minimised. That is right for every workspace you have visited, and says nothing about the
rest.

**A corporate VPN is a terminal program.** `openconnect` ships and speaks AnyConnect, GlobalProtect,
Fortinet and Pulse, with `vpnc-script` giving the tunnel its routes, but there is no NetworkManager
plugin for it, so it does not appear in the network surface beside Wi-Fi and OpenVPN. Upstream's
plugin requires `webkit2gtk` unconditionally, and the dialog it builds with it is what the VPN
service hands authentication to. On a host with no GTK the plugin cannot be built, and one built with
the dialog disabled would be a connection type the surface offers and cannot authenticate. WireGuard
and OpenVPN are the two VPN types that do appear.

**A fingerprint unlocks `sudo` and nothing else.** `pam_fprintd` is in `sudo`'s PAM stack ahead of
the password. The lock screen checks the shadow file through `kdos-checkpass`, and the console login
is shadow's, built without PAM, so neither asks for a finger. Enrolment is a terminal command run as
root — `sudo fprintd-enroll <user>` — because fprintd's polkit actions allow only an active session,
which nothing here ever is. No surface offers enrolment. For an account with a fingerprint enrolled,
`sudo` over SSH waits on the machine's reader before asking for the password: the only remote-login
test the module has without logind is `PAM_RHOST`, which `sudo` never sets.

**X11 applications get no input method.** Xwayland gives its X clients no text-input protocol, and
fcitx5 is built without the XIM frontend that X clients use, so CJK input works in Wayland clients
only — host or boxed. See [the session](../03-architecture/session.md#input-methods).

**There is no input-method configuration tool.** The one upstream ships is built on a toolkit this
host does not have. fcitx5 is configured through its text files.

**Nothing reads the desktop aloud.** `kdos-comp` draws pixels, so a screen reader would need a tree
of accessible objects, and no such tree exists for a KDOS surface, which composes its own cells and
publishes them to nobody. What a surface does keep is `libktui`'s announcement record: ten roles of
control state their role there each frame, with their position in the set and — where the widget
holds them rather than the caller's own draw callback — a name and a value, menus included. Nothing
carries that record out of the process — there is no socket, no bus name and no bridge — and the only
thing that reads it is the library's self-test, so what is missing is a route and a client rather
than the material. What exists for applications is a boxed one's own accessibility registry, opted
into with `~/.config/kdos/a11y`. There is no braille route and no voice for the desktop itself. See
[Accessibility](../02-user-guide/accessibility.md).

**There is no graphical login screen (greeter).** `tty1` is handed to `agetty` by `kdos-login`,
which logs in the account `login.conf` names automatically and otherwise asks for a password at the
ordinary text prompt. There is no account chooser, no session chooser and no surface of any kind
before the shell starts. Adding one means a Wayland client that runs before the compositor does,
which is a session-lock-shaped problem nobody has taken on here.

**Nothing starts the screensaver.** `kdos-saver` is a program with eight effects and six committed
reference frames, but `kdos-comp`'s idle policy starts `kdos-lock` and nothing else, so you reach the
saver by running it yourself. `comp.conf` has no `idle_saver` key to give it an idle path.

**The control centre has no pointer page.** Speed, natural scroll, tap-to-click, tap-drag,
disable-while-typing, left-handed and middle-button emulation are all libinput settings that `rc.xml`
carries and that no surface here offers. The Input page says where they live — a commented
`<libinput>` template in `rc.xml` — and stops there.

**The launcher's `files` setting does nothing.** `launcher.conf`'s `files` key is written by the
control centre and read by nothing. `kdos-launcher` searches applications only, and `kdos-palette`
always includes files once you have typed three characters, whatever the file says. Neither reads
`~/.cache/kdos/plocate.db`: the palette runs `fd` over `$HOME`, and the index `kdos-updatedb` builds
nightly is used only by the `locate` command.

**Nerd Font icons are blank on `tty1`, and the shipped configurations turn them off everywhere.**
They are private-use characters and the console font holds 512 glyphs, which is a kernel limit: a
glyph the font does not carry renders as a blank cell, so an icon in front of a filename shows as a
hole. The face itself is on the image — `nerd-fonts-symbols` installs Symbols Nerd Font and its Mono
variant under `/usr/share/fonts/nerd-fonts`, and its `66-nerd-font-symbols.conf` accepts the Mono
face as a fallback for `monospace` and `Terminus`, so anything with a pixel layer can draw one. What
turns them off is each program's own configuration: `yazi`'s generated theme empties all five
`[icon]` tables, `starship`'s format uses box drawing only, the `eza` aliases say `--icons=never`
rather than relying on a default, and `lazygit` 0.65 ships `showIcons: false`. To turn them back on,
edit those four configurations one program at a time, knowing the same shell on `tty1` will show
holes. There is no console-font patching, and the console is where this cannot be fixed.

## Applications and boxes

**Changing the accent colour reaches running GTK3 applications, not running libadwaita or Qt
ones.** GTK rebuilds its style when `gtk-theme-name` changes, so the settings portal's change signal
plus a stylesheet directory named after the accent is enough for GTK3, which is most of the
catalogue. libadwaita ignores GTK themes entirely and reads `~/.config/gtk-4.0/gtk.css`, which GTK
loads once at startup. Qt under the KDE platform theme reads `~/.config/kdeglobals`, which KDE
re-reads only on its own global-settings signal, and there is no KDE daemon here to send it. Both
pick up the new accent the next time they start. libadwaita 1.6 follows the portal's
`accent-color` on its own, which changes the accent and not the rest of the palette.

**A box cannot be granted individual Wayland protocols beyond its profile's list.** The compositor's
sandbox filter is a fixed allowlist: a client is sandboxed or it is not. A profile can open named
globals; teaching the filter to consult a box's profile for anything finer is work that has not been
done.

**A boxed application is given your location without being asked.** The portal front end asks
before sharing location only with an application in a sandbox it recognises — Flatpak, Snap or
Linyaps — and a KDOS box is none of them, so it counts as an unsandboxed host program. GeoClue runs
with no consent agent and an empty whitelist, and answers any client on the system bus. For the same
reason, the Camera and Screenshot questions are asked once for all boxes together, since the front
end sees no application id on any of them. No surface shows or changes a stored answer: each portal's
answers are a file under `~/.local/share/flatpak/db/`, and deleting that file makes it ask again. See
[The session](../03-architecture/session.md#access-and-what-it-unlocks).

**A catalogue change reaches an installed application only when it is rebuilt.** An installed
application is a built image, so editing its catalogue row changes nothing a running system can see
until `kdos app install <id>` builds it again. For example, the base row carries `libva` and the
VA-API driver set; a boxed browser built before that row reports no hardware decoder and decodes
every frame on the CPU. Nothing tells you a row changed — the catalogue has no per-row version, so an
image counts as current until somebody removes it.

**X11 applications get no OpenGL.** The X server is built without the GL extension, because the
graphics stack is built without X11 platform support. Enabling it means rebuilding the graphics
stack and adding several X libraries. Wayland-native applications are unaffected.

**A live session cannot create a persistent box.** The home directory is on the boot overlay, and
the kernel refuses to stack a container's writable layer on an overlay. A pack is mounted from the
medium and is gone when the session ends; `kdos doctor` reports this as a property of the session
rather than as a failure.

**The *Containers* menu entries have not been opened on a built image.** `kdos-podman-api` starts
`podman system service` for `podman-tui` and `lazydocker`; what is measured is its start, lock and
idle-restart logic against a stand-in engine, not either tool connecting to a real service. Pods and
`--init` rely on `catatonit`, which is measured to build statically and run a command, not yet to
start a pod.

**A box is not a security boundary against you.** It shares your home directory in full, and it
limits what an application can do to the *desktop*, not to your data. See
[The security model](../03-architecture/security-model.md#what-is-not-protected).

**Copying from `iamb` or `atuin` has not been confirmed to reach the clipboard manager.** Text you
copy in either should appear in `kdos-clip`'s history, and nobody has yet watched it arrive. It is
expected to work: both programs copy through the Wayland data-control protocol, compiled into the
binary itself, and `kdos-comp` provides that protocol. If a copy from either goes missing, that is a
bug worth reporting.

**A video call has never been placed.** `baresip` is the SIP phone here, and its interface is a
terminal menu. The far end's picture goes in an `sdl.so` window; sending your own means enabling
`v4l2.so`, which the generated configuration leaves commented out because which camera to send is
your choice. The rig has no second endpoint and no camera, so what is measured is that the modules
load. GStreamer's `webrtcbin` is in the same state: it is built, with `libnice`'s `nicesrc` and
`nicesink` and `libsrtp`, and no pipeline has negotiated with a peer.

**`mbsync` supports XOAUTH2 but not OAUTHBEARER.** `cyrus-sasl` is the mechanism loader and ships no
XOAUTH2 of its own, so the mechanism comes from `cyrus-sasl-xoauth2` beside it, and that plugin
implements only the one. A mail server offering only OAUTHBEARER cannot be mirrored into a local
Maildir, and with it goes `notmuch`'s index and offline search for that account. `aerc` reads such an
account and `msmtp` sends through it: measured on the image, `msmtp --version` reports
`Authentication library: built-in` and lists `oauthbearer` and `xoauth2`, and `aerc` carries
`imaps+oauthbearer`, `smtps+oauthbearer` and its own `xoauth2Client`.

**The list of Windows shares on the network is whatever answered a broadcast.** Samba here is built
without winbind and without a domain controller, so there is no browse master to ask:
`kdos-mount browse` is an mDNS query plus a NetBIOS one, and a machine that is asleep, on another
subnet, or behind a router that does not forward broadcasts is missing from it. It can still be
reached by name.

**Kerberos is built and has never been given a ticket.** `kinit`, `cifs.upcall` and the
`request-key` rule that joins them are on the image, and `kdos-mount krb5` mounts with `sec=krb5` —
but there is no KDC here and no realm to join, since the server half of krb5 is not installed. What
is measured is the request the daemon makes, not that a domain controller accepts it; no share has
been mounted with a ticket on this image. `cifs.idmap` is not built either: it needs winbind's client
library, and this desktop does not need it, because every share is mounted with an explicit `uid=`
and `gid=`.

**Neither mail-and-calendar synchroniser has run against a server.** The rig has no account, no
network and no IMAP or CalDAV server, so what is measured of `mbsync` and `vdirsyncer` is that each
runs, reports its version, does nothing and exits cleanly with nothing configured. That a password
account synchronises is unproven here and can only be proven against a real account. The XOAUTH2
route is unproven in the same way and one step further back: that `libxoauth2.so` is in
`/usr/lib/sasl2` and that `mbsync` links `libsasl2` can be measured on the image; that a provider
accepts the token `pizauth` mints cannot.

**`gpg` asks for a passphrase only in a terminal.** The pinentries built are `curses` and `tty`,
because every graphical one is GTK, Qt, EFL or FLTK. Each interactive `bash` exports `GPG_TTY`, so
anything run from a terminal prompts there. A program started from a launcher has no terminal, and a
signature or decryption that needs a passphrase fails, with no prompt anywhere, unless `gpg-agent`
already holds it.

**`rga` does not search `.htm` files.** Its `pandoc` adapter, which reads `.docx`, `.odt`, `.epub`,
`.fb2`, `.ipynb` and `.html` into text, claims `.htm` too and passes the extension as the input
format, and pandoc has no reader named `htm`: each such file is reported as *Unknown input format
'htm'* and yields no match. `rga --rga-adapters=-pandoc` drops the adapter, and the file is then
searched as plain text. The other formats, `.html` included, are searched through pandoc.

## Hardware and platform

**x86-64 only.** There is no other build target. See [Roadmap](roadmap.md#aarch64-and-mobile).

**Secure Boot is not supported.** Limine's EFI binary is unsigned and KDOS enrols no keys, so a
machine with Secure Boot enabled refuses to load it. The installer reports the firmware state on its
first page; turning Secure Boot off in firmware setup is the only way.

**32-bit UEFI is built and has never been booted.** `BOOTIA32.EFI` is on the ISO's EFI system
partition tree, on an installed machine's, and in the El Torito UEFI record, and the installer writes
a firmware boot entry naming it where `/sys/firmware/efi/fw_platform_size` says 32 — but the machine
that needs it is an early Atom tablet, and nothing here has one. What is verified is that the binary
is built and placed. BIOS and 64-bit UEFI are the two that have been booted.

**Broad hardware support is not a goal.** The firmware tree ships whole and unpruned, which covers a
great deal, but nothing here is tested against a wide range of devices.

**`btop`'s GPU panel is empty on Intel graphics unless btop runs as root.** It reads the i915
performance counters with `perf_event_open`, a system-wide event that the kernel's default
`perf_event_paranoid` of 2 refuses to a process without `CAP_PERFMON`, and nothing grants that
capability to the binary or lowers the setting. `sudo btop` shows the panel.

**LVM volume groups are activated only at boot.** The initramfs activates them on a disk boot, then
the `03_lvm` init script does. A disk carrying LVM that is plugged in later shows no logical volumes
until you run `sudo vgchange -aay`: lvm2's own hotplug activation runs through `systemd-run`, so it
is built off and its udev rule is not installed.

**A root filesystem on LVM has not been booted.** The installer's erase plan creates the volume group
and slot A's volume, its reuse plan lists existing volumes, and the initramfs carries `thin_check`,
`cache_check` and the thin, cache and snapshot targets. The commands were run against a loop device
in a container and the installer's probe and dry run were read back there, but no install onto LVM —
plain, encrypted, thin or cached — has been booted. The installer lays out only one shape, a volume
group of one physical volume on the target disk, and makes no `root_b`: slot B's space is left free,
and its volume is made by hand like the rest of slot B — see
[Activating volume groups](../03-architecture/boot-and-init.md#activating-volume-groups).

**`lsblk` shows a filesystem's type, label and UUID to root only.** util-linux is built without
libudev — eudev needs util-linux's libblkid first — so lsblk can learn them only by probing the
device, which it does only as root. `sudo lsblk -f`, or `blkid` as root, answers.

**The power button and the sleep key work only inside a graphical session.** They arrive as keys,
which the compositor's `rc.xml` binds. There is no acpid, and nothing below the session listens for
them, so at a text login or on the console desktop the power button does nothing. Neither binding has
been pressed on real hardware.

**A USB modem that first appears as a storage device is not switched into a modem.**
`usb_modeswitch` is not a port, so such a stick shows up as a small read-only disk and ModemManager
never sees it. A built-in WWAN card, and a stick that appears as a modem straight away, connect
through NetworkManager. Mobile broadband, PPPoE and Bluetooth tethering are built and have not been
connected on hardware.

**A UEFI firmware (capsule) update has not been applied.** What is verified is that `fwupd-efi`
builds its loader with the NX flag and a KDOS SBAT line, and that the EFI-partition probe the `fwupd`
recipe patches in reads the right partition number, offset, UUID and type from a mounted EFI
partition. No capsule has been staged and booted: the rig's firmware publishes no ESRT, so it has no
device to update.

**A phone is mounted by hand.** `kdos-mountd` offers block devices only, so an MTP phone never
appears in its list. `aft-mtp-mount ~/Phone` is the way in, and no phone has been mounted on this
system. Likewise built and never tried against the hardware: `opensc` with a card in a reader,
`libwacom`'s pairing with a tablet, and the SoapySDR modules with a radio. SDRplay receivers have a
udev permission rule and no driver, because SDRplay's API is a closed library.

**Intel Quick Sync through oneVPL has no runtime.** `libvpl` and `vpl-gpu-rt` are not ports, so
ffmpeg's and GStreamer's `qsv` paths find nothing. VA-API reaches the same decode and encode hardware
through `intel-media-driver` and `libva-intel-driver`.

**Encrypted discs do not play.** A CSS-encrypted DVD needs `libdvdcss` and an AACS-encrypted Blu-ray
needs `libaacs`, and neither is a port, so `mpv`, `ffmpeg` and GStreamer open only an unencrypted
disc or a backup. A `libdvdcss.so.2` you install by hand is loaded, because `libdvdread` looks for it
at run time. A Blu-ray's BD-J menus do not run either: they are Java, and `libbluray` is built without
its jar because the host has no JDK and no JVM, so a disc plays its titles without them. An audio CD
lists as numbered tracks, because the CDDB lookup `cmus` and `libcdio` can make needs `libcddb`,
which is not a port. None of the optical paths has read a disc here: the rig has no drive, and what is
verified is that each library builds and each consumer links it.

**Much of `kdos doctor` cannot answer in a virtual machine.** That is why it has a *skip with a
reason* level rather than reporting those checks as passing.

**No speech-recognition model ships, and a transcription has never been read back.** What is
verified is the model check, the `whisper-cli` command line, starting it and its exit status. A model
is downloaded rather than packaged: `kdos speech get` picks one from a table, checks its sha256 and
writes it to `~/.local/share/whisper.cpp/models`, and `kdos-rec`'s third button reads *Get model*
until one is there. What is missing is a machine with a model on it and somebody reading the result,
not a way to get one.

**Live transcription is a terminal program, not a desktop action.** `whisper-stream` is built and
transcribes a microphone, and nothing on the desktop starts it. `kdos-rec`'s *Transcribe* runs
`whisper-cli` over the file it has just recorded. Both need the same model, and none ships.

## Boot and updates

**A disk-encryption key is typed, not sealed in the TPM.** `tpm2-tss` and the `tpm2_*` tools ship,
so a key *can* be sealed to the chip by hand, but nothing in the boot path unseals one: `unlock_root`
reads a passphrase from `tty1` and has no other route. Adding one means a sealed blob on the EFI
partition, a PCR policy, and an answer for what happens when a firmware update changes the
measurements — none of which is decided.

**Nothing creates or first fills the second root slot.** The installer lays down slot A only and
writes a state with `slot_b` empty. A second slot's filesystem has to be made, filled with a copy of
the system and recorded with `kdos-bootctl set-slot b <uuid> [<luks-uuid>]`, all by hand; the steps
are in [Installation](../02-user-guide/installation.md#ab-root-slots). From there `kdos update apply`
installs into the mounted inactive slot, deploys its kernel and marks it to try;
the initramfs selects a slot, unlocks that slot's own encrypted container and counts attempts; and a
boot that reaches the end of initialisation confirms the slot. A second *encrypted* slot has never
been booted. What is measured is on the build host: rolling back from slot B to A hands back A's
container and not B's, which is the failure the mechanism exists to prevent.

**Per-slot kernels have not been booted.** `deploy`, the regenerated boot menu, the hand-picked entry
and the rollback onto the confirmed slot's kernel are asserted on the host against a fixture EFI
partition, and the `linux` postinstall's appended initramfs was assembled and inspected, not
started. The UEFI `BootNext` trial has not met a real firmware: the load option is compared byte for
byte with the specification's layout, and the trial menu, rollback and clean-up are asserted against
a fixture EFI partition, a fixture GPT image and a directory standing in for `efivarfs`. A firmware
that ignores `BootNext`, or deletes a `Boot####` entry that is not in `BootOrder`, rolls every
candidate back unbooted rather than looping. On BIOS, a candidate kernel that dies before its
initramfs runs spends no attempt, because Limine counts nothing: the menu keeps leading with it until
you pick the confirmed slot's `/KDOS (slot <x>)` entry by hand — see
[One boot through BootNext](../03-architecture/boot-and-init.md#one-boot-through-bootnext).

**A slot keeps the init it was installed with until that slot is updated.** This affects a machine
installed from an image whose initramfs `init` does not read `kdos_slot=`, the word on each boot
entry naming the slot its kernel came from (see
[Command-line parameters KDOS reads](../03-architecture/boot-and-init.md#command-line-parameters-kdos-reads)).
An update appends modules to an initramfs and never replaces its `init`, and the confirmed slot's
`init` is the one that carries out a rollback. On such a machine, while an update is on trial:

- in a trial led by the boot menu, a rollback returns to the confirmed slot's root but still on the
  candidate's kernel, until `kdos-bootctl mark-good` corrects the menu at the end of that boot;
- after a UEFI `BootNext` trial's one boot, the next boot starts the candidate's root on the
  confirmed slot's kernel once, and `mark-good` then sees the confirmed slot's `kdos_slot=` and rolls
  the candidate back.

Both stop once the confirmed slot is itself updated, which gives it a current `init`. See
[One kernel per slot](../03-architecture/boot-and-init.md#one-kernel-per-slot).

A root installed from an image without `/boot/initramfs.modules` builds a new kernel's initramfs
from the module set its own initramfs carries, rather than from a list; that path has been exercised
on test archives only. See [A new kernel](../03-architecture/boot-and-init.md#a-new-kernel).

**There is no single-user mode.** The installed boot menu has a `KDOS (single user)` entry, which
passes `single` on the kernel command line, but toybox init ignores its arguments and no script
reads the word, so that entry boots like `KDOS (verbose)`: every service and the desktop start. For
a shell without the desktop, log in on `tty2`; for a machine that cannot finish booting, see
[When a boot stops](../03-architecture/boot-and-init.md#when-a-boot-stops).

## Security

The full statement is
[What is not protected](../03-architecture/security-model.md#what-is-not-protected). In brief:

- no mandatory access control, no verified boot and no measured boot;
- disk encryption protects data at rest only;
- membership of `wheel` is effectively root;
- a registry base image fetches unsigned content;
- an unsigned pack mounts, while a pack whose signature fails does not;
- nothing installs updates automatically: a daily timer runs `kdos update check`, and
  `kdos update apply` runs only when you run it.

## Build and packaging

**Go has no race detector and no BoringCrypto.** Both are objects upstream compiles and ships inside
the source tarball, and the `go` port does not install them, so `go build -race`, `go test -race` and
`GOEXPERIMENT=boringcrypto` fail to link. Every other Go build is unaffected.

**GHC has no profiling libraries.** The `ghc` port builds Hadrian's `release` flavour without its
profiled variants, so `ghc -prof` and `cabal --enable-profiling` fail to find the profiled `base`.
Of GHC's documentation only the `ghc(1)` manual page is installed. The User's Guide and the Haddock
pages of the shipped libraries are not, so `cabal haddock` writes pages whose references into `base`
and the other GHC libraries are plain text rather than links.

**nmap has no `jdwp-exec` and no `jdwp-info` script.** Both inject Java classes that upstream
compiles and ships inside the source tarball, and the `nmap` port installs neither the classes nor
the two scripts, so `--script jdwp-info` names nothing and the `default` category runs without it.
`jdwp-inject`, which injects a class you supply, and `jdwp-version` are unaffected.

**qemu carries firmware only for its three targets' default machines.** That is the x86_64 PC
machines, aarch64 and riscv64 `virt`, and microvm, each through its defaults and its firmware
descriptors. The Nuvoton and ASPEED BMC boards inside `qemu-system-aarch64` stop at startup with
*Could not find ROM image* unless `-bios` names one, and there is no 32-bit Arm UEFI, no 32-bit x86
OVMF and no OVMF build for microvm. The riscv64 `virt` UEFI image is upstream's rather than compiled
here — see [what is not built from source](../01-philosophy/why-kdos.md#what-is-not-built-from-source).

**A qemu guest cannot join a host bridge as an ordinary user.** `-netdev bridge` runs
`qemu-bridge-helper`, which is installed without its setuid bit and with no `/etc/qemu/bridge.conf`,
so it serves only root, and only once root writes an `allow <bridge>` line there. Making it setuid
would add a root program to the [setuid list](../03-architecture/security-model.md), and that is not
decided. User-mode networking through `passt` needs no privilege.

**PROJ has no transformation grids, and no port carries them.** A transformation that needs a grid
— NAD27 to NAD83, OSGB36, a national geoid — falls back to a ballpark one or fails, naming the grid
it wanted. proj is built with `ENABLE_CURL=OFF`, so grids are never fetched on demand either; the
transformations that need no grid are the ones the bundled `proj.db` answers exactly.

**presenterm's seven stock syntax themes are not compiled here.** `base16-ocean.dark`,
`InspiredGitHub`, the Solarized pair and the rest come from the serialized `default.themedump` inside
the vendored `syntect` crate. Their `.tmTheme` sources are in syntect's repository and not in the
crate, and the only way to swap in a rebuilt dump is a patch to presenterm's theme loading. Its
grammars, and bat's themes, are compiled here by the `bat` port.

**A `tzdata` upgrade can reset the time zone to UTC, once.** This happens on a machine whose
installed `tzdata` manifest lists `/etc/localtime`. The new version does not own the link, so the
upgrade removes it as an orphan, `rcS` links UTC on the next boot, and `TZ=':/etc/localtime'` follows
it. kpkg has no rule that keeps a file a package has given up, so run
`kdos-power timezone <Area/City>` to set the zone again. Later upgrades leave it alone.

**Upgrading toybox on its own can delete tools other ports own.** This affects a machine whose
installed toybox manifest (`/var/lib/kpkg/db/toybox`) lists names another port owns — `mount`,
`umount`, `losetup`, `kill`, `dmesg`, `readelf`, `strings`, `cmp`, `gunzip`, `insmod`, `lsattr` and
the rest. That happens when toybox was installed after util-linux, binutils, diffutils, gzip, bzip2,
attr, kmod, ncurses, e2fsprogs or procps-ng: an install to the same path takes the file into the
later manifest, so the owning port's manifest does not list it, and toybox's upgrade removes it as an
orphan. On such a machine, upgrade toybox together with those ports, whose release bumps reinstall
the names.

**There is no public binary host.** The mechanism is complete — a signed index, three equality
tests, deltas — but it is one you run yourself.

**The source archive has one public location.** Upstream sources live only as release assets on
`kunaldawn/kdos`. If it cannot be reached, `make fetch` falls back to each recipe's upstream
URL, which works only while upstream still serves the exact file. `KDOS_SOURCES_BASE` can point
`make fetch` at another copy laid out the same way, and nothing publishes such a copy. See
[Developing](../05-developer/developing.md#where-sources-come-from).

**A clone does not check what it pushes unless you enable the hook.** `script/hooks/pre-push`, which
refuses a push naming a source hash the archive does not hold, runs only after
`git config core.hooksPath script/hooks`. Without it, a push can name a source nobody has published,
and every other clone's `make fetch` then depends on upstream still serving that file. Setting
`core.hooksPath` replaces `.git/hooks` as a whole, so git-lfs's hooks, or any other installed there,
stop running in that clone. With the hook on, a push is also refused when the archive cannot be
reached, when it answers anything but 200 or 404, or when `KDOS_SOURCES_BASE` is empty, because a
missing source cannot then be ruled out; so pushing while offline needs the bypass.
`KDOS_SKIP_PUBLISH_CHECK=1 git push …` skips the check for one push.

**No port uses node vendoring.** No port declares `vendoring = node`, though `ports/fetch` implements
it beside the four that are used: 60 recipes declare `rust`, 34 `go`, 25 `python` and 4 `haskell`.
The npm path has never been run, so what stands behind it is the code and not a tarball it produced.

## Testing

**The compositor and the shell are not compiled by the self-test on a bare host.** Their Wayland,
font, D-Bus and audio libraries are not there, so those blocks report as skipped on most machines.
`testing/devdeps-image.sh` builds a container where they run.

**No test builds the distribution.** The orchestrator runs end to end against a synthetic tree
(`kdosbuild --selftest`), but the package manager and the recipes can only really be tested by
building the whole distribution, which takes hours and a container.

**No harness exercises the source archive end to end.** Nothing publishes to an archive and fetches
back from it under test. `ports/publish` accepts `KDOS_GITHUB_API` and `KDOS_GITHUB_UPLOADS`, and
`ports/fetch` accepts `KDOS_SOURCES_BASE`, so a local stand-in could be used, but no test does so.
What `testing/preflight.sh` checks is that the scripts parse, that git tracks no recipe-hashed
archive, and that the ignore rules cover every source suffix.

**The rig cannot show a live microphone level on its own.** The rig is the QEMU harness that boots
a real KDOS image and photographs it (see [Testing](../05-developer/testing.md)). Its emulated sound
card gives the guest no capture signal, so a recording level meter stays flat even with the rig's
`--audio` option on. Loading
the `snd-aloop` kernel module in the guest, from a root script, gives it a loopback capture device to
record from.

**The phosphor shader is not in any rig photograph.** The rig's virtual display puts the compositor
on software rendering, where the pass switches itself off. What is photographed is the cell grid
underneath it.

## Documentation

**Some measurements in this book are quoted rather than re-taken.** Contrast ratios, launch times,
freeze ratios and transport throughput figures were measured once and are repeated here. Where a
number is a measurement, the conditions are stated; where it has not been re-taken, that is the
honest caveat.

## See also

- [Decisions](../01-philosophy/decisions.md) — what is absent on purpose, and why
- [Roadmap](roadmap.md) — what is intended
- [Status](status.md) — maturity per subsystem, and the evidence behind each verdict
- [The security model](../03-architecture/security-model.md) — the full statement of what is not protected
