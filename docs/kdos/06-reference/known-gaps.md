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

Fingerprint login needs the reader enrolled from a terminal. `fprintd` and `pam_fprintd` ship, so a
reader unlocks a session once a finger is on file, and `fprintd-enroll` is what puts one there. No
surface offers enrolment.

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
`avformat.so`, which the generated config leaves commented because which camera to send is a choice.
The rig has no second endpoint and no camera, so what is measured is that the modules load.

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

`ocrmypdf` does not open a HEIF image. It asks for the `pillow_heif` module, and the HEIF plugin
this tree builds is `python3-pi-heif`, whose module is `pi_heif`, so a phone's HEIC photo has to be
converted to PNG or JPEG before it can be made into a searchable PDF.

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

Filling the second root slot is an updater's job, and there is no updater. What exists is the
complete state machine: the installer writes the initial state and each slot's LUKS container, the
initramfs selects a slot, unlocks that slot's container and counts attempts, and a boot that reaches
the end of initialisation confirms the slot. A second *encrypted* slot has therefore never been
booted, because nothing fills one. What is measured is on the host: `select` rolling from B back to A
hands back A's container and not B's, which is the failure the mechanism exists to prevent.

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
