# The udev hardware database

[udev]{.idx} names devices and applies per-model quirks from the [hardware database]{.idx}: text files
under `hwdb.d` that match a device by its identifiers and give it properties, such as a laptop's
key mapping, a touchpad's real axis ranges, or a vendor and model name. udev does not read the text
files at run time. It reads a compiled trie, `hwdb.bin`, and eudev ships the text files and no
trie. `script/phases/70_image/060_udev_hwdb.sh` compiles the trie over the finished tree and checks
the result.

## Why the trie matters

Many of eudev's rules begin with `IMPORT{builtin}="hwdb ..."`. With no `hwdb.bin` that import
returns nothing and logs nothing above debug level. Nothing fails; the machine behaves as
hardware with no quirks and no names: `60-keyboard.hwdb`'s laptop key mappings for brightness, wifi
and volume never apply, `60-evdev.hwdb`'s `EVDEV_ABS_*` values never reach libinput, which then
sizes those touchpads from the wrong ranges, and `ID_VENDOR_FROM_DATABASE` and
`ID_MODEL_FROM_DATABASE` are missing from every PCI and USB device.

kpkg's hwdb trigger keeps the trie current as packages are installed and removed during the
build. This step rebuilds it from nothing over the finished tree, so the image never carries a trie
that an interrupted build left half-written.

## The step

<!-- kfs:include script/phases/70_image/060_udev_hwdb.sh lines=42-50 pin=2cbbfe4f507b -->

`DIRS` names the two directories udevadm reads source files from, in its own order; the step uses
the list only to count and report what is there. Without eudev's `udevadm`, the step says so and
exits 0.

<!-- kfs:include script/phases/70_image/060_udev_hwdb.sh lines=52-62 pin=ee0c49f10be2 -->

The source count is the check. Given no `*.hwdb` file at all, `udevadm hwdb --update` still writes
an 80-byte header for an empty trie and exits 0, so neither its status nor the file's existence
would show that nothing was compiled.

<!-- kfs:include script/phases/70_image/060_udev_hwdb.sh lines=64-76 pin=1723716fd49d -->

The old trie is removed first, so the file measured afterwards is the one this run wrote. A trie
that was not written stops the phase. The log ends with its size.

## Why /etc/udev and not /usr/lib/udev

`udevadm hwdb --update` without `--usr` writes [`/etc/udev/hwdb.bin`]{.idx}. libudev reads that path first
and its library directory's `hwdb.bin` second. The [eudev](#port-eudev) port's
`--with-rootlibdir=/lib` makes that second path `/lib/udev/hwdb.bin`, which is
`/usr/lib/udev/hwdb.bin` through the merged-`/usr` link. The initramfs build copies `/usr/lib/udev`
whole, so a trie there would be loaded into memory at every boot for an early stage that uses no
hwdb property; in [`/etc/udev`]{.idx} it stays in the root filesystem.

## Why it runs here

After [040_orphans.sh](040-orphans.md), so a removed package's `hwdb.d` files are not compiled in,
and before `090_initramfs.sh` and `110_iso.sh`, which carry the tree into the image.

## What it leaves

`/etc/udev/hwdb.bin` on the image.

## See also

- [Databases stamped into the image](../../kdos/05-developer/build-system.md#databases-stamped-into-the-image): the three indexes this phase writes
- [The device groups and the udev rules](../../kdos/02-user-guide/administration.md#the-device-groups-and-the-udev-rules): udev on the running system
- [Shared indexes](../../kdos/05-developer/writing-ports.md#shared-indexes): kpkg's triggers

<!-- book-nav -->
---

*Part XV. The image, chapter 348.* Previous: [347. Seeding the theme](050-theme.md) · [Contents](../README.md) · Next: [349. Creating the homes](070-user.md)
