## About

ISO Image Writer writes an ISO or raw disk image to a USB stick: it lists the removable drives,
writes the image block by block with a progress bar, and before writing checks the signature of
an image from a distribution it knows. It is the program that opens a disk image by default on
KDOS, and the way to write a KDOS installer stick from the desktop.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_Gpgmepp=ON` — With
  `-D CMAKE_REQUIRE_FIND_PACKAGE_QGpgmeQt6=ON`: make [gpgmepp](#port-gpgmepp) and
  [qgpgme](#port-qgpgme) requirements. Upstream looks for both quietly and compiles the
  signature check only when QGpgme is found; without it every image is written unchecked.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Patches

`status-wrap.patch` lets the status line under the USB drive field wrap, gives it the stretch of
the row it shares with the Create button, and reserves two lines of height for it. Upstream leaves
it on one line, so at the window's default width a sentence such as "Could not find the key to
verify the ISO image", which every image from an unknown distribution shows, is cut off where the
button begins. The row is otherwise as tall as the button, and the main window does not grow for
a label's height-for-width, so without the reserved height a wrapped sentence loses the bottom of
its second line. It is to be offered upstream as a merge request against
`utilities/isoimagewriter`.

`live-medium.patch` makes four changes to the drive list and the write, and is to be offered
upstream against `utilities/isoimagewriter` as one merge request.

The list skips a block device whose `org.freedesktop.UDisks2.Block` property `HintIgnore` is
true. That is udev's `UDISKS_IGNORE`, which `fs/etc/udev/rules.d/70-kdos-live-medium.rules` sets
on the medium the live session runs from, and which Dolphin and the Device Notifier already
honour. Upstream lists every removable or USB drive, so the running stick is offered as a target,
and choosing it fails at `OpenDevice` because the system holds it open. The rule marks the disk
mounted at `/mnt/iso` and its partitions; under Ventoy that is the `/dev/mapper/ventoy` mapping,
so the Ventoy stick beneath it is still listed, and choosing it fails at `OpenDevice` with that
reason.

The list is sorted by UDisks2 object path. Upstream walks the `GetManagedObjects` reply in hash
order, which differs in every process, so the order of the drives and the one selected first
change from launch to launch.

Each drive is named by its `/dev` node (`Block.Device`) rather than its UDisks2 object path, so
the list reads `/dev/sdb - QEMU HARDDISK (8.0 GiB)` and two sticks of one model stay apart; the
object path is still what the D-Bus calls use. The confirmation before writing names the chosen
drive by the same text, where upstream says only "the USB drive".

The `OpenDevice` reply and the open of its descriptor are checked, and a failure is reported as
"Could not open /dev/sdX: " followed by UDisks2's or Qt's reason. Upstream writes on with no
descriptor and reports only "The last block was not fully written (-1 of 1,048,576 bytes)".

## Notes

The check is by file name and covers KDE neon and Arch Linux (a detached `.sig` beside the image,
against the keys the port installs in `/usr/share/isoimagewriter`), Kubuntu and Ubuntu (a signed
`SHA256SUMS`), and Debian and Netrunner (a checksum the person pastes in). Any other image is
reported as not a known distribution's and can still be written. A KDOS image is one of them: the
status line reads "Could not find the key to verify the ISO image", and
`kdos verify kdos-<version>.iso`, with the release's `SHA256SUMS` and `SHA256SUMS.sig` beside it,
checks the image and the list's signature against the KDOS release key.

The device is opened through UDisks2's `OpenDevice` call on the system bus, so
[udisks2](#port-udisks2), in `depends` for run time, must be running, and polkit decides the
`org.freedesktop.udisks2.open-device` action: `fs/etc/polkit-1/rules.d/50-kdos.rules` grants it
to the `wheel` group, and anyone else is asked for an administrator's password.
