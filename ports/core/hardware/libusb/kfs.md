## About

libusb is the user-space USB library: a program uses it to find a USB device and talk to it
directly, with control, bulk, interrupt and isochronous transfers, without a kernel driver of its
own. On KDOS it is what the software-defined-radio receivers ([airspy](#port-airspy) and the
rest), the device flashers and debuggers, [android-tools](#port-android-tools), the UPS drivers of
[nut](#port-nut) and many more use to reach their hardware.

It enumerates devices and receives hotplug events through `libudev`, which on KDOS is
[eudev](#port-eudev)'s. Upstream's `configure` makes udev the default on Linux and stops when
`libudev` is missing, so eudev is a dependency.

## Flags

- `-j1` — Run the compile with one job at a time, overriding the `-j$KDOS_JOBS` the tree exports
  in `MAKEFLAGS` for that one `make`. The install runs with the tree's usual parallelism.
