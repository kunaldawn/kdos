## About

Solid is KDE's hardware abstraction: one API for the storage devices, partitions, optical drives,
batteries, cameras, media players and network shares a machine has, with mounting, unmounting and
ejecting. Each kind of information comes from a backend: udev for devices, udisks2 for storage and
mounting, UPower for batteries and AC power, `/etc/fstab` and the mount table for network shares,
libimobiledevice for iPhones and iPads, and KDE Connect for paired phones. `solid-hardware6`
queries it from the shell. [dolphin](#port-dolphin)'s Places panel, [kio](#port-kio), the Plasma
device notifier and battery applet, and [digikam](#port-digikam)'s camera import are built on it.

## Flags

- `-D UDEV_DISABLED=OFF` — Use udev, which the configure then requires on Linux; it is upstream's
  default. The udev backend and, with it, the udisks2 backend are built only with udev, so with
  udev disabled Solid would see no storage device at all. libudev comes from [eudev](#port-eudev).
- `-D CMAKE_REQUIRE_FIND_PACKAGE_IMobileDevice=ON` — With
  `-D CMAKE_REQUIRE_FIND_PACKAGE_PList=ON`: [libimobiledevice](#port-libimobiledevice) and
  [libplist](#port-libplist) are optional upstream, and without both the backend for Apple
  devices is left out with only a line in the configure summary. Required, a missing one stops
  the configure.
- `-D KF_SKIP_PO_PROCESSING=ON` — Build none of the Qt translation catalogues under `poqm/`,
  which `ecm_install_po_files_as_qm()` would compile into `.qm` files. The strings in the source
  are English, and the package-wide English-only rule (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
  would remove every catalogue in another language.

## Notes

The device predicate parser is generated with [flex](#port-flex) and [bison](#port-bison), and
the udisks2 backend reads mount points through libmount from [util-linux](#port-util-linux); all
three are requirements of the configure. [udisks2](#port-udisks2) and [upower](#port-upower) are
run-time dependencies, reached over the system bus: without `udisksd` no disk can be mounted from
a KDE program, and without `upowerd` no battery is shown. The configure also looks for the data
files of media-player-info, a run-time option of the udev backend that names portable music
players; it is not a port, so such players are seen as plain storage.
