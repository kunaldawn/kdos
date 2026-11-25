## About

libblockdev is the library [udisks2](#port-udisks2) does its block-device work through: a plugin
per technology (partitions, filesystems, LVM, MD RAID, encryption, NVMe, SMART), loaded by the
soname lists in `/etc/libblockdev/3/conf.d/00-default.cfg`. The `smart` plugin is
`libbd_smart.so.3`, built on [libatasmart](#port-libatasmart), with `libbd_smartmontools.so.3`
as its fallback.

## Patches

`c-locale-strerror.patch` defines the `_C_LOCALE` that the fs, NVMe, SMART and device-utility
code pass to `strerror_l()` as `newlocale (LC_ALL_MASK, "C", (locale_t) 0)`. Upstream defines it
as `(locale_t) 0`. glibc's `strerror_l()` takes a null locale as the current one, but musl's
reads the locale's `LC_MESSAGES` slot, so every error path that formats `errno` faults at address
`0x28` inside `libc.so`. udisksd hits one on every SATA disk whose SMART data cannot be read,
QEMU's emulated AHCI disk among them, and dies on the worker thread that refreshes SMART; the bus
starts it again on the next call and it crashes again, so Solid, Dolphin's Places panel and the
Device Notifier lose every device. `newlocale()` of `"C"` with no base returns the C library's
static C locale on both C libraries, so the macro allocates nothing and cannot fail. The patch is
to be offered upstream.
