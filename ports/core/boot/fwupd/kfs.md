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
- `-Dsystemd=disabled` — Build without systemd's unit files and libsystemd
  ([No systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd-init-units-or-service-manager)).
- `-Dlogind=enabled` — Build the `logind` plugin, which meson finds through `systemd` or, here,
  [elogind](#port-elogind)'s `libelogind`, and which speaks to `org.freedesktop.login1` over
  D-Bus. While a device is being flashed the daemon holds a shutdown and sleep inhibitor, so a
  power-off or suspend requested in the middle of an update waits for it, and `fwupdmgr` can
  reboot or power off the machine itself when an update needs it. Without it, a shutdown cuts a
  flash short and `fwupdmgr` answers "No way to perform the operation".
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
  Python module with the Pango, PangoCairo and cairo typelibs, and the `cairo` Python module;
  `po/test-deps` checks for them at configure time and stops the configure when one is missing.
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
- `-Dlvfs=disabled` — Install the LVFS remote, `/etc/fwupd/remotes.d/lvfs.conf`, switched off.
  Discover's firmware backend refreshes every enabled download remote each time Discover starts
  and after every transaction, so an enabled LVFS remote is a download at every start of the
  software centre. Firmware is listed instead from the `kdos-snapshot` local remote, the LVFS
  metadata a release carries ([fwupd-snapshot](#port-fwupd-snapshot)); `fwupdmgr enable-remote
  lvfs` turns live metadata on, after which Discover refreshes it at each start and `fwupdmgr
  refresh` fetches it by hand.
- `-Dvendor_ids_dir=/usr/share/hwdata` — Where fwupd reads `usb.ids`, `pci.ids` and `pnp.ids` to
  name vendors. [hwdata](#port-hwdata) installs them there. Left empty, meson probes a list of
  fixed paths at configure time for `usb.ids` and stops with "could not auto-detect
  -Dvendor_ids_dir=" when none holds one.

## Patches
- `esp-mount-path.patch` — Lets the daemon find the EFI system partition without udisks. Upstream first asks udisks for ESP and then for BDP partitions; when that gives no FAT ESP and no internal FAT BDP, whether because udisks is not running or because it knows of none, the patch reads `EspLocation` from `fwupd.conf` and, if set, describes the filesystem mounted there itself, before upstream's last udisks call, the block-device check that turns "nothing found" into an error: it must be a mount point (its device differs from its parent's), a FAT filesystem, and on a partition; the partition number, start and size are read from `/sys/dev/block/<major>:<minor>`, the device node from its `uevent`, and the partition UUID and type from the udev database under `/run/udev/data`. A partition the udev database does not describe still gives a volume with no UUID, usable for capsule-on-disk and not for a boot entry. The patch adds `fu_volume_new_esp_from_mount_path()` and, when there is no udisks proxy, makes the volume's size, offset and number return the probed values and its block size come from the probed device node.

## Notes
- **`EspLocation` and the patch go together.** `build.sh` writes `/etc/fwupd/fwupd.conf` with
  `EspLocation=/boot/efi`, the mount point the installer's `fstab` gives the ESP, and the patch
  is what reads it when udisks reports no ESP. [udisks2](#port-udisks2) is installed later, in `43_toolkits`,
  for gvfs; with `EspLocation` set and mounted, fwupd does not need it. The live image mounts no
  ESP, and the patch refuses a path that is not a FAT mount point, so there fwupd reports no ESP
  rather than a wrong one. The file keeps mode `0640`, the mode upstream installs it with,
  because `fwupd.conf` may hold a remote's credentials.
- **Who may update.** The daemon is started as root by `dbus-daemon-launch-helper` on the first
  client call and exits when idle; there is no init script. polkit answers each request from the
  caller's elogind session: fwupd's own rule, installed under `/usr/share/polkit-1/rules.d`, lets
  `wheel` do everything, and `/etc/polkit-1/rules.d/50-kdos.rules`, read first, holds
  `org.freedesktop.fwupd.update-internal`, the system firmware flash, at an administrator's
  password even for the person at the machine. Plasma's polkit agent asks for it in Discover and
  in `fwupdmgr` alike.
- **No timer refreshes the metadata.** Upstream's `fwupd-refresh.timer` is a systemd unit and is
  not installed, and KDOS ships no timer in its place, because nothing the system ships reaches
  the network on its own. `fwupdmgr get-updates` and Discover answer from the `kdos-snapshot`
  local remote, the signed LVFS metadata of the release ([fwupd-snapshot](#port-fwupd-snapshot)),
  installed under `/usr/share/fwupd/remotes.d/`, so firmware is listed offline and is as fresh as
  the release. A person who wants firmware published since the release turns on the LVFS remote
  (`fwupdmgr enable-remote lvfs`) and runs `fwupdmgr refresh`. See
  [Offline by construction](../../../../docs/kdos/01-philosophy/principles.md#offline-by-construction).
- **No GRUB snippet.** The capsule plugin installs `/etc/grub.d/35_fwupd`, a GRUB menu entry for
  the loader; `build.sh` removes it, because KDOS boots with Limine and carries no GRUB.
