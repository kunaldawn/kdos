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

## Notes

The check is by file name and covers KDE neon and Arch Linux (a detached `.sig` beside the image,
against the keys the port installs in `/usr/share/isoimagewriter`), Kubuntu and Ubuntu (a signed
`SHA256SUMS`), and Debian and Netrunner (a checksum the person pastes in). Any other image, a
KDOS one among them, is reported as not a known distribution's and can still be written.

The device is opened through UDisks2's `OpenDevice` call on the system bus, so
[udisks2](#port-udisks2), in `depends` for run time, must be running, and polkit decides the
`org.freedesktop.udisks2.open-device` action: `fs/etc/polkit-1/rules.d/50-kdos.rules` grants it
to the `wheel` group, and anyone else is asked for an administrator's password.
