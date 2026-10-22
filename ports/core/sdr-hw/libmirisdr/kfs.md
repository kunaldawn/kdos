## About

libmirisdr is a user-space driver library for receivers built on the Mirics MSi2500 USB bridge and
MSi001 tuner, the chips of the early SDRplay receivers among others. It talks to the device
over [libusb](#port-libusb), and installs `libmirisdr.so`, its header and pkg-config file, and the
`miri_sdr` and `miri_fm` tools. [gr-osmosdr](#port-gr-osmosdr) and [sdrangel](#port-sdrangel)
use it. The source is libmirisdr-4, a further development of Miroslav Slugen's libmirisdr-2
and its libmirisdr-3-bsz fork.

## Flags

- `-DLIB_SUFFIX=` — Install the library into `lib` with nothing appended. The project's
  `CMakeLists.txt` writes `lib${LIB_SUFFIX}` for both the install path and the pkg-config
  `libdir`, and does not use `GNUInstallDirs`, so `CMAKE_INSTALL_LIBDIR` has no effect on it.
- `-DCMAKE_SKIP_INSTALL_RPATH=ON` — Install the library and tools with no `RPATH`. The project
  sets its install `RPATH` to `${CMAKE_INSTALL_PREFIX}/lib`, which is `/usr/lib`, a directory the
  loader searches anyway.
- `-DDETACH_KERNEL_DRIVER=ON` — When a kernel driver has claimed the device's USB interface (Linux
  has one for this hardware, `msi2500`, which presents it as a V4L2 device), detach it when the
  device is opened and reattach it when it is closed. Off upstream, where opening the device would
  fail while that driver holds it.
- `-DMIRISDR_PC_VERSION=$version` — The version written into `libmirisdr.pc`. The option exists
  only with this port's patch, `pc-version.patch`.

## Patches

- `pc-version.patch` — After `include(Version)` in `CMakeLists.txt`, sets `VERSION` from `MIRISDR_PC_VERSION` when that is given. Upstream marks its patch level as `git`, and its `Version.cmake` then takes `VERSION` from `git describe`. A release tarball is not a git checkout, so that is either empty or, with no git at all, the placeholder `v4.0.x-xxx-xunknown`, and either one ends up on the `Version:` line of `libmirisdr.pc`.

## Notes

The project always builds a static library beside the shared one; `build.sh` removes
`libmirisdr.a` after the install.

The three `test` lines and the `grep` at the end fail the build unless the shared library, the
header, the `miri_sdr` program and a non-empty `Version:` line in the pkg-config file are all in
the package. The `grep` is the one that proves the patch took effect.

Access to the device for a user comes from the tree's udev rules, which give the SDRplay vendor id
to the `dialout` group (see
[The device groups and the udev rules](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/02-user-guide/administration.md#the-device-groups-and-the-udev-rules)).
