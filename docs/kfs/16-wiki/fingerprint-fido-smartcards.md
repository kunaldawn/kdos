# Fingerprints, FIDO keys and smart cards

Three kinds of hardware can stand in for, or add to, a password on KDOS: a fingerprint reader, a
FIDO2 or U2F security key, and a [smart card]{.idx} in a reader. This page covers what each needs, the
files that set it up, and the procedures that use it: enrolling a finger for [`sudo`]{.idx}, making an SSH
key that lives on a security key, and reaching a smart card's certificates. What each one does
not reach is listed under Known gaps.

## Installation

All of these are installed by `41_system` on every image, live and installed:

| Hardware | Ports | Started by |
|:--|:--|:--|
| Fingerprint reader | [libfprint](#port-libfprint) (the reader drivers), [fprintd](#port-fprintd) (the daemon, `pam_fprintd.so` and the `fprintd-*` tools) | The [system bus]{.idx}, on the first call to `net.reactivated.Fprint`. It runs `/usr/libexec/fprintd` as root through `dbus-daemon-launch-helper`; there is no service script |
| FIDO2 / U2F key | [libfido2](#port-libfido2) (the library and the `fido2-token`, `fido2-cred` and `fido2-assert` tools); [openssh](#port-openssh), built `--with-security-key-builtin` | Nothing: the key is a USB HID device, opened by the program that uses it |
| Smart card | [pcsc-lite](#port-pcsc-lite) ([`pcscd`]{.idx}), [ccid](#port-ccid) (the driver for USB CCID readers), [opensc](#port-opensc) (`opensc-pkcs11.so` and the `pkcs11-tool` and `opensc-tool` tools), [p11-kit](#port-p11-kit) | [`/etc/init.d/47_pcscd.sh`]{.idx}, at boot, after `41_polkitd` |

[pam](#port-pam) provides the [PAM]{.idx} stacks, and [sudo](#port-sudo) is the one program on the image
whose stack asks for a finger. The console login and SSH do not use PAM: shadow is built
`--without-libpam` and openssh without `--with-pam`.

`pcscd` is enabled on both the live medium and an installed system; `service list` shows it as
`pcscd`. A fingerprint enrolled on the live medium is kept in the overlay's upper layer, so it is
gone at power-off unless a persistence store exists; see
[The live medium and persistence](../../kdos/03-architecture/boot-and-init.md#the-live-medium-and-persistence).

## Configuration

### /etc/pam.d/sudo

Written by the `sudo` port's `build.sh`. The first line tries an enrolled finger, and the leading
`-` skips the line when `pam_fprintd.so` is missing:

<!-- kfs:include ports/core/auth/sudo/build.sh lines=35-53 pin=8ee863bc4592 -->

`system-auth`, which it includes, is written by the `pam` port's `build.sh`:

<!-- kfs:include ports/core/auth/pam/build.sh lines=36-43 pin=d80101a3032d -->

### /etc/fprintd.conf

Installed by fprintd as upstream ships it. It holds one setting, `type=file` under `[storage]`,
so enrolled prints are files under `/var/lib/fprint`, one directory per account. The directory
does not exist until the first finger is enrolled.

### /etc/udev/rules.d/70-kdos-fido.rules

Gives every FIDO2 and U2F key to the `dialout` group. From `fs/etc/udev/rules.d/`:

<!-- kfs:include fs/etc/udev/rules.d/70-kdos-fido.rules -->

The `kdos` account is in `dialout`. Any other account needs it for a key:
`sudo usermod -aG dialout <user>`, then log in again. The other device rules are in
[The device groups and the udev rules](../../kdos/02-user-guide/administration.md#the-device-groups-and-the-udev-rules).

### /etc/init.d/47_pcscd.sh

From `fs/etc/init.d/`. It runs `pcscd` in the foreground under the supervisor:

<!-- kfs:include fs/etc/init.d/47_pcscd.sh lines=7-19 pin=120540c49910 -->

### /etc/polkit-1/rules.d/50-kdos.rules

`pcscd` asks polkit about every client and every reader. The rule from `fs/etc/polkit-1/rules.d/`
grants both actions to [`wheel`]{.idx}:

<!-- kfs:include fs/etc/polkit-1/rules.d/50-kdos.rules lines=43-58 pin=261550fdeb7d -->

An account outside `wheel` reaches no smart card. The whole rule file is explained in
[What is granted instead](../../kdos/03-architecture/security-model.md#what-is-granted-instead-50-kdosrules).

### Other files

| File | Provided by | Holds |
|:--|:--|:--|
| `/usr/lib/udev/rules.d/92_pcscd_ccid.rules` | ccid's `build.sh` | Gives CCID readers to the `pcscd` group, which pcsc-lite's `postinstall.sh` creates |
| [`/etc/reader.conf.d/`]{.idx term="/etc/reader.conf.d"} | pcsc-lite's `build.sh` makes it; ccid installs `libccidtwin` in it, every line commented out | Serial readers, which udev cannot find. `libccidtwin` is the template for a Gemalto serial reader: uncomment its `DEVICENAME`, `FRIENDLYNAME` and `LIBPATH` lines and name the port |

## Usage

### Enrolling a finger for sudo

Not verified on real hardware: no reader has been through these steps on this system.

1. Check that the reader is seen. With the reader plugged in:

   ```bash
   sudo fprintd-list "$USER"
   ```

   It prints `found 1 devices` and the device's path, then the account's enrolled fingers, or
   `No devices available` when libfprint drives no reader on the machine. Every other
   `fprintd-*` command answers the same way without a reader; `fprintd-enroll` prints
   `Impossible to enroll: GDBus.Error:net.reactivated.Fprint.Error.NoSuchDevice: No devices available`.

2. Enrol. The command must run as root, because fprintd's polkit actions are granted only to an
   active session, which nothing on KDOS is:

   ```bash
   sudo fprintd-enroll "$USER"
   ```

   Touch the reader each time it asks, until it prints `Enroll result: enroll-completed`. Without
   `-f` it enrols `right-index-finger`; `-f left-thumb`, or any other name the command lists when
   given a wrong one, enrols that finger instead.

3. Check it:

   ```bash
   sudo fprintd-verify "$USER"
   ```

4. Use it: the next `sudo` waits for a finger on the reader before it asks for the password. A
   failed or skipped finger falls through to the password.

To remove every print of an account, run `sudo fprintd-delete "$USER"`.

### An SSH key on a security key

1. Plug in the key, and check that it is seen:

   ```bash
   fido2-token -L
   ```

   It prints one line per key, and nothing, with exit status 0, when it finds none. Nothing
   printed with the key plugged in means the account cannot open it; see Troubleshooting.

2. Make a key pair whose private half stays on the security key:

   ```bash
   ssh-keygen -t ed25519-sk
   ```

   Touch the key when `ssh-keygen` asks. Add `-O resident` to store the key on the token, so it can be
   loaded on another machine with `ssh-keygen -K`.

3. Copy the public half to the server, as for any key: `ssh-copy-id -i ~/.ssh/id_ed25519_sk.pub
   user@server`.

4. Log in with `ssh user@server`, and touch the key when asked.

The same key type works for logging in to this machine; see [SSH server](ssh-server.md).

### Reaching a smart card

Not verified on real hardware: no card has been used in a reader on this system.

1. Check that the service runs and sees the reader:

   ```bash
   service status pcscd
   opensc-tool --list-readers
   ```

2. Check that the card is seen:

   ```bash
   pkcs11-tool --list-slots
   pkcs11-tool --list-objects
   ```

   With no reader attached, `opensc-tool` prints `No smart card readers found.` and
   `pkcs11-tool` prints `No slots.`. An account outside `wheel` is refused by `pcscd` and gets the
   same two answers; the refusal is only in the service's log (see Troubleshooting).

3. Use it. For SSH, name the PKCS#11 module:

   ```bash
   ssh -I /usr/lib/opensc-pkcs11.so user@server
   ```

   GnuTLS programs and openconnect find the card through p11-kit, which loads `opensc-pkcs11.so`
   from its module file; see
   [Thunderbolt, phones, smart cards and firmware updates](../../kdos/02-user-guide/administration.md#thunderbolt-phones-smart-cards-and-firmware-updates).

## Tips

- `kdos doctor` checks `dbus-daemon-launch-helper`'s setuid bit and its `messagebus` group.
  Without them the system bus cannot start fprintd, and no fingerprint command works.
- A FIDO key asks for a touch before it signs, so `dialout` membership alone does not let another
  process use it silently.
- `ssh-keygen -t ecdsa-sk` makes an ECDSA key instead, for a token that does not support Ed25519.
- The pcscd service's log is `/run/kdos-svc.pcscd.log`.

## Troubleshooting

**`sudo fprintd-list` prints `Failed to get Fprintd manager` or `Impossible to get devices`.**
The system bus could not start the daemon. Run `kdos doctor` and read the
`dbus-daemon-launch-helper` line in the Desktop section; then check that
`/usr/share/dbus-1/system-services/net.reactivated.Fprint.service` exists.

**`fprintd-enroll` without `sudo` prints `failed to claim device`.**
fprintd's polkit actions allow only an active session. Run it with `sudo`, as above.

**`sudo` over SSH waits before asking for the password.**
For an account with a finger enrolled, `pam_fprintd` waits on the machine's reader first; see the
first item under Known gaps. Wait for it to time out, or remove the account's prints with
`sudo fprintd-delete <user>` if it never uses the reader locally.

**`fido2-token -L` prints nothing, or `ssh-keygen -t ed25519-sk` ends `Key enrollment failed: device not found`.**
Check the account's groups with `id`: it needs `dialout`. Check the node: `ls -l /dev/hidraw*`
should show one owned by group `dialout`, mode `crw-rw----`. If the node is `root root`, mode
`crw-------`, the rule did not match: `udevadm info /dev/hidrawN | grep ID_SECURITY_TOKEN` prints
nothing for a device whose HID descriptor does not declare FIDO. Every other HID device, such as
a virtual machine's tablet, has such a node.

**`pkcs11-tool --list-slots` reports no slots, or access is denied.**
Run `service status pcscd`; start it with `sudo service start pcscd`. Check that the account is in
`wheel`, since polkit refuses everyone else: `pkcheck --action-id org.debian.pcsc-lite.access_pcsc
--process $$` answers `Not authorized.` outside it, and `/run/kdos-svc.pcscd.log` shows
`Process <pid> (user: <uid>) is NOT authorized for action: access_pcsc` and
`Rejected unauthorized PC/SC client` for every attempt. Check that `pcscd` sees the reader: `opensc-tool
--list-readers`.

## Known gaps

- [A fingerprint unlocks sudo and nothing else](../../kdos/06-reference/known-gaps.md#a-fingerprint-unlocks-sudo-and-nothing-else)
- [Smart cards, drawing tablets and software radios have not met hardware](../../kdos/06-reference/known-gaps.md#smart-cards-drawing-tablets-and-software-radios-have-not-met-hardware)
- No port provides a PAM module for FIDO keys, so a security key cannot log in to the console or
  unlock `sudo`.

## See also

- [The device groups and the udev rules](../../kdos/02-user-guide/administration.md#the-device-groups-and-the-udev-rules):
  why `dialout`, and why upstream's `plugdev` rules are left out
- [polkit, and why the desktop has no authentication agent](../../kdos/03-architecture/security-model.md#polkit-and-why-the-desktop-has-no-authentication-agent):
  why fprintd's actions need `sudo`
- [Thunderbolt, phones, smart cards and firmware updates](../../kdos/02-user-guide/administration.md#thunderbolt-phones-smart-cards-and-firmware-updates)
- [sudo, PAM and polkit](sudo-pam-polkit.md), [udev and device groups](udev-and-device-groups.md),
  [SSH server](ssh-server.md), [Screen lock](screen-lock.md), [Live persistence](live-persistence.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 383.* Previous: [382. Screen lock](screen-lock.md) · [Contents](../README.md) · Next: [384. Firewall](firewall.md)
