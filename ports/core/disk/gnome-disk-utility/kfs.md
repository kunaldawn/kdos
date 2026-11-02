## About

GNOME Disks partitions, formats, encrypts, images, restores and benchmarks drives, and reads
their SMART data, all through [udisks2](#port-udisks2). It is a GTK 3 application installed beside
Plasma's own disk tools for what they do not do: writing and restoring whole-disk images and
benchmarking a drive. `gnome-disk-image-mounter` mounts an image file as a loop device.

## Flags

- `--wrap-mode=nodownload` — Fail the configure on a missing dependency instead of letting meson
  look for a subproject to fetch.
- `-Dlogind=libelogind` — Link [elogind](#port-elogind)'s `libelogind` (upstream's choices are
  `libsystemd`, `libelogind` and `none`). Disks asks `sd-login` which seat its own session is on
  and, in its "attached to another seat" check, leaves out a drive udisks tags with a different
  seat. A drive with no seat tag, which is every drive on a single-seat machine, is shown either
  way.
- `-Dgsd_plugin=false` — Build no plug-in for `gnome-settings-daemon`, which warns about failing
  disks in a GNOME session. KDOS runs no GNOME session.
- `-Dman=true` — Install `gnome-disks(1)` and `gnome-disk-image-mounter(1)`, built with
  [libxslt](#port-libxslt) and [docbook-xsl](#port-docbook-xsl).

## Notes

`build.sh` removes `gnome-disk-image-mounter.desktop` and `gnome-disk-image-writer.desktop`,
hidden entries that exist only to claim `application/x-cd-image` and the raw disk-image types.
Installed, they would compete for those types with the handler `mimeapps.list` names, and an
image would open in whichever entry sorted first.

It also replaces upstream's `org.gnome.DiskUtility.desktop` with its own, named
*Disks (GNOME)* so that the menu does not show two rows called *Disks*, and without
`DBusActivatable=true`: no D-Bus service file is installed for the application, so an activatable
entry would start nothing. Upstream installs the icon as SVG only; `build.sh` renders it to PNG in
three hicolor sizes with [librsvg](#port-librsvg)'s `rsvg-convert`.
