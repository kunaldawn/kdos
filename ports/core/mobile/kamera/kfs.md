## About

kamera gives KDE programs access to digital cameras through gPhoto2. It is the `camera:/` KIO
worker, which Dolphin and Gwenview browse and copy pictures from, a System Settings page that lists
and configures the cameras gPhoto2 sees, and a Solid action that offers to open a camera when one is
plugged in.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — Make [kdoctools](#port-kdoctools) a
  requirement. Upstream finds it as optional and drops the settings page's handbook without one.
- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`, which kpkg's
  English-only rule would remove from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Notes

The bundled `FindGphoto2.cmake` asks `gphoto2-config` and `gphoto2-port-config` from
[libgphoto2](#port-libgphoto2) for its flags, through `exec_program()`. That command is a
deprecated one CMake still runs under the project's 3.16 policy level, with a warning.

A camera is a USB device, and only a user allowed to open its `/dev/bus/usb` node can reach it.
KDOS's udev rules give PTP and MTP cameras to the `dialout` group.

The Solid action runs `kioclient5 exec camera:/`; `kioclient5` is the compatibility link
kde-cli-tools installs beside `kioclient`, so the action works in the Plasma session, where that
port is installed.
