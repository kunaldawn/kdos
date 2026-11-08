## About

KDiskFree lists every mounted filesystem with its type, size, free space, usage bar and mount
point, and mounts or unmounts the entries of `/etc/fstab` with a click. The port installs three
front ends over one private library: the `kdf` window, `kwikdisk`, a tray icon with the same list
as a menu, and a Storage Devices page for System Settings.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — The handbook, through
  [kdoctools](#port-kdoctools), which upstream looks for as optional; required, a missing one
  stops the configure instead of leaving the handbook out.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The list is the output of `df -kT`, run as a child process and parsed by column, and the mount
points are read from `/etc/fstab`. The `-T` column is what gives each row its type, so the program
needs a `df` that has it: [coreutils](#port-coreutils)' in `/usr/bin` does, and it is in
`depends` for run time only. Mounting runs `mount %d` and unmounting `umount %d` through the shell
command line kdf builds, which [util-linux](#port-util-linux) answers; an ordinary user can mount
only the `fstab` entries marked `user`.

The tray icon is a StatusNotifierItem, so `kwikdisk` shows only where a tray host is running, as
it is in the Plasma panel.
