# zram and swap

A KDOS machine swaps first to [zram]{.idx}, a compressed block device held in RAM, and then to any
[swap]{.idx} partition or swap file listed in [`/etc/fstab`]{.idx}. This page shows where each is set up, how to
see what is in use, how to resize or turn off zram, and how to add or remove a swap file on an
installed system. What zram does to zswap and to swap read-ahead, and why, is in the KDOS book's
[Storage](../../kdos/02-user-guide/administration.md#storage) section; this page does not repeat
it.

## Installation

Nothing needs installing. The zram device is made by [`/etc/init.d/12_zram.sh`]{.idx}, which comes from the
tree's `fs/` overlay rather than from a port, and its programs come from
[util-linux](#port-util-linux): [`swapon`]{.idx}, [`swapoff`]{.idx}, `mkswap`, [`zramctl`]{.idx}, `findmnt` and
[`fallocate`]{.idx}. A swap file on btrfs is made with `btrfs` from [btrfs-progs](#port-btrfs-progs). The
driver is a kernel module of the [linux](#port-linux) port, `zram`, which the script loads itself.

The service is enabled on the image: `/etc/service.disabled/` holds no `zram` marker, so `rcS` runs
the script at every boot, on the live medium and on an installed system alike. It is one of the
scripts that start no daemon; see
[Services with no daemon](../../kdos/02-user-guide/administration.md#services-with-no-daemon).

Disk swap is the installer's choice. On the erase plan it offers a swap file, a swap partition or
none (see [Swap](../../kdos/02-user-guide/installation.md#swap)), and writes the `/etc/fstab` line
for it. The reuse plan shows no swap choice and installs with the erase plan's last setting, a
4096 MiB swap file unless it was changed; a swap *partition* setting installs no swap there, since
only the erase plan makes partitions. The live medium has no disk swap.

## Configuration

**`/etc/kdos/zram.conf`**, from `fs/etc/kdos/zram.conf`. Both keys are optional; a missing file
means the defaults. Every key, its range and when it applies are in
[`/etc/kdos/zram.conf`](../../kdos/06-reference/configuration.md#etckdoszramconf).

<!-- kfs:include fs/etc/kdos/zram.conf -->

**`/etc/init.d/12_zram.sh`**, from `fs/etc/init.d/12_zram.sh`, reads that file. The part of `start`
that sizes the device, makes it swap at priority 100 and then changes the two kernel settings:

<!-- kfs:include fs/etc/init.d/12_zram.sh lines=69-98 pin=4b429f69db28 -->

**`/etc/fstab`** carries any disk swap. The image's own `fs/etc/fstab` has no swap line; the
installer writes its own lines, swap among them, ahead of the image's, in one of these two forms,
from `src/system/kdos-installer/install.c`:

<!-- kfs:include src/system/kdos-installer/install.c lines=1639-1646 pin=99d9a475fbf5 -->

`rcS` turns on every swap line of `/etc/fstab` once the filesystems are mounted, from
`fs/etc/init.d/rcS`:

<!-- kfs:include fs/etc/init.d/rcS lines=106-109 pin=99061a33a0a0 -->

and `/etc/inittab`, from `fs/etc/inittab`, turns all swap off again at power-off and reboot, after
the services are stopped and before the filesystems are unmounted:

<!-- kfs:include fs/etc/inittab lines=57-61 pin=3481280b9e99 -->

## Usage

### Seeing what swap is in use

1. List every active swap area with its priority:

   ```bash
   swapon --show
   ```

   [`/dev/zram0`]{.idx} is listed at priority 100. A swap file or partition from `/etc/fstab` follows it at
   a lower priority, so the kernel uses it only once zram is full.

2. Show how much the zram device holds and how well it compresses:

   ```bash
   zramctl
   ```

   `DISKSIZE` is the size the script worked out from `size` in `zram.conf`, `DATA` what has been
   swapped to the device, `COMPR` the space that data takes once compressed, and `TOTAL` the RAM
   the device occupies, its own bookkeeping included.

3. Ask the service itself:

   ```bash
   service status zram
   ```

   It prints `zram: /dev/zram0 active, <bytes> bytes nominal`. With exit status 1 it prints
   `zram: /dev/zram0 present but not swap` when the device exists but is not swap, as after `service stop zram`, and
   `zram: no zram device` when the module is not loaded.

### Changing the size or the compression algorithm

1. Edit the file:

   ```bash
   sudoedit /etc/kdos/zram.conf
   ```

2. Apply it by rebooting, or at once with:

   ```bash
   sudo service restart zram
   ```

   `stop` runs `swapoff` on the device, which has to bring every page held in it back into RAM
   before the device can be reset; on a machine that is short of memory, reboot instead. If that
   `swapoff` fails, the restart keeps the old device, prints `[FAIL] zram: swapoff /dev/zram0 — it
   is still swap` and then `zram: /dev/zram0 is already swap` (see [Troubleshooting](#troubleshooting)).

3. Check the result with `zramctl`. An algorithm the kernel does not carry, or a `size` outside
   1–90, is reported in the script's output and replaced (see
   [Troubleshooting](#troubleshooting)).

On the live medium the edit lasts until power-off, like every other change to `/etc`, unless the
medium has a persistence store ([kdos persist](../../kdos/04-programs/kdos-command.md#kdos-persist)).

### Turning zram off

1. Stop it at once; zswap and the kernel's swap read-ahead are put back as the service stops:

   ```bash
   sudo service stop zram
   ```

2. Keep it off at boot:

   ```bash
   sudo service disable zram
   ```

   This creates [`/etc/service.disabled/zram`]{.idx}. `sudo service enable zram` removes it again. Type the
   name exactly; see
   [`/etc/service.disabled/<name>`](../../kdos/06-reference/configuration.md#etcservicedisabledname).

### Adding a swap file on an installed system

An installed machine may already have one: the installer's swap file is `/swapfile`, and
`swapon --show` lists it. The commands below use that name, so pick another, in every step, to add
a second file.

The way the file is made depends on the root filesystem. `swapon` refuses a file made the wrong
way, and at boot `rcS` throws that error away, so a wrong file in `/etc/fstab` leaves the machine
with no disk swap and no message on the screen.

1. Find the filesystem type of `/`:

   ```bash
   findmnt -no FSTYPE /
   ```

2. Create the file, here 4 GiB, with the command for that filesystem.

   On **ext4**:

   ```bash
   sudo fallocate -l 4G /swapfile
   sudo chmod 600 /swapfile
   sudo mkswap /swapfile
   ```

   On **xfs** and **f2fs** the installer writes the file out in full, and this does the same. The
   KDOS kernel's `swapon` also accepts a file `fallocate` made on either, so the ext4 commands work
   there too; `dd` is the form that works whatever the kernel:

   ```bash
   sudo dd if=/dev/zero of=/swapfile bs=1M count=4096
   sudo chmod 600 /swapfile
   sudo mkswap /swapfile
   ```

   On **btrfs**, which needs a file that is neither copy-on-write nor compressed, and answers
   `swapon failed: Invalid argument` for one `fallocate` made; this one command makes it and
   formats it as swap:

   ```bash
   sudo btrfs filesystem mkswapfile -s 4G /swapfile
   ```

   These are the three ways the installer uses, chosen per filesystem in
   `src/system/kdos-installer/conf.c`.

3. Turn it on:

   ```bash
   sudo swapon /swapfile
   ```

4. Add it to `/etc/fstab` so that `rcS` turns it on at every boot, in the form the installer
   writes:

   ```bash
   printf '/swapfile\tnone\tswap\tdefaults\t0 0\n' | sudo tee -a /etc/fstab
   ```

5. Check with `swapon --show`.

### Removing a swap file

1. `sudo swapoff /swapfile`
2. Delete its line from `/etc/fstab` with `sudoedit /etc/fstab`.
3. `sudo rm /swapfile`

## Tips

- zram's `size` is the most the device may claim to hold, not the memory it occupies. Raising it
  does not reserve RAM; the compressed pages take RAM only as they arrive.
- A full swap is not what [kdos-oomd](out-of-memory.md) watches. It acts when the machine spends
  time stalled on memory, which is what the desktop feels, and `MemAvailable` is at most a tenth
  of `MemTotal` as well ([how it decides](../../kdos/04-programs/daemons.md#how-it-decides)),
  whether or not swap is full.
- `kdos explain swapfile` prints the reasoning behind the three ways of making a swap file.

## Troubleshooting

**`swapon --show` lists no `/dev/zram0`.** Read the script's boot output:

```bash
cat /run/kdos-init.zram.log
```

- `[SKIP] zram: the zram module will not load`: the kernel has no `zram` module, or the modules of
  the running kernel are not installed. Check that `/usr/lib/modules/$(uname -r)` exists, then try
  `sudo modprobe zram` and read its error.
- `[FAIL] zram: the kernel refused a N% disksize`, `mkswap /dev/zram0` or `swapon /dev/zram0`: the
  step named failed. `sudo dmesg | tail` usually says why; the kernel lets only root read its
  log.
- No log at all: the service is disabled. `ls /etc/service.disabled/` shows a `zram` marker;
  `sudo service enable zram` removes it.

**The log says `no '<name>' backend in this kernel — keeping the default`.** The algorithm in
`zram.conf` is not one this kernel offers. The KDOS kernel carries all six that `zram.conf` lists,
so on it this is usually a misspelt name. The script reads a value only up to its first character
that is not a letter or a digit, so `lzo-rle` is read as `lzo`. List the algorithms the kernel
offers; the bracketed name is in use:

```bash
cat /sys/block/zram0/comp_algorithm
```

**The log says `size N is outside 1-90, using 50`.** `size` is a percentage of RAM, not a byte
count. A value with a letter in it, such as `4G`, is ignored without a message and the device is
made at 50 per cent; `zramctl` shows the size it got.

**`/etc/fstab` lists a swap file, but after a boot there is no disk swap.** Turn it on by hand to
see the error the boot threw away:

```bash
sudo swapon /swapfile
```

On btrfs, `swapon failed: Invalid argument` means the file is copy-on-write or compressed, which
is what `btrfs filesystem mkswapfile` avoids. Remove the file and make it again as
[above](#adding-a-swap-file-on-an-installed-system).

**`service restart zram` hangs, or prints `[FAIL] zram: swapoff /dev/zram0 — it is still swap`.**
`swapoff` could not bring the pages held in zram back into RAM. `stop` exits 1 at that point and
touches nothing else: the device keeps its old size and algorithm, zswap stays off and
`vm.page-cluster` stays 0. `restart` runs `start` anyway, which finds the device still in use and
prints `zram: /dev/zram0 is already swap` after the failure. Close some programs and run the
restart again, or reboot.

## Known gaps

[Known gaps](../../kdos/06-reference/known-gaps.md) has no entry for zram or swap. Hibernation is
not offered at all; [What happens around a suspend](../../kdos/04-programs/daemons.md#what-happens-around-a-suspend)
says why.

## See also

- [Storage](../../kdos/02-user-guide/administration.md#storage): zram, zswap, `vm.page-cluster` and the boot-time swap
- [`/etc/kdos/zram.conf`](../../kdos/06-reference/configuration.md#etckdoszramconf): the keys, their ranges and when they apply
- [Swap](../../kdos/02-user-guide/installation.md#swap): what the installer offers
- [Shutdown](../../kdos/03-architecture/boot-and-init.md#shutdown): where swap is turned off
- [The live medium and persistence](../../kdos/03-architecture/boot-and-init.md#the-live-medium-and-persistence): why a change on the live medium does not last
- [Out of memory](out-of-memory.md): what acts when swap is not enough
- [Services](services.md): `service` and the disable markers
- [Filesystems and fstab](filesystems-and-fstab.md): the rest of `/etc/fstab`
- [Live persistence](live-persistence.md): keeping a change on the live medium

<!-- book-nav -->
---

*Part XVI. System administration, chapter 364.* Previous: [363. Modules and sysctl](modules-and-sysctl.md) · [Contents](../README.md) · Next: [365. Out of memory](out-of-memory.md)
