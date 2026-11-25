## About

`kdos-settingsd` is the root daemon that serves `org.freedesktop.hostname1`,
`org.freedesktop.timedate1` and `org.freedesktop.locale1` on the system bus: the three interfaces
systemd defined for a machine's name, clock, time zone, locale and keyboards, which KDE calls
instead of writing files. The Date & Time page of System Settings saves through timedate1 and
nothing else; plasma-setup uses all three; the SDDM greeter runs `kwin_wayland --locale1`, which
takes the login screen's keyboard layout from locale1 and follows it when it changes; startplasma
copies that layout into each session; the system monitor's OS sensors read hostname1. KDOS has no
systemd, so this daemon answers for them, with every method and property systemd's three
interfaces have except hostname1's `Describe`, which nothing on the system calls.

Each setting is kept in the file the rest of the machine already reads: the static hostname in
`/etc/hostname` (set at boot by `05_hostname.sh`), with the `127.0.1.1` line of `/etc/hosts`
naming whatever the kernel carries, because musl resolves the machine's own name from that file
and nothing else before the DNS; the pretty name, icon, chassis, deployment and
location in `/etc/machine-info`, the zone in `/etc/localtime` with `/etc/profile.d/20-timezone.sh`
and the Wi-Fi country in `/etc/modprobe.d/kdos-regdom.conf`, network time as the
`/etc/service.disabled/chrony` marker, the locale in `/etc/locale.conf` (which SDDM puts into
every session), the console keymap in `/etc/keymap` (loaded by `kdos-getty`, with a copy in
`/boot/efi/EFI/kdos/keymap` for the initramfs's passphrase prompt) and the graphical
layout in `/etc/kdos/xkb.conf`, never under `/etc/X11`, with the same layout in
`/etc/xdg/kxkbrc`. Every method checks its arguments, then
asks polkit under systemd's own action ids, then writes; the pattern the root daemons share is in
[The shape they share](../../../docs/kdos/04-programs/daemons.md#the-shape-they-share).

The port is KDOS's own C with no upstream source. `build.sh` compiles the daemon with the sources
of `libkbase` and links [basu](#port-basu) and nothing else: no glib and no Qt, because everything
a root daemon links is code running as root.

## Notes

- **Three kinds of file beside the binary, and no activation file.** The D-Bus policy
  `/usr/share/dbus-1/system.d/kdos-settingsd.conf` lets root alone own the three names and lets
  anyone call them; [dbus](#port-dbus) reads it, which is why dbus is in `depends`. The polkit
  actions are three `.policy` files under `/usr/share/polkit-1/actions/` with systemd's ids,
  because plasma-setup's rule and the Date & Time page name them; without
  [polkit](#port-polkit) every call from anyone but root is refused. `set-time` implies
  `set-timezone` and `set-ntp`, and those two are allowed to the active local session without a
  password. The Date & Time page authorises `set-time` before it saves anything, so it asks for
  the password on every Apply, a zone-only one included, and that one prompt covers its three
  calls; hostname1's `get-product-uuid` and
  `get-hardware-serial` guard the two values that identify the machine, which the kernel makes
  root's alone. The daemon is started by `/etc/init.d/43_settingsd.sh` under `ksvc`, after
  polkitd and before the display manager; a supervised daemon ships no activation file, or an
  early caller would make the bus start a second, unsupervised copy.
- **What it runs, and what it reads.** `service start|stop chrony` from
  [kdos-tools](#port-kdos-tools) turns network time on and off now, `loadkeys` from
  [kbd](#port-kbd) loads a new console keymap, and `iw` from [iw](#port-iw) applies a new Wi-Fi
  country, or the world domain `00` for a zone with none, so the running radio never keeps the
  previous zone's country; each is an argument vector found through `PATH` and skipped when it is missing, so
  none is a dependency. `CanNTP` is true when chrony's init script and `chronyd` from
  [chrony](#port-chrony) are both there. A zone is checked against the zone tree of
  [tzdata](#port-tzdata), a console keymap against kbd's keymaps and an XKB layout against
  [xkeyboard-config](#port-xkeyboard-config)'s `rules/evdev.lst`, each where it is installed.
  The hardware properties are DMI's files under `/sys/class/dmi/id`, with a vendor's
  placeholder for "unset" read as no value; the chassis is machine-info's, else `vm` in a
  virtual machine, else DMI's, ACPI's or the device tree's, in systemd-hostnamed's order.
- **A kernel with no name gets the default.** `05_hostname.sh` names the kernel from
  `/etc/hostname` and leaves it `(none)` when the file is gone, which is what
  `SetStaticHostname("")` does to it; the daemon gives such a kernel the default name
  (`DEFAULT_HOSTNAME=` in os-release, else `kdos`) when it starts, as systemd's init would.
- **An empty layout follows the console.** The installer writes the console keymap alone and the
  shipped `xkb.conf` leaves `XKBLAYOUT` empty, so locale1's `X11Layout` is then the layout
  nearest `/etc/keymap`, through a table of the console names whose first two letters are not
  their XKB layout, and the login screen has the installed keyboard from the first boot. A
  derived layout is given only when xkeyboard-config carries it: KWin falls back to US for a
  layout it cannot compile and says so only in its log.
- **Every layout change rewrites `/etc/xdg/kxkbrc`.** KWin and the lock screen read the kxkbrc
  cascade before the `XKB_DEFAULT_*` environment startplasma builds from `xkb.conf`, so a system
  `kxkbrc` naming a layout outranks locale1 in every session of an account with no `kxkbrc` of
  its own. The installer writes the file from its Keyboard page; on an OEM machine that is the
  preparer's layout, and plasma-setup's Keyboard page sets the owner's through
  `SetX11Keyboard`. So `SetX11Keyboard`, and `SetVConsoleKeyboard` when `convert` changes the
  layout, write the file whole in the installer's shape (`[Layout]`, `LayoutList`,
  `VariantList`, `Model` and `Options` with `ResetOldOptions=true` when set, `Use=true`), and an
  empty layout removes it, as the installer does when no layout was named. Without it the
  session and the lock screen type the installer's layout while the login screen types the
  owner's, and a password with keys the two lay out differently cannot unlock the screen.
- **The console keymap is copied to the ESP.** The initramfs types the disk passphrase before any
  root is open, so it loads the map named in `/boot/efi/EFI/kdos/keymap`. Every keymap the daemon
  writes is written there too, an empty one removes it, and at start the copy is rewritten from
  `/etc/keymap` when it differs or is missing; only where `/boot/efi/EFI/kdos` exists. The write
  is `kdos-bootctl`'s: a temporary file, its `fsync`, the rename, the file's `fsync` and a
  `syncfs`, without an `fchmod`, which vfat refuses. Without it a passphrase set on any layout but
  US cannot be typed at boot.
- **The console map nearest a layout is `<layout>-latin1` before `<layout>`.** kbd's bare `de`,
  `dk`, `no`, `fr`, `sg` and `fr_CH` are the 7-bit ISO-646 maps, with no ü ö ä ß, and its bare
  `pt` is the OLPC laptop's, so `convert` names the 8-bit maps in its table and takes the bare
  name last.
- **The RTC is UTC.** The kernel reads the RTC as UTC at boot and writes it back every eleven
  minutes while chrony has the clock synchronised, so `LocalRTC` is false and `SetLocalRTC(true)`
  is refused. A clock set by hand is not synchronised, so `SetTime` writes the RTC itself.
- **`/etc/locale.conf` is bare `NAME=value` lines.** SDDM splits each line at `=` and passes the
  field after the first one on verbatim, so a quoted value would reach the session with its
  quotes and a comment holding an `=` would become a variable; the daemon writes neither, and
  `SetLocale` writes only `C`, `C.UTF-8`, `POSIX` or `ll[_CC][.UTF-8|.utf8][@modifier]` whose
  language has a file in `<share>/i18n/locales/musl` or a directory in `<share>/locale`, so a
  typo cannot reach every session. It does not demand a musl-locales file for the whole name:
  plasma-setup's language page sends `LANG=<ll_CC>.UTF-8` for every language Plasma has
  translations for, and a refusal there leaves the system language English with only a warning in
  its log.
- **The test tree.** `KDOS_SETTINGSD_ETC`, `KDOS_SETTINGSD_SHARE` and `KDOS_SETTINGSD_SYS` replace
  `/etc`, `/usr/share` and `/sys`, and `KDOS_SETTINGSD_ESP` replaces `/boot/efi`. The first also
  keeps the daemon off the machine it runs on: with it set the kernel hostname, the clock and the
  RTC are left alone, and no ESP is written unless `KDOS_SETTINGSD_ESP` names one. `testing/selftest.sh`
  drives every method that way, over a private bus, against a polkit stand-in
  (`testing/fixtures/settingsd/`).
