## About

Plasma Camera is the Plasma Mobile camera: it takes photos and records video from any camera
[libcamera](#port-libcamera) drives, laptop UVC webcams included, and tags photos with EXIF data
through [exiv2](#port-exiv2). The interface is Kirigami, made for a phone held either way up, and
it follows the device's orientation through [qt6-qtsensors](#port-qt6-qtsensors).

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Patches

`app-id.patch` is KDOS's own and adds one line to upstream's desktop entry; the reason is in its
header. The fix that belongs upstream is the program naming its desktop file, which would make
the line unnecessary.

## Notes

The program opens the camera through libcamera, which reads `/dev/media*` and `/dev/video*`
itself; it goes through neither PipeWire nor the camera portal. The user therefore needs read and
write access to those device nodes, which udev gives the `video` group; the default account,
`kdos`, is a member (`fs/etc/group`).

`main.cpp` sets `QT_MEDIA_BACKEND=ffmpeg` before Qt Multimedia starts, so video is encoded by the
FFmpeg backend [qt6-qtmultimedia](#port-qt6-qtmultimedia) builds, in software. libcamera is
found through `pkg_check_modules(... REQUIRED)`, which needs [pkgconf](#port-pkgconf).

The entry's icon is `camera-photo`, a Breeze icon; upstream installs no icon of its own.
