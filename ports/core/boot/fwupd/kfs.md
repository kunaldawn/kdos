## About
[fwupd]{.idx} is the daemon and the `fwupdmgr` client that update device firmware from the LVFS,
the Linux Vendor Firmware Service: SSDs, docks, USB peripherals, and system firmware shipped as a
UEFI capsule. On KDOS the system bus starts the daemon on first use; nothing supervises it. How a
person uses it, and where the capsule goes on the ESP, is in
[Thunderbolt, phones, smart cards and firmware updates](../../../../docs/kdos/02-user-guide/administration.md#thunderbolt-phones-smart-cards-and-firmware-updates).

## Flags
- `--wrap-mode=nodownload` — The tree's usual meson switch, with a reason specific to this
  source: the tarball ships git wraps for libxmlb, fwupd-efi, Passim and gi-docgen under
  `subprojects/`, and the `xmlb` and `fwupd-efi` dependencies name theirs as fallbacks, so a
  build root without [libxmlb](#port-libxmlb) or [fwupd-efi](#port-fwupd-efi) would make meson
  clone and build a private copy. With the switch a missing one is a configure error, and fwupd
  links the libxmlb the rest of the image links.
- `-Dsystemd=disabled`, `-Dlogind=disabled` — Build without systemd's unit files and
  libsystemd, and without the logind support, which meson finds through `systemd` or
  `libelogind`; the tree has neither, so `enabled` would stop the configure
  ([No systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd)). Without logind the
  daemon takes no inhibitor lock while it flashes, so nothing delays a shutdown requested in
  the middle of an update, and `fwupdmgr` cannot reboot or power off the machine itself when an
  update needs it: it answers "No way to perform the operation", and the restart is done by hand.
- `-Dintrospection=disabled` — Generate no GObject Introspection data for `libfwupd`, so no
  language binding can load it through GI. Turning it on needs
  [gobject-introspection](#port-gobject-introspection) and
  [glib-introspection](#port-glib-introspection) in `depends`: the typelib is generated against
  glib's own GIRs.
- `-Dman=true` — Build the manual pages, `fwupdmgr(1)`, `fwupd.conf(5)` and the rest. It is
  upstream's default; the line states it beside `-Ddocs=disabled`, which turns off only the
  developer documentation.
- `-Defi_binary=true` — Make the UEFI capsule plugin require the `fwupd-efi` dependency at
  configure time; with `--wrap-mode=nodownload` its wrap cannot stand in, so a build root without
  it stops the configure. [fwupd-efi](#port-fwupd-efi) is in `depends`: it installs the loader,
  `fwupdx64.efi`, under `/usr/libexec/fwupd/efi`, which the machine boots once to hand a capsule
  to the firmware, and where the daemon looks for it at run time.
- `-Defi_os_dir=kdos` — Compile in the name of the OS directory on the ESP, so the plugin writes
  capsules and the loader under `EFI/kdos/`, the directory that already holds KDOS's boot state and
  slot kernels. Without it the plugin probes `EFI/systemd`, then `EFI/<ID>` and `EFI/<ID_LIKE>`
  from `/etc/os-release`, and creates `EFI/<ID>` when none exists.
- `-Dplugin_uefi_capsule_splash=false` — Build without the "Installing firmware update…" splash
  images the capsule loader can show. Making them runs `make-images.py`, which needs the `gi`
  Python module with the Pango, PangoCairo and cairo typelibs at configure time; `po/test-deps`
  checks for them and stops the configure when one is missing.
  [python3-gobject](#port-python3-gobject) is not in `depends`.
- `-Dplugin_modem_manager=enabled` — Build the plugin that updates the firmware of mobile
  broadband modems through ModemManager. [modemmanager](#port-modemmanager),
  [libqmi](#port-libqmi) and [libmbim](#port-libmbim) are in `depends`.
- `-Dpassim=disabled`, `-Dp2p_policy=none` — Build without Passim, a local caching service that
  shares downloaded metadata and firmware with other machines on the network, and compile in a
  default `P2pPolicy` that publishes nothing. The tree carries no Passim.
- `-Dumockdev_tests=disabled`, `-Dvalgrind=disabled` — Build no tests that run under umockdev or
  valgrind; with `-Dtests=false` no test is built at all.
- `-Dgnutls=enabled`, `-Dopenssl=disabled` — Verify signatures and checksums with GnuTLS, and
  only with it. fwupd probes GnuTLS first and falls back to OpenSSL's libcrypto when GnuTLS is
  not found; the embedded jcat verifier uses whichever it finds, so OpenSSL would be a second
  crypto library nothing calls.
- `-Dpolkit=enabled` — Ask polkit before a client may install firmware or change the daemon's
  settings. [polkit](#port-polkit) is in `depends`; how KDOS answers polkit is in the
  [security model](../../../../docs/kdos/03-architecture/security-model.md).
- `-Dblkid=enabled`, `-Dlibdrm=enabled`, `-Dlibmnl=enabled`, `-Dreadline=enabled` — Require
  libblkid (block device probing), libdrm (display and GPU devices, and the `libdrm_amdgpu`
  device information), libmnl (the netlink library the `devlink` plugin is built on, for network
  adapters' firmware) and readline (line editing at the prompts of `fwupdmgr` and `fwupdtool`). Each is a port in `depends`; `enabled` turns a missing one into a configure
  error instead of a plugin silently left out.
- `-Dbluez=enabled` — Build the plugins for Bluetooth devices. fwupd reaches BlueZ over D-Bus;
  the switch only compiles the support in, and no BlueZ library is linked.
- `-Dhsi=enabled` — Build Host Security ID support, `fwupdmgr security`, which grades the
  platform's firmware protections. meson allows it only on Linux on x86.
- `-Dbash_completion=true`, `-Dfish_completion=false` — Install the bash completion, which
  [bash-completion](#port-bash-completion) loads, and not the fish one.
- `-Dsupported_build=disabled` — Build as a package without upstream's validation. The client
  then prints "This package has not been validated, it may not work properly." unless
  `FWUPD_SUPPORTED` is set in the environment, and reports sent to the LVFS carry
  `FwupdSupported=false`.
- `-Dvendor_ids_dir=/usr/share/hwdata` — Where fwupd reads `usb.ids`, `pci.ids` and `pnp.ids` to
  name vendors. [hwdata](#port-hwdata) installs them there. Left empty, meson probes a list of
  fixed paths at configure time for `usb.ids` and stops with "could not auto-detect
  -Dvendor_ids_dir=" when none holds one.

## Patches
- `esp-mount-path.patch` — Lets the daemon find the EFI system partition without udisks. When no ESP volume is known, it reads `EspLocation` from `fwupd.conf` and, if set, describes the filesystem mounted there itself before asking udisks: it must be a mount point (its device differs from its parent's) and a FAT filesystem; the partition number, start and size are read from `/sys/dev/block/<major>:<minor>`, and the partition UUID and type from the udev database under `/run/udev/data`. A partition the udev database does not describe still gives a volume with no UUID, usable for capsule-on-disk and not for a boot entry. The patch adds `fu_volume_new_esp_from_mount_path()` and makes the volume's size, offset, number and block size fall back to the probed values when there is no udisks proxy.

## Notes
- **`EspLocation` and the patch go together.** `build.sh` writes `/etc/fwupd/fwupd.conf` with
  `EspLocation=/boot/efi`, the mount point the installer's `fstab` gives the ESP, and the patch
  is what reads it before udisks. [udisks2](#port-udisks2) is installed later, in `43_toolkits`,
  for gvfs; with `EspLocation` set and mounted, fwupd does not need it. The live image mounts no
  ESP, and the patch refuses a path that is not a FAT mount point, so there fwupd reports no ESP
  rather than a wrong one. The file keeps mode `0640`, the mode upstream installs it with,
  because `fwupd.conf` may hold a remote's credentials.
- **Updating takes `sudo`.** The daemon is started as root by `dbus-daemon-launch-helper` on the
  first client call and exits when idle; there is no init script. polkit sees no subject in an
  active session on KDOS, so every check falls to an action's `allow_any`, which is `auth_admin`
  for each of fwupd's actions, and the rule fwupd installs for `wheel` requires an active
  session as well. `fwupdmgr update` is therefore run with `sudo`; the reasoning is in
  `/etc/polkit-1/rules.d/50-kdos.rules`.
- **The metadata refresh is a KDOS timer.** Upstream's `fwupd-refresh.timer` is a systemd unit
  and is not installed; without a refresh, `fwupdmgr get-updates` compares against whatever was
  last downloaded by hand, which on a new machine is nothing. `build.sh` ships
  `/etc/kdos/timers.d/30-fwupd-refresh.timer`, which runs `fwupdmgr refresh` daily at 13:43, with
  a 20-hour slack for a machine that was asleep through the slot. The slot is at midday because
  a slot missed while the machine is off is not run later, and a desktop switched off every
  night would never reach an overnight one. The timer format is in
  [Periodic jobs](../../../../docs/kdos/02-user-guide/administration.md#periodic-jobs).
- **No GRUB snippet.** The capsule plugin installs `/etc/grub.d/35_fwupd`, a GRUB menu entry for
  the loader; `build.sh` removes it, because KDOS boots with Limine and carries no GRUB.
